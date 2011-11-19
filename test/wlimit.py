import zmq
import unittest
from http import client as http

from .simple import Base, TimeoutError

CONFIG='test/wlimit.yaml'
CHAT_FW = "ipc:///tmp/zerogw-test-chatfw"

class Wlimit(Base):
    timeout = 2  # in zmq.select units (seconds)
    config = CONFIG

    def setUp(self):
        self.zmq = zmq.Context(1)
        self.addCleanup(self.zmq.term)
        super().setUp()
        self.chatfw = self.zmq.socket(zmq.PULL)
        self.addCleanup(self.chatfw.close)
        self.chatfw.connect(CHAT_FW)

    def backend_recv(self, backend=None):
        if backend is None:
            sock = self.chatfw
        else:
            sock = self.minigame
        if (([sock], [], []) !=
            zmq.select([sock], [], [], timeout=self.timeout)):
            raise TimeoutError()
        val =  sock.recv_multipart()
        if val[1] == b'heartbeat':
            return self.backend_recv(backend=backend)
        return val

    def testWorking(self):
        ws1 = self.websock()
        ws1.connect()
        ws1.client_send('hello1') # checks backend delivery itself
        ws2 = self.websock()
        ws2.connect()
        ws2.client_send('hello2')
        ws1.client_send('hello3')
        ws3 = self.websock()
        ws3.connect()
        ws3.client_send('hello1') # checks backend delivery itself
        ws4 = self.websock()
        ws4.connect()
        ws4.client_send('hello2')
        ws1.close()
        ws5 = self.websock()
        ws5.connect()
        ws5.client_send("hello4")
        ws2.client_send("fifth_hello")
        ws2.close()
        ws3.close()
        ws4.close()
        ws5.close()

    def testNoMoreSlots(self):
        ws1 = self.websock()
        ws1.connect()
        self.addCleanup(ws1.close)
        ws1.client_send('hello1') # checks backend delivery itself
        ws2 = self.websock()
        ws2.connect()
        self.addCleanup(ws2.close)
        ws2.client_send('hello2')
        ws1.client_send('hello3')
        ws3 = self.websock()
        ws3.connect()
        self.addCleanup(ws3.close)
        ws3.client_send('hello1') # checks backend delivery itself
        ws4 = self.websock()
        ws4.connect()
        self.addCleanup(ws4.close)
        ws4.client_send('hello2')
        ws5 = self.websock()
        with self.assertRaisesRegex(http.BadStatusLine, "''"):
            ws5.connect()
        self.addCleanup(ws5.http.close)
        ws2.client_send("fifth_hello")



if __name__ == '__main__':
    unittest.main()
