#include <zmq.h>
#include <malloc.h>
#include <strings.h>
#include <sys/queue.h>
#include <stdlib.h>

#include "websocket.h"
#include "log.h"
#include "zutils.h"
#include "resolve.h"
#include "uidgen.h"
#include "main.h"
#include "http.h"

#define MSG_CONNECT ((void *)1)
#define MSG_DISCONNECT ((void *)2)

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
        pool_free(&root.hybi.subscriber_pool, sub);
        topic->table->nsubscriptions -= 1;
        root.stat.websock_unsubscribed += 1;
    }
    LIST_REMOVE(topic, topic_list);
    topic->table->ntopics -= 1;
    root.stat.topics_removed += 1;
    free(topic);
}

void hybi_free(hybi_t *hybi) {
    if(hybi->flags & WS_HAS_COOKIE) {
        zmq_msg_close(&hybi->cookie);
    }
    free(hybi);
}

void websock_stop(ws_connection_t *hint) {
    connection_t *conn = (connection_t *)hint;
    ev_timer_stop(root.loop, &conn->idle_timer);
    root.stat.websock_disconnects += 1;
    root.stat.disconnects += 1;
    hybi_stop(conn->hybi);
}

void websock_free_message(void *data, void *hint) {
    message_t *msg = (message_t *)hint;
    ws_MESSAGE_DECREF(&msg->ws);
}

int backend_send_real(config_zmqsocket_t *sock, hybi_t *hybi, void *msg) {
    zmq_msg_t zmsg;
    SNIMPL(zmq_msg_init_size(&zmsg, UID_LEN));
    memcpy(zmq_msg_data(&zmsg), hybi->uid, UID_LEN);
    if(zmq_send(sock->_sock, &zmsg, ZMQ_SNDMORE|ZMQ_NOBLOCK) < 0) {
        if(errno == EAGAIN || errno == EINTR) {
            zmq_msg_close(&zmsg);
            return -1;
        }
        SNIMPL(-1);
    }
    int len;
    char *kind;
    bool is_msg = FALSE;
    if(msg == MSG_CONNECT) {
        kind = "connect";
        len = strlen("connect");  // compiler will take care
    } else if(msg == MSG_DISCONNECT) {
        kind = "disconnect";
        len = strlen("disconnect");  // we have a smart compiler
    } else {
        is_msg = TRUE;
        if(hybi->flags & WS_HAS_COOKIE) {
            kind = "msgfrom";
            len = strlen("msgfrom");   // still compiler is smart
        } else {
            kind = "message";
            len = strlen("message");  // compiler is smarter than you
        }
    }
    SNIMPL(zmq_msg_init_size(&zmsg, len));
    memcpy(zmq_msg_data(&zmsg), kind, len);
    int flag = (is_msg || hybi->flags & WS_HAS_COOKIE) ? ZMQ_SNDMORE : 0;
    SNIMPL(zmq_send(sock->_sock, &zmsg, ZMQ_NOBLOCK | flag));
    if(hybi->flags & WS_HAS_COOKIE) {
        flag = is_msg ? ZMQ_SNDMORE : 0;
        SNIMPL(zmq_msg_copy(&zmsg, &hybi->cookie));
        SNIMPL(zmq_send(sock->_sock, &zmsg, ZMQ_NOBLOCK | flag));
    }
    if(is_msg) {
        if(hybi->type == HYBI_COMET) {
            request_t *req = msg;
            SNIMPL(zmq_msg_init_data(&zmsg, req->ws.body, req->ws.bodylen,
                request_decref, req));
        } else {
            message_t *wmsg = msg;
            SNIMPL(zmq_msg_init_data(&zmsg, wmsg->ws.data, wmsg->ws.length,
                websock_free_message, wmsg));
        }
        SNIMPL(zmq_send(sock->_sock, &zmsg, ZMQ_NOBLOCK));
    }
    return 0;
}

