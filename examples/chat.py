import zmq

ctx = zmq.Context(1)
sub = ctx.socket(zmq.SUB)
sub.connect('tcp://127.0.0.1:7002')
sub.setsockopt(zmq.SUBSCRIBE, b"")
pub = ctx.socket(zmq.PUB)
pub.connect('tcp://127.0.0.1:7003')
while True:
    parts = sub.recv_multipart()
    print("GOT", parts)
    if parts[1] == b'connect':
        pub.send(b'subscribe', zmq.SNDMORE)
        pub.send(parts[0], zmq.SNDMORE)
        pub.send(parts[0])
        
        pub.send(b'subscribe', zmq.SNDMORE)
        pub.send(parts[0], zmq.SNDMORE)
        pub.send(b'room:default')
        
        pub.send(b'subscribe', zmq.SNDMORE)
        pub.send(parts[0], zmq.SNDMORE)
        pub.send(b'room:default:joins')
        
        pub.send(b'publish', zmq.SNDMORE)
        pub.send(b'room:default:joins', zmq.SNDMORE)
        pub.send(b'User joined room')
        
    elif parts[1] == b'message':
        if parts[2] == b'no_spam':
            pub.send(b'unsubscribe', zmq.SNDMORE)
            pub.send(parts[0], zmq.SNDMORE)
            pub.send(b'room:default:joins')
        else:
            pub.send(b'publish', zmq.SNDMORE)
            pub.send(b'room:default', zmq.SNDMORE)
            pub.send(parts[2])
    elif parts[1] == b'disconnect':
        pass # will be unsubscribed automatically
