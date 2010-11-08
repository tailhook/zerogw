#ifndef _H_MAIN
#define _H_MAIN

#include <website.h>

#include "config.h"

typedef struct connection_s {
    ws_connection_t ws;
    config_Route_t *route;
    char connection_id[64];
} connection_t;

#endif
