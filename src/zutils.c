#include <stddef.h>
#include <zmq.h>
#include <ev.h>

#include "zutils.h"
#include "log.h"
#include "main.h"

int zmq_open(config_zmqsocket_t *sock, int kinds, int defkind,
    sock_callback callback, struct ev_loop *loop) {
    int zkind, mask;
    switch(sock->kind) {
    case CONFIG_auto:
        zkind = defkind;
        mask = ~0;
        break;
    case CONFIG_zmq_Req:
        zkind = ZMQ_XREQ;
        mask = ZMASK_REQ;
        break;
    case CONFIG_zmq_Rep:
        zkind = ZMQ_XREP;
        mask = ZMASK_REP;
        break;
    case CONFIG_zmq_Push:
        zkind = ZMQ_PUSH;
        mask = ZMASK_PUSH;
        break;
    case CONFIG_zmq_Pull:
        zkind = ZMQ_PULL;
        mask = ZMASK_PULL;
        break;
    case CONFIG_zmq_Pub:
        zkind = ZMQ_PUB;
        mask = ZMASK_PUB;
        break;
    case CONFIG_zmq_Sub:
        zkind = ZMQ_SUB;
        mask = ZMASK_SUB;
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    if(!(mask & kinds)) {
        LERR("Unsupported socket type %d", zkind);
        return -1;
    }
    void *result = zmq_socket(root.zmq, zkind);
    ANIMPL(result);
    LDEBUG("Socket 0x%x is of kind %d", result, zkind);
    CONFIG_ZMQADDR_LOOP(addr, sock->value) {
        if(addr->value.kind == CONFIG_zmq_Bind) {
            LDEBUG("Binding 0x%x to ``%s''", result, addr->value.value);
            if(zmq_bind(result, addr->value.value) < 0) {
                LERR("Can't bind to ``%s'': %m", addr->value.value);
                zmq_close(result);
                return -1;
            }
        } else if(addr->value.kind == CONFIG_zmq_Connect) {
            LDEBUG("Connecting 0x%x to ``%s''", result, addr->value.value);
            if(zmq_connect(result, addr->value.value) < 0) {
                LERR("Can't connect to ``%s'': %m", addr->value.value);
                zmq_close(result);
                return -1;
            }
        } else {
            LNIMPL("Unknown socket type %d", addr->value.kind);
            zmq_close(result);
            return -1;
        }
    }
    if(sock->hwm) {
        uint64_t hwm = sock->hwm;
        SNIMPL(zmq_setsockopt(result, ZMQ_HWM, &hwm, sizeof(hwm)));
    }
    if(sock->identity && sock->identity_len) {
        SNIMPL(zmq_setsockopt(result, ZMQ_IDENTITY,
            sock->identity, sock->identity_len));
    }
    if(sock->swap) {
        uint64_t swap = sock->swap;
        SNIMPL(zmq_setsockopt(result, ZMQ_SWAP, &swap, sizeof(swap)));
    }
    if(sock->affinity) {
        uint64_t affinity = sock->affinity;
        SNIMPL(zmq_setsockopt(result, ZMQ_AFFINITY,
            &affinity, sizeof(affinity)));
    }
    if(sock->rcvbuf) {
        uint64_t rcvbuf = sock->rcvbuf;
        SNIMPL(zmq_setsockopt(result, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf)));
    }
    if(sock->sndbuf) {
        uint64_t sndbuf = sock->sndbuf;
        SNIMPL(zmq_setsockopt(result, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf)));
    }
    int linger = sock->linger;
    SNIMPL(zmq_setsockopt(result, ZMQ_LINGER, &linger, sizeof(linger)));

    sock->_sock = result;
    if(callback) {
        int fd;
        size_t len = sizeof(fd);
        SNIMPL(zmq_getsockopt(result, ZMQ_FD, &fd, &len));
        ev_io_init(&sock->_watch, callback, fd, EV_READ);
        if(loop) {
            ev_io_start(loop, &sock->_watch);
        }
    }
    return 0;
}

void skip_message(void * sock) {
    int64_t opt = 1;
    size_t len = sizeof(opt);
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init(&msg));
    while(opt) {
        SNIMPL(zmq_recv(sock, &msg, 0));
        LDEBUG("Skipped garbage: [%d] %.*s", zmq_msg_size(&msg),
            zmq_msg_size(&msg), zmq_msg_data(&msg));
        SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
    }
    SNIMPL(zmq_msg_close(&msg));
    return;
}

int z_close(config_zmqsocket_t *sock, struct ev_loop *loop) {
    if(sock->_watch.active) {
        ev_io_stop(loop, &sock->_watch);
    }
    SNIMPL(zmq_close(sock->_sock));
    return 0;
}
