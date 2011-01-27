#ifndef _H_POLLING
#define _H_POLLING

#include "request.h"

struct connection_s;
struct hybi_s;
struct message_s;

typedef struct comet_s {
    request_t *cur_request;
    int overflow;
    int cur_format;
    int cur_limit;
    int queue_size;
    int first_index;
    int current_queue;
    ev_idle sendlater;
    ev_timer inactivity;
    struct message_s *queue[];
} comet_t;

int comet_send(struct hybi_s *hybi, struct message_s *msg);
int comet_request(struct request_s *req);

#endif //_H_POLLING
