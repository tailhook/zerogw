#include <zmq.h>
#include <ev.h>
#include <website.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"
#include "config.h"

typedef void *zmq_context_t;
typedef void *zmq_socket_t;
typedef struct ev_loop *evloop_t;
typedef struct ev_io watch_t;
typedef int socket_t;

typedef struct serverroot_s {
    ws_server_t ws;
    zmq_context_t zmq;
    zmq_socket_t worker_sock;
    evloop_t loop;
    socket_t worker_event;
    watch_t worker_watch;
} serverroot_t;

typedef struct status_s {
    ev_tstamp time;
    int http_requests;
    int http_responses;
    int http_in_bytes;
    int http_out_bytes;
    int zmq_requests;
    int zmq_responses;
    int zmq_in_bytes;
    int zmq_out_bytes;
} status_t;

typedef struct worker_s {
    pthread_t thread;
    zmq_pollitem_t *poll;
    int nsockets;
    zmq_socket_t server_sock;
    zmq_socket_t status_sock;
    socket_t server_event;
} worker_t;

typedef struct request_s {
    ws_request_t ws;
    uint64_t index;
    int hole;
    socket_t socket;
} request_t;

typedef struct sieve_s {
    uint64_t index;
    size_t max;
    size_t offset;
    size_t in_progress;
    request_t *requests[];
} sieve_t;

serverroot_t root;
worker_t worker;
status_t status;
sieve_t *sieve;
config_main_t config;

void http_static_response(request_t *req, config_StaticResponse_t *resp) {
    char status[resp->status_len + 5];
    sprintf(status, "%03d %s", resp->code, resp->status);
    ws_statusline(&req->ws, status);
    ws_add_header(&req->ws, "Server", config.Server.header);
    CONFIG_STRING_STRING_LOOP(line, resp->headers) {
        ws_add_header(&req->ws, line->key, line->value);
    }
    LDEBUG("Replying with %d bytes", resp->body_len);
    ws_reply_data(&req->ws, resp->body, resp->body_len);
}

size_t find_hole() {
    request_t **cur = sieve->requests + sieve->offset;
    request_t **end = sieve->requests + sieve->max;
    for(request_t **i = cur; i != end; ++i) {
        if(!*i) {
            sieve->offset = i+1 - sieve->requests;
            return i - sieve->requests;
        }
    }
    for(request_t **i = sieve->requests; i != cur; ++i) {
        if(!*i) {
            sieve->offset = i+1 - sieve->requests;
            return i - sieve->requests;
        }
    }
    LNIMPL("Not reachable code");
}

void prepare_sieve() {
    int ssize = sizeof(sieve_t) + sizeof(request_t*)*config.Server.max_requests;
    sieve = SAFE_MALLOC(ssize);
    bzero(sieve, ssize);
    sieve->max = config.Server.max_requests;
}

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

