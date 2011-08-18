import json
import time

from .simple import Base

class Frontend(Base):

    def testEcho(self):
        ws = self.websock()
        ws.connect_only()
        ws.client_send_only("['frontend.ping', 'text1']")
        ws.client_got("['pong', 'text1']")

    def testTime(self):
        ws = self.websock()
        ws.connect_only()
        ws.client_send_only("['frontend.timestamp', 'text2']")
        msg = ws.client_read()
        data = json.loads(msg.decode('ascii'))
        self.assertEqual(data[0], 'timestamp')
        self.assertEqual(data[2], 'text2')
        self.assertAlmostEqual(data[1], time.time(), 2)

