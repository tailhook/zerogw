import zmq
import random

ctx = zmq.Context(1)
sock = ctx.socket(zmq.REP)
sock.connect('tcp://127.0.0.1:7001')
while True:
    parts = sock.recv_multipart()
    print("GOT", parts)
    if random.randrange(3) == 0:
        import os, signal
        os.kill(os.getpid(), signal.SIGKILL)
    sock.send(b"Echo: " + b' '.join(parts))