void backend_unqueue(struct ev_loop *loop, struct ev_io *watch, int rev) {
    ADEBUG(rev == EV_READ);
    config_zmqsocket_t *socket = SHIFT(watch, config_zmqsocket_t, _watch);
    int64_t opt;
    size_t size = sizeof(opt);
    SNIMPL(zmq_getsockopt(socket->_sock, ZMQ_EVENTS, &opt, &size));
    if(!(opt & EV_WRITE && socket->_queue.size && !root.hybi.paused)) {
        return;  // Just have nothing to do
    }
    queue_item_t *cur = TAILQ_FIRST(&socket->_queue.items), *nxt;
    for(;cur; cur = nxt) {
        nxt = TAILQ_NEXT(cur, list);

        backend_msg_t *ptr = (backend_msg_t *)cur;
        while(backend_send_real(socket, ptr->hybi, ptr->msg) < 0) {
            if(errno == EINTR) continue;
            else if (errno == EAGAIN) return;
            SNIMPL(-1);
        }
        hybi_DECREF(ptr->hybi);
        root.stat.websock_backend_queued -= 1;
        queue_remove(&socket->_queue, cur);
    }
}

// This method needs own reference to msg (if refcounted)
int backend_send(config_zmqsocket_t *sock, hybi_t *hybi, void *msg, bool force) {
    while(!root.hybi.paused && !sock->_queue.size) {
        if(backend_send_real(sock, hybi, msg) < 0) {
            if(errno == EAGAIN) {
                LWARN("Can't send a message, started queueing");
                break;
            } else if(errno == EINTR) {
                continue;
            }
            SNIMPL(-1);
        }
        return 0;
    }

    backend_msg_t *q;
    if(force) {
        q = (backend_msg_t *)queue_push(&sock->_queue);
    } else {
        q = (backend_msg_t *)queue_force_push(&sock->_queue);
    }
    if(!q) {
        errno = EAGAIN;
        return -1;
    }
    root.stat.websock_backend_queued += 1;
    hybi_INCREF(hybi);
    q->hybi = hybi;
    q->msg = msg;
    return 0;
}

void hybi_stop(hybi_t *hybi) {
    sieve_empty(root.hybi.sieve, UID_HOLE(hybi->uid));
    backend_send(&hybi->route->websocket.forward, hybi, MSG_DISCONNECT, TRUE);

    subscriber_t *sub, *nxt;
    for(sub = LIST_FIRST(&hybi->subscribers); sub; sub = nxt) {
        nxt = LIST_NEXT(sub, client_list);
        LIST_REMOVE(sub, client_list);
        LIST_REMOVE(sub, topic_list);
        topic_t *topic = sub->topic;
        topic->table->nsubscriptions -= 1;
        root.stat.websock_unsubscribed += 1;
        if(!LIST_FIRST(&topic->subscribers)) {
            free_topic(topic);
        }
        pool_free(&root.hybi.subscriber_pool, sub);
    }
    for(output_t *out = LIST_FIRST(&hybi->outputs), *nxt; out; out=nxt) {
        nxt = LIST_NEXT(out, client_list);
        if(!nxt || nxt->socket != out->socket) {
            // We send on the last one, because we don't care on which one
            // but do care to send only once for each output
            // Subscriptions are guaranteed to have single output in subsequent
            // list entries
            backend_send((config_zmqsocket_t *)out->socket,
                hybi, MSG_DISCONNECT, TRUE);
        }
        LIST_REMOVE(out, output_list);
        LIST_REMOVE(out, client_list);
        free(out);
    }
    hybi_DECREF(hybi);
}

void websock_idle(struct ev_loop *loop, struct ev_timer *watch, int rev) {
    ANIMPL(rev == EV_TIMER);
    connection_t *conn = SHIFT(watch, connection_t, idle_timer);
    uint32_t num = random() & 0xFFFF;
    root.stat.websock_sent_pings += 1;
    ws_message_t *msg = ws_message_copy_data(&conn->ws, &num, sizeof(num));
    if(msg) {
        msg->flags = WS_MSG_PING;
        ws_message_send(&conn->ws, msg);
        ws_MESSAGE_DECREF(msg);
    } else {
        TWARN("Can't allocate memory for ping message");
    }
}

