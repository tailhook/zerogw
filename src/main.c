#include <zmq.h>

#include <ev.h>
#include <website.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "log.h"
#include "config.h"
#include "main.h"
#include "websocket.h"
#include "sieve.h"

#define REQ_DECREF(req) if(!--(req)->refcnt) { ws_request_free(&(req)->ws); }
#define REQ_INCREF(req) (++(req)->refcnt)
#define HARDCODED_SOCKETS 2

typedef struct worker_s {
    pthread_t thread;
    zmq_pollitem_t *poll;
    int nsockets;
    int http_sockets;
    int websock_forward;
    int websock_subscribe;
    zmq_socket_t server_pull;
    zmq_socket_t server_push;
    zmq_socket_t status_sock;
    socket_t server_event;
} worker_t;

typedef struct request_s {
    ws_request_t ws;
    uint64_t index;
    size_t hole;
    socket_t socket;
    int refcnt;
    bool has_message;
    zmq_msg_t response_msg;
} request_t;

serverroot_t root;
worker_t worker;
sieve_t *sieve;
config_main_t config;
char instance_id[INSTANCE_ID_LEN];

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

void request_decref(void *_data, void *request) {
    REQ_DECREF((request_t *)request);
}

config_Route_t *resolve_url(request_t *req) {
    req->refcnt = 1;
    req->has_message = FALSE;
    if(sieve_full(sieve)) {
        LWARN("Too many requests");
        http_static_response(req,
           &config.Globals.responses.service_unavailable);
        return NULL;
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

int start_websocket(request_t *req) {
    config_Route_t *route = resolve_url(req);
    if(!route->websock_subscribe_len || !route->websock_forward_len) {
        return -1;
    }
    return websock_start((connection_t *)req->ws.conn, route);
}

/* server thread callback */
int http_request(request_t *req) {
    config_Route_t *route = resolve_url(req);

    if(!route) { // already replied
        return 0;
    }

    // Let's decide whether it's static
    if(!route->zmq_forward_len) {
        http_static_response(req, &route->responses.default_);
        return 0;
    }
    // Ok, it's zeromq forward
    sieve_find_hole(sieve, req, &req->index, &req->hole);
    req->socket = route->_http_zmq_index;
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_size(&msg, 16));
    LDEBUG("Preparing %d bytes", zmq_msg_size(&msg));
    void *data = zmq_msg_data(&msg);
    ((uint64_t*)data)[0] = req->index;
    ((uint64_t*)data)[1] = req->hole;
    SNIMPL(zmq_send(root.worker_push, &msg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_close(&msg));
    config_a_RequestField_t *contents = route->zmq_contents;
    ANIMPL(contents);
    for(config_a_RequestField_t *item=contents; item; item = item->head.next) {
        size_t len;
        const char *value = get_field(req, &item->value, &len);
        zmq_msg_t msg;
        REQ_INCREF(req);
        SNIMPL(zmq_msg_init_data(&msg, (void *)value, len, request_decref, req));
        SNIMPL(zmq_send(root.worker_push, &msg,
            (item->head.next ? ZMQ_SNDMORE : 0)));
        SNIMPL(zmq_msg_close(&msg));
    }
    return 0;
}

int http_request_finish(request_t *req) {
    if(req->has_message) {
        zmq_msg_close(&req->response_msg);
    }
    REQ_DECREF(req);
    return 1;
}

/* server thread callback */
void process_http(zmq_socket_t sock) {
    zmq_msg_t msg;
    size_t opt;
    size_t len = sizeof(opt);
    SNIMPL(zmq_msg_init(&msg));
    SNIMPL(zmq_recv(sock, &msg, 0));
    SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
    if(!opt) {
        zmq_msg_close(&msg);
        skip_message(sock);
        return;
    }
    uint64_t reqid = ((uint64_t*)zmq_msg_data(&msg))[0];
    uint64_t holeid = ((uint64_t*)zmq_msg_data(&msg))[1];
    request_t *req = sieve_get(sieve, holeid);
    if(!req || req->index != reqid) {
        zmq_msg_close(&msg);
        skip_message(sock);
        return;
    }
    SNIMPL(zmq_recv(sock, &msg, 0));
    SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
    if(!opt) {
        zmq_msg_close(&msg);
        skip_message(sock);
        return;
    }
    ANIMPL(zmq_msg_size(&msg) == 0); // The sentinel of routing data
    SNIMPL(zmq_recv(sock, &msg, 0));
    SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
    if(opt) { //first is a status-line if its not last
        char *data = zmq_msg_data(&msg);
        char *tail;
        int dlen = zmq_msg_size(&msg);
        LDEBUG("Status line: [%d] %s", dlen, data);
        ws_statusline(&req->ws, data);

        SNIMPL(zmq_recv(sock, &msg, 0));
        SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
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
            SNIMPL(zmq_recv(sock, &msg, 0));
            SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
            if(opt) {
                LWARN("Too many message parts");
                http_static_response(req,
                    &config.Routing.responses.internal_error);
                zmq_msg_close(&msg);
                skip_message(root.worker_pull);
                return;
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
    sieve_empty(sieve, req->hole);
    SNIMPL(zmq_msg_close(&msg));
}

/* server thread callback */
void send_message(evloop_t loop, watch_t *watch, int revents) {
    LDEBUG("Got something");
    uint64_t nevents;
    SNIMPL(read(root.worker_event, &nevents, sizeof(nevents)) != 8);
    LDEBUG("Got %d replies, processing...", nevents);
    Z_SEQ_INIT(msg, root.worker_pull);
    for(int i = nevents; i > 0; --i) {
        LDEBUG("Processing. %d to go...", i);
        int statuscode = 200;
        char statusline[32] = "OK";
        Z_RECV_NEXT(msg);
        char *kind = zmq_msg_data(&msg);
        LDEBUG("Got message kind ``%s''", kind);
        if(*kind == 'h') {
            process_http(root.worker_pull);
        } else if(*kind == 'w') {
            // some zeromq intimate message
            websock_process(root.worker_pull);
        } else {
            goto msg_error;
        }
        Z_SEQ_FINISH(msg);
        continue;
        
        msg_error:
        Z_SEQ_ERROR(msg);
        return;
    }
    LDEBUG("Done processing...");
    return;
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
            SNIMPL(zmq_recv(worker.server_pull, &msg, 0));
            SNIMPL(zmq_getsockopt(worker.server_pull, ZMQ_RCVMORE, &opt, &len));
            ANIMPL(opt);
            ANIMPL(zmq_msg_size(&msg) == 16);
            uint64_t reqid = ((uint64_t*)zmq_msg_data(&msg))[0];
            uint64_t holeid = ((uint64_t*)zmq_msg_data(&msg))[1];
            request_t *req = sieve_get(sieve, holeid);
            if(req->index != reqid) {
                req = NULL;
            }
            if(req) {
                int sockn = req->socket;
                ANIMPL(sockn > 1 && sockn < worker.nsockets);
                SNIMPL(zmq_send(worker.poll[sockn].socket, &msg, ZMQ_SNDMORE));
                // empty message, to close routing (message cleared by zmq_send)
                SNIMPL(zmq_send(worker.poll[sockn].socket, &msg, ZMQ_SNDMORE));
                while(opt) {
                    SNIMPL(zmq_recv(worker.server_pull, &msg, 0));
                    SNIMPL(zmq_getsockopt(worker.server_pull,
                        ZMQ_RCVMORE, &opt, &len));
                    SNIMPL(zmq_send(worker.poll[sockn].socket, &msg,
                        opt ? ZMQ_SNDMORE : 0));
                }
            } else {
                // else: request already abandoned, discard whole message
                while(opt) {
                    SNIMPL(zmq_recv(worker.server_pull, &msg, 0));
                    SNIMPL(zmq_getsockopt(worker.server_pull,
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
        for(int i = HARDCODED_SOCKETS; i < worker.nsockets; ++i) {
            if(worker.poll[i].revents & ZMQ_POLLIN) {
                --count;
                zmq_msg_t msg;
                SNIMPL(zmq_msg_init_size(&msg, 1));
                if(i >= HARDCODED_SOCKETS + worker.http_sockets) {
                    *(char *)zmq_msg_data(&msg) = 'w';
                } else {
                    *(char *)zmq_msg_data(&msg) = 'h';
                }
                uint64_t opt = 1;
                size_t len = sizeof(opt);
                zmq_socket_t sock = worker.poll[i].socket;
                SNIMPL(zmq_send(worker.server_push, &msg, ZMQ_SNDMORE));
                while(opt) {
                    SNIMPL(zmq_recv(sock, &msg, 0));
                    SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
                    LDEBUG("Message %d bytes from %d (%d)",
                        zmq_msg_size(&msg), i, opt);
                    SNIMPL(zmq_send(worker.server_push, &msg,
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

static void count_sockets(config_Route_t *route, worker_t *worker) {
    if(route->zmq_forward_len) {
        worker->http_sockets += 1;
    }
    if(route->websock_forward_len) {
        worker->websock_forward += 1;
    }
    if(route->websock_subscribe_len) {
        worker->websock_subscribe += 1;
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        count_sockets(&item->value, worker);
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        count_sockets(&item->value, worker);
    }
}

int zmq_open(zmq_socket_t target, config_zmqaddr_t *addr) {
    if(addr->kind == CONFIG_zmq_Bind) {
        LDEBUG("Binding 0x%x to ``%s''", target, addr->value);
        return zmq_bind(target, addr->value);
    } else if(addr->kind == CONFIG_zmq_Connect) {
        LDEBUG("Connecting 0x%x to ``%s''", target, addr->value);
        return zmq_connect(target, addr->value);
    } else {
        LNIMPL("Unknown socket type %d", addr->kind);
    }
}

static int worker_socket_visitor(config_Route_t *route,
    int *http_index, int *websock_index) {
    if(route->zmq_forward_len) {
        zmq_socket_t sock = zmq_socket(root.zmq, ZMQ_XREQ);
        ANIMPL(sock);
        CONFIG_ZMQADDR_LOOP(item, route->zmq_forward) {
            SNIMPL(zmq_open(sock, &item->value));
        }
        worker.poll[*http_index].socket = sock;
        worker.poll[*http_index].events = ZMQ_POLLIN;
        route->_http_zmq_index = *http_index;
        *http_index += 1;
    }
    if(route->websock_subscribe_len) {
        zmq_socket_t sock = zmq_socket(root.zmq, ZMQ_SUB);
        ANIMPL(sock);
        CONFIG_ZMQADDR_LOOP(item, route->websock_subscribe) {
            SNIMPL(zmq_open(sock, &item->value));
        }
        zmq_setsockopt(sock, ZMQ_SUBSCRIBE, NULL, 0);
        worker.poll[*websock_index].socket = sock;
        worker.poll[*websock_index].events = ZMQ_POLLIN;
        route->_websock_zmq_index = *websock_index;
        *websock_index += 1;
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
                SNIMPL(worker_socket_visitor(&item->value,
                    http_index, websock_index));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                void *val = (void *)ws_match_add(route->_child_match, item->key,
                    (size_t)&item->value);
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(worker_socket_visitor(&item->value,
                    http_index, websock_index));
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
                SNIMPL(worker_socket_visitor(&item->value,
                    http_index, websock_index));
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
                SNIMPL(worker_socket_visitor(&item->value,
                    http_index, websock_index));
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
                SNIMPL(worker_socket_visitor(&item->value,
                    http_index, websock_index));
            }
            CONFIG_STRING_ROUTE_LOOP(item, route->map) {
                char *star = strchr(item->key, '*');
                void *val = (void *)ws_fuzzy_add(route->_child_match,
                    star ? star+1 : item->key, !!star, (size_t)&item->value);
                if(val != &item->value) {
                    LWARN("Conflicting route \"%s\"", item->key);
                }
                SNIMPL(worker_socket_visitor(&item->value,
                    http_index, websock_index));
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

static int server_socket_visitor(config_Route_t *route) {
    if(route->websock_forward_len) {
        zmq_socket_t sock = zmq_socket(root.zmq, ZMQ_PUB);
        ANIMPL(sock);
        LDEBUG("Opening websocket forwarder 0x%x", sock);
        CONFIG_ZMQADDR_LOOP(item, route->websock_forward) {
            SNIMPL(zmq_open(sock, &item->value));
        }
        route->_websock_forward = sock;
    }
    CONFIG_ROUTE_LOOP(item, route->children) {
        server_socket_visitor(&item->value);
    }
    CONFIG_STRING_ROUTE_LOOP(item, route->map) {
        server_socket_visitor(&item->value);
    }
    return 0;
}
/*
Must prepare in a worker, because zmq does not allow to change thread of socket
*/
void prepare_sockets() {

    // Let's count our sockets
    count_sockets(&config.Routing, &worker);
    worker.nsockets = HARDCODED_SOCKETS // server socket and status socket
        + worker.http_sockets
        + worker.websock_subscribe;

    // Ok, now lets fill them
    worker.poll = SAFE_MALLOC(sizeof(zmq_pollitem_t)*worker.nsockets);
    bzero(worker.poll, sizeof(zmq_pollitem_t)*worker.nsockets);

    worker.server_push = zmq_socket(root.zmq, ZMQ_DOWNSTREAM);
    ANIMPL(worker.server_push);
    SNIMPL(zmq_connect(worker.server_push, "inproc://server"));

    worker.server_pull = zmq_socket(root.zmq, ZMQ_UPSTREAM);
    ANIMPL(worker.server_pull);
    SNIMPL(zmq_connect(worker.server_pull, "inproc://worker"));
    worker.poll[0].socket = worker.server_pull;
    worker.poll[0].events = ZMQ_POLLIN;

    LINFO("Binding status socket: %s", config.Server.status_socket.value);
    worker.status_sock = zmq_socket(root.zmq, ZMQ_REP);
    ANIMPL(worker.status_sock);
    SNIMPL(zmq_open(worker.status_sock, &config.Server.status_socket));
    worker.poll[1].socket = worker.status_sock;
    worker.poll[1].events = ZMQ_POLLIN;

    int http_index = HARDCODED_SOCKETS;
    int websock_index = HARDCODED_SOCKETS + worker.http_sockets;

    SNIMPL(worker_socket_visitor(&config.Routing, &http_index, &websock_index));
    ANIMPL(websock_index == worker.nsockets);
    ANIMPL(http_index = worker.http_sockets + HARDCODED_SOCKETS);
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

    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    root.loop = ev_default_loop(0);
    ANIMPL(root.loop);
    ws_server_init(&root.ws, root.loop);
    CONFIG_LISTENADDR_LOOP(slisten, config.Server.listen) {
        if(slisten->value.fd >= 0) {
            LDEBUG("Using socket %d", slisten->value.fd);
            SNIMPL(ws_add_fd(&root.ws, slisten->value.fd));
        } else if(slisten->value.unix_socket && *slisten->value.unix_socket) {
            LDEBUG("Using unix socket \"%s\"", slisten->value.unix_socket);
            SNIMPL(ws_add_unix(&root.ws, slisten->value.unix_socket,
                slisten->value.unix_socket_len));
        } else {
            LDEBUG("Using host %s port %d",
                slisten->value.host, slisten->value.port);
            SNIMPL(ws_add_tcp(&root.ws, inet_addr(slisten->value.host),
                slisten->value.port));
        }
    }
    ws_REQUEST_STRUCT(&root.ws, request_t);
    ws_REQUEST_CB(&root.ws, http_request);
    ws_FINISH_CB(&root.ws, http_request_finish);
    ws_CONNECTION_STRUCT(&root.ws, connection_t);
    ws_WEBSOCKET_CB(&root.ws, start_websocket);
    ws_MESSAGE_CB(&root.ws, websock_message);
    ws_MESSAGE_STRUCT(&root.ws, message_t);

    // Probably here is a place to fork! :)

    int urand = open("/dev/urandom", O_RDONLY);
    ANIMPL(urand);
    SNIMPL(read(urand, instance_id, INSTANCE_ID_LEN) != INSTANCE_ID_LEN);
    SNIMPL(close(urand));

    root.worker_event = eventfd(0, 0);
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    ANIMPL(root.zmq);
    root.worker_pull = zmq_socket(root.zmq, ZMQ_UPSTREAM);
    SNIMPL(root.worker_pull == 0);
    SNIMPL(zmq_bind(root.worker_pull, "inproc://server"));
    root.worker_push = zmq_socket(root.zmq, ZMQ_DOWNSTREAM);
    SNIMPL(root.worker_push == 0);
    SNIMPL(zmq_bind(root.worker_push, "inproc://worker"));
    pthread_create(&worker.thread, NULL, worker_fun, NULL);

    sieve_prepare(&sieve, config.Server.max_requests);
    SNIMPL(server_socket_visitor(&config.Routing));

    int64_t event = 0;
    /* first event means configuration is setup */
    ANIMPL(read(root.worker_event, &event, sizeof(event)) == 8);
    ANIMPL(event == 1);
    ev_io_init(&root.worker_watch, send_message, root.worker_event, EV_READ);
    ev_io_start(root.loop, &root.worker_watch);
    ws_server_start(&root.ws);
    ev_loop(root.loop, 0);
}
