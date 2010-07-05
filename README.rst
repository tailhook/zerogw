Zerogw
======

Zerogw is a http to zeromq gateway. Which means it listens HTTP, parses
request and sends it using zeromq socket (ZMQ_REP). Then waits for the reply
and responds with data received from zeromq socket.

Use it for:
 * RPC's
 * REST API's
 * Ajax

Don't use it for:
 * Serving static
 * File uploads
 * Full web pages (maybe)
