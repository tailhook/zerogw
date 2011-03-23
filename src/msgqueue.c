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
        free(cur);
    }
}
