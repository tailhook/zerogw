import os
import gzip
from .simple import Base

class Static(Base):

    def testJustFile(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/testfile.txt')
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        self.assertTrue(resp.headers['Date'])
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')

    def testIfModifiedSince(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/testfile.txt')
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        lm = resp.headers['Last-Modified']
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')
        conn.request('GET', '/test/testfile.txt',
            headers={'If-Modified-Since': lm})
        resp = conn.getresponse()
        self.assertEqual(resp.code, 304)
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, '')
        os.utime("test/testfile.txt", None)
        conn.request('GET', '/test/testfile.txt',
            headers={'If-Modified-Since': lm})
        resp = conn.getresponse()
        lm1 = resp.headers['Last-Modified']
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')
        self.assertNotEqual(lm, lm1)

    def testGzipped(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/testfile.txt', headers={
            'Accept-Encoding': 'gzip',
            })
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        self.assertEqual(resp.headers['Content-Encoding'], 'gzip')
        self.assertTrue(resp.headers['Date'])
        txt = gzip.decompress(resp.read()).decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')

    def testNoGzipped(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/test_nogz.txt', headers={
            'Accept-Encoding': 'gzip',
            })
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        self.assertEqual(resp.headers['Content-Encoding'], None)
        self.assertTrue(resp.headers['Date'])
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'NO GZIP\n')

    def testQuery(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/testfile.txt?justquery')
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')
        conn.request('GET', '/test/testfile.txt?another=query&var2=1')
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')
        conn.request('GET', '/test/testfile.txt?strange.query')
        resp = conn.getresponse()
        self.assertEqual(resp.headers['Content-Type'], 'text/plain')
        txt = resp.read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')

    def testDenied(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/static.py')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 403)
        self.assertTrue('403 Forbidden' in resp.read().decode('ascii'))

    def testDeniedNotexist(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/something_not_esistent.py')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 403)
        self.assertTrue('403 Forbidden' in resp.read().decode('ascii'))

    def testDeniedHidden(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/.hidden')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 403)
        self.assertTrue('403 Forbidden' in resp.read().decode('ascii'))

    def testNotFoundHidden(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/.hidden')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 404)
        self.assertTrue('404 Page Not Found' in resp.read().decode('ascii'))

    def testNotFound(self):
        conn = self.http()
        self.addCleanup(conn.close)
        conn.request('GET', '/test/nothing_special')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 404)
        self.assertTrue('404 Page Not Found' in resp.read().decode('ascii'))


if __name__ == '__main__':
    import unittest
    unittest.main()
