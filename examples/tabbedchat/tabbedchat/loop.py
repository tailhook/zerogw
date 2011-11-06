import zmq
import json

from . import redis


import logging
log = logging.getLogger(__name__)


def utf(val):
    """Little helper function which turns strings to utf-8 encoded bytes"""
    if isinstance(val, str):
        return val.encode('utf-8')
    else:
        return val


def blob(val):
    """Little helper function which turns data into bytes by either encoding
    or json.dumping them

    In production it can be a bad practice because you can't send json-dumped
    string (it will be just encoded). But for our example its fun that we
    can send pre-serialized data.

    And you always send json objects anyway, don't you?
    """
    if isinstance(val, (dict, list)):
        return json.dumps(val).encode('utf-8')
    elif isinstance(val, str):
        return val.encode('utf-8')
    else:
        return val

def cid(val):
    if hasattr(val, 'cid'):
        return val.cid
    assert isinstance(val, (bytes, bytearray)), ("Connection must be bytes "
        "or object having cid property")
    return val


class Loop(object):

    def __init__(self):
        self._ctx = zmq.Context(1)
        self._poller = zmq.Poller()
        self._handlers = {}
        self._redises = {}
        self._outputs = {}

    def add_service(self, name, obj, **settings):
        sock = self._make_socket(zmq.PULL, settings)
        self._poller.register(sock, zmq.POLLIN)
        self._handlers[sock] = obj
        obj.configure(self)

    def add_output(self, name, **settings):
        sock = self._make_socket(zmq.PUB, settings)
        self._outputs[name] = Output(sock)

    def add_redis(self, name, *, socket):
        self._redises[name] = redis.Redis(socket_path=socket)

    def get(self, name):
        if name in self._outputs:
            return self._outputs[name]
        if name in self._redises:
            return self._redises[name]
        raise KeyError(name)

    def _make_socket(self, kind, settings):
        sock = self._ctx.socket(kind)
        sock.setsockopt(zmq.HWM, settings.get('hwm', 100))
        # TODO(tailhook) implement socket options
        for i in settings.get('bind', ()):
            sock.bind(i)
        for i in settings.get('connect', ()):
            sock.connect(i)
        return sock

    def run(self):
        while True:
            socks = self._poller.poll()
            for s, _ in socks:
                msg = s.recv_multipart()
                log.debug("Received from zerogw: %r", msg)
                self._handlers[s](msg)


class Output(object):

    def __init__(self, sock):
        self._sock = sock

    def subscribe(self, conn, topic):
        self._do_send((b'subscribe', cid(conn), utf(topic)))

    def unsubscribe(self, conn, topic):
        self._do_send((b'unsubscribe', cid(conn), utf(topic)))

    def drop(self, topic):
        self._do_send((b'drop', utf(topic)))

    def send(self, conn, data):
        self._do_send((b'send', cid(conn), blob(data)))

    def publish(self, topic, data):
        self._do_send((b'publish', utf(topic), blob(data)))

    def set_cookie(self, conn, cookie):
        self._do_send((b'set_cookie', cid(conn), utf(cookie)))

    def add_output(self, conn, prefix, name):
        self._do_send((b'add_output', cid(conn), utf(prefix), utf(name)))

    def del_output(self, conn, prefix, name):
        self._do_send((b'del_output', cid(conn), utf(prefix), utf(name)))

    def disconnect(self, conn):
        self._do_send((b'disconnect', cid(conn)))

    def _do_send(self, data):
        log.debug("Sending to zerogw: %r", data)
        # TODO(tailhook) handle errors
        self._sock.send_multipart(data)

