#ifndef _H_WEBSOCKET
#define _H_WEBSOCKET

#include <website.h>
#include <zmq.h>

#include "main.h"

typedef struct message_s {
    ws_message_t ws;
    zmq_msg_t msg;
} message_t;

void websock_start(connection_t *conn, config_Route_t *route);
int websock_message(connection_t *conn, message_t *msg);

#endif // _H_WEBSOCKET
