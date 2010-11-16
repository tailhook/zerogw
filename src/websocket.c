#include <zmq.h>
#include <malloc.h>
#include <strings.h>

#include "websocket.h"
#include "log.h"
#include "zutils.h"

#define HASH_SIZE_MIN 512

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
        subscriber_t *nsub = sub->topic_next;
        free(sub);
        sub = nsub;
        topic->table->nsubscriptions -= 1;
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
    free(topic);
}

void websock_stop(ws_connection_t *hint) {
    connection_t *conn = (connection_t *)hint;
    for(subscriber_t *sub = conn->first_sub; sub;) {
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
        if(!sub->topic->first_sub) {
            free_topic(sub->topic);
        }
        subscriber_t *nsub = sub->client_next;
        free(sub);
        sub = nsub;
    }
}

int websock_start(connection_t *conn, config_Route_t *route) {
    LDEBUG("Websocket started");
    conn->route = route;
    void *sock = route->_websock_forward;
    memcpy(conn->connection_id, instance_id, INSTANCE_ID_LEN);
    conn->first_sub = NULL;
    conn->last_sub = NULL;
    sieve_find_hole(sieve, conn,
        (size_t *)(conn->connection_id + INSTANCE_ID_LEN + 8),
        (size_t *)(conn->connection_id + INSTANCE_ID_LEN));
    zmq_msg_t zmsg;
    SNIMPL(zmq_msg_init_data(&zmsg, conn->connection_id,
        sizeof(conn->connection_id), NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_init_data(&zmsg, "connect", 7, NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, 0));
    LDEBUG("Websocket sent hello to 0x%x", route->_websock_forward);
    ws_DISCONNECT_CB(&conn->ws, websock_stop);
    return 0;
}

void websock_free_message(void *data, void *hint) {
    message_t *msg = (message_t *)hint;
    ws_MESSAGE_DECREF(&msg->ws);
}

int websock_message(connection_t *conn, message_t *msg) {
    ws_MESSAGE_INCREF(&msg->ws);
    void *sock = conn->route->_websock_forward;
    zmq_msg_t zmsg;
    SNIMPL(zmq_msg_init_data(&zmsg, conn->connection_id,
        sizeof(conn->connection_id), NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_init_data(&zmsg, "message", 7, NULL, NULL));
    SNIMPL(zmq_send(sock, &zmsg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_init_data(&zmsg, msg->ws.data, msg->ws.length,
        websock_free_message, msg));
    SNIMPL(zmq_send(sock, &zmsg, 0));
}

static connection_t *find_connection(zmq_msg_t *msg) {
    if(zmq_msg_size(msg) != CONNECTION_ID_LEN) return NULL;
    void *data = zmq_msg_data(msg);
    connection_t *conn = (connection_t *)sieve_get(sieve,
        *(uint64_t *)(data + INSTANCE_ID_LEN));
    if(!conn) return NULL;
    if(memcmp(conn->connection_id, data, CONNECTION_ID_LEN)) return NULL;
    return conn;
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
    topic_hash_t *table = route->_websock_topics;
    if(!route->_websock_topics) {
        if(!create) return NULL;
        table = malloc(sizeof(topic_hash_t));
        if(!table) return NULL;
        bzero(table, sizeof(topic_hash_t));
        route->_websock_topics = table;
    }
    if(!table->table) {
        if(!create) return NULL;
        table->hash_size = HASH_SIZE_MIN;
        table->table = (topic_t **)malloc(HASH_SIZE_MIN*sizeof(topic_t*));
        bzero(table->table, HASH_SIZE_MIN*sizeof(topic_t*));
        if(!table->table) return NULL;
        table->ntopics = 1;
        table->nsubscriptions = 0;
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
    result = mktopic(table, topic_name, topic_len, hash);
    result->prev = current;
    current->next = result;
    return result;
}

static bool topic_subscribe(connection_t *conn, topic_t *topic) {
    subscriber_t *sub = (subscriber_t *)malloc(sizeof(subscriber_t));
    if(!sub) {
        return FALSE;
    }
    sub->topic = topic;
    sub->connection = conn;
    sub->topic_prev = topic->last_sub;
    if(!topic->first_sub) {
        topic->first_sub = sub;
    } else {
        topic->last_sub->topic_next = sub;
    }
    topic->last_sub = sub;
    sub->topic_next = NULL;
    sub->client_prev = conn->last_sub;
    if(!conn->first_sub) {
        conn->first_sub = sub;
    } else {
        conn->last_sub->client_next = sub;
    }
    conn->last_sub = sub;
    sub->client_next = NULL;
    topic->table->nsubscriptions += 1;
    LDEBUG("Subscription done, topics: %d, subscriptions: %d",
        topic->table->ntopics, topic->table->nsubscriptions);
    return TRUE;
}

static bool topic_unsubscribe(connection_t *conn, topic_t *topic) {
    subscriber_t *sub = topic->first_sub;
    ANIMPL(sub);
    if(sub->connection == conn) {
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
        for(sub = conn->first_sub; sub; sub = sub->client_next)
            if(sub->topic == topic) break;
    }
    if(!sub) return FALSE;
    if(sub->client_prev) {
        sub->client_prev->client_next = sub->client_next;
    } else {
        conn->first_sub = sub->client_next;
    }
    if(sub->client_next) {
        sub->client_next->client_prev = sub->client_prev;
    } else {
        conn->last_sub = sub->client_prev;
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
    message_t *msg = (message_t*)ws_message_new(&sub->connection->ws);
    zmq_msg_init(&msg->zmq);
    zmq_msg_move(&msg->zmq, omsg);
    ws_MESSAGE_DATA(&msg->ws, (char *)zmq_msg_data(&msg->zmq),
        zmq_msg_size(&msg->zmq), websock_message_free);
    for(; sub; sub = sub->topic_next) {
        LDEBUG("SENDING");
        ws_message_send(&sub->connection->ws, &msg->ws);
    }
    ws_MESSAGE_DECREF(&msg->ws); // we own a single reference
}


void websock_process(config_Route_t *route, zmq_socket_t sock) {
    Z_SEQ_INIT(msg, sock);
    Z_RECV_NEXT(msg);
    char *cmd = zmq_msg_data(&msg);
    int cmdlen = zmq_msg_size(&msg);
    if(cmdlen == 9 && !strncmp(cmd, "subscribe", cmdlen)) {
        LDEBUG("Websocket got SUBSCRIBE request");
        Z_RECV_NEXT(msg);
        connection_t *conn = find_connection(&msg);
        if(!conn) goto msg_error;
        Z_RECV_LAST(msg);
        topic_t *topic = find_topic(conn->route, &msg, TRUE);
        if(topic) { // no topic on memory failure
            topic_subscribe(conn, topic);
        } else {
            LDEBUG("Couldn't make topic");
        }
    } else if(cmdlen == 11 && !strncmp(cmd, "unsubscribe", cmdlen)) {
        LDEBUG("Websocket got UNSUBSCRIBE request");
        Z_RECV_NEXT(msg);
        connection_t *conn = find_connection(&msg);
        if(!conn) goto msg_error;
        if(conn->route != route) {
            LWARN("Connection has wrong route, skipping...");
            goto msg_error;
        }
        Z_RECV_LAST(msg);
        topic_t *topic = find_topic(route, &msg, FALSE);
        if(!topic) goto msg_error;
        topic_unsubscribe(conn, topic);
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
        LWARN("Wrong command [%d]``%s''", cmdlen, cmd);
        goto msg_error;
    }
    Z_SEQ_FINISH(msg);
    return;
msg_error:
    Z_SEQ_ERROR(msg);
    return;
}
