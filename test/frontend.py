import json
import time

from .simple import Base

class Frontend(Base):

    def testEcho(self):
        ws = self.websock()
        self.addCleanup(ws.http.close)
        ws.connect_only()
        ws.client_send_only("ZEROGW:echo:text1")
        ws.client_got("ZEROGW:echo:text1")

    def testTime(self):
        ws = self.websock()
        self.addCleanup(ws.http.close)
        ws.connect_only()
        ws.client_send_only("ZEROGW:timestamp:text2")
        msg = ws.client_read().decode('ascii')
        self.assertTrue(msg.startswith('ZEROGW:timestamp:text2:'))
        self.assertAlmostEqual(float(msg.rsplit(':')[-1]), time.time(), 2)

