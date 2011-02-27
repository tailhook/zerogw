
#include "main.h"
#include "log.h"

static void stream(streamer_t *device) {
    bool working = TRUE;
    size_t events = 0;
    size_t evsz = sizeof(events);
    LDEBUG("Forwarding messages");
    while(working) {
        SNIMPL(zmq_getsockopt(device->output, ZMQ_EVENTS, &events, &evsz));
        if(!(events & ZMQ_POLLOUT)) break;
        Z_SEQ_INIT(msg, device->input);
        Z_RECV_START(msg, break);
        *device->stat_unqueued += 1;
        while(TRUE) {
            if(zmq_send(device->output, &msg,
                (msg_opt ? ZMQ_SNDMORE : 0)|ZMQ_NOBLOCK) < 0) {
                TWARN("Failed to forward queued message");
                if(errno == EAGAIN) { //TODO: EINTR???
                    working = FALSE;
                    goto msg_error;
                } else {
                    SNIMPL(-1);
                }
            }
            if(!msg_opt) break;
            Z_RECV(msg);
        }
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        Z_SEQ_ERROR(msg);
        continue;
    }
    if(events & ZMQ_POLLOUT) { // still writable but nothing to do
        if(device->output_watch.active) {
            ev_io_stop(root.loop, &device->output_watch);
        }
        if(!device->input_watch.active) {
            ev_io_start(root.loop, &device->input_watch);
        }
    }
}

static void input_watch(struct ev_loop *loop, struct ev_io *io, int rev) {
    ADEBUG(!(rev & EV_ERROR));
    streamer_t *device = SHIFT(io, streamer_t, input_watch);

    size_t val = val;
    size_t valsz = sizeof(val);
    SNIMPL(zmq_getsockopt(device->input, ZMQ_EVENTS, &val, &valsz));
    if(!(val & ZMQ_POLLIN)) return;
    SNIMPL(zmq_getsockopt(device->output, ZMQ_EVENTS, &val, &valsz));
    if(!(val & ZMQ_POLLOUT)) {
        if(!device->output_watch.active) {
            ev_io_start(root.loop, &device->output_watch);
        }
        ev_io_stop(root.loop, &device->input_watch);
        return;
    }
    stream(device);
}

static void output_watch(struct ev_loop *loop, struct ev_io *io, int rev) {
    ADEBUG(!(rev & EV_ERROR));
    streamer_t *device = SHIFT(io, streamer_t, output_watch);

    size_t val = val;
    size_t valsz = sizeof(val);
    SNIMPL(zmq_getsockopt(device->output, ZMQ_EVENTS, &val, &valsz));
    if(!(val & ZMQ_POLLOUT)) return;
    SNIMPL(zmq_getsockopt(device->input, ZMQ_EVENTS, &val, &valsz));
    if(!(val & ZMQ_POLLIN)) {
        if(device->output_watch.active) {
            ev_io_stop(root.loop, &device->output_watch);
        }
        if(!device->input_watch.active) {
            ev_io_start(root.loop, &device->input_watch);
        }
        return;
    }
    stream(device);
}

int streamer_init(streamer_t *device, void *output, size_t size,
    size_t *stat_queued, size_t *stat_unqueued) {
    char name[128];
    snprintf(name, 128, "inproc://stream%x", output);
    name[127] = 0;
    uint64_t val = size >> 1;
    uint64_t valsz = sizeof(val);
    device->input = zmq_socket(root.zmq, ZMQ_PULL);
    SNIMPL(!device->input);
    SNIMPL(zmq_bind(device->input, name));
    SNIMPL(zmq_setsockopt(device->input, ZMQ_HWM, &val, valsz));
    valsz = sizeof(val);
    SNIMPL(zmq_getsockopt(device->input, ZMQ_FD, &val, &valsz));
    ev_io_init(&device->input_watch, input_watch, val, EV_READ);
    ev_io_start(root.loop, &device->input_watch);

    val = size - (size >> 1);
    device->forwarded = zmq_socket(root.zmq, ZMQ_PUSH);
    SNIMPL(!device->forwarded);
    SNIMPL(zmq_connect(device->forwarded, name));
    SNIMPL(zmq_setsockopt(device->forwarded, ZMQ_HWM, &val, sizeof(val)));

    device->output = output;
    valsz = sizeof(val);
    SNIMPL(zmq_getsockopt(device->output, ZMQ_FD, &val, &valsz));
    ev_io_init(&device->output_watch, output_watch, val, EV_WRITE);
    device->stat_queued = stat_queued;
    device->stat_unqueued = stat_unqueued;
}

int streamer_close(streamer_t *device) {
    if(device->output_watch.active) {
        ev_io_stop(root.loop, &device->output_watch);
    }
    if(device->input_watch.active) {
        ev_io_stop(root.loop, &device->input_watch);
    }
    SNIMPL(zmq_close(device->forwarded));
    SNIMPL(zmq_close(device->input));
}

void streamer_pause(streamer_t *device, bool pause) {
    device->paused = pause;
}
