#include <unistd.h>

#include "http.h"
#include "config.h"
#include "log.h"
#include "main.h"
#include "resolve.h"

void http_static_response(request_t *req, config_StaticResponse_t *resp) {
    char status[resp->status_len + 5];
    sprintf(status, "%03d %s", resp->code, resp->status);
    ws_statusline(&req->ws, status);
    //ws_add_header(&req->ws, "Server", config.Server.header);
    CONFIG_STRING_STRING_LOOP(line, resp->headers) {
        ws_add_header(&req->ws, line->key, line->value);
    }
    LDEBUG("Replying with %d bytes", resp->body_len);
    ws_reply_data(&req->ws, resp->body, resp->body_len);
}

void request_decref(void *_data, void *request) {
    REQ_DECREF((request_t *)request);
}

int http_request_finish(request_t *req) {
    if(req->has_message) {
        zmq_msg_close(&req->response_msg);
    }
    REQ_DECREF(req);
    return 1;
}

void http_process(struct ev_loop *loop, struct ev_io *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));
    
    config_Route_t *route = (config_Route_t *)((char *)watch
        - offsetof(config_Route_t, zmq_forward._watch));
    while(TRUE) {
        Z_SEQ_INIT(msg, route->zmq_forward._sock);
        Z_RECV_START(msg, break);
        if(zmq_msg_size(&msg) != UID_LEN) {
            LWARN("Wrong uid length %d", zmq_msg_size(&msg));
            goto msg_error;
        }
        request_t *req = sieve_get(root.sieve, UID_HOLE(zmq_msg_data(&msg)));
        if(!req || !UID_EQ(req->uid, zmq_msg_data(&msg))) {
            LWARN("Wrong uid [%d]``%.*s'' (%x)", zmq_msg_size(&msg),
                zmq_msg_size(&msg), zmq_msg_data(&msg),
                UID_HOLE(zmq_msg_data(&msg)));
            goto msg_error;
        }
        if(route->zmq_forward.kind == CONFIG_zmq_Req
            || route->zmq_forward.kind == CONFIG_auto) {
            Z_RECV_NEXT(msg);
            if(zmq_msg_size(&msg)) { // The sentinel of routing data
                goto msg_error;
            }
        }
        Z_RECV(msg);
        if(msg_opt) { //first is a status-line if its not last
            char *data = zmq_msg_data(&msg);
            char *tail;
            int dlen = zmq_msg_size(&msg);
            LDEBUG("Status line: [%d] %s", dlen, data);
            ws_statusline(&req->ws, data);

            Z_RECV(msg);
            if(msg_opt) { //second is headers if its not last
                char *data = zmq_msg_data(&msg);
                char *name = data;
                char *value = NULL;
                int dlen = zmq_msg_size(&msg);
                char *end = data + dlen;
                int state = 0;
                for(char *cur = data; cur < end; ++cur) {
                    for(; cur < end; ++cur) {
                        if(!*cur) {
                            value = cur + 1;
                            ++cur;
                            break;
                        }
                    }
                    for(; cur < end; ++cur) {
                        if(!*cur) {
                            ws_add_header(&req->ws, name, value);
                            name = cur + 1;
                            break;
                        }
                    }
                }
                if(name < end) {
                    LWARN("Some garbage at end of headers. "
                          "Please finish each name and each value "
                          "with '\\0' character");
                }
                Z_RECV(msg);
                if(msg_opt) {
                    LWARN("Too many message parts");
                    http_static_response(req,
                        &REQRCONFIG(req)->responses.internal_error);
                    goto msg_error;
                }
            }
        }
        // the last part is always a body
        ANIMPL(!req->has_message);
        SNIMPL(zmq_msg_init(&req->response_msg));
        req->has_message = TRUE;
        SNIMPL(zmq_msg_move(&req->response_msg, &msg));
        SNIMPL(ws_reply_data(&req->ws, zmq_msg_data(&req->response_msg),
            zmq_msg_size(&req->response_msg)));
        sieve_empty(root.sieve, UID_HOLE(req->uid));
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        Z_SEQ_ERROR(msg);
        continue;
    }
}

