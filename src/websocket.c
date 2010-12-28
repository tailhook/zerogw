#include <zmq.h>
#include <malloc.h>
#include <strings.h>

#include "websocket.h"
#include "log.h"
#include "zutils.h"
#include "resolve.h"
#include "uidgen.h"

typedef struct topic_s {
    struct topic_hash_s *table;
    struct topic_s *next;
    struct topic_s *prev;
    subscriber_t *first_sub;
    subscriber_t *last_sub;
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

void free_topic(topic_t *topic) {
    // Frees both: empty/unused topics and forced free with unsubscribe
    for(subscriber_t *sub = topic->first_sub; sub;) {
        if(!sub->topic_prev) {
            sub->topic->first_sub = sub->topic_next;
        } else {
            sub->topic_prev->topic_next = sub->topic_next;
        }
        if(!sub->topic_next) {
            sub->topic->last_sub = sub->topic_prev;
        } else {
            sub->topic_next->topic_prev = sub->topic_prev;
        }
        if(!sub->client_prev) {
            sub->connection->first_sub = sub->client_next;
        } else {
            sub->client_prev->client_next = sub->client_next;
        }
        if(!sub->client_next) {
            sub->connection->last_sub = sub->client_prev;
        } else {
            sub->client_next->client_prev = sub->client_prev;
        }
        subscriber_t *nsub = sub->topic_next;
        free(sub);
        sub = nsub;
        topic->table->nsubscriptions -= 1;
        root.stat.websock_unsubscribed += 1;
    }
    if(topic->prev) {
        topic->prev->next = topic->next;
        if(topic->next) {
            topic->next->prev = topic->prev;
        }
    } else if(topic->next) {
        topic->next->prev = NULL;
        topic->table->table[topic->hash%topic->table->hash_size] = topic->next;
    } else {
        topic->table->table[topic->hash%topic->table->hash_size] = NULL;
    }
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
    void *sock = hybi->route->websocket.forward._sock;
    SNIMPL(zmq_msg_init_size(&zmsg, UID_LEN));
    memcpy(zmq_msg_data(&zmsg), hybi->uid, UID_LEN);
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE|ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_init_data(&zmsg, "disconnect", 10, NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_NOBLOCK));
    LDEBUG("Websocket sent disconnect to 0x%x", sock);
    
    for(subscriber_t *sub = hybi->first_sub; sub;) {
        if(!sub->topic_prev) {
            sub->topic->first_sub = sub->topic_next;
        } else {
            sub->topic_prev->topic_next = sub->topic_next;
        }
        if(!sub->topic_next) {
            sub->topic->last_sub = sub->topic_prev;
        } else {
            sub->topic_next->topic_prev = sub->topic_prev;
        }
        sub->topic->table->nsubscriptions -= 1;
        root.stat.websock_unsubscribed += 1;
        if(!sub->topic->first_sub) {
            free_topic(sub->topic);
        }
        subscriber_t *nsub = sub->client_next;
        free(sub);
        sub = nsub;
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
        hybi->comet->queue_size = qsize;
    } else {
        hybi = (hybi_t*)malloc(sizeof(hybi_t));
    }
    hybi->type = type;
    SNIMPL(make_hole_uid(hybi, hybi->uid, root.hybi_sieve, type == HYBI_COMET));
    hybi->first_sub = NULL;
    hybi->last_sub = NULL;
    hybi->route = route;
    
    void *sock = route->websocket.forward._sock;
    zmq_msg_t zmsg;
    SNIMPL(zmq_msg_init_size(&zmsg, UID_LEN));
    memcpy(zmq_msg_data(&zmsg), hybi->uid, UID_LEN);
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE|ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_init_data(&zmsg, "connect", 7, NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_NOBLOCK));
    LDEBUG("Websocket sent hello to 0x%x", sock);
    return hybi;
}

void websock_free_message(void *data, void *hint) {
    message_t *msg = (message_t *)hint;
    ws_MESSAGE_DECREF(&msg->ws);
}

int websock_message(connection_t *conn, message_t *msg) {
    ws_MESSAGE_INCREF(&msg->ws);
    void *sock = conn->hybi->route->websocket.forward._sock;
    zmq_msg_t zmsg;
    SNIMPL(zmq_msg_init_data(&zmsg, conn->hybi->uid, UID_LEN, NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE|ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_init_data(&zmsg, "message", 7, NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE|ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_init_data(&zmsg, msg->ws.data, msg->ws.length,
        websock_free_message, msg));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_NOBLOCK));
}

hybi_t *hybi_find(char *data) {
    hybi_t *hybi = (hybi_t *)sieve_get(root.hybi_sieve,
        *(uint64_t *)(data + IID_LEN));
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
    root.stat.websock_connects += 1;
    memcpy(result->topic, name, len);
    result->length = len;
    result->prev = NULL;
    result->next = NULL;
    result->first_sub = NULL;
    result->last_sub = NULL;
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
        root.stat.topics_created += 1;
        result = mktopic(table, topic_name, topic_len, hash);
        table->table[hash % table->hash_size] = result;
        return result;
    }
    size_t cell =  hash % table->hash_size;
    topic_t *current = table->table[cell];
    if(!current) {
        if(!create) return NULL;
        result = mktopic(table, topic_name, topic_len, hash);
        table->ntopics += 1;
        root.stat.topics_created += 1;
        table->table[cell] = result;
        return result;
    }
    if(current->length == topic_len && current->hash == hash
        && !memcmp(current->topic, topic_name, topic_len)) {
        return current;
    }
    while(current->next) {
        if(current->next->length == topic_len && current->next->hash == hash
        && !memcmp(current->next->topic, topic_name, topic_len)) {
            return current;
        }
    }
    if(!create) return NULL;
    table->ntopics += 1;
    root.stat.topics_created += 1;
    result = mktopic(table, topic_name, topic_len, hash);
    result->prev = current;
    current->next = result;
    return result;
}

