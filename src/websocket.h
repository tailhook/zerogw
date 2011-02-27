#ifndef _H_WEBSOCKET
#define _H_WEBSOCKET

#include <website.h>
#include <zmq.h>
#include <sys/queue.h>
#include "main.h"
#include "request.h"
#include "config.h"

typedef struct subscriber_s {
    LIST_ENTRY(subscriber_s) topic_list;
    LIST_ENTRY(subscriber_s) client_list;
    struct topic_s *topic;
    struct hybi_s *connection;
} subscriber_t;


typedef struct message_s {
    ws_message_t ws;
    zmq_msg_t zmq;
} message_t;

int websock_start(connection_t *conn, config_Route_t *route);
int websock_message(connection_t *conn, message_t *msg);
int start_websocket(request_t *req);
int prepare_websockets(config_main_t *config, config_Route_t *root);
int release_websockets(config_main_t *config, config_Route_t *root);
int pause_websockets(bool pause);
hybi_t *hybi_start(config_Route_t *route, hybi_enum type);
void hybi_stop(hybi_t *hybi);
hybi_t *hybi_find(char *data);

#endif // _H_WEBSOCKET
