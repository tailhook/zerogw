#include <zmq.h>
#include <malloc.h>
#include <strings.h>
#include <sys/queue.h>

#include "websocket.h"
#include "log.h"
#include "zutils.h"
#include "resolve.h"
#include "uidgen.h"
#include "device.h"

typedef struct topic_s {
    struct topic_hash_s *table;
    LIST_ENTRY(topic_s) topic_list;
    LIST_HEAD(topic_subsribers_s, subscriber_s) subscribers;
    size_t hash;
    size_t length;
    char topic[];
} topic_t;

typedef struct topic_hash_s {
    int hash_size;
    int ntopics;
    int nsubscriptions;
    topic_t **table;
} topic_hash_t;

LIST_HEAD(topic_head, topic_s);

void free_topic(topic_t *topic) {
    // Frees both: empty/unused topics and forced free with unsubscribe
    subscriber_t *sub, *nxt;
    for (sub = LIST_FIRST(&topic->subscribers); sub; sub = nxt) {
        nxt = LIST_NEXT(sub, topic_list);
        LIST_REMOVE(sub, topic_list);
        LIST_REMOVE(sub, client_list);
        free(sub);
        topic->table->nsubscriptions -= 1;
        root.stat.websock_unsubscribed += 1;
    }
    LIST_REMOVE(topic, topic_list);
    topic->table->ntopics -= 1;
    root.stat.topics_removed += 1;
    free(topic);
}

void websock_stop(ws_connection_t *hint) {
    connection_t *conn = (connection_t *)hint;
    root.stat.websock_disconnects += 1;
    root.stat.disconnects += 1;
    hybi_stop(conn->hybi);
}

void hybi_stop(hybi_t *hybi) {
    sieve_empty(root.hybi_sieve, UID_HOLE(hybi->uid));

    zmq_msg_t zmsg;
    STREAMER_SEND_LOOP(&hybi->route->websocket._device) {
        SNIMPL(zmq_msg_init_size(&zmsg, UID_LEN));
        memcpy(zmq_msg_data(&zmsg), hybi->uid, UID_LEN);
        STREAMER_SEND(&zmsg);
        SNIMPL(zmq_msg_init_size(&zmsg, 10));
        memcpy(zmq_msg_data(&zmsg), "disconnect", 10);
        STREAMER_SEND_LAST(&zmsg);
    }
    if(FALSE) {
streamer_error: // can't do anything very useful
        TWARN("Couldn't send disconnect message");
    }
    LDEBUG("Websocket sent disconnect");

    subscriber_t *sub, *nxt;
    for (sub = LIST_FIRST(&hybi->subscribers); sub; sub = nxt) {
        nxt = LIST_NEXT(sub, client_list);
        LIST_REMOVE(sub, client_list);
        LIST_REMOVE(sub, topic_list);
        topic_t *topic = sub->topic;
        topic->table->nsubscriptions -= 1;
        root.stat.websock_unsubscribed += 1;
        if(!LIST_FIRST(&topic->subscribers)) {
            free_topic(topic);
        }
        free(sub);
    }
    free(hybi);
}

int websock_start(connection_t *conn, config_Route_t *route) {
    LDEBUG("Websocket started");
    root.stat.websock_connects += 1;
    hybi_t *hybi = hybi_start(route, HYBI_WEBSOCKET);
    hybi->conn = conn;
    conn->hybi = hybi;
    ws_DISCONNECT_CB(&conn->ws, websock_stop);
}

