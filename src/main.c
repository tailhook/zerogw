#include <zmq.h>
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <event.h>
#include <evhttp.h>

#include "log.h"
#include "config.h"
#include "automata.h"

typedef void *zmq_context_t;
typedef void *zmq_socket_t;
typedef struct event_base *event_loop_t;
typedef struct evhttp *evhttp_t;
typedef struct evhttp_request *evhttp_request_t;

typedef struct serverroot_s {
    char *automata;
    zmq_context_t zmq;
    event_loop_t event;
    evhttp_t evhttp;
} serverroot_t;

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
} page_t;

serverroot_t root;

void http_request(evhttp_request_t req, void *_) {
    LINFO("REQUEST");
}

int main(int argc, char **argv) {
    prepare_configuration(argc, argv);

    root.event = event_init();
    root.evhttp = evhttp_new(root.event);
    config_zerogw_listen_t *slisten = config.Server.listen;
    while(slisten) {
        evhttp_bind_socket(root.evhttp, slisten->host, slisten->port);
        slisten = (config_zerogw_listen_t *)slisten->head.next;
    }
    evhttp_set_gencb(root.evhttp, http_request, NULL);

    // Probably here is a place to fork! :)

    AUTOMATA dom_automata = automata_ascii_new(TRUE);
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    config_zerogw_Pages_t *sconfig = config.Pages;
    while(sconfig) {
        siteroot_t *site = SAFE_MALLOC(sizeof(siteroot_t));
        site->config = sconfig;
        site->root = &root;
        AUTOMATA pagemach = automata_ascii_new(TRUE);
        config_zerogw_pages_t *pconfig = sconfig->pages;
        while(pconfig) {
            page_t *page = SAFE_MALLOC(sizeof(page_t));
            page->config = pconfig;
            page->site = site;
            page->root = &root;
            page->sock = zmq_socket(root.zmq, ZMQ_XREQ);
            config_zerogw_forward_t *fconfig = pconfig->forward;
            while(fconfig) {
                zmq_connect(page->sock, fconfig->value);
                fconfig = (config_zerogw_forward_t *)fconfig->head.next;
            }
            automata_ascii_add_forwards_star(pagemach, pconfig->uri,
                (size_t)page);
            pconfig = (config_zerogw_pages_t *)pconfig->head.next;
        }
        site->automata = automata_ascii_compile(pagemach, NULL, NULL);
        automata_ascii_free(pagemach);
        automata_ascii_add_backwards_star(dom_automata, sconfig->head.key,
            (size_t)site);
        sconfig = (config_zerogw_Pages_t *)sconfig->head.next;
    }
    root.automata = automata_ascii_compile(dom_automata, NULL, NULL);
    automata_ascii_free(dom_automata);

    event_base_dispatch(root.event);
}
