#ifndef _H_DISK
#define _H_DISK

#include <zmq.h>

#include "config.h"
#include "main.h"

/*
typedef enum {
    RE_NOTCHECKED,
    RE_OK,
    RE_NOTFOUND,
    RE_DENY
} result_t;

typedef struct cache_entry_s {
    UT_hash_handle hh;
    pthread_rwlock_t lock;
    zmq_msg_t data;
    zmq_msg_t gzipped;
    struct stat filestat;
    double check_time;
    char last_modified[30];
    char etag[22];
    int pathlen;
    char realpath[];
} cache_entry_t;

typedef struct cached_url_s {
    UT_hash_handle hh;
    pthread_rwlock_t lock;
    size_t hash;
    double last_access;
    double access_freq;
    int result;
    char *uri;
    char *realpath;
} cached_url_t;

typedef struct disk_cache_s {
    cache_entry_t *table;
} disk_cache_t;

typedef struct url_cache_s {
    cached_url_t *table;
} url_cache_s;
*/

typedef struct disk_request_s {
    config_Route_t *route;
    bool gzipped;
    char *path;
} disk_request_t;

void *disk_loop(void *);
int disk_request(request_t *req);
int prepare_disk(config_main_t *config);
int release_disk(config_main_t *config);

#endif // _H_DISK
