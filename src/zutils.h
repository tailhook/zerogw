#ifndef _H_ZUTILS
#define _H_ZUTILS

typedef void *zmq_socket_t;
typedef void *zmq_context_t;

void skip_message(zmq_socket_t sock);

#define Z_SEQ_INIT(name, sock) \
    zmq_msg_t name; \
    size_t name##_opt = 1; \
    size_t name##_len = sizeof(name##_opt); \
    zmq_socket_t name##_sock = (sock); \
    zmq_msg_init(&name);

#define Z_RECV_NEXT(name) \
    SNIMPL(zmq_recv((name##_sock), (&name), 0)); \
    SNIMPL(zmq_getsockopt((name##_sock), ZMQ_RCVMORE, &name##_opt, &name##_len)); \
    if(!name##_opt) goto name##_error;
    
#define Z_RECV_LAST(name) \
    SNIMPL(zmq_recv((name##_sock), (&name), 0)); \
    SNIMPL(zmq_getsockopt((name##_sock), ZMQ_RCVMORE, &name##_opt, &name##_len)); \
    if(name##_opt) goto name##_error;

#define Z_SEQ_FINISH(name) \
    SNIMPL(zmq_msg_close(&name));
    
#define Z_SEQ_ERROR(name) \
    SNIMPL(zmq_msg_close(&name)); \
    if(name##_opt) skip_message(name##_sock);
    
#endif // _H_ZUTILS
