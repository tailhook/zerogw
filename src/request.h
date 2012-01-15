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
    REQ_OWNS_WSMESSAGE = 8
} request_flags;

#define REQ_DECREF(req) if(!__sync_sub_and_fetch(&(req)->refcnt, 1)) { request_free(req); }
#define REQ_INCREF(req) (__sync_add_and_fetch(&(req)->refcnt, 1))

typedef struct request_s {
    ws_request_t ws;
    struct ev_timer timeout;
    config_Route_t *route;
    struct hybi_s *hybi;
    struct message_s *ws_msg;
    char *path;
    char *ip;
    char uid[UID_LEN];
    int refcnt;
    int retries;
    int flags;
    ev_tstamp incoming_time;
    ev_tstamp outgoing_time;  // Currently keeped only for comet
    zmq_msg_t response_msg;
} request_t;

typedef struct connection_s {
    ws_connection_t ws;
    struct ev_timer idle_timer;
    struct hybi_s *hybi;
} connection_t;

void request_init(request_t *req);
void request_finish(request_t *req);
void request_free(request_t *req);

#endif // _H_REQUEST
