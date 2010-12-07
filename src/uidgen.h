#ifndef _H_UIDGEN
#define _H_UIDGEN

#include "sieve.h"

#define IID_LEN (sizeof(unsigned long) + 8)
#define UID_LEN (IID_LEN + sizeof(unsigned long)*2)
#define UID_HOLE(data) (*(unsigned long *)((char *)(data) + IID_LEN))
#define UID_EQ(a, b) (!memcmp((a), (b), UID_LEN))

int init_uid();
int make_hole_uid(void *object, char data[UID_LEN], sieve_t *sieve);

#endif // _H_UIDGEN
