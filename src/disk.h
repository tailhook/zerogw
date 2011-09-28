#ifndef _H_DISK
#define _H_DISK

#include <zmq.h>

#include "config.h"

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
    char if_modified[32];
    char *path;
} disk_request_t;

typedef struct mime_entry_s {
    LIST_ENTRY(mime_entry_s) lst;
    char *mime;
    char name[];
} mime_entry_t;

typedef struct mime_table_s {
    int size;
    struct obstack pieces;
    LIST_HEAD(mime_table_head_s, mime_entry_s) entries[];
} mime_table_t;

typedef struct disk_global_s {
    void *socket;
    struct ev_io watch;
    struct ev_async async;
    pthread_t *threads;
    int IF_MODIFIED;
    int ACCEPT_ENCODING;
    mime_table_t *mime_table;
} disk_global_t;

#include "main.h"

void *disk_loop(void *);
int disk_request(request_t *req);
int prepare_disk(config_main_t *config);
int release_disk(config_main_t *config);

#endif // _H_DISK
