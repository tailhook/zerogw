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
START_TIMEOUT = float(os.environ.get("ZEROGW_START_TIMEOUT", 0.2))

ZEROGW_BINARY=builddir + "/zerogw"
CONFIG="./test/zerogw.yaml"

ECHO_SOCKET = "ipc:///tmp/zerogw-test-echo"
ECHO2_SOCKET = "ipc:///tmp/zerogw-test-echo2"
ECHOIP_SOCKET = "ipc:///tmp/zerogw-test-echo_ip"
CHAT_FW = "ipc:///tmp/zerogw-test-chatfw"
CHAT_SOCK = "ipc:///tmp/zerogw-test-chat"
MINIGAME = "ipc:///tmp/zerogw-test-minigame"

HTTP_ADDR = "/tmp/zerogw-test"
STATUS_ADDR = "ipc:///tmp/zerogw-test-status"
CONTROL_ADDR = "ipc:///tmp/zerogw-test-control"


def stop_process(proc):
    if proc.poll() is None:
        proc.terminate()
    proc.wait()


class TimeoutError(Exception):
    pass


class Base(unittest.TestCase):
    config = CONFIG

    def setUp(self):
        self.do_init()
        time.sleep(START_TIMEOUT)

    def do_init(self):
        self.proc = subprocess.Popen([ZEROGW_BINARY, '-c', self.config])
        self.addCleanup(stop_process, self.proc)

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

    def websock(self, **kw):
        return CheckingWebsock(self, **kw)


