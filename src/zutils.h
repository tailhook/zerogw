#ifndef _H_ZUTILS
#define _H_ZUTILS

#include "config.h"

typedef void (*sock_callback)(struct ev_loop *ev, struct ev_io *io, int revents);

void skip_message(void * sock);
int zmq_open(config_zmqsocket_t *sock, int kinds, int defkind,
    sock_callback callback, struct ev_loop *loop);
int z_close(config_zmqsocket_t *sock, struct ev_loop *loop);

typedef enum {
    ZMASK_REQ = 1,
    ZMASK_REP = 2,
    ZMASK_PUSH = 4,
    ZMASK_PULL = 8,
    ZMASK_PUB = 16,
    ZMASK_SUB = 32,
} zmq_kind_mask;

#define Z_SEQ_INIT(name, sock) \
    zmq_msg_t name; \
    int64_t name##_opt = 1; \
    size_t name##_len = sizeof(name##_opt); \
    void * name##_sock = (sock); \
    zmq_msg_init(&name);

#define Z_RECV_START(name, break_stmt) \
    if(zmq_recv((name##_sock), (&name), ZMQ_NOBLOCK) < 0) { \
        if(errno == EINTR) goto name##_finish; \
        else if(errno == EAGAIN) { \
            SNIMPL(zmq_msg_close(&name)); \
            break_stmt; \
        } else SNIMPL(-1); \
    } \
    SNIMPL(zmq_getsockopt((name##_sock), ZMQ_RCVMORE, &name##_opt, &name##_len)); \
    if(!name##_opt) goto name##_error;

#define Z_RECV_BLOCK(name) \
    if(zmq_recv((name##_sock), (&name), ZMQ_NOBLOCK) < 0) { \
        if(errno == EINTR || errno == EAGAIN) goto name##_finish; \
        else SNIMPL(-1); \
    } \
    SNIMPL(zmq_getsockopt((name##_sock), ZMQ_RCVMORE, &name##_opt, &name##_len)); \
    if(!name##_opt) goto name##_error;

#define Z_RECV(name) \
    SNIMPL(zmq_recv((name##_sock), (&name), ZMQ_NOBLOCK)); \
    SNIMPL(zmq_getsockopt((name##_sock), ZMQ_RCVMORE, &name##_opt, &name##_len));

#define Z_RECV_NEXT(name) \
    Z_RECV(name); \
    if(!name##_opt) goto name##_error;

#define Z_RECV_LAST(name) \
    Z_RECV(name); \
    if(name##_opt) goto name##_error;

#define Z_SEQ_FINISH(name) \
    SNIMPL(zmq_msg_close(&name));

#define Z_SEQ_ERROR(name) \
    SNIMPL(zmq_msg_close(&name)); \
    if(name##_opt) skip_message(name##_sock);

#endif // _H_ZUTILS
