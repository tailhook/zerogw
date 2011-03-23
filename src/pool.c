#include <stdlib.h>

#include "pool.h"
#include "log.h"


int init_pool(pool_t *pool, int object_size, int max_free) {
    ANIMPL(object_size >= sizeof(pooled_t));
    LIST_INIT(&pool->objects);
    pool->max_free = max_free;
    pool->object_size = object_size;
    pool->current_free = 0;
    return 0;
}

void free_pool(pool_t *pool) {
    pooled_t *cur, *nxt;
    for(cur = LIST_FIRST(&pool->objects); cur; cur = nxt) {
        nxt = LIST_NEXT(cur, list);
        free(cur);
    }
}

void *pool_alloc(pool_t *pool) {
    if(pool->current_free) {
        pooled_t *val = LIST_FIRST(&pool->objects);
        LIST_REMOVE(val, list);
        pool->current_free -= 1;
        return val;
    }
    return malloc(pool->object_size);
}
void pool_free(pool_t *pool, void *obj) {
    if(pool->current_free < pool->max_free) {
        pooled_t *val = obj;
        LIST_INSERT_HEAD(&pool->objects, val, list);
        pool->current_free += 1;
    } else {
        free(obj);
    }
}

