from .simple import Base

class Static(Base):

    def testJustFile(self):
        conn = self.http()
        conn.request('GET', '/test/testfile.txt')
        txt = conn.getresponse().read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')

    def testQuery(self):
        conn = self.http()
        conn.request('GET', '/test/testfile.txt?justquery')
        txt = conn.getresponse().read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')
        conn.request('GET', '/test/testfile.txt?another=query&var2=1')
        txt = conn.getresponse().read().decode('ascii')
        self.assertEqual(txt, 'test\nfile\n')

    def testDenied(self):
        conn = self.http()
        conn.request('GET', '/test/static.py')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 403)
        self.assertTrue('403 Forbidden' in resp.read().decode('ascii'))

    def testDeniedNotexist(self):
        conn = self.http()
        conn.request('GET', '/test/something_not_esistent.py')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 403)
        self.assertTrue('403 Forbidden' in resp.read().decode('ascii'))

    def testDeniedHidden(self):
        conn = self.http()
        conn.request('GET', '/test/.hidden')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 403)
        self.assertTrue('403 Forbidden' in resp.read().decode('ascii'))

    def testNotFoundHidden(self):
        conn = self.http()
        conn.request('GET', '/.hidden')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 404)
        self.assertTrue('404 Page Not Found' in resp.read().decode('ascii'))

    def testNotFound(self):
        conn = self.http()
        conn.request('GET', '/test/nothing_special')
        resp =  conn.getresponse()
        self.assertEqual(resp.code, 404)
        self.assertTrue('404 Page Not Found' in resp.read().decode('ascii'))


if __name__ == '__main__':
    import unittest
    unittest.main()
