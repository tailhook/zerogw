#include <zmq.h>
typedef unsigned char u_char; // for libevent
typedef unsigned short u_short; // for libevent
#include <event.h>
#include <evhttp.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "log.h"
#include "config.h"
#include "automata.h"

typedef void *zmq_context_t;
typedef void *zmq_socket_t;
typedef struct event *watch_t;
typedef struct event_base *event_loop_t;
typedef struct evhttp *evhttp_t;
typedef struct evhttp_request *evhttp_request_t;
typedef struct evbuffer *evbuffer_t;
typedef int socket_t;
typedef struct timeval timestr_t;
typedef struct tm timeparts_t;
typedef double tstamp_t;

typedef struct error_report_s {
    char * status; size_t status_len;
    char * body; size_t body_len;
    int code;
} error_report_t;

typedef struct serverroot_s {
    char *automata;
    zmq_context_t zmq;
    zmq_socket_t worker_sock;
    socket_t worker_event;
    watch_t event_watch;
    event_loop_t event;
    evhttp_t evhttp;
} serverroot_t;

typedef struct status_s {
    timestr_t time;
    int http_requests;
    int http_responses;
    int http_in_bytes;
    int http_out_bytes;
    int zmq_requests;
    int zmq_responses;
    int zmq_in_bytes;
    int zmq_out_bytes;
} status_t;

typedef struct snapshoter_s {
    int timerfd;
    status_t *snapshots;
    int nsnapshots;
    int cur_snapshot;
} snapshoter_t;

typedef struct worker_s {
    pthread_t thread;
    zmq_pollitem_t *poll;
    int nsockets;
    zmq_socket_t server_sock;
    zmq_socket_t status_sock;
    socket_t server_event;
} worker_t;

typedef struct siteroot_s {
    config_zerogw_Pages_t *config;
    char *automata;
    serverroot_t *root;
} siteroot_t;

typedef struct page_s {
    config_zerogw_pages_t *config;
    siteroot_t *site;
    serverroot_t *root;
    zmq_socket_t sock;
    int socket_index;
} page_t;

