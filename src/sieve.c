#include <strings.h>
#include <malloc.h>
#include <errno.h>

#include "sieve.h"
#include "log.h"

int sieve_find_hole(sieve_t *sieve, void *item, size_t *index, size_t *hole) {
    void **cur = sieve->items + sieve->offset;
    void **end = sieve->items + sieve->max;
    for(void **i = cur; i != end; ++i) {
        if(!*i) {
            sieve->offset = i+1 - sieve->items;
            *hole = i - sieve->items;
            *index = sieve->index++;
            sieve->items[*hole] = item;
            sieve->filled += 1;
            return 0;
        }
    }
    for(void **i = sieve->items; i != cur; ++i) {
        if(!*i) {
            sieve->offset = i+1 - sieve->items;
            *hole = i - sieve->items;
            *index = sieve->index++;
            sieve->items[*hole] = item;
            sieve->filled += 1;
            return 0;
        }
    }
    errno = EAGAIN;
    return -1;
}


void sieve_prepare(sieve_t **sieve, int size) {
    int ssize = sizeof(sieve_t) + sizeof(void*)*size;
    *sieve = malloc(ssize);
    ANIMPL2(*sieve, "Not enought memory");
    bzero(*sieve, ssize);
    (*sieve)->max = size;
}

void sieve_free(sieve_t *sieve) {
    free(sieve);
}

bool sieve_full(sieve_t *sieve) {
    return sieve->filled >= sieve->max;
}

void *sieve_get(sieve_t *sieve, size_t holeid) {
    if(holeid > sieve->max) {
        return NULL;
    }
    return sieve->items[holeid];
}

void sieve_empty(sieve_t *sieve, size_t holeid) {
    ANIMPL(sieve->items[holeid]);
    sieve->items[holeid] = NULL;
    sieve->filled -= 1;
}