class HTTP(Base):
    timeout = 2

    def do_init(self):
        super().do_init()
        self.zmq = zmq.Context(1)
        self.addCleanup(self.zmq.term)
        self.echo = self.zmq.socket(zmq.REP)
        self.addCleanup(self.echo.close)
        self.echo.connect(ECHO_SOCKET)
        self.echo2 = self.zmq.socket(zmq.REP)
        self.addCleanup(self.echo2.close)
        self.echo2.connect(ECHO2_SOCKET)
        self.echo_ip = self.zmq.socket(zmq.REP)
        self.addCleanup(self.echo_ip.close)
        self.echo_ip.connect(ECHOIP_SOCKET)

    def backend_send(self, *args, backend='echo'):
        sock = getattr(self, backend)
        self.assertEqual(([], [sock], []),
            zmq.select([], [sock], [], timeout=self.timeout))
        sock.send_multipart([
            a if isinstance(a, bytes) else a.encode('utf-8')
            for a in args], zmq.NOBLOCK)

    def backend_recv(self, backend='echo'):
        sock = getattr(self, backend)
        if (([sock], [], []) !=
            zmq.select([sock], [], [], timeout=self.timeout)):
            raise TimeoutError()
        val = sock.recv_multipart(zmq.NOBLOCK)
        return val

    def testHttp(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/crossdomain.xml')
        resp = conn.getresponse()
        self.assertTrue(b'cross-domain-policy' in resp.read())
        self.assertTrue(resp.headers['Date'])

    def testEcho(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/echo/test')
        self.assertEqual(self.backend_recv(),
                          [b'/echo/test', b''])
        self.backend_send(b'hello')
        resp = conn.getresponse()
        self.assertEqual(b'hello', resp.read())
        self.assertEqual(resp.headers['Content-Type'], None)
        self.assertTrue(resp.headers['Date'])

    def testHeadersEcho(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/echo/test')
        self.assertEqual(self.backend_recv(),
                          [b'/echo/test', b''])
        self.backend_send(b'200 OK', 'Content-Type\0text/plain\0', b'hello')
        resp = conn.getresponse()
        self.assertEqual(b'hello', resp.read())
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')

    def testExtraHeaders(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/echo2/test')
        self.assertEqual(self.backend_recv('echo2'),
                          [b'GET', b'/echo2/test'])
        self.backend_send(b'hellohello', backend='echo2')
        resp = conn.getresponse()
        self.assertEqual(b'hellohello', resp.read())
        self.assertEqual(resp.headers['Cache-Control'], 'no-cache')
        self.assertEqual(resp.headers['Content-Type'], None)

    def testMoreHeaders(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/echo2/test')
        self.assertEqual(self.backend_recv('echo2'),
                          [b'GET', b'/echo2/test'])
        self.backend_send(b'200 OK', 'Content-Type\0text/plain\0', b'test2',
                          backend='echo2')
        resp = conn.getresponse()
        self.assertEqual(b'test2', resp.read())
        self.assertEqual(resp.headers['Cache-Control'], 'no-cache')
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')

    def testIPUnix(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/echo_ip')
        self.assertEqual(self.backend_recv('echo_ip'), [b''])
        self.backend_send(b'hello', backend='echo_ip')
        resp = conn.getresponse()
        self.assertEqual(b'hello', resp.read())

    def testIP(self):
        conn = http.HTTPConnection('localhost', 6941, timeout=1.0)
        conn.request('GET', '/echo_ip')
        self.assertEqual(self.backend_recv('echo_ip'), [b'127.0.0.1'])
        self.backend_send(b'hello', backend='echo_ip')
        resp = conn.getresponse()
        self.assertEqual(b'hello', resp.read())

class WebSocket(Base):

    def testConnAndClose(self):
        conn = self.http()
        self.addCleanup(conn.close)
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

class CheckingWebsock(object):

    def __init__(self, testcase, timestamp=False, prefix='/chat'):
        self.testcase = testcase
        self.http = testcase.http()
        self.ack = ''
        self.timestamp = timestamp
        self.pref = prefix

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

    def connect_only(self):
        self._request('GET', self.pref+'?action=CONNECT')
        req = self._getresponse()
        self.id = req.read().decode('utf-8')

    def connect(self):
        self.connect_only()
        val = self.testcase.backend_recv()
        self.testcase.assertEqual(val[1], b'connect')
        self.intid = val[0]

    def client_send(self, body, name=None):
        self.client_send_only(body, name=name)
        self.client_send_check(body, name=name)

    def client_send_only(self, body, name=None):
        self._request("GET",
            self.pref + '?limit=0&timeout=0&id=' + self.id, body=body)
        self.testcase.assertEqual(b'',
            self._getresponse().read())

    def client_send_check(self, body, name=None):
        self.testcase.assertEqual(self.testcase.backend_recv(name),
            [self.intid, b'message', body.encode('utf-8')])

    def client_send2(self, body, name=None):
        self.http.request("GET",
            self.pref + '?limit=0&timeout=0&id=' + self.id, body=body)
        self.testcase.assertEqual(b'',
            self._getresponse().read())
        self.testcase.assertEqual(self.testcase.backend_recv(name),
            [self.intid, b'msgfrom',
                self.cookie.encode('utf-8'),
                body.encode('utf-8')])

    def client_read(self):
        self.http.request("GET",
            self.pref + '?limit=1&timeout=1.0&ack='+self.ack+'&id=' + self.id)
        resp = self.http.getresponse()
        self.ack = resp.getheader('X-Message-ID')
        return resp.read()

    def client_got(self, body):
        body = body.encode('utf-8')
        rbody = self.client_read()
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

    def close_dropped(self):
        self.http.request('GET', self.pref + '?action=CLOSE&id=' + self.id)
        with self.testcase.assertRaises(http.BadStatusLine):
            resp = self.http.getresponse()
        self.http.close()
        val = self.testcase.backend_recv()
        if hasattr(self, 'cookie'):
            self.testcase.assertEqual(val,
                [self.intid, b'disconnect', self.cookie.encode('ascii')])
        else:
            self.testcase.assertEqual(val,
                [self.intid, b'disconnect'])

    def close(self):
        self.http.request('GET', self.pref + '?action=CLOSE&id=' + self.id)
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
    timeout = 2  # in zmq.select units (seconds)

    def do_init(self):
        self.zmq = zmq.Context(1)
        self.addCleanup(self.zmq.term)
        super().do_init()
        self.chatfw = self.zmq.socket(zmq.PULL)
        self.chatfw.connect(CHAT_FW)
        self.addCleanup(self.chatfw.close)
        self.chatout = self.zmq.socket(zmq.PUB)
        self.chatout.connect(CHAT_SOCK)
        self.addCleanup(self.chatout.close)
        self.minigame = self.zmq.socket(zmq.PULL)
        self.minigame.connect(MINIGAME)
        self.addCleanup(self.minigame.close)
        time.sleep(START_TIMEOUT)  # sorry, need for zmq sockets

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

    def testDupSubCrash(self):
        ws = self.websock()
        ws.connect()
        ws.client_send('hello_world')
        ws.subscribe('chat')
        ws.subscribe('chat')
        for i in range(20):
            self.backend_send('publish', 'chat', 'message: hello_world')
        ws.close_dropped()
        ws1 = self.websock()
        ws1.connect()
        ws1.subscribe('chat')
        self.backend_send('publish', 'chat', 'message: hello_world')
        ws1.client_got('message: hello_world')
        ws1.close()

    def testDupSubscribe(self):
        ws = self.websock()
        ws.connect()
        ws.client_send('hello_world')
        ws.subscribe('chat')
        ws.subscribe('chat')
        self.backend_send('publish', 'chat', 'message: hello_world')
        ws.client_got('message: hello_world')
        self.backend_send('publish', 'chat', 'message: hello_world1')
        ws.client_got('message: hello_world1')
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

    def testClone(self):
        ws1 = self.websock()
        ws2 = self.websock()
        ws3 = self.websock()
        ws1.connect()
        ws2.connect()
        ws3.connect()
        ws1.subscribe('chat')
        ws2.subscribe('chat')
        ws2.subscribe('game')
        ws3.subscribe('game')
        self.backend_send('publish', 'chat', 'message: hello_world')
        ws1.client_got('message: hello_world')
        ws2.client_got('message: hello_world')
        self.backend_send('clone', 'chat', 'chat2')
        self.backend_send('publish', 'chat2', 'message: hello_chat2')
        ws1.client_got('message: hello_chat2')
        ws2.client_got('message: hello_chat2')
        self.backend_send('publish', 'game', 'message: hello_game')
        ws2.client_got('message: hello_game')
        ws3.client_got('message: hello_game')
        ws1.close()
        ws2.close()
        ws3.close()

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

    def checkSync(self, data, connections):
        it = iter(data)
        zgw_id = next(it)
        self.assertEqual(next(it), b'sync')
        self.assertEqual(set(zip(it, it)),
            set((c.intid, getattr(c, 'cookie', '').encode('ascii'))
                for c in connections))

    def testSync(self):
        wa = self.websock()
        wb = self.websock()
        wa.connect()
        wb.connect()
        wa.set_cookie('u1')
        wa.add_output('game1', 'minigame')
        wb.add_output('game2', 'minigame')
        time.sleep(0.1)
        self.assertEqual(self.control('sync_now'), [b'sync_sent'])
        time.sleep(0.1)
        self.checkSync(self.backend_recv('minigame'), [wa, wb])
        wa.del_output('game1')
        time.sleep(0.1)
        self.assertEqual(self.control('sync_now'), [b'sync_sent'])
        time.sleep(0.1)
        self.checkSync(self.backend_recv('minigame'), [wb])
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
        self.addCleanup(ws.http.close)
        ws.connect()
        self.backend_send('disconnect', ws.intid)
        time.sleep(0.1)
        self.backend_send('publish', 'hello', 'world')
        # sorry, zerogw lacks user-friendly errors on websock errors
        with self.assertRaisesRegex(http.BadStatusLine, "''"):
            ws.client_send('hello_world')

    def testLateDisconnect(self):
        ws = self.websock()
        ws.connect()
        ws.close()
        self.backend_send('disconnect', ws.intid)
        ws = self.websock()
        self.addCleanup(ws.http.close)
        ws.connect()
        self.backend_send('publish', 'hello', 'world')

    def testDisconnectBackendMsg(self):
        ws = self.websock()
        self.addCleanup(ws.http.close)
        ws.connect()
        self.backend_send('add_output', ws.intid, 'test', 'minigame')
        self.backend_send('disconnect', ws.intid)
        self.assertEqual([ws.intid, b'disconnect'],
            self.backend_recv('minigame'))

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
        self.addCleanup(self.chatfw.close)
        ws1.client_send_check('test1')
        ws1.client_send('ok')
        ws1.close()

    def testPauseWebsockets(self):
        ws1 = self.websock()
        ws1.connect()
        ws1.subscribe('chat')
        ws1.client_send('hello_world')
        self.control('pause_websockets')
        ws1.client_got('ZEROGW:paused')
        time.sleep(0.1)
        ws1.client_send_only('test1')
        with self.assertRaises(TimeoutError):
            ws1.client_send_check('test1')
        self.control('resume_websockets')
        ws1.client_got('ZEROGW:resumed')
        ws1.client_send_check('test1')
        ws1.client_send('ok')
        ws1.close()

    def testPauseNamed(self):
        ws1 = self.websock()
        ws1.connect()
        ws1.subscribe('chat')
        self.backend_send('add_output', ws1.intid, 'hello-', 'minigame')
        self.control('pause_websockets')
        ws1.client_got('ZEROGW:paused')
        time.sleep(0.1)
        ws1.client_send_only('hello-test1')
        with self.assertRaises(TimeoutError):
            ws1.client_send_check('hello-test1', 'minigame')
        self.control('resume_websockets')
        ws1.client_got('ZEROGW:resumed')
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
