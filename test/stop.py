from .simple import Base

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

if __name__ == '__main__':
    import unittest
    unittest.main()
