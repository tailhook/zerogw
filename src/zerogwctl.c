#include <getopt.h>
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"

typedef struct zerogwctl_flags_s {
    char *config;
    char *socket;
} zerogwctl_flags_t;

void print_usage(FILE *out) {
    fprintf(out, "Usage:\n");
    fprintf(out, "    zerogwctl [options] command argument\n");
    fprintf(out, "\n");
    fprintf(out, "Description:\n");
    fprintf(out, "    An utility to control zerogw behavior\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -c,--config FILE  Configuration file name\n");
    fprintf(out, "  -s,--socket FILE  Overrides socket file name\n");
    fprintf(out, "\n");
    fprintf(out, "Commands:\n");
    fprintf(out, "  list_commands     Query command list from zerogw\n");
    fprintf(out, "  get_statictics    Gets zerogw statistics\n");
    fprintf(out, "  pause_websockets  Pauses forwarding messages from\n");
    fprintf(out, "                    websockets to backends (useful to\n");
    fprintf(out, "                    restart backend)\n");
    fprintf(out, "  resume_websockets Pauses forwarding messages from\n");
    fprintf(out, "                    websockets to backends (useful to\n");
    fprintf(out, "                    restart backend)\n");
    fprintf(out, "  sync_now          Synchonize connected users now");
    fprintf(out, "  reopen_logs       Reopens log files");
    fprintf(out, "\n");
}

void parse_arguments(zerogwctl_flags_t *flags, int argc, char **argv) {
    int opt;
    while((opt = getopt(argc, argv, "hc:s:")) != -1) {
        switch(opt) {
        case 'c':
            flags->config = optarg;
            break;
        case 's':
            flags->socket = optarg;
            break;
        case 'h':
            print_usage(stdout);
            exit(0);
        default:
            print_usage(stderr);
            exit(1);
        }
    }
}


int main(int argc, char **argv) {
    zerogwctl_flags_t flags = {NULL,NULL};
    config_main_t config;
    char *sockaddr;
    char *fakeargs[] = {"zerogwctl", NULL};
    int rc;

    parse_arguments(&flags, argc, argv);
    coyaml_context_t *ctx = config_context(NULL, &config);
    if(flags.config) {
        ctx->root_filename = flags.config;
    }
    assert(ctx);
    assert(coyaml_readfile(ctx) == 0);
    coyaml_context_free(ctx);

    void *zmq = zmq_init(1);
    assert(zmq);
    void *socket = zmq_socket(zmq, ZMQ_REQ);
    if(flags.socket) {
        zmq_connect(socket, flags.socket);
    } else {
        CONFIG_ZMQADDR_LOOP(line, config.Server.control.socket.value) {
            if(line->value.kind == CONFIG_zmq_Connect) {
                zmq_bind(socket, line->value.value);  // We are other party
            } else {
                zmq_connect(socket, line->value.value);
            }
        }
    }
    for(int i = optind; i < argc; ++i) {
        zmq_msg_t msg;
        rc = zmq_msg_init_data(&msg, argv[i], strlen(argv[i]), NULL, NULL);
        assert(rc == 0);
        rc = zmq_send(socket, &msg, (i == argc-1 ? 0: ZMQ_SNDMORE));
        assert(rc == 0);
    }
    while(TRUE) {
        zmq_msg_t msg;
        long opt;
        size_t size = sizeof(opt);
        zmq_msg_init(&msg);
        int rc = zmq_recv(socket, &msg, 0);
        assert(rc == 0);
        rc = zmq_getsockopt(socket, ZMQ_RCVMORE, &opt, &size);
        assert(size == 8);
        assert(rc == 0);
        printf("%.*s\n", (int)zmq_msg_size(&msg), (char *)zmq_msg_data(&msg));
        rc = zmq_msg_close(&msg);
        assert(rc == 0);
        if(!opt) break;
    }
    zmq_close(socket);
    zmq_term(zmq);
    config_free(&config);
}
