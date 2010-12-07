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
#include "resolve.h"
#include "http.h"

serverroot_t root;

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
    root.zmq = zmq_init(config.Server.zmq_io_threads);
    SNIMPL(zmq_open(&config.Server.status_socket,
        ZMASK_PUB|ZMASK_PUSH, ZMQ_PUB, NULL, NULL));

    sieve_prepare(&root.sieve, config.Server.max_requests);
    SNIMPL(prepare_http(&config, &config.Routing));
    SNIMPL(prepare_websockets(&config, &config.Routing));

    ws_server_start(&root.ws);
    ev_run(root.loop, 0);
}
