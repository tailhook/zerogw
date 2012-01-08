#ifndef _H_WEBSOCKET
#define _H_WEBSOCKET

#include <website.h>
#include <zmq.h>
#include <sys/queue.h>
#include "request.h"
#include "config.h"
#include "pool.h"
#include "polling.h"

#define hybi_INCREF(hybi) (hybi)->refcnt += 1;
#define hybi_DECREF(hybi) if(--(hybi)->refcnt <= 0) hybi_free(hybi);

#define WS_HAS_COOKIE 1

typedef enum {
    HYBI_WEBSOCKET,
    HYBI_COMET
} hybi_enum;

typedef struct subscriber_s {
    LIST_ENTRY(subscriber_s) topic_list;
    LIST_ENTRY(subscriber_s) client_list;
    struct topic_s *topic;
    struct hybi_s *connection;
} subscriber_t;

typedef struct output_s {
    LIST_ENTRY(output_s) client_list;
    LIST_ENTRY(output_s) output_list;
    config_namedoutput_t *socket;
    struct hybi_s *connection;
    int prefix_len;
    char prefix[];
} output_t;

typedef struct backend_msg_s {
    queue_item_t qhead;
    struct hybi_s *hybi;
    void *msg;
} backend_msg_t;

typedef struct frontend_msg_s {
    queue_item_t qhead;
    zmq_msg_t zmsg;
} frontend_msg_t;

typedef struct message_s {
    ws_message_t ws;
    zmq_msg_t zmq;
} message_t;

typedef struct hybi_s {
    hybi_enum type;
    unsigned refcnt;
    char uid[UID_LEN];
    int flags;
    zmq_msg_t cookie;
    config_Route_t *route;
    LIST_HEAD(conn_subscribers_s, subscriber_s) subscribers;
    LIST_HEAD(conn_outputs_s, output_s) outputs;
    connection_t *conn;
    comet_t comet[]; // tiny hack, to use less memory, but be efficient
} hybi_t;

typedef struct hybi_global_s {
    bool paused;
    sieve_t *sieve;
    pool_t subscriber_pool; // topic subscribers entity pool (subscriber_t)
    pool_t output_pool; // client backend mappings (output_t)
    pool_t backend_pool; // messages from client to backend
    pool_t frontend_pool; // messages to clients
} hybi_global_t;

int websock_start(connection_t *conn, config_Route_t *route);
int websock_message(connection_t *conn, message_t *msg);
int start_websocket(request_t *req);

int prepare_websockets(config_main_t *config, config_Route_t *root);
int release_websockets(config_main_t *config, config_Route_t *root);
int pause_websockets(bool pause);
void websockets_sync_now();

hybi_t *hybi_start(config_Route_t *route, hybi_enum type);
void hybi_stop(hybi_t *hybi);
hybi_t *hybi_find(char *data);

config_zmqsocket_t *websock_resolve(hybi_t *hybi, char *data, int length);
int backend_send(config_zmqsocket_t *sock, hybi_t *hybi, void *msg, bool force);

#endif // _H_WEBSOCKET
