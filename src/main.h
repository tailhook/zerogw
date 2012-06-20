#ifndef _H_MAIN
#define _H_MAIN

#define SHIFT(ptr, typ, mem) ((typ *)((char *)(ptr) - offsetof(typ, mem)))
#define RANDOM_LENGTH 16
#define STAT_MAXLEN 4096

#include <website.h>
#include <pthread.h>
#include <sys/queue.h>

#include <ev.h>

#include "config.h"
#include "websocket.h"
#include "sieve.h"
#include "zutils.h"
#include "uidgen.h"
#include "polling.h"
#include "disk.h"

typedef struct statistics_s {
    size_t connects;
    size_t disconnects;
    size_t http_requests;
    size_t http_replies;
    size_t zmq_requests;
    size_t zmq_retries;
    size_t zmq_replies;
    size_t zmq_orphan_replies;
    size_t websock_connects;
    size_t websock_disconnects;
    size_t comet_connects;
    size_t comet_disconnects;
    size_t comet_acks;
    size_t comet_empty_replies;
    size_t comet_aborted_replies;
    size_t comet_received_messages;
    size_t comet_received_batches;
    size_t comet_sent_messages;
    size_t comet_sent_batches;
    size_t topics_created;
    size_t topics_removed;
    size_t websock_subscribed;
    size_t websock_unsubscribed;
    size_t websock_published;
    size_t websock_sent;
    size_t websock_received;
    size_t websock_backend_queued;
    size_t websock_backend_unqueued;
    size_t websock_sent_pings;
    size_t disk_requests;
    size_t disk_reads;
    size_t disk_bytes_read;
} statistics_t;

typedef struct serverroot_s {
    ws_server_t ws;
    void *zmq;
    struct ev_loop *loop;

    char instance_id[IID_LEN];
    char random_data[RANDOM_LENGTH];
    config_main_t *config;

    sieve_t *request_sieve;

    hybi_global_t hybi;
    disk_global_t disk;

    statistics_t stat;
} serverroot_t;

extern serverroot_t root;

int format_statistics(char *buf);

#endif
