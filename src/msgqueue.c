#include <stddef.h>
#include <stdlib.h>

#include "msgqueue.h"

int init_queue(queue_t *queue, int capacity, pool_t *element_pool) {
    queue->pool = element_pool;
    queue->capacity = capacity;
    queue->size = 0;
    TAILQ_INIT(&queue->items);
    return 0;
}

void free_queue(queue_t *queue) {
    queue_item_t *cur, *nxt;
    for(cur = TAILQ_FIRST(&queue->items); cur; cur = nxt) {
        nxt = TAILQ_NEXT(cur, list);
        pool_free(queue->pool, cur);
    }
}

queue_item_t *queue_push(queue_t *queue) {
    if(queue->size >= queue->capacity) return NULL;
    return queue_force_push(queue);
}

queue_item_t *queue_force_push(queue_t *queue) {
    queue_item_t *res = pool_alloc(queue->pool);
    if(!res) return NULL;
    TAILQ_INSERT_TAIL(&queue->items, res, list);
    queue->size += 1;
    return res;
}

void queue_remove(queue_t *queue, queue_item_t *item) {
    TAILQ_REMOVE(&queue->items, item, list);
    pool_free(queue->pool, item);
    queue->size -= 1;
}
