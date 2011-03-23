#ifndef _H_POOL
#define _H_POOL

#include <sys/queue.h>

typedef struct pooled_s {
  LIST_ENTRY(pooled_s) list;
} pooled_t;

typedef struct pool_s {
  int object_size;
  int max_free;
  int current_free;
  LIST_HEAD(pool_list_s, pooled_s) objects;
} pool_t;

int init_pool(pool_t *, int object_size, int max_free);
void free_pool(pool_t *);
void *pool_alloc(pool_t *);
void pool_free(pool_t *, void *);

#endif //_H_POOL
