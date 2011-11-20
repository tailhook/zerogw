Zerogw
======

Zerogw is a http to zeromq gateway. Which means it listens HTTP, parses
request and sends it using zeromq socket (ZMQ_REQ). Then waits for the reply
and responds with data received from zeromq socket.

Starting with v0.3 zerogw also supports WebSockets. Websockets are implemented
by forwarding incoming messages using ZMQ_PUB socket, and listening clommands
from ZMQ_SUB socket. Each WebSocket client can be subscribed to unlimited
number of topics. Each zeromq message it either control message (e.g.
subscription) or message to a specified topic which will be efficiently sent
to every WebSocket subscribed to that particular topic.

Zerogw is written in C  and uses libwebsite library for handling HTTP (which
itself uses libev).

Zerogw is not a full-blown http server. It knows nothing about static files,
caches, CGI, whatever. It knows some routing and that's almost it. That makes
it very fast and perfectly scalable.

Use it for:
 * RPC's
 * REST API's
 * Ajax
 * WebSockets

Don't use it for:
 * Serving static
 * File uploads
 * Huge full-blown web pages (maybe)

Installing
----------

Ubuntu::

    sudo add-apt-repository ppa:tailhook/zerogw
    sudo apt-get install zerogw

ArchLinux::

    yaourt -S zerogw

For other distributions refer to Compiling section.

We currently support only Linux.

Dependencies
------------

 * libwebsite_ for handling http
 * coyaml_ for handling configuration
 * python3_ needed for coyaml to build configuration parser
 * libzmq_ and libev_ of course
 * libyaml_ for parsing configuration

First two usually compiled statically, so you don't need them at runtime. Same
with python. (Eventually, I'll release a bundle with precompiled configuration
parser and embedded few other libraries for easier compiling :) )

.. _libwebsite: http://github.com/tailhook/libwebsite
.. _coyaml: http://github.com/tailhook/coyaml
.. _python3: http://python.org/
.. _libyaml: http://pyyaml.org/wiki/LibYAML
.. _libzmq: http://zeromq.org/
.. _libev: http://software.schmorp.de/pkg/libev.html


Compiling
---------

::

    ./waf configure --prefix=/usr
    ./waf build
    ./waf install
