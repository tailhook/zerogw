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
#include "commands.h"

serverroot_t root;

void init_statistics() {
    // quick and dirty
    memset(&root.stat, 0, sizeof(root.stat));
}

int format_statistics(char *buf) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int res = snprintf(buf, STAT_MAXLEN,
        "time: %lu.%06ld\n"
        "interval: %.1f\n"
        #define DEFINE_VALUE(name) #name ": %lu\n"
        #include "statistics.h"
        #undef DEFINE_VALUE
        "%s",
        tv.tv_sec, tv.tv_usec,
        (double)root.config->Server.status.interval,
        #define DEFINE_VALUE(name) root.stat.name,
        #include "statistics.h"
        #undef DEFINE_VALUE
        "");
    buf[STAT_MAXLEN-1] = 0;
    return res;
}

void flush_statistics(struct ev_loop *loop, struct ev_timer *watch, int rev) {
    ANIMPL(!(rev & EV_ERROR));
    config_main_t *config = (config_main_t *)watch->data;
    char buf[STAT_MAXLEN];
    size_t len = format_statistics(buf);
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

static void sighup_cb (int signal)
{
    reopenlogs();
}

int main(int argc, char **argv) {
    config_main_t config;
    config_load(&config, argc, argv);
    logconfig = (config_logging_t *)&config.Server.error_log;
    openlogs();

    signal(SIGHUP, sighup_cb);
    signal(SIGPIPE, SIG_IGN);

    root.loop = ev_default_loop(0);
    ANIMPL(root.loop);
    ws_server_init(&root.ws, root.loop);
    ws_LOGSTD_CB(&root.ws, logstd);
    ws_LOGMSG_CB(&root.ws, logmsg);
    root.config = &config;
    CONFIG_LISTENADDR_LOOP(slisten, config.Server.listen) {
        if(slisten->value.fd >= 0) {
            LDEBUG("Using socket %d", slisten->value.fd);
            SNIMPL(ws_add_fd(&root.ws, slisten->value.fd));
        } else if(slisten->value.unix_socket && *slisten->value.unix_socket) {
            LDEBUG("Using unix socket \"%.*s\"",
                slisten->value.unix_socket_len,
                slisten->value.unix_socket);
            int rc = ws_add_unix(&root.ws, slisten->value.unix_socket,
                slisten->value.unix_socket_len);
            if(rc < 0) {
                LALERT("Can't listen unix socket ``%.*s'': %m",
                    slisten->value.unix_socket_len,
                    slisten->value.unix_socket);
            }
        } else {
            LDEBUG("Using host %s port %d",
                slisten->value.host, slisten->value.port);
            int rc = ws_add_tcp(&root.ws, inet_addr(slisten->value.host),
                slisten->value.port);
            if(rc < 0) {
                LALERT("Can't listen tcp %s:%d: %m",
                    slisten->value.host, slisten->value.port);
            }
        }
    }
    ws_REQUEST_STRUCT(&root.ws, request_t);
    ws_HEADERS_CB(&root.ws, http_headers);
    ws_REQUEST_CB(&root.ws, http_request);
    ws_FINISH_CB(&root.ws, http_request_finish);
    ws_CONNECTION_STRUCT(&root.ws, connection_t);
    ws_CONNECT_CB(&root.ws, on_connect);
    ws_DISCONNECT_CB(&root.ws, on_disconnect);
    ws_WEBSOCKET_CB(&root.ws, start_websocket);
    ws_MESSAGE_CB(&root.ws, websock_message);
    ws_MESSAGE_STRUCT(&root.ws, message_t);
    ws_SET_TIMEOUT(&root.ws, config.Server.network_timeout);

    // Probably here is a place to fork! :)

    init_uid(&config);
    init_statistics();
    root.zmq = zmq_init(config.Server.zmq_io_threads);

    struct ev_timer status_timer;
    if(config.Server.status.socket.value_len) {
        SNIMPL(zmq_open(&config.Server.status.socket,
            ZMASK_PUB|ZMASK_PUSH, ZMQ_PUB, NULL, NULL));
        status_timer.data = &config;
        double ivl = config.Server.status.interval;
        ev_timer_init(&status_timer, flush_statistics,
            ivl - (int)(ev_now(root.loop)/ivl)*ivl, ivl);
        ev_timer_start(root.loop, &status_timer);
    }

    SNIMPL(prepare_commands(&config));
    sieve_prepare(&root.request_sieve, config.Server.max_requests);
    sieve_prepare(&root.hybi.sieve, config.Server.max_websockets);
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
    sieve_free(root.hybi.sieve);

    SNIMPL(release_http(&config, &config.Routing));
    SNIMPL(release_websockets(&config, &config.Routing));
    SNIMPL(release_disk(&config));
    SNIMPL(release_commands(&config));
    if(config.Server.status.socket.value_len) {
        SNIMPL(z_close(&config.Server.status.socket, root.loop));
    }
    config_free(&config);
    ev_loop_destroy(root.loop);

    zmq_term(root.zmq);
    LWARN("Terminated.");
}
