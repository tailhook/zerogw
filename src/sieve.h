#ifndef _H_SIEVE
#define _H_SIEVE

#include <stddef.h>
#include <stdint.h>

#include "config.h"

typedef struct sieve_s {
    uint64_t index;
    size_t max;
    size_t offset;
    size_t filled;
    void *items[];
} sieve_t;


void sieve_prepare(sieve_t **sieve, int size);
void sieve_free(sieve_t *sieve);
int sieve_find_hole(sieve_t *, void *item, size_t *index, size_t *hole);
bool sieve_full(sieve_t *sieve);
void *sieve_get(sieve_t *sieve, size_t holeid);
void sieve_empty(sieve_t *sieve, size_t holeid);

#endif //_H_SIEVE
