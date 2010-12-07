import zmq
from binascii import hexlify

ctx = zmq.Context(1)
sock = ctx.socket(zmq.SUB)
sock.connect('ipc:///tmp/zerogw')
sock.setsockopt(zmq.SUBSCRIBE, b"")
while True:
    print("Status update for", hexlify(sock.recv()).decode('ascii'))
    print(sock.recv().decode('ascii'))
