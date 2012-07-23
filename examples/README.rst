Zerogw Application Examples
===========================

There are three basic application examples:

* `echo.py` -- minimal zerogw web application (works with zerogw.yaml)

* `chat.py` -- minimal chat-like websocket application (needs ws.js,
  zerogw.yaml, websocket.html)

* `tabbedchat` -- full-featured chat application on websocket (and comet) with
  registration, joining multiple rooms simultaneously and so on

All examples depend on:

* `python3`
* `pyzmq` (you may need `cython` to build `pyzmq`)
* The `tabbedchat` example also depends on `redis`



