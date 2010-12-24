#include "resolve.h"
#include "log.h"
#include "sieve.h"
#include "main.h"
#include "http.h"

const char *get_field(request_t *req, config_RequestField_t*value, size_t*len) {
    const char *result;
    switch(value->kind) {
    case CONFIG_Body:
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



config_Route_t *resolve_url(request_t *req) {
    if(sieve_full(root.sieve)) {
        LWARN("Too many requests");
        http_static_response(req,
            &REQRCONFIG(req)->responses.service_unavailable);
        return NULL;
    }

    config_Route_t *route = &REQCONFIG(req)->Routing;
    while(route->_child_match) {
        const char *data = get_field(req, &route->routing_by, NULL);
        LDEBUG("Matching ``%s'' by %d", data, route->routing.kind);
        size_t tmp;
        switch(route->routing.kind) {
        case CONFIG_Exact:
            if(ws_match(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                http_static_response(req, &route->responses.not_found);
                return NULL;
            }
            continue;
        case CONFIG_Prefix:
            if(ws_fuzzy(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                http_static_response(req, &route->responses.not_found);
                return NULL;
            }
            continue;
        case CONFIG_Suffix:
            if(ws_rfuzzy(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
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

