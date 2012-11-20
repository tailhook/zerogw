import unittest
from .simple import Base


class CrashTest(Base):
    config = 'test/crash1.yaml'

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


class CrashTest2(Base):
    config = 'test/crash2.yaml'

    def testBadRequest(self):
        conn = self.http()
        conn.request('GET', '/test?something', headers={
            'Host': 'example.com',
            'Upgrade': 'websocket',
            'Connection': 'Upgrade',
            'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
            'Sec-WebSocket-Origin': 'http://example.com',
            'Sec-WebSocket-Version': '13',
            })
        resp = conn.getresponse()
        assert resp.code == 101, resp.code
        conn.close()

    def testGoodRequest(self):
        conn = self.http('hello')
        conn.request('GET', '/')
        resp = conn.getresponse()
        self.assertTrue(b'Not Found' in resp.read())
        conn.close()


if __name__ == '__main__':
    unittest.main()
