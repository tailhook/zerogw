import json
import hashlib
import re
import os
import uuid
import time
from binascii import hexlify

from .service import BaseService

import logging
log = logging.getLogger(__name__)

name_re = re.compile('^[a-zA-Z0-9 ]+$')

def normlogin(val):
    if not name_re.match(val):
        return
    return ' '.join(val.strip().split())

class Service(BaseService):
    _method_prefix = 'auth.'

    def configure(self, loop):
        super().configure(loop)
        self._redis = loop.get('redis')
        self._output = loop.get('output')

    def login(self, user, info):
        login = normlogin(info['login'])
        if login is None:
            self._output.send(user, ['auth.wrong_nickname'])
            return
        passwd = info['password']
        uid = int(self._redis.execute(b'HGET', b'nicknames', login))
        if not uid:
            self._output.send(user, ['auth.wrong_login'])
            return
        password = self._redis.execute(b'GET', 'user:{0}:password'.format(uid))
        if not password:
            log.warning("There is nikname but no password")
            return
        sl, pw = password.split(b'$')
        hs = hashlib.sha1(hashlib.sha1(passwd.encode('utf-8'))
            .hexdigest().encode('ascii') + sl).hexdigest().encode('ascii')
        if hs == pw:
            sessid = str(uuid.uuid4())
            (name, mood, rooms, bmarks), _, _, _ = self._redis.bulk((
                (b'MGET',
                    'user:{0}:name'.format(uid),
                    'user:{0}:mood'.format(uid),
                    'user:{0}:rooms'.format(uid),
                    'user:{0}:bookmarks'.format(uid)),
                (b'SET', b'conn:' + user.cid, str(uid)),
                (b'SADD', b'connections', user.cid, str(int(time.time()))),
                (b'SET', 'sess:' + sessid, str(uid)),
                ))
            assert user.cid, user.__dict__
            self._output.set_cookie(user, "user:{0}".format(uid))
            self._output.add_output(user, b'["chat.', b'chat')
            self._output.send(user, ['auth.ok', {
                'ident': uid,
                'name': name.decode('utf-8'),
                'mood': mood.decode('utf-8'),
                'session_id': sessid,
                'rooms': [a.decode('utf-8') for a in rooms] if rooms else (),
                'bookmarks': [a.decode('utf-8') for a in bmarks]
                             if bmarks else (),
                }])
        else:
            self._output.send(user, ['auth.wrong_login'])

    def register(self, user, info):
        login = normlogin(info['login'])
        passwd = info['password']
        if login is None:
            self._output.send(user, ['auth.wrong_nickname'])
            return
        _uid = self._redis.execute(b'HGET', b'nicknames', login)
        if _uid:
            self._output.send(user, ['auth.duplicate_nickname'])
            return
        uid = self._redis.execute(b'INCR', b'next:user_id')
        if not self._redis.execute(b'HSETNX', b'nicknames', login, str(uid)):
            self._output.send(user, ['auth.duplicate_nickname'])
            return
        sessid = str(uuid.uuid4())
        sl = hexlify(os.urandom(4))
        pw = hashlib.sha1(hashlib.sha1(passwd.encode('utf-8'))
            .hexdigest().encode('ascii')+sl).hexdigest()
        pw = sl + b'$' + pw.encode('ascii')
        self._redis.bulk((
            (b'MSET',
                'user:{0}:name'.format(uid), login,
                'user:{0}:password'.format(uid), pw,
                'user:{0}:mood'.format(uid), "New user"),
            (b'SET', b'conn:' + user.cid, str(uid)),
            (b'SADD', b'connections', user.cid, str(int(time.time()))),
            (b'SETEX', 'sess:' + sessid, b'7200', str(uid)),
            ))
        self._output.add_output(user, b'["chat.', b'chat')
        self._output.send(user, ['auth.registered', {
            'ident': uid,
            'name': login,
            'mood': "New user",
            'session_id': sessid,
            }])

    def _disconnect_(self, user):
        pass