static bool topic_subscribe(hybi_t *hybi, topic_t *topic) {
    subscriber_t *sub = (subscriber_t *)malloc(sizeof(subscriber_t));
    if(!sub) {
        return FALSE;
    }
    sub->topic = topic;
    sub->connection = hybi;
    sub->topic_prev = topic->last_sub;
    if(!topic->first_sub) {
        topic->first_sub = sub;
    } else {
        topic->last_sub->topic_next = sub;
    }
    topic->last_sub = sub;
    sub->topic_next = NULL;
    sub->client_prev = hybi->last_sub;
    if(!hybi->first_sub) {
        hybi->first_sub = sub;
    } else {
        hybi->last_sub->client_next = sub;
    }
    hybi->last_sub = sub;
    sub->client_next = NULL;
    topic->table->nsubscriptions += 1;
    root.stat.websock_subscribed += 1;
    LDEBUG("Subscription done, topics: %d, subscriptions: %d",
        topic->table->ntopics, topic->table->nsubscriptions);
    return TRUE;
}

static bool topic_unsubscribe(hybi_t *hybi, topic_t *topic) {
    subscriber_t *sub = topic->first_sub;
    ANIMPL(sub);
    if(sub->connection == hybi) {
        if(topic->last_sub == sub) {
            // Only one subscriber in the topic
            // Often it's user's own channel
            free_topic(topic);
            return TRUE;
        }
    } else {
        // It's probably faster to search user topics, because user doesn't
        // expected to have thousands of topics. But topic can actually have
        // thousands of subscibers (very popular news feed or chat room)
        for(sub = hybi->first_sub; sub; sub = sub->client_next)
            if(sub->topic == topic) break;
    }
    if(!sub) return FALSE;
    if(sub->client_prev) {
        sub->client_prev->client_next = sub->client_next;
    } else {
        hybi->first_sub = sub->client_next;
    }
    if(sub->client_next) {
        sub->client_next->client_prev = sub->client_prev;
    } else {
        hybi->last_sub = sub->client_prev;
    }
    if(!sub->topic_prev) {
        sub->topic->first_sub = sub->topic_next;
    } else {
        sub->topic_prev->topic_next = sub->topic_next;
    }
    if(!sub->topic_next) {
        sub->topic->last_sub = sub->topic_prev;
    } else {
        sub->topic_next->topic_prev = sub->topic_prev;
    }
    free(sub);
    topic->table->nsubscriptions -= 1;
    root.stat.websock_unsubscribed += 1;
    return TRUE;
}

static void websock_message_free(void *ws) {
    message_t *msg = ws;
    zmq_msg_close(&msg->zmq);
}

static bool topic_publish(topic_t *topic, zmq_msg_t *omsg) {
    if(!topic->first_sub) return FALSE;
    subscriber_t *sub = topic->first_sub;
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
    for(; sub; sub = sub->topic_next) {
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
    SNIMPL(zmq_send(route->websocket.forward._sock, &msg,
        ZMQ_SNDMORE|ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_init_data(&msg, "heartbeat", 9, NULL, NULL));
    SNIMPL(zmq_send(route->websocket.forward._sock, &msg, ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_close(&msg));
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
        zmq_msg_t msg;
        SNIMPL(zmq_msg_init_data(&msg, root.instance_id, IID_LEN, NULL, NULL));
        SNIMPL(zmq_send(route->websocket.forward._sock, &msg,
            ZMQ_SNDMORE|ZMQ_NOBLOCK));
        SNIMPL(zmq_msg_init_data(&msg, "ready", 5, NULL, NULL));
        SNIMPL(zmq_send(route->websocket.forward._sock, &msg, ZMQ_NOBLOCK));
        SNIMPL(zmq_msg_close(&msg));
        if(route->websocket.heartbeat_interval) {
            ev_timer_init(&route->websocket._heartbeat_timer, heartbeat,
                route->websocket.heartbeat_interval,
                route->websocket.heartbeat_interval);
            ev_timer_start(root.loop, &route->websocket._heartbeat_timer);
        }
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
    if(route->websocket.subscribe.value_len) {
        SNIMPL(z_close(route->websocket.subscribe._sock, root.loop));
    }
    if(route->websocket.forward.value_len) {
        SNIMPL(z_close(route->websocket.forward._sock, root.loop));
        if(route->websocket.heartbeat_interval) {
            ev_timer_stop(root.loop, &route->websocket._heartbeat_timer);
        }
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    return 0;
}

int prepare_websockets(config_main_t *config, config_Route_t *root) {
    SNIMPL(socket_visitor(&config->Routing));
    LINFO("Websocket connections complete");
    return 0;
}

int release_websockets(config_main_t *config, config_Route_t *root) {
    SNIMPL(socket_unvisitor(&config->Routing));
    return 0;
}

