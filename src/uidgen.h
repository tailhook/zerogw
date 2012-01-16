#ifndef _H_UIDGEN
#define _H_UIDGEN

#include "sieve.h"

#define UID_NRANDOM 8
#define IID_LEN (sizeof(unsigned long) + 2*sizeof(uint32_t))
#define UID_LEN (IID_LEN + sizeof(unsigned long)*2 + UID_NRANDOM)
#define UID_HOLE(data) (*(unsigned long *)((char *)(data) + IID_LEN))
#define UID_EQ(a, b) (!memcmp((a), (b), UID_LEN))

int init_uid(config_main_t *config);
int make_hole_uid(void *object, char data[UID_LEN], sieve_t *sieve,
    bool secure);

#endif // _H_UIDGEN
