#ifndef _H_NAMEDOUTPUT
#define _H_NAMEDOUTPUT

typedef struct namedoutput_s {
    LIST_HEAD(nout_clients_s, output_s) outputs;
    struct ev_timer sync_tm;
} namedoutput_t;

#endif //_H_NAMEDOUTPUT
