#ifndef _H_MAIN
#define _H_MAIN

#include <website.h>

#include "config.h"
#include "sieve.h"
#include "zutils.h"

#define INSTANCE_ID_LEN 32
#define CONNECTION_ID_LEN (INSTANCE_ID_LEN + sizeof(size_t)*2)

typedef struct ev_loop *evloop_t;
typedef struct ev_io watch_t;
typedef int socket_t;

typedef struct connection_s {
    ws_connection_t ws;
    config_Route_t *route;
    char connection_id[CONNECTION_ID_LEN];
    struct subscriber_s *first_sub;
    struct subscriber_s *last_sub;
} connection_t;

typedef struct serverroot_s {
    ws_server_t ws;
    zmq_context_t zmq;
    zmq_socket_t worker_push;
    zmq_socket_t worker_pull;
    evloop_t loop;
    socket_t worker_event;
    watch_t worker_watch;
} serverroot_t;

extern sieve_t *sieve;
extern char instance_id[INSTANCE_ID_LEN];
extern serverroot_t root;

#endif
