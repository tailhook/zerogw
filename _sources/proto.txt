Zerogw Backend Protocols
========================

HTTP Forwarding
---------------

We use zeromq's request reply model for http forwarding (except for
:ref:`long polling case <long_polling>`)

Request
^^^^^^^

For each http request zerogw forwards a single multipart message.
Request_id is sent like address data (message parts that can be read
using XREP sockets only, and finished by empty message). After address
data configured parts of the request are sent one by one as multipart
message. E.g. if you have following in configuration::

    zmq-forward:
      contents:
        - !Method
        - !Uri
        - !Header Cookie
        - !Body

And you've got request::

    POST /hello HTTP/1.1
    Host: example.com
    Cookie: example=cookie_value
    Content-Length: 8

    PostBody

You will receive following message parts (one line per message part)::

    POST
    /hello
    example=cookie_value
    PostBody

It's up to the application for how to act upon it. Note if you set
``retry`` to something you can get same request twice. And if ``retry``
is set to ``!RetryFirst <N>`` request id will be same for every attempt,
if you've set ``retry`` to ``!RetryLast <N>`` request id will change on
each attempt. But usually request id is opaque for user in zeromq.

Response
^^^^^^^^

Response can contain one, two or three parts for convenience.

In the simple case you just send message body, as a single part message.
Zerogw will respond with ``200 OK`` and that message body.

If you respond with two messages first one will be status line, so yo
can respond with 404 page like the following::

    404 Not Found
    <h1> Page Not Found</h1>

.. note:: These ways are quite unuseful in real situations.
   ``Content-Length`` header will be added automatically, but you should
   configure specific ``Content-Type`` header in a config to be sure
   that browser will render page correctly when using this method

If you need to supply headers you send 3 message parts. Second one is
constructed from nul-terminated name/value header pairs::

    200 OK
    Content-Type\0text/html\0E-tag\0immortal\0
    <b>Lorem ipsum dolor sit amet</b>

.. note:: Last header value must be nul-terminated. You must not add
   ``Content-Length`` header as it will be generated automatically.
   Currently headers sent from backend will be appended to headers
   specified in config without overwriting, it can lead to unexpected
   behavior on some proxies or browsers so you should use use one or
   another way for each header type throught the whole application.

.. _long_polling:

WebSockets Backend Protocol
---------------------------

Zerogw implements unified interface for application writers for both
long polling and websockets. Both are used for bidirectional message
channels from client to server.

.. note:: There is no overhead of using long polling with normal http
   backend in zerogw if that suits your application. This interface is
   provided to make using either websockets or long polling transparent
   for both frontend and backend developer and provides reliable message
   stream.

Zerogw to Backend Messages
^^^^^^^^^^^^^^^^^^^^^^^^^^

Most messages from server to client consists of client id (long binary
string of nonsense) and ascii command name, following more message parts
which we will call arguments in the text below.

Connection Messages
~~~~~~~~~~~~~~~~~~~

``connect`` - is sent when new connection established, no arguments

``disconnect`` - is sent when connection disconnected. All subscriptions
    (see below) are already cancelled so you don't need to remove them,
    but you can cleanup some application-specific data. No arguments

Messages
~~~~~~~~

``message`` - message sent from frontend to websocket, has single
    argument - message text. Can be binary if the browser (or malicious
    client) sent binary data

Heartbeats
~~~~~~~~~~

If configured server sends heartbeats to the backend to give backend
notion that it's still alive. Heartbeat consists of two part message:
server id and literal ascii ``heartbeat``.

Backend to Zerogw Messages
^^^^^^^^^^^^^^^^^^^^^^^^^^

Usually messages sent from backend are published using pubsub to several
zerogw. This allows not to track where user currently is and also allows
to publish messages to several users without doing that on backend.

Direct Messages
~~~~~~~~~~~~~~~

:samp:`send, {conn_id}, {message}` - sends message directly to the user.
    You can send binary message, but most browsers can read only text
    data, so use utf-8

Topic Subscription
~~~~~~~~~~~~~~~~~~

Topics is a mechanism in zerogw which allows you to send message to
several users effeciently. You first subscribe users to a topic, send
publish a message to a topic, and all users get this message. Topic is
an opaque binary string. Topics are created and removed on demand and
are quite fast to use them for a lot of things.

:samp:`subscribe, {conn_id}, {topic}` -- subscribes user

:samp:`unsubscribe, {conn_id}, {topic}` -- unsubscribes user

:samp:`publish, {topic}, {message}` -- publish message to a topic,
    message will be delivered to all users subscribed on the topic

:samp:`drop, {topic}` -- delete topic, unsubscribing all the users

Outputs
~~~~~~~

In addition to subscription clients on topics you can subscribe subset
of client messages to a specific named backend (``named-outputs`` in
config)

:samp:`add_output, {conn_id}, {msg_prefix}, {name}` -- map prefix to
    specific output

:samp:`del_output, {conn_id}, {msg_prefix}` -- unmap prefix

As with subscriptions don't need to unmap anything from disconnected
user.

.. note:: it's your responsibility to clean user state from the backend.
   ``disconnect`` messages are sent to main backend only