hybi_t *hybi_start(config_Route_t *route, hybi_enum type) {
    hybi_t *hybi;
    if(type == HYBI_COMET) {
        int qsize = route->websocket.polling_fallback.queue_limit;
        hybi = (hybi_t*)malloc(sizeof(hybi_t) + sizeof(comet_t)
            + qsize * sizeof(message_t *));
        ANIMPL(hybi); //FIXME
        hybi->comet->queue_size = qsize;
    } else {
        hybi = (hybi_t*)malloc(sizeof(hybi_t));
    }
    ANIMPL(hybi); //FIXME
    hybi->type = type;
    SNIMPL(make_hole_uid(hybi, hybi->uid, root.hybi_sieve, type == HYBI_COMET));
    LIST_INIT(&hybi->subscribers);
    hybi->route = route;

    zmq_msg_t zmsg;
    STREAMER_SEND_LOOP(&hybi->route->websocket._device) {
        SNIMPL(zmq_msg_init_size(&zmsg, UID_LEN));
        memcpy(zmq_msg_data(&zmsg), hybi->uid, UID_LEN);
        STREAMER_SEND(&zmsg);
        SNIMPL(zmq_msg_init_size(&zmsg, 7));
        memcpy(zmq_msg_data(&zmsg), "connect", 7);
        STREAMER_SEND_LAST(&zmsg);
    }
    LDEBUG("Websocket sent hello");
    return hybi;

streamer_error:
    TWARN("Failed to queue connect message");
    //TODO: close connection
    return hybi;
}

void websock_free_message(void *data, void *hint) {
    message_t *msg = (message_t *)hint;
    ws_MESSAGE_DECREF(&msg->ws);
}

int websock_message(connection_t *conn, message_t *msg) {
    root.stat.websock_received += 1;
    zmq_msg_t zmsg;
    STREAMER_SEND_LOOP(&conn->hybi->route->websocket._device) {
        SNIMPL(zmq_msg_init_size(&zmsg, UID_LEN));
        memcpy(zmq_msg_data(&zmsg), conn->hybi->uid, UID_LEN);
        STREAMER_SEND(&zmsg);
        SNIMPL(zmq_msg_init_size(&zmsg, 7));
        memcpy(zmq_msg_data(&zmsg), "message", 7);
        STREAMER_SEND(&zmsg);
        ws_MESSAGE_INCREF(&msg->ws);
        SNIMPL(zmq_msg_init_data(&zmsg, msg->ws.data, msg->ws.length,
            websock_free_message, msg));
        STREAMER_SEND_LAST(&zmsg);
    }
    return 0;
streamer_error:
    return -1;
}

hybi_t *hybi_find(char *data) {
    hybi_t *hybi = (hybi_t *)sieve_get(root.hybi_sieve, UID_HOLE(data));
    LDEBUG("Searching for hole %d", UID_HOLE(data));
    if(!hybi || !UID_EQ(hybi->uid, data)) return NULL;
    return hybi;
}

size_t topic_hash(const char *s, size_t len) {
    size_t res = 0;
    while (len--) {
	    res += (res<<1) + (res<<4) + (res<<7) + (res<<8) + (res<<24);
    	res ^= (size_t)*s++;
    }
    return res;
}

static topic_t *mktopic(topic_hash_t *table, char *name, int len, size_t hash) {
    topic_t * result = (topic_t *)malloc(sizeof(topic_t) + len);
    if(!result) return NULL;
    result->table = table;
    result->hash = hash;
    root.stat.topics_created += 1;
    memcpy(result->topic, name, len);
    result->length = len;
    LIST_INIT(&result->subscribers);
    return result;
}

static topic_t *find_topic(config_Route_t *route, zmq_msg_t *msg, bool create) {
    char *topic_name = zmq_msg_data(msg);
    int topic_len = zmq_msg_size(msg);
    size_t hash = topic_hash(topic_name, topic_len);
    topic_t *result;
    topic_hash_t *table = route->websocket._topics;
    if(!route->websocket._topics) {
        if(!create) return NULL;
        table = malloc(sizeof(topic_hash_t));
        if(!table) return NULL;
        bzero(table, sizeof(topic_hash_t));
        route->websocket._topics = table;
    }
    if(!table->table) {
        if(!create) return NULL;
        table->hash_size = route->websocket.topic_hash_size;
        table->table = (topic_t **)malloc(
            route->websocket.topic_hash_size*sizeof(topic_t*));
        bzero(table->table, route->websocket.topic_hash_size*sizeof(topic_t*));
        if(!table->table) return NULL;
        table->ntopics = 1;
        table->nsubscriptions = 0;
        result = mktopic(table, topic_name, topic_len, hash);
        int bucket = hash % table->hash_size;
        LIST_INSERT_HEAD((struct topic_head *)&table->table[bucket],
            result, topic_list);
        return result;
    }
    size_t cell =  hash % table->hash_size;
    topic_t *current = LIST_FIRST((struct topic_head *)&table->table[cell]);
    if(!current) {
        if(!create) return NULL;
        result = mktopic(table, topic_name, topic_len, hash);
        table->ntopics += 1;
        LIST_INSERT_HEAD((struct topic_head *)&table->table[cell],
            result, topic_list);
        return result;
    }
    LIST_FOREACH(current, (struct topic_head *)&table->table[cell], topic_list) {
        if(current->length == topic_len && current->hash == hash
            && !memcmp(current->topic, topic_name, topic_len)) {
            return current;
        }
    }
    if(!create) return NULL;
    table->ntopics += 1;
    result = mktopic(table, topic_name, topic_len, hash);
    LIST_INSERT_HEAD((struct topic_head *)&table->table[cell],
        result, topic_list);
    return result;
}