typedef struct request_s {
    uint64_t index;
    int hole;
    int socket;
    evhttp_request_t evreq;
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
status_t *status_snapshots;
snapshoter_t *snapshoters;
sieve_t *sieve;

void http_error_response(evhttp_request_t req, error_report_t *resp) {
    evbuffer_t buf = evbuffer_new();
    evbuffer_add(buf, resp->body, resp->body_len);
    evhttp_send_reply(req, resp->code, resp->status, buf);
    evbuffer_free(buf);
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

const char *decode_method(int type)
{
    switch (type) {
        case EVHTTP_REQ_GET: return "GET";
        case EVHTTP_REQ_POST: return "POST";
        case EVHTTP_REQ_HEAD: return "HEAD";
        default: LNIMPL("Request type %d", type);
    }
}
/* server thread callback */
void http_request(evhttp_request_t req, void *_) {
    if(sieve->in_progress >= sieve->max) {
        LWARN("Too many requests");
        http_error_response(req,
            (error_report_t *)&config.Globals.responses.service_unavailable);
        return;
    }
    //TODO: check allowed method
    char *host = (char *)evhttp_find_header(req->input_headers, "Host");
    if(!host || !*host) {
        host = config.Server.default_host;
    }
    LDEBUG("Input request, host: \"%s\", uri: \"%s\"",
        host, req->uri);
    siteroot_t *site = (siteroot_t*)automata_ascii_backwards_select(
        root.automata, host);
    if(!site) {
        LDEBUG("Domain not matched");
        http_error_response(req,
            (error_report_t *)&config.Globals.responses.domain_not_found);
        return;
    }
    page_t *page = (page_t*)automata_ascii_forwards_select(
        site->automata, req->uri);
    if(!page) {
        LDEBUG("URI not matched");
        if(site->config->responses.domain_not_found.code) {
            http_error_response(req,
                (error_report_t *)&site->config->responses.domain_not_found);
        } else {
            http_error_response(req,
                (error_report_t *)&config.Globals.responses.domain_not_found);
        }
        return;
    }
    // Let's populate headers
    CARRAY_LOOP(config_zerogw_headers_t *, header, config.Globals.headers) {
        evhttp_add_header(req->output_headers, header->head.key,
            header->value);
    }
    CARRAY_LOOP(config_zerogw_headers_t *, header, site->config->headers) {
        evhttp_add_header(req->output_headers, header->head.key,
            header->value);
    }
    CARRAY_LOOP(config_zerogw_headers_t *, header, page->config->headers) {
        evhttp_add_header(req->output_headers, header->head.key,
            header->value);
    }
    // Let's decide whether it's static
    if(page->config->static_string) {
        evbuffer_t buf = evbuffer_new();
        evbuffer_add(buf, page->config->static_string,
            page->config->static_string_len);
        evhttp_send_reply(req, 200, "OK", buf);
        evbuffer_free(buf);
        return;
    }
    // Ok, it's zeromq forward
    request_t *zreq = SAFE_MALLOC(sizeof(request_t));
    zreq->index = sieve->index;
    zreq->hole = find_hole();
    zreq->evreq = req;
    zreq->socket = page->socket_index;
    sieve->requests[zreq->hole] = zreq;
    ++sieve->index;
    ++sieve->in_progress;
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_size(&msg, 16));
    LDEBUG("Preparing %d bytes", zmq_msg_size(&msg));
    void *data = zmq_msg_data(&msg);
    ((uint64_t*)data)[0] = zreq->index;
    ((uint64_t*)data)[1] = zreq->hole;
    SNIMPL(zmq_send(root.worker_sock, &msg, ZMQ_SNDMORE));
    // empty message, the sentinel for routing data (cleared in zmq_send)
    SNIMPL(zmq_send(root.worker_sock, &msg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_close(&msg));
    config_zerogw_contents_t *contents = page->config->contents;
    if(!contents) contents = site->config->contents;
    if(!contents) contents = config.Globals.contents;
    ANIMPL(contents);
    CARRAY_LOOP(config_zerogw_contents_t *, item, contents) {
        zmq_msg_t msg;
        if(!strcmp(item->value, "PostBody")) {
            SNIMPL(zmq_msg_init_data(&msg, (void *)req->input_buffer->buffer,
                req->input_buffer->off, NULL, NULL));
        } else if(!strncmp(item->value, "Header-", 7)) {
            const char *val = evhttp_find_header(req->input_headers,
                item->value + 7 /*strlen("Header-")*/);
            if(!val) {
                val = ""; // Empty message
            }
            int len = strlen(val);
            SNIMPL(zmq_msg_init_data(&msg, (void *)val, len, NULL, NULL));
        } else if(!strcmp(item->value, "URI")) {
            const char *val = req->uri;
            int len = strlen(val);
            SNIMPL(zmq_msg_init_data(&msg, (void *)val, len, NULL, NULL));
        } else if(!strcmp(item->value, "Method")) {
            const char *val = decode_method(req->type);
            int len = strlen(val);
            SNIMPL(zmq_msg_init_data(&msg, (void *)val, len, NULL, NULL));
        } else if(!strncmp(item->value, "Cookie-", 7)) {
            //TODO
            LNIMPL("Not implemented message field \"%s\"", item->value);
        } else {
            LNIMPL("Unknown message field \"%s\"", item->value);
        }
        SNIMPL(zmq_send(root.worker_sock, &msg,
            (item->head.next ? ZMQ_SNDMORE : 0)));
        SNIMPL(zmq_msg_close(&msg));
    }
}

/* server thread callback */
void send_message(int socket, short events, void *_) {
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
                int statuscode = strtol(data, &tail, 10);
                if(statuscode > 999 || statuscode < 100) {
                    LWARN("Wrong status returned %d", statuscode);
                    goto skipmessage;
                }
                strncpy(statusline, tail, 31);
                statusline[31] = 0;
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
                                evhttp_add_header(req->evreq->output_headers,
                                    name, value);
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
                        http_error_response(req->evreq, (error_report_t*)
                            &config.Globals.responses.internal_error);
                        goto skipmessage;
                    }
                }
            }
            // the last part is always a body
            evbuffer_t buf = evbuffer_new();
            evbuffer_add(buf, zmq_msg_data(&msg), zmq_msg_size(&msg));
            evhttp_send_reply(req->evreq, statuscode, statusline, buf);
            evbuffer_free(buf);
            -- sieve->in_progress;
            sieve->requests[req->hole] = NULL;
            free(req);
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
    SNIMPL(event_add(root.event_watch, NULL));
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
                ANIMPL(sockn > 0 && sockn < worker.nsockets);
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

