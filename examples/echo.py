import zmq

ctx = zmq.Context(1)
sock = ctx.socket(zmq.REP)
sock.connect('tcp://127.0.0.1:7001')
while True:
    body = sock.recv()
    sock.send(b"Echo: " + body)