static bool topic_subscribe(hybi_t *hybi, topic_t *topic) {
    subscriber_t *sub = (subscriber_t *)malloc(sizeof(subscriber_t));
    if(!sub) {
        return FALSE;
    }
    sub->topic = topic;
    sub->connection = hybi;
    LIST_INSERT_HEAD(&topic->subscribers, sub, topic_list);
    LIST_INSERT_HEAD(&hybi->subscribers, sub, client_list);
    topic->table->nsubscriptions += 1;
    root.stat.websock_subscribed += 1;
    LDEBUG("Subscription done, topics: %d, subscriptions: %d",
        topic->table->ntopics, topic->table->nsubscriptions);
    return TRUE;
}

static bool topic_unsubscribe(hybi_t *hybi, topic_t *topic) {
    // It's probably faster to search user topics, because user doesn't
    // expected to have thousands of topics. But topic can actually have
    // thousands of subscibers (very popular news feed or chat room)
    subscriber_t *sub;
    LIST_FOREACH(sub, &hybi->subscribers, client_list)
        if(sub->topic == topic) break;
    if(!sub) return FALSE;
    LIST_REMOVE(sub, topic_list);
    LIST_REMOVE(sub, client_list);
    topic_hash_t *tbl = topic->table;
    if(!LIST_FIRST(&topic->subscribers)) {
        free_topic(topic);
    }
    free(sub);
    tbl->nsubscriptions -= 1;
    root.stat.websock_unsubscribed += 1;
    return TRUE;
}

static void websock_message_free(void *ws) {
    message_t *msg = ws;
    zmq_msg_close(&msg->zmq);
}

static bool topic_publish(topic_t *topic, zmq_msg_t *omsg) {
    subscriber_t *sub = LIST_FIRST(&topic->subscribers);
    // every connection are same, we use first so that libwebsite could
    // determine size of message, but reusing message for each subscriber
    message_t *msg = (message_t*)malloc(sizeof(message_t));
    ANIMPL(msg);
    SNIMPL(ws_message_init(&msg->ws));
    zmq_msg_init(&msg->zmq);
    LDEBUG("Sending %x [%d]``%.*s''", omsg,
        zmq_msg_size(omsg), zmq_msg_size(omsg), zmq_msg_data(omsg));
    zmq_msg_move(&msg->zmq, omsg);
    ws_MESSAGE_DATA(&msg->ws, (char *)zmq_msg_data(&msg->zmq),
        zmq_msg_size(&msg->zmq), websock_message_free);
    LDEBUG("Sending %x [%d]``%.*s''", msg,
        msg->ws.length, msg->ws.length, msg->ws.data);
    root.stat.websock_published += 1;
    LIST_FOREACH(sub, &topic->subscribers, topic_list) {
        root.stat.websock_sent += 1;
        if(sub->connection->type == HYBI_WEBSOCKET) {
            ws_message_send(&sub->connection->conn->ws, &msg->ws);
        } else if(sub->connection->type == HYBI_COMET) {
            comet_send(sub->connection, msg);
        } else {
            LNIMPL("Uknown connection type");
        }
    }
    ws_MESSAGE_DECREF(&msg->ws); // we own a single reference
}


