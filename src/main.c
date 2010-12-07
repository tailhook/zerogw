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
#include <sys/time.h>

#include "log.h"
#include "config.h"
#include "main.h"
#include "websocket.h"
#include "sieve.h"
#include "resolve.h"
#include "http.h"

serverroot_t root;

void init_statistics() {
    // quick and dirty
    memset(&root.stat, 0, sizeof(root.stat));
}

void flush_statistics(struct ev_loop *loop, struct ev_timer *watch, int rev) {
    ANIMPL(!(rev & EV_ERROR));
    config_main_t *config = (config_main_t *)watch->data;
    char buf[1024];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    size_t len = snprintf(buf, sizeof(buf),
        "%lu.%06d\n"
        "http_requests: %lu\n"
        "http_replies: %lu\n"
        "zmq_requests: %lu\n"
        "zmq_replies: %lu\n"
        "websock_connects: %lu\n"
        "websock_disconnects: %lu\n"
        "topics_created: %lu\n"
        "topics_removed: %lu\n"
        "websock_subscribed: %lu\n"
        "websock_unsubscribed: %lu\n"
        "websock_published: %lu\n"
        "websock_sent: %lu\n"
        ,
        tv.tv_sec, tv.tv_usec,
        root.stat.http_requests,
        root.stat.http_replies,
        root.stat.zmq_requests,
        root.stat.zmq_replies,
        root.stat.websock_connects,
        root.stat.websock_disconnects,
        root.stat.topics_created,
        root.stat.topics_removed,
        root.stat.websock_subscribed,
        root.stat.websock_unsubscribed,
        root.stat.websock_published,
        root.stat.websock_sent
        );
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_data(&msg, root.instance_id, IID_LEN, NULL, NULL));
    SNIMPL(zmq_send(config->Server.status_socket._sock, &msg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_init_size(&msg, len));
    memcpy(zmq_msg_data(&msg), buf, len);
    SNIMPL(zmq_send(config->Server.status_socket._sock, &msg, 0));
    LDEBUG("STATISTICS ``%s''", buf);
}

int main(int argc, char **argv) {
    config_main_t config;
    config_load(&config, argc, argv);
    logconfig = &config.Globals.error_log;

    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    root.loop = ev_default_loop(0);
    ANIMPL(root.loop);
    ws_server_init(&root.ws, root.loop);
    root.config = &config;
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
    
    init_uid();
    init_statistics();
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    
    struct ev_timer status_timer;
    if(config.Server.status_socket.value_len) {
        SNIMPL(zmq_open(&config.Server.status_socket,
            ZMASK_PUB|ZMASK_PUSH, ZMQ_PUB, NULL, NULL));
        status_timer.data = &config;
        ev_timer_init(&status_timer, flush_statistics,
            config.Server.status_interval, config.Server.status_interval);
        ev_timer_start(root.loop, &status_timer);
    }

    sieve_prepare(&root.sieve, config.Server.max_requests);
    SNIMPL(prepare_http(&config, &config.Routing));
    SNIMPL(prepare_websockets(&config, &config.Routing));

    ws_server_start(&root.ws);
    ev_run(root.loop, 0);
}