static int socket_visitor(config_Route_t *route) {
    if(route->zmq_forward.value_len) {
        SNIMPL(zmq_open(&route->zmq_forward, ZMASK_REQ, ZMQ_XREQ,
            http_process, root.loop));
    }
    if(route->routing.kind) {
        switch(route->routing.kind) {
        case CONFIG_Exact:
            route->_child_match = ws_match_new();
            CONFIG_ROUTE_LOOP(item, route->children) {
                CONFIG_STRING_LOOP(value, item->value.match) {
                    void *val = (void *)ws_match_add(route->_child_match,
                        value->value, (size_t)&item->value);
                    if(val != &item->value) {
                        LWARN("Conflicting route \"%s\"", value->value);
                    }
                }
                SNIMPL(socket_visitor(&item->value));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                void *val = (void *)ws_match_add(route->_child_match, item->key,
                    (size_t)&item->value);
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(socket_visitor(&item->value));
            }
            ws_match_compile(route->_child_match);
            break;
        case CONFIG_Prefix:
            route->_child_match = ws_fuzzy_new();
            CONFIG_ROUTE_LOOP(item, route->children) {
                CONFIG_STRING_LOOP(value, item->value.match) {
                    char *star = strchr(value->value, '*');
                    void *val;
                    if(star) {
                        *star = '\0';
                        val = (void *)ws_fuzzy_add(route->_child_match,
                            value->value, TRUE, (size_t)&item->value);
                        *star = '*';
                    } else {
                        val = (void *)ws_fuzzy_add(route->_child_match,
                            value->value, FALSE, (size_t)&item->value);
                    }
                    if(val != &item->value) {
                        LWARN("Conflicting route \"%s\"", value->value);
                    }
                }
                SNIMPL(socket_visitor(&item->value));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                char *star = strchr(item->key, '*');
                void *val;
                if(star) {
                    *star = '\0';
                    val = (void *)ws_fuzzy_add(route->_child_match,
                        item->key, TRUE, (size_t)&item->value);
                    *star = '*';
                } else {
                    val = (void *)ws_fuzzy_add(route->_child_match,
                        item->key, FALSE, (size_t)&item->value);
                }
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(socket_visitor(&item->value));
            }
            ws_fuzzy_compile(route->_child_match);
            break;
        case CONFIG_Suffix:
            route->_child_match = ws_fuzzy_new();
            CONFIG_ROUTE_LOOP(item, route->children) {
                CONFIG_STRING_LOOP(value, item->value.match) {
                    char *star = strchr(value->value, '*');
                    void *val = (void *)ws_fuzzy_add(route->_child_match,
                        star?star+1:value->value, !!star, (size_t)&item->value);
                    if(val != &item->value) {
                        LWARN("Conflicting route \"%s\"", value->value);
                    }
                }
                SNIMPL(socket_visitor(&item->value));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                char *star = strchr(item->key, '*');
                void *val = (void *)ws_fuzzy_add(route->_child_match,
                    star ? star+1 : item->key, !!star, (size_t)&item->value);
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(socket_visitor(&item->value));
            }
            ws_rfuzzy_compile(route->_child_match);
            break;
        default:
            LNIMPL("Routing tag ", route->routing.kind);
        }
        switch(route->routing_by.kind) {
        case CONFIG_Header:
            route->routing_by._field_index = ws_index_header(&root.ws,
                route->routing_by.value);
            break;
        }
    } else {
        if(route->children || route->map) {
            LWARN("Children in route with no routing specified");
        }
    }
    return 0;
}

int http_request(request_t *req) {
    req->refcnt = 1;
    req->has_message = FALSE;
    req->route = NULL;
    config_Route_t *route = resolve_url(req);

    if(!route) { // already replied
        return 0;
    }

    // Let's decide whether it's static
    if(!route->zmq_forward.value_len) {
        http_static_response(req, &route->responses.default_);
        return 0;
    }
    // Ok, it's zeromq forward
    make_hole_uid(req, req->uid, root.sieve);
    req->socket = route->zmq_forward._sock;
    zmq_msg_t msg;
    REQ_INCREF(req);
    SNIMPL(zmq_msg_init_data(&msg, req->uid, UID_LEN, request_decref, req));
    SNIMPL(zmq_send(req->socket, &msg, ZMQ_SNDMORE));
    if(route->zmq_forward.kind == CONFIG_zmq_Req
        || route->zmq_forward.kind == CONFIG_auto) {
        SNIMPL(zmq_send(req->socket, &msg, ZMQ_SNDMORE)); // empty sentinel
    }
    config_a_RequestField_t *contents = route->zmq_contents;
    ANIMPL(contents);
    for(config_a_RequestField_t *item=contents; item; item = item->head.next) {
        size_t len;
        const char *value = get_field(req, &item->value, &len);
        zmq_msg_t msg;
        REQ_INCREF(req);
        SNIMPL(zmq_msg_init_data(&msg, (void *)value, len, request_decref, req));
        SNIMPL(zmq_send(req->socket, &msg,
            (item->head.next ? ZMQ_SNDMORE : 0)));
        SNIMPL(zmq_msg_close(&msg));
    }
    return 0;
}

int prepare_http(config_main_t *config, config_Route_t *root) {
    SNIMPL(socket_visitor(&config->Routing));
    LINFO("Http backend connections complete");
    return 0;
}