void websock_process(struct ev_loop *loop, struct ev_io *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));
    config_Route_t *route = (config_Route_t *)((char *)watch
        - offsetof(config_Route_t, websocket.subscribe._watch));
    size_t opt, optlen = sizeof(opt);
    SNIMPL(zmq_getsockopt(route->websocket.subscribe._sock, ZMQ_EVENTS, &opt, &optlen));
    while(TRUE) {
        Z_SEQ_INIT(msg, route->websocket.subscribe._sock);
        Z_RECV_START(msg, break);
        char *cmd = zmq_msg_data(&msg);
        int cmdlen = zmq_msg_size(&msg);
        if(cmdlen == 9 && !strncmp(cmd, "subscribe", cmdlen)) {
            LDEBUG("Websocket got SUBSCRIBE request");
            Z_RECV_NEXT(msg);
            if(zmq_msg_size(&msg) != UID_LEN) goto msg_error;
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi) goto msg_error;
            Z_RECV_LAST(msg);
            topic_t *topic = find_topic(hybi->route, &msg, TRUE);
            if(topic) { // no topic on memory failure
                topic_subscribe(hybi, topic);
            } else {
                LDEBUG("Couldn't make topic");
            }
        } else if(cmdlen == 11 && !strncmp(cmd, "unsubscribe", cmdlen)) {
            LDEBUG("Websocket got UNSUBSCRIBE request");
            Z_RECV_NEXT(msg);
            if(zmq_msg_size(&msg) != UID_LEN) goto msg_error;
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi) goto msg_error;
            if(hybi->route != route) {
                TWARN("Connection has wrong route, skipping...");
                goto msg_error;
            }
            Z_RECV_LAST(msg);
            topic_t *topic = find_topic(route, &msg, FALSE);
            if(!topic) goto msg_error;
            topic_unsubscribe(hybi, topic);
        } else if(cmdlen == 7 && !strncmp(cmd, "publish", cmdlen)) {
            LDEBUG("Websocket got PUBLISH request");
            Z_RECV_NEXT(msg);
            topic_t *topic = find_topic(route, &msg, FALSE);
            if(!topic) {
                LDEBUG("Couldn't find topic to publish to");
                goto msg_error;
            }
            LDEBUG("Publishing to 0x%x", topic);
            Z_RECV_LAST(msg);
            topic_publish(topic, &msg);
        } else if(cmdlen == 4 && !strncmp(cmd, "send", cmdlen)) {
            LDEBUG("Websocket got SEND request");
            Z_RECV_NEXT(msg);
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi) goto msg_error;
            Z_RECV_LAST(msg);
            message_t *mm = (message_t*)malloc(sizeof(message_t));
            ANIMPL(mm);
            SNIMPL(ws_message_init(&mm->ws));
            zmq_msg_init(&mm->zmq);
            LDEBUG("Sending %x [%d]``%.*s''", &msg,
                zmq_msg_size(&msg), zmq_msg_size(&msg), zmq_msg_data(&msg));
            zmq_msg_move(&mm->zmq, &msg);
            ws_MESSAGE_DATA(&mm->ws, (char *)zmq_msg_data(&mm->zmq),
                zmq_msg_size(&mm->zmq), websock_message_free);
            root.stat.websock_sent += 1;
            if(hybi->type == HYBI_WEBSOCKET) {
                int r = ws_message_send(&hybi->conn->ws, &mm->ws);
                if(r < 0) {
                    if(errno == EXFULL) {
                        shutdown(hybi->conn->ws.watch.fd, SHUT_RDWR);
                    } else {
                        SNIMPL(r);
                    }
                }
            } else if(hybi->type == HYBI_COMET) {
                comet_send(hybi, mm);
            } else {
                LNIMPL("Uknown connection type");
            }
            ws_MESSAGE_DECREF(&mm->ws);
        } else if(cmdlen == 4 && !strncmp(cmd, "drop", cmdlen)) {
            LDEBUG("Websocket got DROP request");
            Z_RECV_LAST(msg);
            topic_t *topic = find_topic(route, &msg, FALSE);
            if(topic) {
                free_topic(topic);
            }
        } else {
            TWARN("Wrong command [%d]``%s''", cmdlen, cmd);
            goto msg_error;
        }
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        Z_SEQ_ERROR(msg);
        continue;
    }
}

