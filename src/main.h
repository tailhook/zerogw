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

typedef struct statistics_s {
    size_t http_requests;
    size_t http_replies;
    size_t zmq_requests;
    size_t zmq_retries;
    size_t zmq_replies;
    size_t zmq_orphan_replies;
    size_t websock_connects;
    size_t websock_disconnects;
    size_t topics_created;
    size_t topics_removed;
    size_t websock_subscribed;
    size_t websock_unsubscribed;
    size_t websock_published;
    size_t websock_sent;
} statistics_t;

typedef struct serverroot_s {
    ws_server_t ws;
    void *zmq;
    evloop_t loop;
    char instance_id[IID_LEN];
    config_main_t *config;
    config_Route_t **wsock_routes;
    sieve_t *sieve;
    statistics_t stat;
} serverroot_t;

extern serverroot_t root;

#endif
