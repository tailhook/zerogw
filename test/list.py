import zmq
import time
import json
from .simple import (Base, CheckingWebsock, TimeoutError,
    CHAT_FW, CHAT_SOCK, START_TIMEOUT,
    )

class ListWebsock(CheckingWebsock):

    def read_list(self):
        self.http.request("GET", '/chat?limit=10&timeout=1.0&outfmt=jsonlist'
            '&ack='+self.ack+'&id=' + self.id)
        resp = self.http.getresponse()
        self.ack = resp.getheader('X-Last-ID')
        data = resp.read().decode('utf-8')
        if not data:
            res = []
        else:
            res = json.loads(data)
        assert len(res) == int(resp.getheader('X-Messages'))
        return res


class JsonList(Base):
    timeout = 1

    def do_init(self):
        self.zmq = zmq.Context(1)
        super().do_init()
        self.chatfw = self.zmq.socket(zmq.PULL)
        self.chatfw.connect(CHAT_FW)
        self.chatout = self.zmq.socket(zmq.PUB)
        self.chatout.connect(CHAT_SOCK)
        time.sleep(START_TIMEOUT)  # sorry, need for zmq sockets

    def backend_send(self, *args):
        self.assertEqual(([], [self.chatout], []),
            zmq.select([], [self.chatout], [], timeout=self.timeout))
        self.chatout.send_multipart([
            a if isinstance(a, bytes) else a.encode('utf-8')
            for a in args])

    def backend_recv(self, backend=None):
        if backend is None:
            sock = self.chatfw
        else:
            raise NotImplementedError(backend)
        if (([sock], [], []) !=
            zmq.select([sock], [], [], timeout=self.timeout)):
            raise TimeoutError()
        val =  sock.recv_multipart()
        if val[1] == b'heartbeat':
            return self.backend_recv(backend=backend)
        return val

    def websock(self, **kw):
        return ListWebsock(self, **kw)

    def testSimple(self):
        ws = self.websock()
        ws.connect()
        ws.subscribe('chat')
        self.backend_send('publish', 'chat', '["message: hello_world"]')
        self.backend_send('publish', 'chat', '["message: another_hello"]')
        self.assertEquals([["message: hello_world"],
                           ["message: another_hello"]],
            ws.read_list())
        self.backend_send('publish', 'chat', '["message: msg1"]')
        self.assertEquals([["message: msg1"]],
            ws.read_list())
        # mixing two styles
        self.backend_send('publish', 'chat', '["message: msg2"]')
        ws.client_got('["message: msg2"]')
        # frontend
        self.backend_send('publish', 'chat', '["message: msg3"]')
        ws.client_send_only("ZEROGW:echo:text1")
        self.assertEquals([["message: msg3"], "ZEROGW:echo:text1"],
            ws.read_list())
        ws.close()