int start_websocket(request_t *req) {
    request_init(req);
    config_Route_t *route = resolve_url(req);
    if(!route->websocket.subscribe.value_len
        || !route->websocket.forward.value_len) {
        return -1;
    }
    return websock_start((connection_t *)req->ws.conn, route);
}

static void heartbeat(struct ev_loop *loop,
    struct ev_timer *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));
    config_Route_t *route = (config_Route_t *)((char *)watch
        - offsetof(config_Route_t, websocket._heartbeat_timer));
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_data(&msg, root.instance_id, IID_LEN, NULL, NULL));
    if(zmq_send(route->websocket.forward._sock, &msg,
        ZMQ_SNDMORE|ZMQ_NOBLOCK) < 0) {
        if(errno != EAGAIN) { //TODO: EINTR???
            SNIMPL(-1);
        } else {
            return; // nevermind
        }
    }
    SNIMPL(zmq_msg_init_data(&msg, "heartbeat", 9, NULL, NULL));
    if(zmq_send(route->websocket.forward._sock, &msg, ZMQ_NOBLOCK) < 0) {
        if(errno != EAGAIN) { //TODO: EINTR???
            SNIMPL(-1);
        } // else: nevermind
    }
}

static int socket_visitor(config_Route_t *route) {
    if(route->websocket.subscribe.value_len) {
        SNIMPL(zmq_open(&route->websocket.subscribe,
            ZMASK_PULL|ZMASK_SUB, ZMQ_SUB, websock_process, root.loop));
        if(route->websocket.subscribe.kind == CONFIG_zmq_Sub
            || route->websocket.subscribe.kind == CONFIG_auto) {
            SNIMPL(zmq_setsockopt(route->websocket.subscribe._sock,
                ZMQ_SUBSCRIBE, NULL, 0));
        }
    }
    if(route->websocket.forward.value_len) {
        SNIMPL(zmq_open(&route->websocket.forward,
            ZMASK_PUSH|ZMASK_PUB, ZMQ_PUB, NULL, NULL));
        if(route->websocket.heartbeat_interval) {
            ev_timer_init(&route->websocket._heartbeat_timer, heartbeat,
                route->websocket.heartbeat_interval,
                route->websocket.heartbeat_interval);
            ev_timer_start(root.loop, &route->websocket._heartbeat_timer);
        }
        streamer_init(&route->websocket._device,
            route->websocket.forward._sock,
            route->websocket.max_backend_queue,
            &root.stat.websock_backend_queued,
            &root.stat.websock_backend_unqueued);
        LIST_INSERT_HEAD(&root.hybi_devices, &route->websocket._device, lst);
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        SNIMPL(socket_visitor(&item->value));
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        SNIMPL(socket_visitor(&item->value));
    }
    return 0;
}

static int socket_unvisitor(config_Route_t *route) {
    if(route->websocket._topics) {
        if(((topic_hash_t *)route->websocket._topics)->table) {
            free(((topic_hash_t *)route->websocket._topics)->table);
        }
        free(route->websocket._topics);
    }
    if(route->websocket.subscribe.value_len) {
        SNIMPL(z_close(&route->websocket.subscribe, root.loop));
    }
    if(route->websocket.forward.value_len) {
        SNIMPL(z_close(&route->websocket.forward, root.loop));
        if(route->websocket.heartbeat_interval) {
            ev_timer_stop(root.loop, &route->websocket._heartbeat_timer);
        }
        SNIMPL(streamer_close(&route->websocket._device));
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    return 0;
}

int pause_websockets(bool pause) {
    streamer_t *dev;
    LIST_FOREACH(dev, &root.hybi_devices, lst) {
        streamer_pause(dev, pause);
    }
}

int prepare_websockets(config_main_t *config, config_Route_t *rroot) {
    LIST_INIT(&root.hybi_devices);
    SNIMPL(socket_visitor(rroot));
    LINFO("Websocket connections complete");
    return 0;
}

int release_websockets(config_main_t *config, config_Route_t *rroot) {
    SNIMPL(socket_unvisitor(rroot));
    return 0;
}

