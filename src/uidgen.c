#include <sys/time.h>

#include "uidgen.h"
#include "config.h"
#include "main.h"

struct uid_pieces_s {
    unsigned long id;
    uint32_t time_sec;
    uint32_t time_micro;
};

int init_uid(config_main_t *config) {
    struct uid_pieces_s *pieces = (struct uid_pieces_s *)root.instance_id;
    pieces->id = config->Server.ident;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    pieces->time_sec = tv.tv_sec;
    pieces->time_micro = tv.tv_usec;
}

int make_hole_uid(void *object, char data[UID_LEN], sieve_t *sieve) {
    memcpy(data, root.instance_id, IID_LEN);
    if(sieve_find_hole(sieve, object,
        (size_t *)(data + IID_LEN + 8),
        (size_t *)(data + IID_LEN))) return -1;
    return 0;
}
