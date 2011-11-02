import json

from .service import BaseService

import logging
log = logging.getLogger(__name__)


class Service(BaseService):
    _method_prefix = 'chat.'

    def configure(self, loop):
        super().configure(loop)
        self._redis = loop.get('redis')
        self._output = loop.get('output')

    def join(self, usr, room):
        assert usr.uid is not None, usr.__dict__
        if isinstance(room, list):
            for i in room:
                self.join(usr, room)
            return
        id = self._redis.execute(b'HGET', b'rooms', room)
        if id is None:
            id = self._create_room(room, usr)
        else:
            id = int(id)
        username = self._redis.execute(b'GET', 'user:{0}:name'.format(usr.uid))
        username = username.decode('utf-8')
        assert username, id
        self._output.subscribe(usr, 'room:{0}'.format(id))
        room_history = 'room:{0}:history'.format(id)
        ul, _, _, _, _, hist = self._redis.bulk((
            (b"SORT", 'room:{0}:users'.format(id), b'BY', b'nosort',
                b'GET', b'#', b'GET', b'user:*:name', b'GET', b'user:*:mood'),
            (b"SADD", 'room:{0}:users'.format(id), str(usr.uid)),
            (b"SADD", 'user:{0}:rooms'.format(usr.uid), str(id)),
            (b"RPUSH", room_history, json.dumps(
                {"kind": "join", "author": username, "uid": usr.uid})),
            (b"LTRIM", room_history, b'-100', b'-1'),
            (b"LRANGE", room_history, b'0', b'-1'),
            ))
        uit = iter(ul)
        self._output.send(usr, ['chat.room', {
            'ident': id,
            'name': room,
            'users': [{'ident': int(uid), 'name': name.decode('utf-8'),
                       'mood': mood.decode('utf-8')}
                      for uid, name, mood in zip(uit, uit, uit)],
            'history': [json.loads(m.decode('utf-8')) for m in hist],
            }])

    def _create_room(self, room, usr):
        rid = int(self._redis.execute(b'INCR', b'next:room_id'))
        self._redis.bulk((
            (b'MSET',
                'room:{0}:name'.format(rid), room,
                'room:{0}:topic'.format(rid), 'Discussing '+room,
                ),
            (b'SADD', 'room:{0}:moderators'.format(rid), str(usr.uid)),
            (b'HSET', b'rooms', room, str(rid)),
            ))
        return rid

    def message(self, usr, room, txt):
        username, ismem = self._redis.bulk((
            (b'GET', 'user:{0}:name'.format(usr.uid)),
            (b'SISMEMBER', 'user:{0}:rooms'.format(usr.uid), str(room)),
            ))
        if not ismem:
            log.warning("Trying to write in non-subscribed room")
            return
        username = username.decode('utf-8')
        room_history = 'room:{0}:history'.format(id)
        self._redis.bulk((
            (b"RPUSH", room_history, json.dumps(
                {"text": txt, "author": username, "uid": usr.uid})),
            (b"LTRIM", room_history, b'-100', b'-1'),
            ))
        self._output.publish('room:{0}'.format(room), ['chat.message', room, {
            'author': username,
            'uid': usr.uid,
            'text': txt,
            }])


