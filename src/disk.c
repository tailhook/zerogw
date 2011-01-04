#define _GNU_SOURCE
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <zmq.h>

#include "disk.h"
#include "main.h"
#include "zutils.h"
#include "log.h"
#include "http.h"
#include "resolve.h"

char *join_paths(disk_request_t *req) {
    char *path = req->path;
    char *pathend = strchrnul(path, '?');
    int nstrip = req->route->static_.strip_dirs+1; // always strip first slash
    for(; *path; ++path) {
        if(*path == '/') {
            for(;*path && *path == '/'; ++path);
            --nstrip;
            if(!nstrip) break;
        }
    }
    if(!*path) return NULL;
    char fullpath[req->route->static_.root_len + pathend - path + 2];
    memcpy(fullpath, req->route->static_.root, req->route->static_.root_len);
    fullpath[req->route->static_.root_len] = '/';
    memcpy(fullpath + req->route->static_.root_len + 1, path, pathend - path);
    fullpath[req->route->static_.root_len + 1 + pathend - path] = 0;
    return realpath(fullpath, NULL);
}

int get_file(char *path, zmq_msg_t *msg) {
    int fd;
    do {
        fd = open(path, O_RDONLY);
        if(fd < 0) {
            if(errno != EINTR) {
                TWARN("Can't open file ``%s''", path);
                return -1;
            }
        }
    } while(fd < 0);
    struct stat statinfo;
    if(fstat(fd, &statinfo)) {
        TWARN("Can't stat file ``%s''", path);
        SNIMPL(close(fd));
        return -1;
    }
    size_t to_read = statinfo.st_size;
    if(zmq_msg_init_size(msg, to_read)) {
        TWARN("Can't allocate buffer for file");
        SNIMPL(close(fd));
        return -1;
    }
    void *data = zmq_msg_data(msg);
    while(to_read) {
        ssize_t bytes = read(fd, data, to_read);
        if(bytes < 0) {
            if(errno == EAGAIN || errno == EINTR) {
                TWARN("Can't read file");
                SNIMPL(close(fd));
                return -1;
            }
        }
        data += bytes;
        to_read -= bytes;
        ANIMPL(to_read >= 0);
    }
    SNIMPL(close(fd));
    return 0;
}

void *disk_loop(void *_) {
    void *sock = zmq_socket(root.zmq, ZMQ_REP);
    SNIMPL(zmq_connect(sock, "inproc://disk"));
    while(1) {
        disk_request_t *req;
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        if(zmq_recv(sock, &msg, 0) < 0) {
            if(errno == EINTR || errno == EAGAIN) {
                continue;
            }
            SNIMPL(-1);
        }
        int64_t opt;
        size_t optlen = sizeof(opt);
        SNIMPL(zmq_getsockopt(sock, ZMQ_RCVMORE, &opt, &optlen));
        ANIMPL(optlen == sizeof(opt) && !opt);
        req = zmq_msg_data(&msg);
        size_t reqlen = zmq_msg_size(&msg);
        if(reqlen == 8 && !memcmp(req, "shutdown", 8)) break;
        LDEBUG("Got disk request for ``%s''", req->path);
        char *realpath = join_paths(req);
        if(!realpath) {
            SWARN();
            SNIMPL(zmq_msg_init_data(&msg, "404", 4, NULL, NULL));
            SNIMPL(zmq_send(sock, &msg, 0));
            continue;
        }
        LDEBUG("Resolved ``%s'' -> ``%s''", req->path, realpath);
        zmq_msg_close(&msg);
        zmq_msg_t result;
        zmq_msg_init(&result);
        if(get_file(realpath, &result)) {
            zmq_msg_close(&result);
            SNIMPL(zmq_msg_init_data(&result, "500", 4, NULL, NULL));
            SNIMPL(zmq_send(sock, &result, 0));
            continue;
        }
        SNIMPL(zmq_msg_init_data(&msg, "200 OK", 6, NULL, NULL));
        SNIMPL(zmq_send(sock, &msg, ZMQ_SNDMORE));
        SNIMPL(zmq_msg_init_data(&msg,
            "Content-Type\0text/html\0", 23, NULL, NULL));
        SNIMPL(zmq_send(sock, &msg, ZMQ_SNDMORE));
        SNIMPL(zmq_send(sock, &result, 0));
        LDEBUG("Replied for ``%s''", req->path);
        free(realpath);
        ev_async_send(root.loop, &root.disk_async);
        continue;
    }
    SNIMPL(zmq_close(sock));
    LDEBUG("Disk thread shut down");
}

