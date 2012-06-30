#include <stdlib.h>
#include <openssl/md5.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "polling.h"
#include "log.h"
#include "websocket.h"
#include "http.h"
#include "main.h"

typedef enum {
    FMT_SINGLE,
    FMT_JSONLIST
} format_enum;

typedef enum {
    ACT_UNKNOWN,
    ACT_CONNECT,
    ACT_GET,
    ACT_POST,
    ACT_BIDI,
    ACT_CLOSE
} action_enum;

typedef struct comet_args_s {
    action_enum action;
    char uid[UID_LEN];
    size_t last_message;
    float timeout;
    format_enum in_format;
    format_enum out_format;
    int limit;
} comet_args_t;

const char hexdigits[16] = "0123456789ABCDEF";

static format_enum parse_format(char *name, int nlen) {
    if(!nlen)
        return FMT_SINGLE;
    switch(*name) {
        case 's':
            if(nlen == 6 && !strncmp(name, "single", 6)) {
                return FMT_SINGLE;
            }
            break;
        case 'j':
            if(nlen == 8 && !strncmp(name, "jsonlist", 8)) {
                return FMT_JSONLIST;
            }
            break;
    }
    return FMT_SINGLE;
}

/*
    action=CONNECT
    id=XXX&ack=123&timeout=20&infmt=single&outfmt=single&limit=1
    action=CLOSE&id=XXX
*/
static int add_line(comet_args_t *args, char *name, int nl, char *value, int vl) {
    if(nl == 2 && !strncmp("id", name, 2)) {
        if(vl != UID_LEN*2) {
            UWARN("Wrong uid ``%.*s''", vl, value);
            errno = EINVAL;
            return -1;
        }
        for(int i = 0; i < UID_LEN; ++i) {
            unsigned char c = value[i*2];
            if(c >= '0' && c <= '9') {
                c -= '0';
            } else if(c >= 'A' && c <= 'F') {
                c -= 'A' - 10;
            } else {
                errno = EINVAL;
                return -1;
            }
            unsigned char d = value[i*2+1];
            if(d >= '0' && d <= '9') {
                d -= '0';
            } else if(d >= 'A' && d <= 'F') {
                d -= 'A' - 10;
            } else {
                errno = EINVAL;
                return -1;
            }
            c = (c << 4) | d;
            args->uid[i] = c;
        }
        char mask[MD5_DIGEST_LENGTH];
        MD5_CTX md5;
        MD5_Init(&md5);
        MD5_Update(&md5, args->uid + (UID_LEN - UID_NRANDOM), UID_NRANDOM);
        MD5_Update(&md5, root.random_data, RANDOM_LENGTH);
        MD5_Final(mask, &md5);
        for(int i = 0; i < UID_LEN - UID_NRANDOM; ++i) {
            args->uid[i] ^= mask[i % MD5_DIGEST_LENGTH];
        }
    }
    if(nl == 6 && !strncmp("action", name, 6)) {
        if(vl == 7 && !strncmp("CONNECT", value, 7)) {
            args->action = ACT_CONNECT;
        } else if(vl == 5 && !strncmp("CLOSE", value, 5)) {
            args->action = ACT_CLOSE;
        } else {
            TWARN("Wrong action ``%.*s''", vl, value);
            errno = EINVAL;
            return -1;
        }
    }
    if(nl == 3 && !strncmp("ack", name, 3)) {
        char copy[vl+1];
        memcpy(copy, value, vl);
        copy[vl] = 0;
        char *end;
        args->last_message = strtol(copy, &end, 10);
        if(end != copy + vl) {
            TWARN("Wrong message id ``%.*s''", vl, value);
            errno = EINVAL;
            return -1;
        }
    }
    if(nl == 7 && !strncmp("timeout", name, 7)) {
        char copy[vl+1];
        memcpy(copy, value, vl);
        copy[vl] = 0;
        char *end;
        args->timeout = strtof(copy, &end);
        if(end != copy + vl) {
            TWARN("Wrong timeout ``%.*s''", vl, value);
        }
    }
    if(nl == 5 && !strncmp("limit", name, 5)) {
        char copy[vl+1];
        memcpy(copy, value, vl);
        copy[vl] = 0;
        char *end;
        args->limit = strtol(copy, &end, 10);
        if(end != value + vl) {
            TWARN("Wrong limit ``%.*s''", vl, value);
        }
    }
    if(nl == 5 && !strncmp("infmt", name, 5)) {
        args->in_format = parse_format(value, vl);
    }
    if(nl == 6 && !strncmp("outfmt", name, 6)) {
        args->out_format = parse_format(value, vl);
    }
    // Skip unknown, maybe for forward compatibility
    // and also for adding random seed, to disable caching
    return 0;
}

