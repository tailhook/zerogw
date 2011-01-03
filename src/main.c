#include <zmq.h>

#include <ev.h>
#include <website.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
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
#include "disk.h"

serverroot_t root;

void init_statistics() {
    // quick and dirty
    memset(&root.stat, 0, sizeof(root.stat));
}

void flush_statistics(struct ev_loop *loop, struct ev_timer *watch, int rev) {
    ANIMPL(!(rev & EV_ERROR));
    config_main_t *config = (config_main_t *)watch->data;
    char buf[4096];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    size_t len = snprintf(buf, sizeof(buf),
        "%lu.%06d\n"
        "connects: %lu\n"
        "disconnects: %lu\n"
        "http_requests: %lu\n"
        "http_replies: %lu\n"
        "zmq_requests: %lu\n"
        "zmq_retries: %lu\n"
        "zmq_replies: %lu\n"
        "zmq_orphan_replies: %lu\n"
        "websock_connects: %lu\n"
        "websock_disconnects: %lu\n"
        "comet_connects: %lu\n"
        "comet_disconnects: %lu\n"
        "comet_acks: %lu\n"
        "comet_empty_replies: %lu\n"
        "comet_received_messages: %lu\n"
        "comet_received_batches: %lu\n"
        "comet_sent_messages: %lu\n"
        "comet_sent_batches: %lu\n"
        "topics_created: %lu\n"
        "topics_removed: %lu\n"
        "websock_subscribed: %lu\n"
        "websock_unsubscribed: %lu\n"
        "websock_published: %lu\n"
        "websock_sent: %lu\n"
        "websock_received: %lu\n"
        "disk_requests: %lu\n"
        "disk_reads: %lu\n"
        "disk_bytes_read: %lu\n"
        ,
        tv.tv_sec, tv.tv_usec,
        root.stat.connects,
        root.stat.disconnects,
        root.stat.http_requests,
        root.stat.http_replies,
        root.stat.zmq_requests,
        root.stat.zmq_retries,
        root.stat.zmq_replies,
        root.stat.zmq_orphan_replies,
        root.stat.websock_connects,
        root.stat.websock_disconnects,
        root.stat.comet_connects,
        root.stat.comet_disconnects,
        root.stat.comet_acks,
        root.stat.comet_empty_replies,
        root.stat.comet_received_messages,
        root.stat.comet_received_batches,
        root.stat.comet_sent_messages,
        root.stat.comet_sent_batches,
        root.stat.topics_created,
        root.stat.topics_removed,
        root.stat.websock_subscribed,
        root.stat.websock_unsubscribed,
        root.stat.websock_published,
        root.stat.websock_sent,
        root.stat.websock_received,
        root.stat.disk_requests,
        root.stat.disk_reads,
        root.stat.disk_bytes_read
        );
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init_data(&msg, root.instance_id, IID_LEN, NULL, NULL));
    SNIMPL(zmq_send(config->Server.status.socket._sock, &msg,
        ZMQ_SNDMORE|ZMQ_NOBLOCK));
    SNIMPL(zmq_msg_init_size(&msg, len));
    memcpy(zmq_msg_data(&msg), buf, len);
    SNIMPL(zmq_send(config->Server.status.socket._sock, &msg, ZMQ_NOBLOCK));
    LDEBUG("STATISTICS ``%s''", buf);
}

int on_connect(connection_t *conn) {
    conn->hybi = NULL;
    root.stat.connects += 1;
}

int on_disconnect(connection_t *conn) {
    root.stat.disconnects += 1;
}

static void sigint_cb (struct ev_loop *loop, ev_signal *w, int revents)
{
    LWARN("Received SIGINT, terminating main loop");
    ev_break (loop, EVBREAK_ALL);
}

int main(int argc, char **argv) {
    config_main_t config;
    config_load(&config, argc, argv);
    logconfig = (config_logging_t *)&config.Server.error_log;

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
    ws_CONNECT_CB(&root.ws, on_connect);
    ws_DISCONNECT_CB(&root.ws, on_disconnect);
    ws_WEBSOCKET_CB(&root.ws, start_websocket);
    ws_MESSAGE_CB(&root.ws, websock_message);
    ws_MESSAGE_STRUCT(&root.ws, message_t);

    // Probably here is a place to fork! :)
    
    init_uid();
    init_statistics();
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    
    struct ev_timer status_timer;
    if(config.Server.status.socket.value_len) {
        SNIMPL(zmq_open(&config.Server.status.socket,
            ZMASK_PUB|ZMASK_PUSH, ZMQ_PUB, NULL, NULL));
        status_timer.data = &config;
        ev_timer_init(&status_timer, flush_statistics,
            config.Server.status.interval, config.Server.status.interval);
        ev_timer_start(root.loop, &status_timer);
    }

    sieve_prepare(&root.request_sieve, config.Server.max_requests);
    sieve_prepare(&root.hybi_sieve, config.Server.max_websockets);
    SNIMPL(prepare_http(&config, &config.Routing));
    SNIMPL(prepare_websockets(&config, &config.Routing));
    SNIMPL(prepare_disk(&config));

    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, sigint_cb, SIGINT);
    ev_signal_start(root.loop, &signal_watcher);

    ws_server_start(&root.ws);
    ev_run(root.loop, 0);
    ws_server_destroy(&root.ws);
    
    sieve_free(root.request_sieve);
    sieve_free(root.hybi_sieve);
    
    SNIMPL(release_http(&config, &config.Routing));
    SNIMPL(release_websockets(&config, &config.Routing));
    if(config.Server.status.socket.value_len) {
        SNIMPL(z_close(config.Server.status.socket._sock, root.loop));
    }
    config_free(&config);
    ev_loop_destroy(root.loop);
    
    zmq_term(root.zmq);
    LWARN("Terminated.");
}