int disk_request(request_t *req) {
    if(!root.disk_socket) {
        TWARN("Configured static route with non-positive `disk-io-threads`");
        http_static_response(req,
            &req->route->responses.internal_error);
        return 0;
    }
    zmq_msg_t msg;
    REQ_INCREF(req);
    make_hole_uid(req, req->uid, root.request_sieve, FALSE);
    req->flags |= REQ_IN_SIEVE;
    root.stat.disk_requests += 1;
    SNIMPL(zmq_msg_init_data(&msg, req->uid, UID_LEN, request_decref, req));
    while(zmq_send(root.disk_socket, &msg, ZMQ_SNDMORE|ZMQ_NOBLOCK) < 0) {
        if(errno == EAGAIN) {
            http_static_response(req,
                &req->route->responses.service_unavailable);
            return 0;
        } else if(errno == EINTR) {
            continue;
        } else {
            http_static_response(req,
                &req->route->responses.internal_error);
            return 0;
        }
    }
    SNIMPL(zmq_msg_init(&msg));
    SNIMPL(zmq_send(root.disk_socket, &msg, ZMQ_SNDMORE));
    SNIMPL(zmq_msg_init_size(&msg, sizeof(disk_request_t)));
    disk_request_t *dreq = zmq_msg_data(&msg);
    dreq->route = req->route;
    dreq->path = req->ws.uri;
    dreq->gzipped = FALSE; //TODO
    SNIMPL(zmq_send(root.disk_socket, &msg, 0));
    return 0;
}


void disk_process(struct ev_loop *loop, struct ev_io *watch, int revents) {
    ANIMPL(!(revents & EV_ERROR));
    while(TRUE) {
        Z_SEQ_INIT(msg, root.disk_socket);
        LDEBUG("Checking disk...");
        Z_RECV_START(msg, break);
        LDEBUG("Got something from disk");
        if(zmq_msg_size(&msg) != UID_LEN) {
            TWARN("Wrong uid length %d", zmq_msg_size(&msg));
            goto msg_error;
        }
        request_t *req = sieve_get(root.request_sieve,
            UID_HOLE(zmq_msg_data(&msg)));
        ANIMPL(req && UID_EQ(req->uid, zmq_msg_data(&msg)));
        Z_RECV_NEXT(msg);
        ANIMPL(!zmq_msg_size(&msg)); // The sentinel of routing data
        Z_RECV(msg);
        //first is a status-line
        char *data = zmq_msg_data(&msg);
        char *tail;
        int dlen = zmq_msg_size(&msg);
        LDEBUG("Disk status line: [%d] %.*s", dlen, dlen, data);
        if(!msg_opt) { // if there are no subsequent parts
            // then it's error response
            int code = atoi(data);
            if(code == 404) {
                http_static_response(req,
                    &REQRCONFIG(req)->responses.not_found);
            } else {
                http_static_response(req,
                    &REQRCONFIG(req)->responses.internal_error);
            }
            request_finish(req);
            goto msg_error;
        } else {
            ws_statusline(&req->ws, data);
            Z_RECV(msg);
            if(msg_opt) { //second is headers if its not last
                char *data = zmq_msg_data(&msg);
                char *name = data;
                char *value = NULL;
                int dlen = zmq_msg_size(&msg);
                char *end = data + dlen;
                int state = 0;
                for(char *cur = data; cur < end; ++cur) {
                    for(; cur < end; ++cur) {
                        if(!*cur) {
                            value = cur + 1;
                            ++cur;
                            break;
                        }
                    }
                    for(; cur < end; ++cur) {
                        if(!*cur) {
                            ws_add_header(&req->ws, name, value);
                            name = cur + 1;
                            break;
                        }
                    }
                }
                if(name < end) {
                    TWARN("Some garbage at end of headers. "
                          "Please finish each name and each value "
                          "with '\\0' character");
                }
                Z_RECV(msg);
                if(msg_opt) {
                    TWARN("Too many message parts");
                    http_static_response(req,
                        &REQRCONFIG(req)->responses.internal_error);
                    request_finish(req);
                    goto msg_error;
                }
            }
        }
        ws_finish_headers(&req->ws);
        root.stat.disk_reads += 1;
        root.stat.disk_bytes_read = zmq_msg_size(&msg);
        // the last part is always a body
        ANIMPL(!(req->flags & REQ_HAS_MESSAGE));
        SNIMPL(zmq_msg_init(&req->response_msg));
        req->flags |= REQ_HAS_MESSAGE;
        SNIMPL(zmq_msg_move(&req->response_msg, &msg));
        SNIMPL(ws_reply_data(&req->ws, zmq_msg_data(&req->response_msg),
            zmq_msg_size(&req->response_msg)));
        req->flags |= REQ_REPLIED;
        request_finish(req);
    msg_finish:
        Z_SEQ_FINISH(msg);
        continue;
    msg_error:
        Z_SEQ_ERROR(msg);
        continue;
    }
    LDEBUG("Out of disk...");
}