int websock_start(connection_t *conn, config_Route_t *route) {
    LDEBUG("Websocket started");
    hybi_t *hybi = hybi_start(route, HYBI_WEBSOCKET);
    if(!hybi) {
        return -1;
    }
    hybi->conn = conn;
    conn->hybi = hybi;
    ws_DISCONNECT_CB(&conn->ws, websock_stop);
    unsigned long ivl = route->websocket.idle_ping_interval;
    ev_timer_init(&conn->idle_timer, websock_idle, ivl, ivl);
    ev_timer_again(root.loop, &conn->idle_timer);
    root.stat.websock_connects += 1;
    return 0;
}

hybi_t *hybi_start(config_Route_t *route, hybi_enum type) {
    hybi_t *hybi;
    if(type == HYBI_COMET) {
        hybi = (hybi_t*)malloc(sizeof(hybi_t) + sizeof(comet_t));
    } else {
        hybi = (hybi_t*)malloc(sizeof(hybi_t));
    }
    if(!hybi) {
        return NULL;
    }
    hybi->type = type;
    hybi->refcnt = 1;
    hybi->flags = 0;
    if(make_hole_uid(hybi, hybi->uid, root.hybi.sieve, type==HYBI_COMET) < 0) {
        free(hybi);
        return NULL;
    }
    LIST_INIT(&hybi->subscribers);
    hybi->route = route;
    LIST_INIT(&hybi->outputs);

    if(backend_send(&hybi->route->websocket.forward, hybi, MSG_CONNECT, FALSE)) {
        LWARN("Failed to queue hello message");
        sieve_empty(root.hybi.sieve, UID_HOLE(hybi->uid));
        free(hybi);
        return NULL;
    } else {
        LDEBUG("Websocket sent hello");
        return hybi;
    }
}

static void do_send(hybi_t *hybi, message_t *msg) {
    root.stat.websock_sent += 1;
    if(hybi->type == HYBI_WEBSOCKET) {
        int r = ws_message_send(&hybi->conn->ws, &msg->ws);
        if(r < 0) {
            if(errno == EXFULL) {
                ws_connection_close(&hybi->conn->ws);
            } else {
                SNIMPL(r);
            }
        }
    } else if(hybi->type == HYBI_COMET) {
        comet_send(hybi, msg);
    } else {
        LNIMPL("Unknown connection type");
    }
}

static void websock_message_free(void *ws) {
    message_t *msg = ws;
    zmq_msg_close(&msg->zmq);
}


config_zmqsocket_t *websock_resolve(hybi_t *hybi, char *data, int length) {
    if(hybi->route->websocket.frontend_commands.enabled
        && length >= 8 /*strlen("ZEROGW:")+1*/
        && !memcmp(data, "ZEROGW:", 7)) {
        if(length >= 11 && !memcmp(data+7, "echo", 4)) {
            message_t *mm = (message_t*)malloc(sizeof(message_t));
            ANIMPL(mm);
            SNIMPL(ws_message_init(&mm->ws));
            zmq_msg_init_size(&mm->zmq, length);
            char *zdata = zmq_msg_data(&mm->zmq);
            memcpy(zdata, data, length);
            ws_MESSAGE_DATA(&mm->ws, zdata, length, websock_message_free);
            do_send(hybi, mm);
            ws_MESSAGE_DECREF(&mm->ws);
        } else if(length >= 16 && !memcmp(data+7, "timestamp", 9)) {
            message_t *mm = (message_t*)malloc(sizeof(message_t));
            ANIMPL(mm);
            SNIMPL(ws_message_init(&mm->ws));
            zmq_msg_init_size(&mm->zmq, length+15);
            char *zdata = zmq_msg_data(&mm->zmq);
            memcpy(zdata, data, length);
            char printbuf[16];
            int tlen = snprintf(printbuf, 16, ":%14.3f", ev_now(root.loop));
            ADEBUG2(tlen == 15, "LENGTH %d (need 15) ``%s''", tlen, printbuf);
            memcpy(zdata+length, printbuf, 15);
            ws_MESSAGE_DATA(&mm->ws, zdata, length+tlen, websock_message_free);
            do_send(hybi, mm);
            ws_MESSAGE_DECREF(&mm->ws);
        }
        return NULL;
    }
    output_t *item;
    LIST_FOREACH(item, &hybi->outputs, client_list) {
        if(item->prefix_len <= length &&
                !strncmp(data, item->prefix, item->prefix_len)) {
            return (config_zmqsocket_t *)item->socket;
        }
    }
    return &hybi->route->websocket.forward;
}

