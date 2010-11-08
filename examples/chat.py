import zmq

ctx = zmq.Context(1)
sub = ctx.socket(zmq.SUB)
sub.connect('ipc:///tmp/echo-input')
pub = ctx.socket(zmq.PUB)
pub.connect('ipc:///tmp/echo-output')
while True:
    parts = sub.recv_multipart()
    print("GOT", parts)
    if parts[1] == b'connect':
        pub.send(b'\x00subscribe')
        pub.send(parts[0], zmq.SNDMORE)
        pub.send(parts[0])
    elif parts[1] == b'message':
        pub.send(parts[0], zmq.SNDMORE)
        pub.send(parts[2])
    elif parts[1] == b'disconnect':
        pass # will be unsubscribed automatically
