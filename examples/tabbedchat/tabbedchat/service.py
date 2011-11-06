import json


import logging
log = logging.getLogger(__name__)


class User(object):

    def __init__(self, cid=None, uid=None):
        self.cid = cid
        self.uid = uid


class BaseService(object):
    _method_prefix = None

    def configure(self, loop):
        """Usually used for getting dependencies"""

    def _checkname(self, val):
        if not isinstance(val, str):
            return False
        if not val.startswith(self._method_prefix):
            return False
        val = val[len(self._method_prefix):]
        if val.startswith('_'):
            return False
        return True

    def __call__(self, msg):
        if len(msg) < 2:
            log.warning("Wrong message %r", msg)
            return
        if msg[1] == b'heartbeat':
            return
        elif msg[1] == b'connect':
            return
        elif msg[1] == b'message':
            try:
                data = json.loads(msg[2].decode('utf-8'))
            except ValueError:
                log.warning("Can't unpack json")
                return
            if not data or not self._checkname(data[0]):
                log.warning("Wrong json")
                return
            try:
                usr = User(cid=msg[0])
                meth = getattr(self, data[0][len(self._method_prefix):])
                meth(usr, *data[1:])
            except Exception:
                log.exception("Error handling %r", data[0])
        elif msg[1] == b'msgfrom':
            try:
                data = json.loads(msg[3].decode('utf-8'))
            except ValueError:
                log.warning("Can't unpack json")
                return
            if not data or not self._checkname(data[0]):
                log.warning("Wrong json")
                return
            try:
                usr = User(cid=msg[0], uid=int(msg[2].split(b':')[1]))
                getattr(self, data[0][len('auth.'):])(usr, *data[1:])
            except Exception:
                log.exception("Error handling %r", data[0])
        elif msg[1] == b'disconnect':
            self._disconnect_(User(cid=msg[0]))
        elif msg[1] == b'sync':
            self._sync_(msg[2:])
        else:
            log.warning("Wrong message %r", msg)
            return
