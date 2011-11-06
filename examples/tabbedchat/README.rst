Tabbed Chat Example
===================

Running::

    ./start.sh

You can also try::

    ./scale.sh

Which runs multiple processes of python and zerogw, to show how it could be
scaled. But you need to turn sockets into tcp ones to scale it to multiple
machines, and also fix redis config which is awfully slow for testing purposes.


Hacking Remarks
---------------

Redis Data Layout
`````````````````

* ``user:<id>:name`` -- user's name
* ``user:<id>:password`` -- user's password hash
* ``user:<id>:mood`` -- user's status string
* ``user:<id>:rooms`` -- currently joined rooms for this user
* ``user:<id>:bookmarks`` -- rooms for last session, to recover on login
* ``user:<id>:conn`` -- user's current connection id
* ``session:<sid>`` -- id of user with specified session (until expires)
* ``nicknames`` -- hash of nickname to user id
* ``next:user_id`` -- next user id counter
* ``conn:<cid>`` -- mapping of connection id to userid
* ``connections`` -- sorted set of connections to timestamps of last sync
* ``sess:<sessid>`` -- mapping of session id to userid
* ``room:<id>:name`` -- name of the room
* ``room:<id>:topic`` -- topic of the room
* ``room:<id>:users`` -- redis set of user id's of participants
* ``room:<id>:moderators`` -- redis set of user id's of moderators
* ``room:<id>:history`` -- redis list of room's history (last 100 messages)
* ``rooms`` -- hash of room names to room ids
* ``next:room_id`` -- next user id counter
