#include <sys/time.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "uidgen.h"
#include "config.h"
#include "main.h"
#include "log.h"

struct uid_pieces_s {
    uint64_t id;
    uint32_t time_sec;
    uint32_t time_micro;
};

int init_uid(config_main_t *config) {
    struct uid_pieces_s *pieces = (struct uid_pieces_s *)root.instance_id;
    pieces->id = htobe64(config->Server.ident);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    pieces->time_sec = tv.tv_sec;
    pieces->time_micro = tv.tv_usec;
    int fd = open("/dev/urandom", O_RDONLY);
    while(fd < 0) {
        SNIMPL(errno != EINTR);
        fd = open("/dev/urandom", O_RDONLY);
    }
    int res = 0;
    while(res < RANDOM_LENGTH) { //TODO: maybe optimize, but i think not worth
        res = read(fd, root.random_data, RANDOM_LENGTH);
    }
    SNIMPL(close(fd));
    return 0;
}

int make_hole_uid(void *object, char data[UID_LEN], sieve_t *sieve,
    bool secure)
{
    memcpy(data, root.instance_id, IID_LEN);
    if(sieve_find_hole(sieve, object,
        (size_t *)(data + IID_LEN + 8),
        (size_t *)(data + IID_LEN))) return -1;
    if(secure) {
        int fd = open("/dev/urandom", O_RDONLY);
        while(fd < 0) {
            SNIMPL(errno != EINTR);
            fd = open("/dev/urandom", O_RDONLY);
        }
        int res = 0;
        while(res < UID_NRANDOM) { //TODO: maybe optimize, but i think not worth
            res = read(fd, data+(UID_LEN - UID_NRANDOM), UID_NRANDOM);
        }
        SNIMPL(close(fd));
    } else {
        memset(data+(UID_LEN - UID_NRANDOM), 0, UID_NRANDOM);
    }
    return 0;
}