static int parse_uri(request_t *req, comet_args_t *args) {
    memset(args, 0, sizeof(*args));
    args->limit = -1;
    args->timeout = -1;
    char *c = req->ws.uri;
    for(;*c && *c != '?'; ++c);
    while(*c++) {
        char *name = c;
        for(;*c && *c != '='; ++c);
        int namelen = c - name;
        if(!*c) {
            if(add_line(args, name, namelen, NULL, 0) < 0) return -1;
            break;
        }
        char *value = ++c;
        for(;*c && *c != '&'; ++c);
        int valuelen = c - value;
        if(add_line(args, name, namelen, value, valuelen) < 0) return -1;
    }
    return 0;
}

static void nocache_headers(ws_request_t *req) {
    ws_add_header(req, "Cache-Control",
        "no-cache, no-store, must-revalidate,"
        " max-age=0, pre-check=0, post-check=0");
    ws_add_header(req, "Pragma", "no-cache");
    ws_add_header(req, "Expires", "Sat, 01 Dec 2001 00:00:00 GMT");
}

static void timestamp_headers(request_t *req) {
    if(req->route->websocket.polling_fallback.timestamps) {
        char buf[32];
        req->outgoing_time = ev_now(root.loop);
        sprintf(buf, "%f", req->outgoing_time);
        ws_add_header(&req->ws, "X-Timestamp", buf);
        sprintf(buf, "%f", req->outgoing_time - req->incoming_time);
        ws_add_header(&req->ws, "X-Wait-Time", buf);
    }
}

static void empty_reply(hybi_t *hybi) {
    root.stat.comet_empty_replies += 1;
    request_t *mreq = hybi->comet->cur_request;
    if(mreq->timeout.active) {
        ev_timer_stop(root.loop, &mreq->timeout);
    }
    ws_request_t *req = &mreq->ws;
    ws_statusline(req, "200 OK");
    ws_add_header(req, "X-Messages", "0");
    nocache_headers(req);
    timestamp_headers(mreq);
    ws_reply_data(req, "", 0);
    mreq->hybi = NULL;
    REQ_DECREF(mreq);
    hybi->comet->cur_request = NULL;
}

static void free_comet(hybi_t *hybi) {
    root.stat.comet_disconnects += 1;
    ANIMPL(!hybi->comet->cur_request);
    if(hybi->comet->sendlater.active) {
        ev_idle_stop(root.loop, &hybi->comet->sendlater);
    }
    if(hybi->comet->inactivity.active) {
        ev_timer_stop(root.loop, &hybi->comet->inactivity);
    }
    queue_item_t *cur;
    TAILQ_FOREACH(cur, &hybi->comet->queue.items, list) {
        zmq_msg_close(&((frontend_msg_t *)cur)->zmsg);
    }
    hybi_stop(hybi);
}

static void close_reply(hybi_t *hybi) {
    request_t *mreq = hybi->comet->cur_request;
    if(mreq->timeout.active) {
        ev_timer_stop(root.loop, &mreq->timeout);
    }
    ws_request_t *req = &mreq->ws;
    ws_statusline(req, "200 OK");
    ws_add_header(req, "X-Messages", "0");
    ws_add_header(req, "X-Connection", "close");
    nocache_headers(req);
    timestamp_headers(mreq);  // for consistency
    ws_reply_data(req, "", 0);
    mreq->hybi = NULL;
    REQ_DECREF(mreq);
    hybi->comet->cur_request = NULL;
}

void comet_close(hybi_t *hybi) {
    if(hybi->comet->cur_request) {
        close_reply(hybi);
    }
    free_comet(hybi);
}


static void send_message(hybi_t *hybi, request_t *req) {
    root.stat.comet_received_batches += 1;
    root.stat.comet_received_messages += 1;
    config_zmqsocket_t *sock = websock_resolve(hybi,
        req->ws.body, req->ws.bodylen);
    if(sock) {
        REQ_INCREF(req);
        if(backend_send(sock, hybi, req, FALSE)) {
            //TODO: disconnect hybi
            TWARN("Failed to queue message from http");
            return;
        }
    } else {
        // This is frontend command
        return;
    }
    return;
}

