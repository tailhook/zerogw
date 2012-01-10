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
    #define DEFINE_VALUE(name) size_t name;
    #include "statistics.h"
    #undef DEFINE_VALUE
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
