#include "resolve.h"
#include "log.h"
#include "sieve.h"
#include "main.h"
#include "http.h"

const char *get_field(request_t *req, config_RequestField_t*value, size_t*len) {
    const char *result;
    switch(value->kind) {
    case CONFIG_Body:
        if(!req->ws.bodyposition) { // only headers read
            if(len) {
                *len = 0;
            }
            return NULL;
        }
        if(len) {
            *len = req->ws.bodylen;
        }
        return req->ws.body;
    case CONFIG_Header:
        result = req->ws.headerindex[value->_field_index];
        break;
    case CONFIG_Uri:
        result = req->ws.uri;
        break;
    case CONFIG_Path:
        result = req->path;
        break;
    case CONFIG_IP:
        if(!req->ip) {
            int family = req->ws.conn->addr.sin_family;
            if(family == AF_INET) {
                req->ip = obstack_alloc(&req->ws.pieces, 16);
                in_addr_t ip = ntohl(req->ws.conn->addr.sin_addr.s_addr);
                snprintf(req->ip, 16, "%d.%d.%d.%d",
                    ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
            } else {
                req->ip = "";
            }
        }
        result = req->ip;
        break;
    case CONFIG_Method:
        result = req->ws.method;
        break;
    case CONFIG_Cookie:
        LNIMPL("Cookie field");
    case CONFIG_Nothing:
        return NULL;
    default:
        LNIMPL("Unknown field");
    }
    if(len) {
        if(result) {
            *len = strlen(result);
        } else {
            *len = 0;
        }
    }
    return result;
}

static config_Route_t *do_resolve_url(request_t *req, bool ready) {
    if(sieve_full(root.request_sieve)) {
        LWARN("Too many requests");
        if(ready) {  // TODO: investigate this further
            http_static_response(req,
                &REQRCONFIG(req)->responses.service_unavailable);
        }
        return NULL;
    }

    config_Route_t *route = &REQCONFIG(req)->Routing;
    while(route->_child_match) {
        const char *data = get_field(req, &route->routing_by, NULL);
        if(!data) data = "";  // absence of field (e.g. no header)
                              // treated as if the header would be empty
        LDEBUG("Matching ``%s'' by %d", data, route->routing.kind);
        size_t tmp;
        switch(route->routing.kind) {
        case CONFIG_Exact:
            if(ws_match(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                if(ready)
                    http_static_response(req, &route->responses.not_found);
                return NULL;
            }
            continue;
        case CONFIG_Prefix:
            if(ws_fuzzy(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                if(ready)
                    http_static_response(req, &route->responses.not_found);
                return NULL;
            }
            continue;
        case CONFIG_Suffix:
            if(ws_rfuzzy(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                if(ready)
                    http_static_response(req, &route->responses.not_found);
                return NULL;
            }
            continue;
        case CONFIG_Hash:
            LNIMPL("Hash matching");
            continue;
        case CONFIG_Hash1024:
            LNIMPL("Consistent hash");
            continue;
        default:
            LNIMPL("Unknown routing %d", route->routing.kind);
        }
        break;
    }
    return route;
}


config_Route_t *preliminary_resolve(request_t *req) {
    return do_resolve_url(req, FALSE);
}
config_Route_t *resolve_url(request_t *req) {
    return do_resolve_url(req, TRUE);
}

