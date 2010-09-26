import zmq

ctx = zmq.Context(1)
sock = ctx.socket(zmq.REP)
sock.connect('ipc:///tmp/chat')
while True:
    cookie = sock.recv()
    body = sock.recv()
    sock.send(b"Hello from chat!!!")
