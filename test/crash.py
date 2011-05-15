import unittest
from .simple import Base

CONFIG='test/crash1.yaml'

class CrashTest(Base):
    config = CONFIG

    def testBadRequest(self):
        conn = self.http()
        conn.request('GET', '/')
        resp = conn.getresponse()
        self.assertTrue(b'Not Found' in resp.read())
        conn.close()

    def testGoodRequest(self):
        conn = self.http('hello')
        conn.request('GET', '/')
        resp = conn.getresponse()
        self.assertTrue(b'Not Found' not in resp.read())
        conn.close()

if __name__ == '__main__':
    unittest.main()
