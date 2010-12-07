#ifndef _H_REQUEST
#define _H_REQUEST

#include <website.h>
#include <zmq.h>
#include "config.h"
#include "uidgen.h"

typedef struct request_s {
    ws_request_t ws;
    config_Route_t *route;
    char uid[UID_LEN];
    void *socket;
    int refcnt;
    bool has_message;
    zmq_msg_t response_msg;
} request_t;

#endif // _H_REQUEST
