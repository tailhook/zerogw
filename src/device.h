#ifndef _H_DEVICE
#define _H_DEVICE

#include <sys/queue.h>
#include <ev.h>

#define STREAMER_SEND_LOOP(dev) streamer_t *_dev = (dev); for(void *_sock = \
    (_dev->paused || _dev->output_watch.active) ? _dev->forwarded : _dev->output; \
    _sock; _sock = (!_sock || _sock == _dev->forwarded) ? NULL : _dev->forwarded)
#define STREAMER_SEND(msg) \
    if(zmq_send(_sock, msg, ZMQ_NOBLOCK|ZMQ_SNDMORE) < 0) { \
        if(errno == EAGAIN && _sock == _dev->output) { \
            TWARN("Failed to send message, queueing"); \
            continue; \
        } else { \
            goto streamer_error; \
        } \
    }
#define STREAMER_SEND_LAST(msg) \
    if(zmq_send(_sock, msg, ZMQ_NOBLOCK) < 0) { \
        if(errno == EAGAIN && _sock == _dev->output) { \
            TWARN("Failed to send message, queueing"); \
            continue; \
        } else { \
            goto streamer_error; \
        } \
    } else { \
        if(_sock == _dev->forwarded) { \
            *_dev->stat_queued += 1; \
        } \
    } \
    _sock = NULL;


typedef struct streamer_s {
    void *forwarded;
    void *input;
    void *output;
    struct ev_io input_watch;
    struct ev_io output_watch;
    bool paused;
    size_t *stat_queued;
    size_t *stat_unqueued;
    LIST_ENTRY(streamer_s) lst;
} streamer_t;

int streamer_init(streamer_t *device, void *output, size_t size,
    size_t *stat_queued, size_t *stat_unqueued);
int streamer_close(streamer_t *device);
void streamer_pause(streamer_t *device, bool pause);

#endif //_H_DEVICE
