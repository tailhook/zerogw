#include "commands.h"
#include "main.h"
#include "zutils.h"
#include "log.h"
#include "websocket.h"

#define REPLY_COMMAND(sock, msg, more) \
            if(zmq_send(sock, &msg, ZMQ_NOBLOCK \
                | (more ? ZMQ_SNDMORE : 0)) < 0) { \
                LWARN("Can't send reply on command"); \
                goto msg_error; \
            }
#define REPLY_SHORT(sock, msg, str, more) \
    zmq_msg_init_size(&msg, strlen(str)); \
    memcpy(zmq_msg_data(&msg), str, strlen(str)); \
    REPLY_COMMAND(sock, msg, more)

#define COMMAND(name) (len == strlen(#name) \
            && !memcmp(data, #name, strlen(#name)))

void recv_command(struct ev_loop *loop, struct ev_io *io, int rev) {
    ADEBUG(!(rev & EV_ERROR));
    void *sock = SHIFT(io, config_zmqsocket_t, _watch)->_sock;
    while(TRUE) {
        Z_SEQ_INIT(msg, sock);
        Z_RECV_START(msg, break);
        size_t len;
        while(TRUE) {
            len = zmq_msg_size(&msg);
            REPLY_COMMAND(sock, msg, TRUE);
            if(!len) break;
            Z_RECV_NEXT(msg);
        }
        Z_RECV(msg);
        len = zmq_msg_size(&msg);
        char *data = zmq_msg_data(&msg);
        LDEBUG("Got command `%.*s`", len, data);
        if COMMAND(list_commands) {
            // Keep alphabetically sorted
            REPLY_SHORT(sock, msg, "list_commands", TRUE);
            REPLY_SHORT(sock, msg, "pause_websockets", TRUE);
            REPLY_SHORT(sock, msg, "resume_websockets", TRUE);
            REPLY_SHORT(sock, msg, "sync_now", TRUE);
            REPLY_SHORT(sock, msg, "reopen_logs", FALSE);
        } else if COMMAND(pause_websockets) {
            LWARN("Pausing websockets because of command");
            pause_websockets(TRUE);
            REPLY_SHORT(sock, msg, "paused", FALSE);
        } else if COMMAND(resume_websockets) {
            LWARN("Resuming websockets because of command");
            pause_websockets(FALSE);
            REPLY_SHORT(sock, msg, "resumed", FALSE);
        } else if COMMAND(sync_now) {
            LWARN("Forcing users sync");
            websockets_sync_now();
            REPLY_SHORT(sock, msg, "sync_sent", FALSE);
        } else if COMMAND(reopen_logs) {
            LWARN("Forcing log reopen");
            if(reopenlogs()) {
                REPLY_SHORT(sock, msg, "logs_reopened", FALSE);
            } else {
                REPLY_SHORT(sock, msg, "cant_reopen_logs", FALSE);
            }
        } else {
            REPLY_SHORT(sock, msg, "error", FALSE);
        }
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        Z_SEQ_ERROR(msg);
        continue;
    }
}

int prepare_commands(config_main_t *config) {
    SNIMPL(zmq_open(&config->Server.control.socket,
        ZMASK_REP, ZMQ_XREP, recv_command, root.loop));
    return 0;
}
int release_commands(config_main_t *config) {
    SNIMPL(z_close(&config->Server.control.socket, root.loop));
    return 0;
}