int prepare_disk(config_main_t *config) {
    if(config->Server.disk_io_threads <= 0) {
        root.disk_socket = NULL;
        return 0;
    }
    root.disk_socket = zmq_socket(root.zmq, ZMQ_XREQ);
    SNIMPL(root.disk_socket == NULL);
    SNIMPL(zmq_bind(root.disk_socket, "inproc://disk"));
    int64_t fd;
    size_t fdsize = sizeof(fd);
    SNIMPL(zmq_getsockopt(root.disk_socket, ZMQ_FD, &fd, &fdsize));
    ev_io_init(&root.disk_watch, disk_process, fd, EV_READ);
    ev_io_start(root.loop, &root.disk_watch);
    ev_async_init(&root.disk_async, disk_process);
    ev_async_start(root.loop, &root.disk_async);
    root.disk_threads =malloc(sizeof(pthread_t)*config->Server.disk_io_threads);
    ANIMPL(root.disk_threads);
    
    for(int i = 0; i < config->Server.disk_io_threads; ++i) {
        SNIMPL(pthread_create(&root.disk_threads[i], NULL, disk_loop, NULL));
    }
    
    LWARN("%d disk threads ready", config->Server.disk_io_threads);
    
    return 0;
}

int release_disk(config_main_t *config) {
    while(TRUE) {
        zmq_msg_t msg;
        SNIMPL(zmq_msg_init(&msg));
        if(zmq_send(root.disk_socket, &msg, ZMQ_NOBLOCK|ZMQ_SNDMORE) < 0) {
            if(errno == EAGAIN) break;
            SNIMPL(-1);
        }
        SNIMPL(zmq_msg_init_size(&msg, 8));
        memcpy(zmq_msg_data(&msg), "shutdown", 8);
        zmq_send(root.disk_socket, &msg, ZMQ_NOBLOCK); // don't care if fails
    }
    for(int i = 0; i < config->Server.disk_io_threads; ++i) {
        SNIMPL(pthread_join(root.disk_threads[i], NULL));
    }
    ev_io_stop(root.loop, &root.disk_watch);
    SNIMPL(zmq_close(root.disk_socket));
    free(root.disk_threads);
    return 0;
}

