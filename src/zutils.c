#include <stddef.h>
#include <zmq.h>

#include "zutils.h"
#include "log.h"

void skip_message(zmq_socket_t sock) {
    size_t opt = 1;
    size_t len = sizeof(opt);
    zmq_msg_t msg;
    SNIMPL(zmq_msg_init(&msg));
    while(opt) {
        SNIMPL(zmq_recv(sock, &msg, 0));
        LDEBUG("Skipped garbage: [%d] %s", zmq_msg_size(&msg), zmq_msg_data(&msg));
        SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &len));
    }
    SNIMPL(zmq_msg_close(&msg));
    return;
}