int websock_message(connection_t *conn, message_t *msg) {
    root.stat.websock_received += 1;
    ev_timer_again(root.loop, &conn->idle_timer);
    config_zmqsocket_t *sock = websock_resolve(conn->hybi,
        msg->ws.data, msg->ws.length);
    if(sock) {
        ws_MESSAGE_INCREF(&msg->ws);
        return backend_send(sock, conn->hybi, msg, FALSE);
    }
    return 0; // No socket means it's frontend command
}

hybi_t *hybi_find(char *data) {
    hybi_t *hybi = (hybi_t *)sieve_get(root.hybi.sieve, UID_HOLE(data));
    LDEBUG("Searching for hole %d", UID_HOLE(data));
    if(!hybi || !UID_EQ(hybi->uid, data)) {
        return NULL;
    }
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
    subscriber_t *sub;
    LIST_FOREACH(sub, &hybi->subscribers, client_list)
        if(sub->topic == topic) return TRUE; // already subscribed
    sub = (subscriber_t *)pool_alloc(&root.hybi.subscriber_pool);
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
    pool_free(&root.hybi.subscriber_pool, sub);
    tbl->nsubscriptions -= 1;
    root.stat.websock_unsubscribed += 1;
    return TRUE;
}

static bool topic_publish(topic_t *topic, zmq_msg_t *omsg) {
    message_t *msg = (message_t*)malloc(sizeof(message_t));
    ANIMPL(msg);
    SNIMPL(ws_message_init(&msg->ws));
    zmq_msg_init(&msg->zmq);
    LDEBUG("Sending %x [%d]``%.*s''", omsg,
        zmq_msg_size(omsg), zmq_msg_size(omsg), zmq_msg_data(omsg));
    zmq_msg_move(&msg->zmq, omsg);
    ws_MESSAGE_DATA(&msg->ws, (char *)zmq_msg_data(&msg->zmq),
        zmq_msg_size(&msg->zmq), websock_message_free);
    root.stat.websock_published += 1;
    subscriber_t *nxt, *sub;
    for (sub = LIST_FIRST(&topic->subscribers); sub; sub = nxt) {
        nxt = LIST_NEXT(sub, topic_list);
        // Subscribers, and whole topic can be deleted while traversing
        // list of subscribers. Note that topic can be deleted only
        // after each of the subscribers are deleted
        do_send(sub->connection, msg);
    }
    ws_MESSAGE_DECREF(&msg->ws); // we own a single reference
}

static bool send_all(config_Route_t *route, zmq_msg_t *omsg) {
    message_t *msg = (message_t*)malloc(sizeof(message_t));
    ANIMPL(msg);
    SNIMPL(ws_message_init(&msg->ws));
    zmq_msg_init(&msg->zmq);
    LDEBUG("Sending to everybody [%d]``%.*s''",
        zmq_msg_size(omsg), zmq_msg_size(omsg), zmq_msg_data(omsg));
    zmq_msg_move(&msg->zmq, omsg);
    ws_MESSAGE_DATA(&msg->ws, (char *)zmq_msg_data(&msg->zmq),
        zmq_msg_size(&msg->zmq), websock_message_free);
    root.stat.websock_published += 1;
    for(int i = 0; i < root.hybi.sieve->max; i++) {
        hybi_t *hybi = root.hybi.sieve->items[i];
        if(!hybi || hybi->route != route) continue;
        do_send(hybi, msg);
    }
    ws_MESSAGE_DECREF(&msg->ws); // we own a single reference
}

