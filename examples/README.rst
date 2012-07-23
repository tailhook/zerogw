===========================
Zerogw Application Examples
===========================

There are three basic application examples:

* ``echo.py`` -- minimal zerogw web application (works with zerogw.yaml)

* ``chat.py`` -- minimal chat-like websocket application (needs ws.js,
  zerogw.yaml, websocket.html)

* ``tabbedchat`` -- full-featured chat application on websocket (and comet) with
  registration, joining multiple rooms simultaneously and so on

All examples depend on:

* ``python3``
* ``pyzmq`` (you may need ``cython`` to build ``pyzmq``)
* The ``tabbedchat`` example also depends on ``redis``


Online Examples
===============

More examples are available online:

* zorro_ python framework based on greenlets has minimal wrapping to build
  zerogw applications
* fedor_ is an online to do list with vim-like keybindings

.. _zorro: http://github.com/tailhook/zorro
.. _fedor: http://github.com/tailhook/fedor