void comet_request_aborted(request_t *req) {
    TWARN("Aborted comet request %x", req->hybi->comet->cur_request);
    root.stat.comet_aborted_replies += 1;
    ANIMPL(req->hybi->comet->cur_request == req);
    if(req->timeout.active) {
        ev_timer_stop(root.loop, &req->timeout);
    }
    hybi_t *hybi = req->hybi;
    req->hybi->comet->cur_request = NULL;
    REQ_DECREF(req);
    ev_timer_again(root.loop, &hybi->comet->inactivity);
}

static void polling_timeout(struct ev_loop *loop, struct ev_timer *timer,
    int rev)
{
    ANIMPL(!(rev & EV_ERROR));
    request_t *req = SHIFT(timer, request_t, timeout);
    hybi_t *hybi = req->hybi;
    ANIMPL(req->hybi->comet->cur_request == req);
    empty_reply(hybi);
    ev_timer_again(root.loop, &hybi->comet->inactivity);
}

static void inactivity_timeout(struct ev_loop *loop, struct ev_timer *timer,
    int rev)
{
    LNOTICE("Closing polling websocket due to inactivity");
    ANIMPL(!(rev & EV_ERROR));
    ev_timer_stop(loop, timer);
    hybi_t *hybi = SHIFT(timer, hybi_t, comet[0].inactivity);
    free_comet(hybi);
}

static int move_queue(hybi_t *hybi, size_t msgid) {
    if(msgid == 0 || msgid < hybi->comet->first_index) return 0;
    LDEBUG("Acked %d while at %d with %d items", msgid,
	   hybi->comet->first_index, hybi->comet->queue.size);
    size_t amount = msgid - hybi->comet->first_index + 1;
    if(amount > hybi->comet->queue.size) {
        TWARN("Wrong ack ``%d''/%d+%d", msgid,
            hybi->comet->first_index, hybi->comet->queue.size);
        return -1;
    }
    if(amount) {
        root.stat.comet_acks += amount;
        queue_item_t *cur = TAILQ_FIRST(&hybi->comet->queue.items), *nxt;
        for(int i = 0; i < amount; cur = nxt, i += 1) {
            ANIMPL(cur);
            nxt = TAILQ_NEXT(cur, list);
            zmq_msg_close(&((frontend_msg_t *)cur)->zmsg);
            queue_remove(&hybi->comet->queue, cur);
        }
        hybi->comet->first_index += amount;
    }
    return 0;
}

static int comet_request_sent(ws_request_t *hint) {
    request_t *req = (request_t *)hint;
    if(req->hybi) {
        comet_request_aborted(req);
    } else {
        if(req->flags & REQ_OWNS_WSMESSAGE) {
            ws_MESSAGE_DECREF(&req->ws_msg->ws);
        }
        REQ_DECREF(req);
    }
    return 1;
};

