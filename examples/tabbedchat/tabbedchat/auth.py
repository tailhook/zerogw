import json
import hashlib
import re

import logging
log = logging.getLogger(__name__)

name_re = re.compile('^[a-zA-Z0-9 ]+$')

def checkname(val):
    if not isinstance(val, str):
        return False
    if not val.startswith('auth.'):
        return False
    val = val[len('auth.'):]
    if val.startswith('_'):
        return False
    return True

def normlogin(val):
    if not name_re.match(val):
        return
    return ' '.join(val.strip().split())

class Service(object):

    def __init__(self):
        pass

    def configure(self, loop):
        self._redis = loop.get('redis')
        self._output = loop.get('output')

    def login(self, cid, info):
        login = normlogin(info['login'])
        if login is None:
            self._output.send(cid, ['auth.wrong_nickname'])
            return
        passwd = info['password']
        id = self._redis.execute(b'HGET', b'nicknames', login)
        if not id:
            self._output.send(cid, ['auth.wrong_login'])
            return
        password = self._redis.execute(b'GET', 'user:{0}:password'.format(id))
        if not password:
            log.warning("There is nikname but no password")
            return
        sl, pw = password.split('$')
        if hashlib.sha1(hashlib.sha1(passwd).hexdigest()+sl).hexdigest() == pw:
            name, mood, rooms, bmarks = self._redis.execute(b'MGET',
                'user:{0}:name'.format(id),
                'user:{0}:mood'.format(id),
                'user:{0}:rooms'.format(id),
                'user:{0}:bookmarks'.format(id)
                )
            self._output.send(cid, ['auth.ok', {
                'ident': id,
                'name': name,
                'mood': mood,
                'rooms': rooms,
                'bookmarks': bookmarks,
                }])
        else:
            self._output.send(cid, ['auth.wrong_login'])

    def register(self, cid, info):
        login = normlogin(info['login'])
        passwd = info['password']
        if login is None:
            self._output.send(cid, ['auth.wrong_nickname'])
            return
        id = self._redis.execute(b'HGET', b'nicknames', login)
        if id:
            self._output.send(cid, ['auth.duplicate_nickname'])
            return
        uid = self._redis.execute(b'INCR', b'next:user_id')
        sl = hexlify(os.urandom(4))
        pw = hashlib.sha1(hashlib.sha1(passwd).hexdigest()+sl).hexdigest()
        pw = sl + '$' + pw
        self._redis.execute(b'HSET',
            'user:{0}:name'.format(uid), login,
            'user:{0}:password'.format(uid), pw,
            'user:{0}:mood'.format(uid), "New user",
            )

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
