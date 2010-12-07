#ifndef _H_WEBSOCKET
#define _H_WEBSOCKET

#include <website.h>
#include <zmq.h>
#include "main.h"
#include "request.h"
#include "config.h"

typedef struct subscriber_s {
    struct subscriber_s *topic_next;
    struct subscriber_s *topic_prev;
    struct subscriber_s *client_next;
    struct subscriber_s *client_prev;
    struct topic_s *topic;
    struct connection_s *connection;
} subscriber_t;


typedef struct message_s {
    ws_message_t ws;
    zmq_msg_t zmq;
} message_t;

int websock_start(connection_t *conn, config_Route_t *route);
int websock_message(connection_t *conn, message_t *msg);
int start_websocket(request_t *req);
int prepare_websockets(config_main_t *config, config_Route_t *root);

#endif // _H_WEBSOCKET
