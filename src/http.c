#include <unistd.h>
#include <time.h>

#include "http.h"
#include "config.h"
#include "log.h"
#include "main.h"
#include "resolve.h"
#include "disk.h"

int http_common_headers(request_t *req) {
    char buf[32];
    struct tm tmstruct;
    time_t inctime = req->incoming_time;
    if(!gmtime_r(&inctime, &tmstruct))
        return -1;
    int tlen = strftime(buf, 32, "%a, %d %b %Y %T GMT", &tmstruct);
    if(tlen <= 0)
        return -1;
    ANIMPL(tlen < 32);
    if(ws_add_header(&req->ws, "Date", buf) < 0)
        return -1;
    return 0;
}

void http_static_response(request_t *req, config_StaticResponse_t *resp) {
    char status[resp->status_len + 5];
    sprintf(status, "%03ld %s", resp->code, resp->status);
    SNIMPL(ws_statusline(&req->ws, status));
    SNIMPL(http_common_headers(req));
    CONFIG_STRING_STRING_LOOP(line, resp->headers) {
        SNIMPL(ws_add_header(&req->ws, line->key, line->value));
    }
    LDEBUG("Replying with %d bytes", resp->body_len);
    SNIMPL(ws_reply_data(&req->ws, resp->body, resp->body_len));
}

void request_decref(void *_data, void *request) {
    REQ_DECREF((request_t *)request);
}

int http_request_finish(request_t *req) {
    ANIMPL(!req->hybi);
    REQ_DECREF(req);
    return 1;
}

void http_process(struct ev_loop *loop, struct ev_io *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));

    config_Route_t *route = (config_Route_t *)((char *)watch
        - offsetof(config_Route_t, zmq_forward.socket._watch));
    while(TRUE) {
        Z_SEQ_INIT(msg, route->zmq_forward.socket._sock);
        Z_RECV_START(msg, break);
        if(zmq_msg_size(&msg) != UID_LEN) {
            TWARN("Wrong uid length %d", zmq_msg_size(&msg));
            goto msg_error;
        }
        request_t *req = sieve_get(root.request_sieve,
            UID_HOLE(zmq_msg_data(&msg)));
        if(!req || !UID_EQ(req->uid, zmq_msg_data(&msg))) {
            root.stat.zmq_orphan_replies += 1;
            TWARN("Wrong uid [%d]``%.*s'' (%x)", zmq_msg_size(&msg),
                zmq_msg_size(&msg), zmq_msg_data(&msg),
                UID_HOLE(zmq_msg_data(&msg)));
            goto msg_error;
        }
        if(route->zmq_forward.socket.kind == CONFIG_zmq_Req
            || route->zmq_forward.socket.kind == CONFIG_auto) {
            Z_RECV_NEXT(msg);
            if(zmq_msg_size(&msg)) { // The sentinel of routing data
                goto msg_error;
            }
        }
        root.stat.zmq_replies += 1;
        Z_RECV(msg);
        if(msg_opt) { //first is a status-line if its not last
            char *data = zmq_msg_data(&msg);
            char *tail;
            int dlen = zmq_msg_size(&msg);
            LDEBUG("Status line: ``%.*s''", dlen, data);
            ws_statusline_len(&req->ws, data, dlen);

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
                    TWARN("Some garbage at end of headers. "
                          "Please finish each name and each value "
                          "with '\\0' character");
                }
                Z_RECV(msg);
                if(msg_opt) {
                    TWARN("Too many message parts");
                    http_static_response(req,
                        &REQRCONFIG(req)->responses.internal_error);
                    request_finish(req);
                    goto msg_error;
                }
            }
        } else {
            ws_statusline(&req->ws, "200 OK");
        }
        http_common_headers(req);
        CONFIG_STRING_STRING_LOOP(line, route->headers) {
            SNIMPL(ws_add_header(&req->ws, line->key, line->value));
        }
        ws_finish_headers(&req->ws);
        // the last part is always a body
        ANIMPL(!(req->flags & REQ_HAS_MESSAGE));
        SNIMPL(zmq_msg_init(&req->response_msg));
        req->flags |= REQ_HAS_MESSAGE;
        SNIMPL(zmq_msg_move(&req->response_msg, &msg));
        SNIMPL(ws_reply_data(&req->ws, zmq_msg_data(&req->response_msg),
            zmq_msg_size(&req->response_msg)));
        req->flags |= REQ_REPLIED;
        request_finish(req);
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        Z_SEQ_ERROR(msg);
        continue;
    }
}

