#include "config.h"
#include "automata.h"

typedef void *zmq_context;
typedef void *zmq_socket;

typedef struct serverroot_s {
    char *automata;
    zmq_context zmq;
} serverroot_t;

typedef struct siteroot_s {
    char *automata;
    serverroot_t *root;
} siteroot_t;

typedef struct page_s {
    config_zerogw_Pages_t *config;
    siteroot_t *site;
    serverroot_t *root;
    zmq_socket sock;
} page_t;

serverroot_t root;

int main(int argc, char **argv) {
    prepare_configuration(argc, argv);

    AUTOMATA dom_automata = automata_ascii_new();

    config_zerogw_Pages_t *site = config.Pages;
    while(page) {
        //~ automata_ascii_add_backwards_star(dom_automata, page->
    }
    root->automata = automata_ascii_compile(dom_automata, NULL, NULL);
    automata_ascii_free(dom_automata);
}
