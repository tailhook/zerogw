#include <stddef.h>
#include <xs.h>
#include <ev.h>

#include "zutils.h"
#include "log.h"
#include "main.h"

int xs_open(config_zmqsocket_t *sock, int kinds, int defkind,
    sock_callback callback, struct ev_loop *loop) {
    int zkind, mask;
    switch(sock->kind) {
    case CONFIG_auto:
        zkind = defkind;
        mask = ~0;
        break;
    case CONFIG_zmq_Req:
        zkind = XS_XREQ;
        mask = ZMASK_REQ;
        break;
    case CONFIG_zmq_Rep:
        zkind = XS_XREP;
        mask = ZMASK_REP;
        break;
    case CONFIG_zmq_Push:
        zkind = XS_PUSH;
        mask = ZMASK_PUSH;
        break;
    case CONFIG_zmq_Pull:
        zkind = XS_PULL;
        mask = ZMASK_PULL;
        break;
    case CONFIG_zmq_Pub:
        zkind = XS_PUB;
        mask = ZMASK_PUB;
        break;
    case CONFIG_zmq_Sub:
        zkind = XS_SUB;
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
    void *result = xs_socket(root.zmq, zkind);
    ANIMPL(result);
    LDEBUG("Socket 0x%x is of kind %d", result, zkind);
    CONFIG_ZMQADDR_LOOP(addr, sock->value) {
        if(addr->value.kind == CONFIG_zmq_Bind) {
            LDEBUG("Binding 0x%x to ``%s''", result, addr->value.value);
            if(xs_bind(result, addr->value.value) < 0) {
                LERR("Can't bind to ``%s'': %m", addr->value.value);
                xs_close(result);
                return -1;
            }
        } else if(addr->value.kind == CONFIG_zmq_Connect) {
            LDEBUG("Connecting 0x%x to ``%s''", result, addr->value.value);
            if(xs_connect(result, addr->value.value) < 0) {
                LERR("Can't connect to ``%s'': %m", addr->value.value);
                xs_close(result);
                return -1;
            }
        } else {
            LNIMPL("Unknown socket type %d", addr->value.kind);
            xs_close(result);
            return -1;
        }
    }
    if(sock->sndhwm) {
        int hwm = sock->sndhwm;
        SNIMPL(xs_setsockopt(result, XS_SNDHWM, &hwm, sizeof(hwm)));
    }
    if(sock->rcvhwm) {
        int hwm = sock->rcvhwm;
        SNIMPL(xs_setsockopt(result, XS_RCVHWM, &hwm, sizeof(hwm)));
    }
    if(sock->identity && sock->identity_len) {
        SNIMPL(xs_setsockopt(result, XS_IDENTITY,
            sock->identity, sock->identity_len));
    }
    if(sock->affinity) {
        uint64_t affinity = sock->affinity;
        SNIMPL(xs_setsockopt(result, XS_AFFINITY,
            &affinity, sizeof(affinity)));
    }
    if(sock->rcvbuf) {
        int rcvbuf = sock->rcvbuf;
        SNIMPL(xs_setsockopt(result, XS_RCVBUF, &rcvbuf, sizeof(rcvbuf)));
    }
    if(sock->sndbuf) {
        int sndbuf = sock->sndbuf;
        SNIMPL(xs_setsockopt(result, XS_SNDBUF, &sndbuf, sizeof(sndbuf)));
    }
    int linger = sock->linger;
    SNIMPL(xs_setsockopt(result, XS_LINGER, &linger, sizeof(linger)));

    sock->_sock = result;
    if(callback) {
        int fd;
        size_t len = sizeof(fd);
        SNIMPL(xs_getsockopt(result, XS_FD, &fd, &len));
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
    xs_msg_t msg;
    SNIMPL(xs_msg_init(&msg));
    while(opt) {
        NNIMPL(xs_recvmsg(sock, &msg, 0));
        LDEBUG("Skipped garbage: [%d] %.*s", xs_msg_size(&msg),
            xs_msg_size(&msg), xs_msg_data(&msg));
        SNIMPL(xs_getsockopt(sock, XS_RCVMORE, &opt, &len));
    }
    SNIMPL(xs_msg_close(&msg));
    return;
}

int z_close(config_zmqsocket_t *sock, struct ev_loop *loop) {
    if(sock->_watch.active) {
        ev_io_stop(loop, &sock->_watch);
    }
    SNIMPL(xs_close(sock->_sock));
    return 0;
}
