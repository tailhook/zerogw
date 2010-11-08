import zmq

ctx = zmq.Context(1)
sock = ctx.socket(zmq.REP)
sock.connect('ipc:///tmp/echo')
while True:
    body = sock.recv()
    sock.send(b"Echo: " + body)
