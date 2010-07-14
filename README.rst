Zerogw
======

Zerogw is a http to zeromq gateway. Which means it listens HTTP, parses
request and sends it using zeromq socket (ZMQ_REQ). Then waits for the reply
and responds with data received from zeromq socket.

Zerogw is written in C. And uses libevent library for handling HTTP.

Zerogw is not a full-blown http server. It knows nothing about static files,
caches, CGI, whatever. It knows some routing and that's almost it. That makes
it very fast and perfectly scalable.

Use it for:
 * RPC's
 * REST API's
 * Ajax

Don't use it for:
 * Serving static
 * File uploads
 * Full web pages (maybe)
