#include <zmq.h>
#include <malloc.h>

#include "websocket.h"
#include "log.h"
#include "zutils.h"

#define HASH_SIZE_MIN 512

typedef struct topic_s {
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

topic_hash_t topic_table;

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

static topic_t *mktopic(char *name, int len, size_t hash) {
    topic_t * result = (topic_t *)malloc(sizeof(topic_t) + len);
    if(!result) return NULL;
    result->hash = hash;
    memcpy(result->topic, name, len);
    result->length = len;
    result->prev = NULL;
    result->next = NULL;
    result->first_sub = NULL;
    result->last_sub = NULL;
    return result;
}

static topic_t *find_topic(zmq_msg_t *msg, bool create) {
    char *topic_name = zmq_msg_data(msg);
    int topic_len = zmq_msg_size(msg);
    size_t hash = topic_hash(topic_name, topic_len);
    topic_t *result;
    if(!topic_table.table) {
        if(!create) return NULL;
        topic_table.hash_size = HASH_SIZE_MIN;
        topic_table.table = (topic_t **)malloc(HASH_SIZE_MIN*sizeof(topic_t*));
        if(!topic_table.table) return NULL;
        topic_table.ntopics = 1;
        topic_table.nsubscriptions = 0;
        result = mktopic(topic_name, topic_len, hash);
        topic_table.table[hash % topic_table.hash_size] = result;
        return result;
    }
    size_t cell =  hash % topic_table.hash_size;
    topic_t *current = topic_table.table[cell];
    if(!current) {
        if(!create) return NULL;
        result = mktopic(topic_name, topic_len, hash);
        topic_table.table[cell] = result;
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
    result = mktopic(topic_name, topic_len, hash);
    result->prev = current;
    current->next = result;
    return result;
}

static bool topic_subscribe(connection_t *conn, topic_t *topic) {
    subscriber_t *sub = (subscriber_t *)malloc(sizeof(subscriber_t));
    if(!sub) {
        return FALSE;
    }
    sub->connection = conn;
    sub->topic_prev = topic->last_sub;
    if(!topic->first_sub) topic->first_sub = sub;
    topic->last_sub = sub;
    sub->topic_next = NULL;
    sub->client_prev = conn->last_sub;
    if(!conn->first_sub) conn->first_sub = sub;
    conn->last_sub = sub;
    sub->client_next = NULL;
    return TRUE;
}

static bool topic_unsubscribe(connection_t *conn, topic_t *topic) {
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


void websock_process(zmq_socket_t sock) {
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
        topic_t *topic = find_topic(&msg, TRUE);
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
        Z_RECV_LAST(msg);
        topic_t *topic = find_topic(&msg, FALSE);
        if(!topic) goto msg_error;
        topic_unsubscribe(conn, topic);
    } else if(cmdlen == 7 && !strncmp(cmd, "publish", cmdlen)) {
        LDEBUG("Websocket got PUBLISH request");
        Z_RECV_NEXT(msg);
        topic_t *topic = find_topic(&msg, FALSE);
        if(!topic) {
            LDEBUG("Couldn't find topic to publish to");
            goto msg_error;
        }
        LDEBUG("Publishing to 0x%x", topic);
        Z_RECV_LAST(msg);
        topic_publish(topic, &msg);
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
