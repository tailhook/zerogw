#ifndef _H_HTTP
#define _H_HTTP

#include "request.h"
#include "config.h"

int http_headers(request_t *req);
int http_request(request_t *req);
int http_request_finish(request_t *req);
void http_static_response(request_t *req, config_StaticResponse_t *resp);
int prepare_http(config_main_t *config, config_Route_t *root);
int release_http(config_main_t *config, config_Route_t *root);
void request_decref(void *_data, void *request);

int http_common_headers(request_t *req);

#endif // _H_HTTP
