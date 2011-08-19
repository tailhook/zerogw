from http import client as http
import socket
import subprocess
import unittest
import time
import os
import signal
import datetime

import zmq

builddir = os.environ.get("BUILDDIR", './build')

ZEROGW_BINARY=builddir + "/zerogw"
CONFIG="./test/zerogw.yaml"

ECHO_SOCKET = "ipc:///tmp/zerogw-test-echo"
CHAT_FW = "ipc:///tmp/zerogw-test-chatfw"
CHAT_SOCK = "ipc:///tmp/zerogw-test-chat"
MINIGAME = "ipc:///tmp/zerogw-test-minigame"

HTTP_ADDR = "/tmp/zerogw-test"
STATUS_ADDR = "ipc:///tmp/zerogw-test-status"
CONTROL_ADDR = "ipc:///tmp/zerogw-test-control"

class TimeoutError(Exception):
    pass

class Base(unittest.TestCase):
    config = CONFIG

    def setUp(self):
        for i in (HTTP_ADDR,):
            try:
                os.unlink(i)
            except OSError:
                pass
        self.proc = subprocess.Popen([ZEROGW_BINARY, '-c', self.config])

    def http(self, host='localhost'):
        conn = http.HTTPConnection(host, timeout=1.0)
        conn.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        for i in range(100):
            try:
                conn.sock.connect(HTTP_ADDR)
            except socket.error:
                time.sleep(0.1)
                continue
            else:
                break
        else:
            raise RuntimeError("Can't connect to zerogw")
        return conn

    def tearDown(self):
        self.proc.terminate()
        self.proc.wait()

class HTTP(Base):

    def testHttp(self):
        conn = self.http()
        conn.request('GET', '/crossdomain.xml')
        resp = conn.getresponse()
        self.assertTrue(b'cross-domain-policy' in resp.read())
        conn.close()

class WebSocket(Base):

    def testConnAndClose(self):
        conn = self.http()
        conn.request('GET', '/chat?action=CONNECT')
        id = conn.getresponse().read().decode('ascii')
        self.assertTrue(id)
        conn.request('GET', '/chat?timeout=0&id='+id)
        resp = conn.getresponse()
        self.assertEqual(resp.getheader('X-Messages'), '0')
        self.assertEqual(resp.read(), b'')
        conn.request('GET', '/chat?action=CLOSE&id=' + id)
        resp = conn.getresponse()
        self.assertEqual(resp.getheader('X-Connection'), 'close')
        conn.close()

