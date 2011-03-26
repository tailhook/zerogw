from http import client as http
import socket
import subprocess
import unittest
import time
import os

import zmq

ZEROGW_BINARY="./build/zerogw"
CONFIG="./test/zerogw.yaml"

ECHO_SOCKET = "ipc:///tmp/zerogw-test-echo"
CHAT_FW = "ipc:///tmp/zerogw-test-chatfw"
CHAT_SOCK = "ipc:///tmp/zerogw-test-chat"
MINIGAME = "ipc:///tmp/zerogw-test-minigame"

HTTP_ADDR = "/tmp/zerogw-test"
STATUS_ADDR = "ipc:///tmp/zerogw-test-status"

class Plain(unittest.TestCase):

    def setUp(self):
        for i in (HTTP_ADDR,):
            try:
                os.unlink(i)
            except OSError:
                pass
        self.proc = subprocess.Popen([ZEROGW_BINARY, '-c', CONFIG])
    
    def http(self):
        conn = http.HTTPConnection('localhost')
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

    def testHttp(self):
        conn = self.http()
        conn.request('GET', '/crossdomain.xml')
        resp = conn.getresponse()
        self.assertTrue('cross-domain-policy', resp.read())
        conn.close()

class Chat(Plain):

    def setUp(self):
        self.zmq = zmq.Context(1)
        super().setUp()
        self.chatfw = self.zmq.socket(zmq.PULL)
        #self.chatfw.connect(CHAT_FW)
        self.chatout = self.zmq.socket(zmq.PUB)
        #self.chatout.connect(CHAT_SOCK)

    def tearDown(self):
        super().tearDown()

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

if __name__ == '__main__':
    unittest.main()