/* server thread callback */
void http_request(request_t *req) {
    if(sieve->in_progress >= sieve->max) {
        LWARN("Too many requests");
        http_static_response(req,
           &config.Globals.responses.service_unavailable);
        return;
    }

    config_Route_t *route = &config.Routing;
    while(route->routing.kind != CONFIG_Leaf) {
        const char *data = get_field(req, &route->routing_by, NULL);
        LDEBUG("Matching ``%s'' by %d", data, route->routing.kind);
        size_t tmp;
        switch(route->routing.kind) {
        case CONFIG_Exact:
            if(ws_match(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                http_static_response(req, &route->responses.not_found);
                return;
            }
            continue;
        case CONFIG_Prefix:
            if(ws_fuzzy(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                http_static_response(req, &route->responses.not_found);
                return;
            }
            continue;
        case CONFIG_Suffix:
            if(ws_rfuzzy(route->_child_match, data, &tmp)) {
                route = (config_Route_t *)tmp;
            } else {
                http_static_response(req, &route->responses.not_found);
                return;
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

    // Let's decide whether it's static
    if(!route->zmq_forward_len) {
        http_static_response(req, &route->responses.default_);
        return;
    }
    // Ok, it's zeromq forward
    req->index = sieve->index;
    req->hole = find_hole();
    req->socket = route->_socket_index;
    sieve->requests[req->hole] = req;
    ++sieve->index;
    ++sieve->in_progress;
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_size(&msg, 16));
    LDEBUG("Preparing %d bytes", zmq_msg_size(&msg));
    void *data = zmq_msg_data(&msg);
    ((uint64_t*)data)[0] = req->index;
    ((uint64_t*)data)[1] = req->hole;
    SNIMPL(zmq_send(root.worker_sock, &msg, ZMQ_SNDMORE));
    // empty message, the sentinel for routing data (cleared in zmq_send)
    SNIMPL(zmq_send(root.worker_sock, &msg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_close(&msg));
    config_a_RequestField_t *contents = route->zmq_contents;
    ANIMPL(contents);
    for(config_a_RequestField_t *item=contents; item; item = item->head.next) {
        size_t len;
        const char *value = get_field(req, &item->value, &len);
        zmq_msg_t msg;
        SNIMPL(zmq_msg_init_data(&msg, (void *)value, len, NULL, NULL));
        SNIMPL(zmq_send(root.worker_sock, &msg,
            (item->head.next ? ZMQ_SNDMORE : 0)));
        SNIMPL(zmq_msg_close(&msg));
    }
}

/* server thread callback */
void send_message(evloop_t loop, watch_t *watch, int revents) {
    LDEBUG("Got something");
    uint64_t opt;
    size_t len = sizeof(opt);
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init(&msg));
    SNIMPL(read(root.worker_event, &opt, len) != 8);
    LDEBUG("Got %d replies, processing...", opt);
    for(int i = opt; i > 0; --i) {
        LDEBUG("Processing. %d to go...", i);
        int statuscode = 200;
        char statusline[32] = "OK";
        SNIMPL(zmq_recv(root.worker_sock, &msg, 0));
        SNIMPL(zmq_getsockopt(root.worker_sock, ZMQ_RCVMORE, &opt, &len));
        ANIMPL(opt);
        ANIMPL(zmq_msg_size(&msg) == 16);
        uint64_t reqid = ((uint64_t*)zmq_msg_data(&msg))[0];
        uint64_t holeid = ((uint64_t*)zmq_msg_data(&msg))[1];
        ANIMPL(holeid < sieve->max);
        request_t *req = sieve->requests[holeid];
        if(req && req->index == reqid) {
            SNIMPL(zmq_recv(root.worker_sock, &msg, 0));
            SNIMPL(zmq_getsockopt(root.worker_sock, ZMQ_RCVMORE, &opt, &len));
            if(!opt) goto skipmessage;
            ANIMPL(zmq_msg_size(&msg) == 0); // The sentinel of routing data
            SNIMPL(zmq_recv(root.worker_sock, &msg, 0));
            SNIMPL(zmq_getsockopt(root.worker_sock,
                ZMQ_RCVMORE, &opt, &len));
            if(opt) { //first is a status-line if its not last
                char *data = zmq_msg_data(&msg);
                char *tail;
                int dlen = zmq_msg_size(&msg);
                LDEBUG("Status line: [%d] %s", dlen, data);
                ws_statusline(&req->ws, data);

                SNIMPL(zmq_recv(root.worker_sock, &msg, 0));
                SNIMPL(zmq_getsockopt(root.worker_sock,
                    ZMQ_RCVMORE, &opt, &len));
                if(opt) { //second is headers if its not last
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
                    SNIMPL(zmq_recv(root.worker_sock, &msg, 0));
                    SNIMPL(zmq_getsockopt(root.worker_sock,
                        ZMQ_RCVMORE, &opt, &len));
                    if(opt) {
                        LWARN("Too many message parts");
                        http_static_response(req,
                            &config.Routing.responses.internal_error);
                        goto skipmessage;
                    }
                }
            }
            // the last part is always a body
            ws_reply_data(&req->ws, zmq_msg_data(&msg), zmq_msg_size(&msg));
            -- sieve->in_progress;
            sieve->requests[req->hole] = NULL;
        } else {
            // else: request already abandoned, discard whole message
            skipmessage:
            while(opt) {
                SNIMPL(zmq_recv(root.worker_sock, &msg, 0));
                LDEBUG("Skipped garbage: [%d] %s", zmq_msg_size(&msg), zmq_msg_data(&msg));
                SNIMPL(zmq_getsockopt(root.worker_sock,
                    ZMQ_RCVMORE, &opt, &len));
            }
        }
    }
    SNIMPL(zmq_msg_close(&msg));
    LDEBUG("Done processing...", opt);
}

/* worker thread callback */
void worker_loop() {
    while(TRUE) {
        LDEBUG("Entering poll with %d sockets", worker.nsockets);
        int count = zmq_poll(worker.poll, worker.nsockets, -1);
        SNIMPL(count < 0);
        LDEBUG("Poll returned %d events", count);
        if(!count) continue;
        if(worker.poll[0].revents & ZMQ_POLLIN) { /* got new request */
            --count;
            zmq_msg_t msg;
            uint64_t opt;
            size_t len = sizeof(opt);
            SNIMPL(zmq_msg_init(&msg));
            SNIMPL(zmq_recv(worker.server_sock, &msg, 0));
            SNIMPL(zmq_getsockopt(worker.server_sock, ZMQ_RCVMORE, &opt, &len));
            ANIMPL(opt);
            // Need to discard empty message at start, it's some zmq magick
            ANIMPL(zmq_msg_size(&msg) == 0);
            SNIMPL(zmq_recv(worker.server_sock, &msg, 0));
            SNIMPL(zmq_getsockopt(worker.server_sock, ZMQ_RCVMORE, &opt, &len));
            ANIMPL(opt);
            ANIMPL(zmq_msg_size(&msg) == 16);
            uint64_t reqid = ((uint64_t*)zmq_msg_data(&msg))[0];
            uint64_t holeid = ((uint64_t*)zmq_msg_data(&msg))[1];
            ANIMPL(holeid < sieve->max);
            request_t *req = sieve->requests[holeid];
            if(req && req->index == reqid) {
                int sockn = req->socket;
                ANIMPL(sockn > 1 && sockn < worker.nsockets);
                SNIMPL(zmq_send(worker.poll[sockn].socket, &msg, ZMQ_SNDMORE));
                while(opt) {
                    SNIMPL(zmq_recv(worker.server_sock, &msg, 0));
                    SNIMPL(zmq_getsockopt(worker.server_sock,
                        ZMQ_RCVMORE, &opt, &len));
                    SNIMPL(zmq_send(worker.poll[sockn].socket, &msg,
                        opt ? ZMQ_SNDMORE : 0));
                }
            } else {
                // else: request already abandoned, discard whole message
                while(opt) {
                    SNIMPL(zmq_recv(worker.server_sock, &msg, 0));
                    SNIMPL(zmq_getsockopt(worker.server_sock,
                        ZMQ_RCVMORE, &opt, &len));
                }
            }
            SNIMPL(zmq_msg_close(&msg));
        }
        if(!count) continue;
        if(worker.poll[1].revents & ZMQ_POLLIN) { /* got status request */
            --count;
            LNIMPL("Status request");
        }
        if(!count) break;
        for(int i = 2; i < worker.nsockets; ++i) {
            if(worker.poll[i].revents & ZMQ_POLLIN) {
                --count;
                zmq_msg_t msg;
                uint64_t opt = 1;
                size_t len = sizeof(opt);
                zmq_socket_t sock = worker.poll[i].socket;
                zmq_msg_init(&msg);
                // Need to send empty message at start, it's some zmq magick
                zmq_send(worker.server_sock, &msg, ZMQ_SNDMORE);
                while(opt) {
                    SNIMPL(zmq_recv(sock, &msg, 0));
                    SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
                    LDEBUG("Message %d bytes from %d (%d)", zmq_msg_size(&msg), i, opt);
                    SNIMPL(zmq_send(worker.server_sock, &msg,
                        opt ? ZMQ_SNDMORE : 0));
                }
                SNIMPL(zmq_msg_close(&msg));
                LDEBUG("OK. Finished, now signalling");
                opt = 1;
                write(worker.server_event, &opt, sizeof(opt));
                if(!count) break;
            }
        }
    }
}

static size_t count_sockets(config_Route_t *route) {
    size_t res = route->zmq_forward_len ? 1 : 0;
    CONFIG_ROUTE_LOOP(item, route->children) {
        res += count_sockets(&item->value);
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        res += count_sockets(&item->value);
    }
    return res;
}

int zmq_open(zmq_socket_t target, config_zmqaddr_t *addr) {
    if(addr->kind == CONFIG_zmq_Bind) {
        return zmq_bind(target, addr->value);
    } else if(addr->kind == CONFIG_zmq_Connect) {
        return zmq_connect(target, addr->value);
    } else {
        LNIMPL("Uknown socket type %d", addr->kind);
    }
}

static int socket_visitor(config_Route_t *route, int *sock_index) {
    if(route->zmq_forward_len) {
        zmq_socket_t sock = zmq_socket(root.zmq, ZMQ_XREQ);
        ANIMPL(sock);
        CONFIG_ZMQADDR_LOOP(item, route->zmq_forward) {
            SNIMPL(zmq_open(sock, &item->value));
        }
        worker.poll[*sock_index].socket = sock;
        worker.poll[*sock_index].events = ZMQ_POLLIN;
        route->_socket_index = *sock_index;
        *sock_index += 1;
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
                SNIMPL(socket_visitor(&item->value, sock_index));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                void *val = (void *)ws_match_add(route->_child_match, item->key,
                    (size_t)&item->value);
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(socket_visitor(&item->value, sock_index));
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
                SNIMPL(socket_visitor(&item->value, sock_index));
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
                SNIMPL(socket_visitor(&item->value, sock_index));
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
                SNIMPL(socket_visitor(&item->value, sock_index));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                char *star = strchr(item->key, '*');
                void *val = (void *)ws_fuzzy_add(route->_child_match,
                    star ? star+1 : item->key, !!star, (size_t)&item->value);
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(socket_visitor(&item->value, sock_index));
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

/*
Must prepare in a worker, because zmq does not allow to change thread of socket
*/
void prepare_sockets() {

    // Let's count our sockets
    worker.nsockets = count_sockets(&config.Routing);
    worker.nsockets += 2; // server socket and status socket

    // Ok, now lets fill them
    worker.poll = SAFE_MALLOC(sizeof(zmq_pollitem_t)*worker.nsockets);
    bzero(worker.poll, sizeof(zmq_pollitem_t)*worker.nsockets);

    worker.server_sock = zmq_socket(root.zmq, ZMQ_XREP);
    ANIMPL(worker.server_sock);
    SNIMPL(zmq_connect(worker.server_sock, "inproc://worker"));
    worker.poll[0].socket = worker.server_sock;
    worker.poll[0].events = ZMQ_POLLIN;

    LINFO("Binding status socket: %s", config.Server.status_socket.value);
    worker.status_sock = zmq_socket(root.zmq, ZMQ_XREP);
    ANIMPL(worker.status_sock);
    SNIMPL(zmq_open(worker.status_sock, &config.Server.status_socket));
    worker.poll[1].socket = worker.status_sock;
    worker.poll[1].events = ZMQ_POLLIN;
    int sock_index = 2;

    SNIMPL(socket_visitor(&config.Routing, &sock_index));
    ANIMPL(sock_index == worker.nsockets);
    LINFO("All connections complete");

    int64_t report = 1;
    worker.server_event = root.worker_event;
    ANIMPL(write(worker.server_event, &report, sizeof(report)) == 8);
}

void *worker_fun(void *_) {
    prepare_sockets();
    worker_loop();
}

int main(int argc, char **argv) {
    config_load(&config, argc, argv);
    loglevel = config.Globals.logging.level;

    root.loop = ev_default_loop(0);
    ANIMPL(root.loop);
    ws_server_init(&root.ws, root.loop);
    CONFIG_LISTENADDR_LOOP(slisten, config.Server.listen) {
        if(slisten->value.fd > 0) {
            LDEBUG("Using socket %d", slisten->value.fd);
            ws_add_fd(&root.ws, slisten->value.fd);
        } else if(slisten->value.unix_socket && *slisten->value.unix_socket) {
            LDEBUG("Using unix socket \"%s\"", slisten->value.unix_socket);
            ws_add_unix(&root.ws, slisten->value.unix_socket,
                slisten->value.unix_socket_len);
        } else {
            LDEBUG("Using host %s port %d",
                slisten->value.host, slisten->value.port);
            ws_add_tcp(&root.ws, inet_addr(slisten->value.host),
                slisten->value.port);
        }
    }
    ws_REQUEST_STRUCT(&root.ws, request_t);
    ws_REQUEST_CB(&root.ws, http_request);

    // Probably here is a place to fork! :)

    root.worker_event = eventfd(0, 0);
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    ANIMPL(root.zmq);
    root.worker_sock = zmq_socket(root.zmq, ZMQ_XREQ);
    SNIMPL(root.worker_sock == 0);
    SNIMPL(zmq_bind(root.worker_sock, "inproc://worker"));
    pthread_create(&worker.thread, NULL, worker_fun, NULL);

    prepare_sieve();

    int64_t event = 0;
    /* first event means configuration is setup */
    ANIMPL(read(root.worker_event, &event, sizeof(event)) == 8);
    ANIMPL(event == 1);
    ev_io_init(&root.worker_watch, send_message, root.worker_event, EV_READ);
    ev_io_start(root.loop, &root.worker_watch);
    ws_server_start(&root.ws);
    ev_loop(root.loop, 0);
}
