import json
import hashlib
import re
import os
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

    def login(self, cid, info):
        login = normlogin(info['login'])
        if login is None:
            self._output.send(cid, ['auth.wrong_nickname'])
            return
        passwd = info['password']
        id = int(self._redis.execute(b'HGET', b'nicknames', login))
        if not id:
            self._output.send(cid, ['auth.wrong_login'])
            return
        password = self._redis.execute(b'GET', 'user:{0}:password'.format(id))
        if not password:
            log.warning("There is nikname but no password")
            return
        sl, pw = password.split(b'$')
        hs = hashlib.sha1(hashlib.sha1(passwd.encode('utf-8'))
            .hexdigest().encode('ascii') + sl).hexdigest().encode('ascii')
        if hs == pw:
            name, mood, rooms, bmarks = self._redis.execute(b'MGET',
                'user:{0}:name'.format(id),
                'user:{0}:mood'.format(id),
                'user:{0}:rooms'.format(id),
                'user:{0}:bookmarks'.format(id)
                )
            assert cid.cid, cid.__dict__
            self._output.set_cookie(cid, "user:{0}".format(id))
            self._output.add_output(cid, b'["chat.', b'chat')
            self._output.send(cid, ['auth.ok', {
                'ident': id,
                'name': name.decode('utf-8'),
                'mood': mood.decode('utf-8'),
                'rooms': [a.decode('utf-8') for a in rooms] if rooms else (),
                'bookmarks': [a.decode('utf-8') for a in bmarks]
                             if bmarks else (),
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
        if not self._redis.execute(b'HSETNX', b'nicknames', login, str(uid)):
            self._output.send(cid, ['auth.duplicate_nickname'])
            return
        sl = hexlify(os.urandom(4))
        pw = hashlib.sha1(hashlib.sha1(passwd.encode('utf-8'))
            .hexdigest().encode('ascii')+sl).hexdigest()
        pw = sl + b'$' + pw.encode('ascii')
        self._redis.execute(b'MSET',
            'user:{0}:name'.format(uid), login,
            'user:{0}:password'.format(uid), pw,
            'user:{0}:mood'.format(uid), "New user",
            )
        self._output.add_output(cid, b'["chat.', b'chat')
        self._output.send(cid, ['auth.registered', {
            'ident': uid,
            'name': login,
            'mood': "New user",
            }])

