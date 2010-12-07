#ifndef _H_MAIN
#define _H_MAIN

#include <website.h>

#include "config.h"
#include "sieve.h"
#include "zutils.h"
#include "uidgen.h"

#define REQ_DECREF(req) if(!--(req)->refcnt) { ws_request_free(&(req)->ws); }
#define REQ_INCREF(req) (++(req)->refcnt)

typedef struct ev_loop *evloop_t;
typedef struct ev_io watch_t;

typedef struct connection_s {
    ws_connection_t ws;
    config_Route_t *route;
    char uid[UID_LEN];
    struct subscriber_s *first_sub;
    struct subscriber_s *last_sub;
} connection_t;

typedef struct serverroot_s {
    ws_server_t ws;
    void *zmq;
    evloop_t loop;
    char instance_id[IID_LEN];
    config_main_t *config;
    config_Route_t **wsock_routes;
    sieve_t *sieve;
} serverroot_t;

extern serverroot_t root;

#endif