void topic_clone(topic_t *source, topic_t *target) {
    subscriber_t *nxt, *sub;
    LIST_FOREACH(sub, &source->subscribers, topic_list) {
        if(!topic_subscribe(sub->connection, target)) {
            TWARN("Can't subscribe while cloning, probably out of memory");
        }
    }
    if(!LIST_FIRST(&target->subscribers)) {
        free_topic(target);
    }
}

void websock_process(struct ev_loop *loop, struct ev_io *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));
    config_Route_t *route = (config_Route_t *)((char *)watch
        - offsetof(config_Route_t, websocket.subscribe._watch));
    size_t opt, optlen = sizeof(opt);
    SNIMPL(zmq_getsockopt(route->websocket.subscribe._sock, ZMQ_EVENTS, &opt, &optlen));
    while(TRUE) {
        output_t *output = NULL;
        Z_SEQ_INIT(msg, route->websocket.subscribe._sock);
        Z_RECV_START(msg, break);
        char *cmd = zmq_msg_data(&msg);
        int cmdlen = zmq_msg_size(&msg);
        if(cmdlen == 9 && !strncmp(cmd, "subscribe", cmdlen)) {
            LDEBUG("Websocket got SUBSCRIBE request");
            Z_RECV_NEXT(msg);
            if(zmq_msg_size(&msg) != UID_LEN) goto msg_error;
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi || hybi->route != route) goto msg_error;
            Z_RECV_LAST(msg);
            topic_t *topic = find_topic(hybi->route, &msg, TRUE);
            if(topic && topic_subscribe(hybi, topic)) {
                LDEBUG("Subscribed to ``%.*s''", topic->length, topic->topic);
            } else {
                TWARN("Couldn't make topic or subscribe, probably no memory");
                if(!LIST_FIRST(&topic->subscribers)) {
                    free_topic(topic);
                }
            }
        } else if(cmdlen == 11 && !strncmp(cmd, "unsubscribe", cmdlen)) {
            LDEBUG("Websocket got UNSUBSCRIBE request");
            Z_RECV_NEXT(msg);
            if(zmq_msg_size(&msg) != UID_LEN) goto msg_error;
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi || hybi->route != route) goto msg_error;
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
            if(!hybi || hybi->route != route) goto msg_error;
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
            do_send(hybi, mm);
            ws_MESSAGE_DECREF(&mm->ws);
        } else if(cmdlen == 5 && !strncmp(cmd, "clone", cmdlen)) {
            LDEBUG("Websocket got CLONE request");
            Z_RECV_NEXT(msg);
            topic_t *source = find_topic(route, &msg, FALSE);
            Z_RECV_LAST(msg);
            if(source) {
                topic_t *target = find_topic(route, &msg, TRUE);
                if(target) {
                    LDEBUG("Cloning ``%.*s'' to ``%.*s''",
                        source->length, source->topic,
                        target->length, target->topic);
                    topic_clone(source, target);
                } else {
                    TWARN("Couldn't make topic or subscribe,"
                          " probably no memory");
                }
            }
        } else if(cmdlen == 4 && !strncmp(cmd, "drop", cmdlen)) {
            LDEBUG("Websocket got DROP request");
            Z_RECV_LAST(msg);
            topic_t *topic = find_topic(route, &msg, FALSE);
            if(topic) {
                free_topic(topic);
            }
        } else if(cmdlen == 10 && !memcmp(cmd, "add_output", cmdlen)) {
            LDEBUG("Adding output");
            Z_RECV_NEXT(msg);
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi || hybi->route != route) goto msg_error;
            Z_RECV_NEXT(msg);
            zmq_msg_t prefix;
            zmq_msg_init(&prefix);
            zmq_msg_move(&prefix, &msg);
            Z_RECV(msg); \
            if(msg_opt) {
                zmq_msg_close(&prefix);
                goto msg_error;
            }

            config_namedoutput_t *outsock = NULL;
            int len = zmq_msg_size(&msg);
            char *data = zmq_msg_data(&msg);
            CONFIG_STRING_NAMEDOUTPUT_LOOP(item,
                route->websocket.named_outputs) {
                if(item->key_len == len && !memcmp(data, item->key, len)) {
                    outsock = &item->value;
                    len = 0;
                    break;
                }
            }
            if(!outsock) {
                TWARN("Can't find named path ``%.*s''", len, data);
            } else {
                len = zmq_msg_size(&prefix);
                data = zmq_msg_data(&prefix);
                output_t *old_prefix = NULL;
                output_t *prev_output = NULL;
                output_t *cur;
                LIST_FOREACH(cur, &hybi->outputs, client_list) {
                    if(cur->prefix_len == len && !memcmp(data, cur->prefix, len)) {
                        old_prefix = cur;
                    }
                    if(cur->socket == outsock) {
                        prev_output = cur;
                    }
                }
                if(!old_prefix) {
                    output = malloc(sizeof(output_t) + len + 1);
                    ANIMPL(output);
                    memcpy(output->prefix, zmq_msg_data(&prefix), len);
                    output->connection = hybi;
                    output->prefix_len = len;
                    output->prefix[len] = 0;
                    output->socket = outsock;
                } else if(old_prefix->socket != outsock) {
                    output = old_prefix;
                    LIST_REMOVE(output, output_list);
                    // The following is to ensure proper order (see below)
                    LIST_REMOVE(output, client_list);
                    output->socket = outsock;
                } else {
                    goto msg_finish;
                }
                // Need to guarantee that single output will use
                // subsequent list entries for a single socket, to rule
                // out duplicates on disconnect, similarly subscriptions
                // for a single socket must be on subsequent entries to
                // eliminate duplicates on sync
                if(prev_output) {
                    LIST_INSERT_AFTER(prev_output, output, client_list);
                    LIST_INSERT_AFTER(prev_output, output, output_list);
                } else {
                    LIST_INSERT_HEAD(&hybi->outputs, output, client_list);
                    LIST_INSERT_HEAD(&outsock->_int.outputs,
                        output, output_list);
                }
            }
        } else if(cmdlen == 10 && !memcmp(cmd, "del_output", cmdlen)) {
            LDEBUG("Removing output");
            Z_RECV_NEXT(msg);
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi || hybi->route != route) goto msg_error;
            Z_RECV_LAST(msg);
            int len = zmq_msg_size(&msg);
            char *data = zmq_msg_data(&msg);
            for(output_t *out = LIST_FIRST(&hybi->outputs), *nxt; out; out=nxt) {
                nxt = LIST_NEXT(out, client_list);
                if(out->prefix_len == len && !memcmp(data, out->prefix, len)) {
                    LIST_REMOVE(out, client_list);
                    LIST_REMOVE(out, output_list);
                    free(out);
                    len = 0;
                    break;
                }
            }
            if(len) {
                TWARN("Can't find prefix ``%.*s''", len, data);
            }
        } else if(cmdlen == 10 && !memcmp(cmd, "set_cookie", cmdlen)) {
            LDEBUG("Setting connection cookie");
            Z_RECV_NEXT(msg);
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(!hybi || hybi->route != route) goto msg_error;
            Z_RECV_LAST(msg);
            if(!(hybi->flags & WS_HAS_COOKIE)) {
                zmq_msg_init(&hybi->cookie);
            }
            SNIMPL(zmq_msg_move(&hybi->cookie, &msg));
            hybi->flags |= WS_HAS_COOKIE;
        } else if(cmdlen == 7 && !strncmp(cmd, "sendall", cmdlen)) {
            LDEBUG("Websocket got SENDALL request");
            Z_RECV_LAST(msg);
            send_all(route, &msg);
        } else if(cmdlen == 10 && !memcmp(cmd, "disconnect", cmdlen)) {
            LDEBUG("Closing connection");
            Z_RECV_LAST(msg);
            hybi_t *hybi = hybi_find(zmq_msg_data(&msg));
            if(hybi && hybi->route == route) {
                if(hybi->type == HYBI_COMET) {
                    comet_close(hybi);
                } else if(hybi->type == HYBI_WEBSOCKET) {
                    ws_connection_close(&hybi->conn->ws);
                } else {
                    LNIMPL("Wrong hybi type %ld", hybi->type);
                }
            }
        } else {
            TWARN("Wrong command ``%.*s''", cmdlen, cmd);
            goto msg_error;
        }
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        if(output) free(output);
        Z_SEQ_ERROR(msg);
        continue;
    }
}

