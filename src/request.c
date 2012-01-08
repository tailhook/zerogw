
#include "request.h"
#include "main.h"

void request_init(request_t *req) {
    req->refcnt = 1;
    req->flags = 0;
    req->route = NULL;
    req->timeout.active = 0;
    req->retries = 0;
    req->uid[0] = 0;
    req->hybi = NULL;
    req->ip = NULL;
}

void request_finish(request_t *req) {
    if(req->flags & REQ_IN_SIEVE) {
        sieve_empty(root.request_sieve, UID_HOLE(req->uid));
        req->flags &= ~REQ_IN_SIEVE;
    }
    if(req->timeout.active) {
        ev_timer_stop(root.loop, &req->timeout);
    }
}

void request_free(request_t *req) {
    if(req->flags & REQ_HAS_MESSAGE) {
        zmq_msg_close(&req->response_msg);
    }
    request_finish(req);
    if(!req->ws.websocket) {
        root.stat.http_replies += 1;
    }
    ws_request_free(&(req)->ws);
}