static void send_later(struct ev_loop *loop, struct ev_idle *watch, int rev) {
    hybi_t *hybi = SHIFT(watch, hybi_t, comet[0].sendlater);
    comet_t *comet = hybi->comet;
    request_t *mreq = comet->cur_request;
    ANIMPL(!comet->overflow);
    if(!mreq) {
        // Seems request aborted before we had chance to send anything
        ev_idle_stop(loop, &comet->sendlater);
        return;
    }
    ws_request_t *req = &mreq->ws;
    ANIMPL(req);
    if(comet->queue.size) {
        if(mreq->timeout.active) {
            ev_timer_stop(loop, &mreq->timeout);
        }
        comet->cur_request = NULL;
        root.stat.comet_sent_batches += 1;
        if(comet->cur_format == FMT_SINGLE) {
            root.stat.comet_sent_messages += 1;
            char buf[64];
            sprintf(buf, "%i", comet->first_index);
            ws_statusline(req, "200 OK");
            ws_add_header(req, "X-Messages", "1");
            ws_add_header(req, "X-Format", "single");
            ws_add_header(req, "X-Message-ID", buf);
            nocache_headers(req);
            timestamp_headers(mreq);
            ws_finish_headers(req);
            mreq->flags |= REQ_HAS_MESSAGE;
            SNIMPL(zmq_msg_init(&mreq->response_msg));
            SNIMPL(zmq_msg_copy(&mreq->response_msg,
                &((frontend_msg_t *)TAILQ_FIRST(&comet->queue.items))->zmsg));
            SNIMPL(ws_reply_data(req, zmq_msg_data(&mreq->response_msg),
                zmq_msg_size(&mreq->response_msg)));
        } else if(comet->cur_format == FMT_JSONLIST) {
            int num = comet->queue.size;
            if(num > comet->cur_limit) {
                num = comet->cur_limit;
            }
            char buf[128];
            ws_statusline(req, "200 OK");
            sprintf(buf, "%i", num);
            ws_add_header(req, "X-Messages", buf);
            sprintf(buf, "%i", comet->first_index + comet->queue.size - 1);
            ws_add_header(req, "X-Last-ID", buf);
            ws_add_header(req, "X-Format", "jsonlist");
            ws_add_header(req, "Content-Type", "application/json");
            nocache_headers(req);
            timestamp_headers(mreq);
            ws_finish_headers(req);
            obstack_blank(&req->pieces, 0);
            obstack_1grow(&req->pieces, '[');
            frontend_msg_t *msg = (frontend_msg_t *)TAILQ_FIRST(
                &comet->queue.items);
            for(int i = num-1; i >= 0; --i) {
                root.stat.comet_sent_messages += 1;
                char *data = zmq_msg_data(&msg->zmsg);
                int datasize = zmq_msg_size(&msg->zmsg);
                if(datasize > 7 /*strlen("ZEROGW:")*/
                    && !strncmp(data, "ZEROGW:", 7)) {
                    obstack_1grow(&req->pieces, '"');
                    // TODO(tailhook) escape value if needed
                    obstack_grow(&req->pieces, data, datasize);
                    obstack_1grow(&req->pieces, '"');
                } else {
                    obstack_grow(&req->pieces, data, datasize);
                }
                if(i > 0) {
                    obstack_1grow(&req->pieces, ',');
                }
                msg = (frontend_msg_t *)TAILQ_NEXT(msg, qhead.list);
            }
            obstack_1grow(&req->pieces, ']');
            int len = obstack_object_size(&req->pieces);
            ws_reply_data(req, obstack_finish(&req->pieces), len);
        } else {
            LNIMPL("Response format %d not implemented", comet->cur_format);
        }
        mreq->hybi = NULL; // don't need hybi any more
        REQ_DECREF(mreq);
    }
    ev_idle_stop(loop, &comet->sendlater);
    ev_timer_again(loop, &comet->inactivity);
}

static int comet_connect(request_t *req) {
    hybi_t *hybi = hybi_start(req->route, HYBI_COMET);
    if(!hybi) {
        return -1; // FIXME: send meaningfull reply
    }
    root.stat.comet_connects += 1;
    hybi->comet->cur_request = NULL;
    hybi->comet->cur_format = FMT_SINGLE;
    init_queue(&hybi->comet->queue,
        req->route->websocket.polling_fallback.queue_limit,
        &root.hybi.frontend_pool);
    hybi->comet->first_index = 1;
    hybi->comet->overflow = FALSE;
    ev_idle_init(&hybi->comet->sendlater, send_later);
    ev_timer_init(&hybi->comet->inactivity, inactivity_timeout,
        req->route->websocket.polling_fallback.inactivity_timeout,
        req->route->websocket.polling_fallback.inactivity_timeout);
    ev_timer_start(root.loop, &hybi->comet->inactivity);
    ws_statusline(&req->ws, "200 OK");
    nocache_headers(&req->ws);
    timestamp_headers(req);
    ws_finish_headers(&req->ws);
    char *uid = obstack_alloc(&req->ws.pieces, UID_LEN*2);
    char mask[MD5_DIGEST_LENGTH];
    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, hybi->uid + (UID_LEN - UID_NRANDOM), UID_NRANDOM);
    MD5_Update(&md5, root.random_data, RANDOM_LENGTH);
    MD5_Final(mask, &md5);
    for(int i = 0; i < UID_LEN - UID_NRANDOM; ++i) {
        char c = hybi->uid[i];
        c ^= mask[i % MD5_DIGEST_LENGTH];
        uid[i*2] = hexdigits[(c >> 4)&0xf];
        uid[i*2+1] = hexdigits[c & 0xf];
    }
    for(int i = UID_LEN - UID_NRANDOM; i < UID_LEN; ++i) {
        char c = hybi->uid[i];
        uid[i*2] = hexdigits[(c >> 4)&0xf];
        uid[i*2+1] = hexdigits[c & 0xf];
    }
    ws_reply_data(&req->ws, uid, UID_LEN*2);
    return 0;
}