/*
Must prepare in a worker, because zmq does not allow to change thread of socket
*/
void prepare_sockets() {

    AUTOMATA dom_automata = automata_ascii_new(TRUE);
    ANIMPL(dom_automata);

    // Let's count our sockets
    worker.nsockets = 2; // status and server
    config_zerogw_Pages_t *sconfig = config.Pages;
    while(sconfig) {
        siteroot_t *site = SAFE_MALLOC(sizeof(siteroot_t));
        config_zerogw_pages_t *pconfig = sconfig->pages;
        while(pconfig) {
            pconfig = (config_zerogw_pages_t *)pconfig->head.next;
            ++ worker.nsockets;
        }
        sconfig = (config_zerogw_Pages_t *)sconfig->head.next;
    }

    // Ok, now lets fill them
    worker.poll = SAFE_MALLOC(sizeof(zmq_pollitem_t)*worker.nsockets);
    bzero(worker.poll, sizeof(zmq_pollitem_t)*worker.nsockets);

    worker.server_sock = zmq_socket(root.zmq, ZMQ_XREP);
    ANIMPL(worker.server_sock);
    SNIMPL(zmq_connect(worker.server_sock, "inproc://worker"));
    worker.poll[0].socket = worker.server_sock;
    worker.poll[0].events = ZMQ_POLLIN;

    LINFO("Binding status socket: %s", config.Server.status_socket);
    worker.status_sock = zmq_socket(root.zmq, ZMQ_XREP);
    ANIMPL(worker.status_sock);
    SNIMPL(zmq_bind(worker.status_sock, config.Server.status_socket));
    worker.poll[1].socket = worker.status_sock;
    worker.poll[1].events = ZMQ_POLLIN;

    int sock_index = 2;
    sconfig = config.Pages;
    while(sconfig) {
        siteroot_t *site = SAFE_MALLOC(sizeof(siteroot_t));
        site->config = sconfig;
        site->root = &root;
        AUTOMATA pagemach = automata_ascii_new(TRUE);
        ANIMPL(pagemach);
        config_zerogw_pages_t *pconfig = sconfig->pages;
        while(pconfig) {
            page_t *page = SAFE_MALLOC(sizeof(page_t));
            page->config = pconfig;
            page->site = site;
            page->root = &root;
            page->sock = zmq_socket(root.zmq, ZMQ_XREQ);
            ANIMPL(page->sock);
            config_zerogw_forward_t *fconfig = pconfig->forward;
            while(fconfig) {
                LINFO("Connecting to: %s", fconfig->value);
                SNIMPL(zmq_connect(page->sock, fconfig->value));
                fconfig = (config_zerogw_forward_t *)fconfig->head.next;
            }
            LDEBUG("Adding page \"%s\" -> %x", pconfig->uri, (size_t)page);
            automata_ascii_add_forwards_star(pagemach, pconfig->uri,
                (size_t)page);
            worker.poll[sock_index].socket = page->sock;
            worker.poll[sock_index].events = ZMQ_POLLIN;
            page->socket_index = sock_index;
            ++ sock_index;
            pconfig = (config_zerogw_pages_t *)pconfig->head.next;
        }
        site->automata = automata_ascii_compile(pagemach, NULL, NULL);
        automata_ascii_free(pagemach);
        LDEBUG("Adding domain \"%s\" -> %x", sconfig->head.key, (size_t)site);
        automata_ascii_add_backwards_star(dom_automata, sconfig->head.key,
            (size_t)site);
        sconfig = (config_zerogw_Pages_t *)sconfig->head.next;
    }
    root.automata = automata_ascii_compile(dom_automata, NULL, NULL);
    automata_ascii_free(dom_automata);
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

void prepare_status_snapshots() {
    int all_snapshots = 0;
    CARRAY_LOOP(config_zerogw_statuses_t *, status, config.Server.statuses) {
        all_snapshots += status->snapshots;
    }
    status_snapshots = SAFE_MALLOC(all_snapshots*sizeof(status_t));

    int cur_snapshot = 0;
    CARRAY_LOOP(config_zerogw_statuses_t *, status, config.Server.statuses) {
        //~ int fd = timerfd(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
        // TODO
    }
}

int main(int argc, char **argv) {
    prepare_configuration(argc, argv);

    root.event = event_init();
    ANIMPL(root.event);
    root.evhttp = evhttp_new(root.event);
    ANIMPL(root.evhttp);
    CARRAY_LOOP(config_zerogw_listen_t *, slisten, config.Server.listen) {
        if(slisten->fd > 0) {
            LDEBUG("Using socket %d", slisten->fd);
            evhttp_accept_socket(root.evhttp, slisten->fd);
        } else {
            LDEBUG("Using host %s port %d", slisten->host, slisten->port);
            evhttp_bind_socket(root.evhttp, slisten->host, slisten->port);
        }
    }
    evhttp_set_gencb(root.evhttp, http_request, NULL);

    // Probably here is a place to fork! :)

    root.worker_event = eventfd(0, 0);
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    ANIMPL(root.zmq);
    root.worker_sock = zmq_socket(root.zmq, ZMQ_XREQ);
    SNIMPL(root.worker_sock == 0);
    SNIMPL(zmq_bind(root.worker_sock, "inproc://worker"));
    pthread_create(&worker.thread, NULL, worker_fun, NULL);

    prepare_status_snapshots();
    prepare_sieve();

    int64_t event = 0;
    /* first event means configuration is setup */
    ANIMPL(read(root.worker_event, &event, sizeof(event)) == 8);
    ANIMPL(event == 1);

    root.event_watch = SAFE_MALLOC(sizeof(*root.event_watch));
    ANIMPL(root.event_watch);
    event_set(root.event_watch, root.worker_event, EV_READ, send_message, NULL);
    SNIMPL(event_add(root.event_watch, NULL));
    event_base_dispatch(root.event);
}