int start_websocket(request_t *req) {
    request_init(req);
    config_Route_t *route = resolve_url(req);
    if(!route->websocket.subscribe.value_len
        || !route->websocket.forward.value_len
        || !route->websocket.enabled
        || route->websocket.disable_websocket) {
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
            zmq_msg_close(&msg);
            return; // nevermind
        }
    }
    SNIMPL(zmq_msg_init_data(&msg, "heartbeat", 9, NULL, NULL));
    if(zmq_send(route->websocket.forward._sock, &msg, ZMQ_NOBLOCK) < 0) {
        if(errno != EAGAIN) { //TODO: EINTR???
            SNIMPL(-1);
        } // else: nevermind
        zmq_msg_close(&msg);
    }
}

static void send_sync(struct ev_loop *loop,
    struct ev_timer *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));
    config_namedoutput_t *socket = SHIFT(watch,
        config_namedoutput_t, _int.sync_tm);
    if(!LIST_FIRST(&socket->_int.outputs)) return;
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_data(&msg, root.instance_id, IID_LEN, NULL, NULL));
    if(zmq_send(socket->_sock, &msg, ZMQ_SNDMORE|ZMQ_NOBLOCK) < 0) {
        if(errno != EAGAIN) { //TODO: EINTR???
            SNIMPL(-1);
        } else {
            zmq_msg_close(&msg);
            return; // nevermind
        }
    }
    SNIMPL(zmq_msg_init_data(&msg, "sync", 4, NULL, NULL));

    output_t *item;
    output_t *prev = NULL;
    LIST_FOREACH(item, &socket->_int.outputs, output_list) {
        // Same users are guaranteed to be on subsequent entries on the list
        if(prev && prev->connection == item->connection) continue;
        prev = item;
        SNIMPL(zmq_send(socket->_sock, &msg, ZMQ_NOBLOCK|ZMQ_SNDMORE));
        SNIMPL(zmq_msg_init_size(&msg, UID_LEN));
        memcpy(zmq_msg_data(&msg), item->connection->uid, UID_LEN);
        SNIMPL(zmq_send(socket->_sock, &msg, ZMQ_NOBLOCK|ZMQ_SNDMORE));
        if(item->connection->flags & WS_HAS_COOKIE) {
            zmq_msg_copy(&msg, &item->connection->cookie);
        }
    }

    SNIMPL(zmq_send(socket->_sock, &msg, ZMQ_NOBLOCK));
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
            ZMASK_PUSH|ZMASK_PUB, ZMQ_PUB, backend_unqueue, root.loop));
        if(route->websocket.heartbeat_interval) {
            ev_timer_init(&route->websocket._heartbeat_timer, heartbeat,
                route->websocket.heartbeat_interval,
                route->websocket.heartbeat_interval);
            ev_timer_start(root.loop, &route->websocket._heartbeat_timer);
        }
        init_queue(&route->websocket.forward._queue,
            route->websocket.max_backend_queue, &root.hybi.backend_pool);
    }
    CONFIG_STRING_NAMEDOUTPUT_LOOP(item, route->websocket.named_outputs) {
        SNIMPL(zmq_open((config_zmqsocket_t*)&item->value,
            ZMASK_PUSH|ZMASK_PUB, ZMQ_PUB, backend_unqueue, root.loop));
        init_queue(&item->value._queue,
            route->websocket.max_backend_queue, &root.hybi.backend_pool);
        if(item->value.sync_interval) {
            ev_timer_init(&item->value._int.sync_tm, send_sync,
                item->value.sync_interval,
                item->value.sync_interval);
            ev_timer_start(root.loop, &item->value._int.sync_tm);
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
        free_queue(&route->websocket.forward._queue);
    }
    CONFIG_STRING_NAMEDOUTPUT_LOOP(item, route->websocket.named_outputs) {
        SNIMPL(z_close((config_zmqsocket_t *)&item->value, root.loop));
        free_queue(&item->value._queue);
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    return 0;
}

static void resume_visitor(config_Route_t *route) {
    if(route->websocket.forward.value_len) {
        ev_feed_event(root.loop, &route->websocket.forward._watch, EV_READ);
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        resume_visitor(&item->value);
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        resume_visitor(&item->value);
    }
    CONFIG_STRING_NAMEDOUTPUT_LOOP(item, route->websocket.named_outputs) {
        ev_feed_event(root.loop, &item->value._watch, EV_READ);
    }
}

static void sync_visitor(config_Route_t *route) {
    CONFIG_ROUTE_LOOP(item, route->children) {
        sync_visitor(&item->value);
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        sync_visitor(&item->value);
    }
    CONFIG_STRING_NAMEDOUTPUT_LOOP(item, route->websocket.named_outputs) {
        ev_feed_event(root.loop, &item->value._int.sync_tm, EV_READ);
    }
}


void websockets_sync_now() {
    sync_visitor(&root.config->Routing);
}

int pause_websockets(bool pause) {
    if(root.hybi.paused == pause) return 0;

    message_t *mm = (message_t*)malloc(sizeof(message_t));
    ANIMPL(mm);
    SNIMPL(ws_message_init(&mm->ws));

    if(pause) {
        zmq_msg_init_size(&mm->zmq, 13 /*strlen("ZEROGW:paused")*/);
        char *zdata = zmq_msg_data(&mm->zmq);
        memcpy(zdata, "ZEROGW:paused", 13);
        ws_MESSAGE_DATA(&mm->ws, zdata, 13, NULL);
    } else {
        zmq_msg_init_size(&mm->zmq, 14 /*strlen("ZEROGW:resumed")*/);
        char *zdata = zmq_msg_data(&mm->zmq);
        memcpy(zdata, "ZEROGW:resumed", 14);
        ws_MESSAGE_DATA(&mm->ws, zdata, 14, NULL);
    }

    for(int i = 0; i < root.hybi.sieve->max; i++) {
        hybi_t *hybi = root.hybi.sieve->items[i];
        if(!hybi) continue;
        if(hybi->route->websocket.frontend_commands.enabled
           && hybi->route->websocket.frontend_commands.commands.paused.enabled) {
            do_send(hybi, mm);
        }
    }
    ws_MESSAGE_DECREF(&mm->ws);

    if(pause) {
        root.hybi.paused = TRUE;
    } else {
        root.hybi.paused = FALSE;
        resume_visitor(&root.config->Routing);
    }
    return 0;
}

int prepare_websockets(config_main_t *config, config_Route_t *rroot) {
    root.hybi.paused = FALSE;
    SNIMPL(init_pool(&root.hybi.backend_pool, sizeof(backend_msg_t),
        config->Server.pools.backend_messages));
    SNIMPL(init_pool(&root.hybi.frontend_pool, sizeof(frontend_msg_t),
        config->Server.pools.frontend_messages));
    SNIMPL(init_pool(&root.hybi.subscriber_pool, sizeof(subscriber_t),
        config->Server.pools.subscriptions));
    SNIMPL(socket_visitor(rroot));
    LINFO("Websocket connections complete");
    return 0;
}

int release_websockets(config_main_t *config, config_Route_t *rroot) {
    SNIMPL(socket_unvisitor(rroot));
    free_pool(&root.hybi.backend_pool);
    free_pool(&root.hybi.frontend_pool);
    free_pool(&root.hybi.subscriber_pool);
    return 0;
}

