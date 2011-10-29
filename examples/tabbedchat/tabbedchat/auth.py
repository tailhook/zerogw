import json

import logging
log = logging.getLogger(__name__)


def checkname(val):
    if not isinstance(val, str):
        return False
    if not val.startswith('auth.'):
        return False
    val = val[len('auth.'):]
    if val.startswith('_'):
        return False
    return True


class Service(object):

    def __init__(self):
        pass

    def login(self, cid, info):
        print("LOGIN", info)

    def register(self, cid, info):
        login = info['login']
        password = info['password']
        print("REGISTERING", login, password)

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
            if not data or not checkname(data[0]):
                log.warning("Wrong json")
                return
            try:
                getattr(self, data[0][len('auth.'):])(msg[0], *data[1:])
            except Exception:
                log.exception("Error handling %r", data[0])
        else:
            log.warning("Wrong message %r", msg)
            return
