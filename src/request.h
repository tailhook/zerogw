#ifndef _H_REQUEST
#define _H_REQUEST

#include <website.h>
#include <zmq.h>
#include "config.h"
#include "uidgen.h"

typedef enum {
    REQ_HAS_MESSAGE = 1,
    REQ_IN_SIEVE = 2,
    REQ_REPLIED = 4,
} request_flags;

#define REQ_DECREF(req) if(!--(req)->refcnt) { request_free(req); }
#define REQ_INCREF(req) (++(req)->refcnt)

typedef struct request_s {
    ws_request_t ws;
    struct ev_timer timeout;
    config_Route_t *route;
    char uid[UID_LEN];
    int refcnt;
    int retries;
    int flags;
    zmq_msg_t response_msg;
} request_t;

void request_init(request_t *req);
void request_finish(request_t *req);
void request_free(request_t *req);

#endif // _H_REQUEST
