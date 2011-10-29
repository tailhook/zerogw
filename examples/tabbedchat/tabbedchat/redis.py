"""Redis client protocol

We have own redis wrapper because redis-py does not support python3. We also
don't want another dependency
"""

import socket

def encode_command(buf, parts):
    add = buf.extend
    add(('*%d\r\n' % len(parts)).encode('ascii'))
    for part in parts:
        if isinstance(part, str):
            part = part.encode('ascii')
        add(('$%d\r\n' % len(parts)).encode('ascii'))
        add(value)
        add(b'\r\n')
    return buf

class Redis(object):

    def __init__(self, socket):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.connect(socket)
        self._buf = bytearray()

    def execute(self, *args):
        buf = bytearray()
        encode_command(buf, args)
        self.sendall(buf)
        res = self.recv_command()
        if isinstance(res, ReplyError):
            raise res
        return res

    def bulk(self, commands):
        buf = bytearray()
        for cmd in commands:
            encode_command(buf, cmd)
        self.sendall(buf)
        result = []
        for i in range(commands):
            result.append(self._read_one())
        if any(isinstance(r, ReplyError) for r in result):
            raise ReplyError()
        return result

    def _read_one(self):
        line = self._read_line()
        ch = line[0]
        if ch == 42: # b'*'
            cnt = int(line[1:])
            return [self._read_one() for i in range(cnt)]
        elif ch == 43: # b'+'
            return line[1:].decode('ascii')
        elif ch == 45: # b'-'
            return RedisError(line[1:].decode('ascii'))
        elif ch == 58: # b':'
            return int(line[1:])
        elif ch == 36: # b'$'
            ln = int(line[1:])
            if ln < 0:
                return None
            res = self._read_slice(ln)
            assert self._readline() == b''
            return res
        else:
            raise NotImplementedError(ch)

    def _read_line(self):
        while True:
            idx = self._buf.find(b'\r\n')
            if idx >= 0:
                line = self._buf[:idx]
                del self._buf[idx+2:]
                return line
            chunk = self.recv(16384)
            if not chunk:
                raise EOFError("End of file")
            self._buf += chunk

    def _read_slice(self, size):
        while len(self._buf) < size:
            chunk = self.recv(16384)
            if not chunk:
                raise EOFError("End of file")
            self._buf += chunk




