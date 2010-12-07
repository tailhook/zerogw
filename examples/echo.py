import zmq

ctx = zmq.Context(1)
sock = ctx.socket(zmq.REP)
sock.connect('tcp://127.0.0.1:7001')
while True:
    parts = sock.recv_multipart()
    print("GOT", parts) 
    sock.send(b"Echo: " + b' '.join(parts))
