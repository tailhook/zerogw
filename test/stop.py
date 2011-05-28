from .simple import Base
import signal

class Kill(Base):

    def testJustkill(self):
        pass

    def testConnActive(self):
        self.h1 = self.http()

    def testHttpActive(self):
        self.h1 = self.http()
        self.h1.request('GET', '/test')

    def testPollingActive(self):
        conn = self.http()
        conn.request('GET', '/chat?action=CONNECT')
        id = conn.getresponse().read().decode('ascii')
        self.assertTrue(id)
        conn.request('GET', '/chat?timeout=0&id='+id)
        resp = conn.getresponse()
        self.c1 = conn  # to keep it alive

    def tearDown(self):
        self.proc.send_signal(signal.SIGINT)
        signal.alarm(12)  # a bit more than linger value
        self.proc.wait()
        signal.alarm(0)


if __name__ == '__main__':
    import unittest
    unittest.main()