int comet_request(request_t *req) {
    comet_args_t args;
    LDEBUG("Got comet request");
    if(parse_uri(req, &args) < 0) {
        UWARN("Can't parse COMET request arguments ``%s''", req->ws.uri);
        return -1; // FIXME: send meaningfull reply
    }
    if(args.action == ACT_CONNECT) {
        LDEBUG("Got comet CONNECT request");
        return comet_connect(req);
    }

    hybi_t *hybi = hybi_find(args.uid);
    if(!hybi) {
        UWARN("Websocket not found, probably already dead ``%s''", req->ws.uri);
        return -1;
    }
    if(hybi->type != HYBI_COMET) {
        TWARN("Trying to use websocket id as comet id");
        return -1; //FIXME: send meaningfull reply
    }
    if(args.action == ACT_UNKNOWN) {
        if(req->ws.bodylen) {
            if(args.limit != 0) {
                args.action = ACT_BIDI;
            } else {
                args.action = ACT_POST;
            }
        } else {
            if(args.limit == 0) {
                TWARN("Comet request with no body and zero limit");
                return -1; // FIXME: send meaningful reply
            } else {
                args.action = ACT_GET;
            }
        }
    }
    ANIMPL(hybi->comet->cur_request != req);
    if(args.action == ACT_GET || args.action == ACT_BIDI
        || args.action == ACT_CLOSE) {
        if(hybi->comet->cur_request) {
            LDEBUG("Closing old request");
            empty_reply(hybi);
        }
    }
    if(move_queue(hybi, args.last_message) < 0 || args.action == ACT_CLOSE) {
        //TODO: maybe don't close, or send another message
        // in case of message move fail (usually wrong ack id)
        if(hybi->comet->cur_request) {
            close_reply(hybi);
        }
        REQ_INCREF(req);
        hybi->comet->cur_request = req;
        comet_close(hybi);
        return 0;
    }
    if(args.action == ACT_POST || args.action == ACT_BIDI)  {
        if(args.in_format == FMT_SINGLE) {
            send_message(hybi, req);
        } else {
            return -1; //FIXME: send meaningful reply
        }
    }
    LDEBUG("Action %d", args.action);
    if(args.action == ACT_GET || args.action == ACT_BIDI) {
        LDEBUG("Queueing request");
        REQ_INCREF(req);
        hybi->comet->cur_request = req;
        hybi->comet->cur_format = args.out_format;
        int limit = hybi->route->websocket.polling_fallback.queue_limit;
        if(args.limit >= 0 && args.limit < limit) {
            limit = args.limit;
        }
        hybi->comet->cur_limit = limit;
        req->hybi = hybi;
        ws_FINISH_CB(&req->ws, comet_request_sent);
        if(hybi->comet->queue.size) {
            ev_idle_start(root.loop, &hybi->comet->sendlater);
        } else {
            // libwebsite works under TCP_CORK if there are unanswered requests
            // which is ok for most cases but not for COMET, so we flush
            // buffers when timing out request. It's cheap if buffer is empty
            int opt = 1;
            setsockopt(req->ws.conn->watch.fd,
                IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
            double timeout = hybi->route->websocket.polling_fallback.max_timeout;
            if(args.timeout >= 0 && timeout > args.timeout) {
                timeout = args.timeout;
            }
            LDEBUG("Setting timeout %f", timeout);
            ev_timer_init(&req->timeout, polling_timeout, timeout, timeout);
            ev_timer_start(root.loop, &req->timeout);
        }
        ev_timer_stop(root.loop, &hybi->comet->inactivity);
    } else if(args.action == ACT_POST) {
        ws_request_t *wreq = &req->ws;
        ws_statusline(wreq, "200 OK");
        ws_add_header(wreq, "X-Messages", "0");
        nocache_headers(wreq);
        timestamp_headers(req);
        ws_reply_data(wreq, "", 0);
    }
    return 0;
}


int comet_send(hybi_t *hybi, message_t *msg) {
    frontend_msg_t *qitem = (frontend_msg_t *)queue_push(&hybi->comet->queue);
    if(!qitem) {
        LWARN("Queue overflowed with %d", hybi->comet->queue.size);
        comet_close(hybi);
        errno = EOVERFLOW;
        return -1;
    }
    SNIMPL(zmq_msg_init(&qitem->zmsg));
    SNIMPL(zmq_msg_copy(&qitem->zmsg, &msg->zmq));
    LDEBUG("Queued up to %d last acked %d",
        hybi->comet->queue.size, hybi->comet->first_index);
    if(hybi->comet->cur_request && !hybi->comet->sendlater.active) {
        ev_idle_start(root.loop, &hybi->comet->sendlater);
    }
    return 0;
}
