HTTP Tutorial
=============

Disclaimer
----------

Unlike most HTTP servers out there zerogw doesn't try to support old CGI
convention of passing environment to your web application. For each
forwarded requests zerogw forwards only a few fields, such as url and
method (configurable) to a backend. This way we produce less internal
traffic and less bloated applications. This also means thay you can't
use most of bloated web frameworks out there. Zerogw is suitable for
fast web applications (and a parts of thereof) which need most out of
machine performance (e.g. you can use it for autocompletion or some
other AJAX). If you want to use big fat web framework there are plenty
of other solutions. If thats ok for you, read on!

Hello World
-----------

Let's start with simple hello world application. The first thing to know
is how to configure zerogw. We will start with simplest possible
configuration and will improve it later.

Minimal configuration::

    Server:
      listen:
        host: 0.0.0.0
        port: 8080

    Routing:
      zmq-forward:
        enabled: yes
        socket:
        - !zmq.Bind "tcp://127.0.0.1:5000"
        contents:
        - !Uri

All above means that zerogw will listen for connections on port 8080 on
all interfaces (0.0.0.0). Then it will forward requests to local host
with port 5000. This port also listens for connections, so you can start
several backend processes (and even several boxes, if you'll change
127.0.0.1 to your local network ip address) to process requests.
Forwarded request will contain just URI part of the original request.

Then we will write a simple script which would make this work::

    import zmq

    ctx = zmq.Context()
    sock = ctx.socket(zmq.REP)
    sock.connect('tcp://127.0.0.1:5000')
    while True:
        uri, = sock.recv_multipart()
        sock.send_multipart([b'Hello from '+uri])

This is everything which is needed to serve requests. Note we are
connecting to the address you specified to bind to in zerogw config.
Now you can go to the browser at http://localhost:8080/ and you should
see ``Hello from /``.

We use ``recv_multipart`` and ``send_multipart`` to simplify working
with sockets. If they are not provided in your language bindings you
will probably need to use ``recv`` and ``send`` while reading
``RCVMORE`` and setting ``SNDMORE`` flags. Refer to zeromq and you
language bindings for more information.

.. note:: This code works perfectly for example, but in reality it can
   except suring recv or send calls. So in production application you
   should use more complicated loop. See ioloop in pyzmq bindings or
   appropriate functionality in your language bindings


Was that hard? I guess no really.