static int socket_visitor(config_Route_t *route) {
    if(route->zmq_forward.socket.value_len) {
        SNIMPL(zmq_open(&route->zmq_forward.socket, ZMASK_REQ, ZMQ_XREQ,
            http_process, root.loop));
        CONFIG_REQUESTFIELD_LOOP(item, route->zmq_forward.contents) {
            switch(item->value.kind) {
            case CONFIG_Header:
                item->value._field_index = ws_index_header(&root.ws,
                    item->value.value);
                break;
            }
        }
    }
    route->_child_match = NULL;
    if(route->routing.kind && (route->map_len || route->children_len)) {
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

static int socket_unvisitor(config_Route_t *route) {
    if(route->zmq_forward.socket.value_len) {
        SNIMPL(z_close(&route->zmq_forward.socket, root.loop));
    }
    if(route->routing.kind && (route->map_len || route->children_len)) {
        switch(route->routing.kind) {
        case CONFIG_Exact:
            ws_match_free(route->_child_match);
            break;
        case CONFIG_Prefix:
        case CONFIG_Suffix:
            ws_fuzzy_free(route->_child_match);
            break;
        default:
            LNIMPL("Routing tag ", route->routing.kind);
        }
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        SNIMPL(socket_unvisitor(&item->value));
    }
    return 0;
}

static int do_forward(request_t *req) {
    config_Route_t *route = req->route;
    void *sock = route->zmq_forward.socket._sock;
    // Must wake up reading and on each send, because the way zmq sockets work
    ev_feed_event(root.loop, &route->zmq_forward.socket._watch, EV_READ);
    zmq_msg_t msg;
    REQ_INCREF(req);
    SNIMPL(zmq_msg_init_data(&msg, req->uid, UID_LEN, request_decref, req));
    if(zmq_send(sock, &msg, ZMQ_SNDMORE|ZMQ_NOBLOCK)) {
        zmq_msg_close(&msg);
        if(errno == EAGAIN) {
            http_static_response(req, &route->responses.service_unavailable);
            request_finish(req);
            return -1;
        } else {
            SWARN2("Can't forward request");
            http_static_response(req, &route->responses.internal_error);
            request_finish(req);
            return -1;
        }
    }
    if(route->zmq_forward.socket.kind == CONFIG_zmq_Req
        || route->zmq_forward.socket.kind == CONFIG_auto) {
        SNIMPL(zmq_send(sock, &msg, ZMQ_SNDMORE|ZMQ_NOBLOCK)); // empty sentinel
    }
    config_a_RequestField_t *contents = route->zmq_forward.contents;
    ANIMPL(contents);
    for(config_a_RequestField_t *item=contents; item; item = item->head.next) {
        size_t len;
        const char *value = get_field(req, &item->value, &len);
        zmq_msg_t msg;
        REQ_INCREF(req);
        SNIMPL(zmq_msg_init_data(&msg, (void *)value, len, request_decref, req));
        SNIMPL(zmq_send(sock, &msg,
            (item->head.next ? ZMQ_SNDMORE : 0)|ZMQ_NOBLOCK));
        SNIMPL(zmq_msg_close(&msg));
    }
    return 0;
}

static void request_timeout(struct ev_loop *loop, struct ev_timer *tm, int rev){
    ANIMPL(!(rev & EV_ERROR));
    request_t *req = (request_t *)((char *)tm - offsetof(request_t, timeout));
    int mode = req->route->zmq_forward.retry.mode;
    if(++req->retries >= req->route->zmq_forward.retry.count) {
        mode = CONFIG_NoRetry;
    }
    switch(mode) {
        case CONFIG_NoRetry:
            http_static_response(req, &req->route->responses.gateway_timeout);
            request_finish(req);
            return;
        case CONFIG_RetryFirst:
            ANIMPL(req->flags & REQ_IN_SIEVE);
            root.stat.zmq_retries += 1;
            if(!do_forward(req)) {
                ev_timer_again(loop, &req->timeout);
            }
            return;
        case CONFIG_RetryLast:
            ANIMPL(req->flags & REQ_IN_SIEVE);
            sieve_empty(root.request_sieve, UID_HOLE(req->uid));
            make_hole_uid(req, req->uid, root.request_sieve, FALSE);
            root.stat.zmq_retries += 1;
            if(!do_forward(req)) {
                ev_timer_again(loop, &req->timeout);
            }
            return;
        default:
            LNIMPL("Wrong retry mode");
    }
}

int http_headers(request_t *req) {
    req->incoming_time = ev_now(root.loop);
    char *query = strchr(req->ws.uri, '?');
    if(query) {
        int len = query - req->ws.uri;
        req->path = obstack_alloc(&req->ws.pieces, len+1);
        memcpy(req->path, req->ws.uri, len);
        req->path[len] = 0;
    } else {
        req->path = req->ws.uri;
    }
    config_Route_t *route = preliminary_resolve(req);
    if(route && req->ws.bodylen > route->limits.max_body_size) {
        TWARN("Request size too big");
        return -1; // Close connection immediately
    }
    request_init(req);
    return 0;
}

int http_request(request_t *req) {
    root.stat.http_requests += 1;
    config_Route_t *route = resolve_url(req);

    if(!route) { // already replied with error
        request_finish(req);
        return 0;
    }
    req->route = route;

    if(route->websocket.enabled && route->websocket.polling_fallback.enabled
        && strchr(req->ws.uri, '?')) {
        return comet_request(req);
    }
    if(route->zmq_forward.enabled) {
        // Ok, it's zeromq forward
        if(make_hole_uid(req, req->uid, root.request_sieve, FALSE) < 0)
            return -1;
        req->flags |= REQ_IN_SIEVE;
        root.stat.zmq_requests += 1;
        if(!do_forward(req)) {
            if(route->zmq_forward.timeout) {
                ev_timer_init(&req->timeout, request_timeout,
                    route->zmq_forward.timeout, route->zmq_forward.timeout);
                ev_timer_start(root.loop, &req->timeout);
            }
        }
        return 0;
    }
    if(route->static_.enabled) {
        return disk_request(req);
    }
    // Probably it's static response
    http_static_response(req, &route->responses.default_);
    request_finish(req);
    return 0;
}

int prepare_http(config_main_t *config, config_Route_t *root) {
    SNIMPL(socket_visitor(&config->Routing));
    LINFO("Http backend connections complete");
    return 0;
}

int release_http(config_main_t *config, config_Route_t *root) {
    SNIMPL(socket_unvisitor(&config->Routing));
    return 0;
}