class CheckingWebsock(object):

    def __init__(self, testcase, timestamp=False):
        self.testcase = testcase
        self.http = testcase.http()
        self.ack = ''
        self.timestamp = timestamp

    def _request(self, *args, **kw):
        self.last_request = time.time()
        return self.http.request(*args, **kw)

    def _getresponse(self):
        resp = self.http.getresponse()
        if self.timestamp:
            resptime = time.time()
            self.last_timestamp = float(resp.headers['X-Timestamp'])
            assert self.last_request < self.last_timestamp < resptime
        return resp

    def connect(self):
        self._request('GET', '/chat?action=CONNECT')
        req = self._getresponse()
        self.id = req.read().decode('utf-8')
        val = self.testcase.backend_recv()
        self.testcase.assertEqual(val[1], b'connect')
        self.intid = val[0]

    def client_send(self, body, name=None):
        self.client_send_only(body, name=name)
        self.client_send_check(body, name=name)

    def client_send_only(self, body, name=None):
        self._request("GET",
            '/chat?limit=0&timeout=0&id=' + self.id, body=body)
        self.testcase.assertEqual(b'',
            self._getresponse().read())

    def client_send_check(self, body, name=None):
        self.testcase.assertEqual(self.testcase.backend_recv(name),
            [self.intid, b'message', body.encode('utf-8')])

    def client_send2(self, body, name=None):
        self.http.request("GET",
            '/chat?limit=0&timeout=0&id=' + self.id, body=body)
        self.testcase.assertEqual(b'',
            self._getresponse().read())
        self.testcase.assertEqual(self.testcase.backend_recv(name),
            [self.intid, b'msgfrom', self.cookie.encode('utf-8'), body.encode('utf-8')])

    def client_got(self, body):
        body = body.encode('utf-8')
        self.http.request("GET",
            '/chat?limit=1&timeout=1.0&ack='+self.ack+'&id=' + self.id)
        resp = self.http.getresponse()
        self.ack = resp.getheader('X-Message-ID')
        rbody = resp.read()
        self.testcase.assertEqual(rbody, body)

    def set_cookie(self, cookie):
        self.cookie = cookie
        self.testcase.backend_send(
            'set_cookie', self.intid, cookie)

    def subscribe(self, topic):
        self.testcase.backend_send(
            'subscribe', self.intid, topic)

    def add_output(self, prefix, name):
        self.testcase.backend_send(
            'add_output', self.intid, prefix, name)

    def unsubscribe(self, topic):
        self.testcase.backend_send(
            'unsubscribe', self.intid, topic)

    def del_output(self, name):
        self.testcase.backend_send(
            'del_output', self.intid, name)

    def close(self):
        self.http.request('GET', '/chat?action=CLOSE&id=' + self.id)
        resp = self.http.getresponse()
        self.testcase.assertEqual(resp.getheader('X-Connection'), 'close')
        self.http.close()
        val = self.testcase.backend_recv()
        if hasattr(self, 'cookie'):
            self.testcase.assertEqual(val,
                [self.intid, b'disconnect', self.cookie.encode('ascii')])
        else:
            self.testcase.assertEqual(val,
                [self.intid, b'disconnect'])

