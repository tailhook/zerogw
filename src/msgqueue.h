#ifndef _H_MSGQUEUE
#define _H_MSGQUEUE

#include <sys/queue.h>
#include "pool.h"

typedef struct queue_item_s {
    TAILQ_ENTRY(queue_item_s) list;
} queue_item_t;

typedef struct queue_s {
    pool_t *pool;
    int size;
    int capacity;
    TAILQ_HEAD(queue_head_s, queue_item_s) items;
} queue_t;

int init_queue(queue_t *queue, int capacity, pool_t *pool);
void free_queue(queue_t *queue);
queue_item_t *queue_push(queue_t *queue);
queue_item_t *queue_force_push(queue_t *queue);
void queue_remove(queue_t *queue, queue_item_t *item); 

#endif //_H_MSGQUEUE
