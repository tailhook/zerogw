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
    int first_index;
    ev_idle sendlater;
    ev_timer inactivity;
    queue_t queue;
} comet_t;

int comet_send(struct hybi_s *hybi, struct message_s *msg);
void comet_close(struct hybi_s *hybi);
int comet_request(struct request_s *req);
void comet_request_aborted(request_t *req);

#endif //_H_POLLING
