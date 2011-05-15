#ifndef _H_RESOLVE
#define _H_RESOLVE

#include "config.h"
#include "request.h"

#define REQCONFIG(req) (((serverroot_t*)((req)->ws.conn->serv))->config)
#define REQROUTE(req) ((req)->route)
#define REQRCONFIG(req) (REQROUTE(req)?REQROUTE(req):&REQCONFIG(req)->Routing)

const char *get_field(request_t *req, config_RequestField_t*value, size_t*len);
config_Route_t *resolve_url(request_t *req);
config_Route_t *preliminary_resolve(request_t *req);

#endif