class Chat(Base):
    timeout = 1  # in zmq.select units (seconds)

    def setUp(self):
        self.zmq = zmq.Context(1)
        super().setUp()
        self.chatfw = self.zmq.socket(zmq.PULL)
        self.chatfw.connect(CHAT_FW)
        self.chatout = self.zmq.socket(zmq.PUB)
        self.chatout.connect(CHAT_SOCK)
        self.minigame = self.zmq.socket(zmq.PULL)
        self.minigame.connect(MINIGAME)
        time.sleep(0.2)  # sorry, need for zmq sockets

    def control(self, *args):
        sock = self.zmq.socket(zmq.REQ)
        try:
            sock.connect(CONTROL_ADDR)
            sock.send_multipart([a.encode('utf-8') for a in args])
            self.assertEqual(([sock], [], []),
                zmq.select([sock], [], [], timeout=self.timeout))
            return sock.recv_multipart()
        finally:
            sock.close()

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
            sock = self.minigame
        if (([sock], [], []) !=
            zmq.select([sock], [], [], timeout=self.timeout)):
            raise TimeoutError()
        val =  sock.recv_multipart()
        if val[1] == b'heartbeat':
            return self.backend_recv(backend=backend)
        return val

    def websock(self, **kw):
        return CheckingWebsock(self, **kw)

    def tearDown(self):
        self.chatout.close()
        self.chatfw.close()
        self.minigame.close()
        super().tearDown()
        self.zmq.term()

    def testSimple(self):
        ws = self.websock()
        ws.connect()
        ws.client_send('hello_world')
        ws.subscribe('chat')
        self.backend_send('publish', 'chat', 'message: hello_world')
        ws.client_got('message: hello_world')
        self.backend_send('publish', 'chat', 'message: another_hello')
        ws.client_got('message: another_hello')
        ws.unsubscribe('chat')
        self.backend_send('publish', 'chat', 'message: silent')
        self.backend_send('send', ws.intid, 'message: personal')
        ws.client_got('message: personal')
        ws.client_send('hurray!')
        ws.close()

    def testTwoUsers(self):
        ws1 = self.websock()
        ws2 = self.websock()
        ws1.connect()
        ws2.connect()
        ws1.subscribe('chat')
        ws2.subscribe('chat')
        self.backend_send('publish', 'chat', 'message: hello_world')
        ws1.client_got('message: hello_world')
        ws2.client_got('message: hello_world')
        ws1.close()
        ws2.close()

    def testNamed(self):
        wa = self.websock()
        wb = self.websock()
        wa.connect()
        wb.connect()
        wa.subscribe('chat')
        wb.subscribe('chat')
        wa.add_output('game1', 'minigame')
        wb.add_output('game2', 'minigame')
        wa.client_send('hello_world')
        wb.client_send('hello_world')
        wb.client_send('game1_action')
        wa.client_send('game1_action', 'game')
        wb.client_send('game2_action', 'game')
        wa.client_send('game2_action')
        wa.del_output('game1')
        time.sleep(0.1)  # waiting for message to be delivered
        wa.client_send('game1_action')
        wa.close()
        wb.close()

    def testCookie(self):
        ws1 = self.websock()
        ws2 = self.websock()
        ws1.connect()
        ws2.connect()
        ws1.subscribe('chat')
        ws2.subscribe('chat')
        ws1.set_cookie('u1')
        ws2.set_cookie('u2')
        time.sleep(0.01)
        ws1.client_send2('hello utwo!')  #checks cookie internally
        ws2.client_send2('hello uone!')
        ws1.set_cookie('u3')
        time.sleep(0.01)
        ws1.client_send2('i am uthree!')
        ws1.close()
        ws2.close()

    def testCrash(self):
        ws = self.websock()
        ws.connect()
        self.proc.send_signal(signal.SIGSTOP)
        self.backend_send('add_output', ws.intid, 'hello-', 'test')
        self.backend_send('publish', 'hello', 'world')
        self.proc.send_signal(signal.SIGCONT)
        ws.client_send('hello_world')
        ws.close()

    def testDisconnect(self):
        ws = self.websock()
        ws.connect()
        self.backend_send('disconnect', ws.intid)
        self.backend_send('publish', 'hello', 'world')
        # sorry, zerogw lacks user-friendly errors on websock errors
        with self.assertRaisesRegex(http.BadStatusLine, "''"):
            ws.client_send('hello_world')

    def testBackendStop(self):
        ws1 = self.websock()
        ws1.connect()
        ws1.subscribe('chat')
        ws1.client_send('hello_world')
        self.chatfw.close()
        time.sleep(0.1)
        ws1.client_send_only('test1')
        time.sleep(0.1)
        self.chatfw = self.zmq.socket(zmq.PULL)
        self.chatfw.connect(CHAT_FW)
        ws1.client_send_check('test1')
        ws1.client_send('ok')
        ws1.close()

    def testPauseWebsockets(self):
        ws1 = self.websock()
        ws1.connect()
        ws1.subscribe('chat')
        ws1.client_send('hello_world')
        self.control('pause_websockets')
        time.sleep(0.1)
        ws1.client_send_only('test1')
        with self.assertRaises(TimeoutError):
            ws1.client_send_check('test1')
        self.control('resume_websockets')
        ws1.client_send_check('test1')
        ws1.client_send('ok')
        ws1.close()

    def testPauseNamed(self):
        ws1 = self.websock()
        ws1.connect()
        ws1.subscribe('chat')
        self.backend_send('add_output', ws1.intid, 'hello-', 'minigame')
        self.control('pause_websockets')
        time.sleep(0.1)
        ws1.client_send_only('hello-test1')
        with self.assertRaises(TimeoutError):
            ws1.client_send_check('hello-test1', 'minigame')
        self.control('resume_websockets')
        ws1.client_send_check('hello-test1', 'minigame')
        ws1.client_send('ok')
        ws1.close()

    def testTimestamp(self):
        ws1 = self.websock(timestamp=True)
        ws1.connect()
        ws1.subscribe('chat')
        ws1.client_send('ok')
        self.assertAlmostEqual(ws1.last_timestamp, time.time(), 2)
        ws1.close()

if __name__ == '__main__':
    unittest.main()
