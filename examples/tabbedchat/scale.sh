#!/bin/sh

# Setting up signal handlers
# All this code is needed to do not leave dangling children
killchildren() {
    if [ "x$(jobs)" == "x" ]; then
        exit 0
    fi
    trap killchildren 0 1 2 3 15
    kill $(jobs -p) 2>/dev/null
}
trap killchildren 0 1 2 3 15

# running several redis instances is given as exercise for the reader
# basically you just need more python code for that
redis-server redis.conf &

# we run two zerogw with different socket directories and different ports
mkdir -p ./run/z{1,2,3} 2> /dev/null
zerogw -c zerogw.yaml -Dport=8001 -Ddir=./run/z1 &
zerogw -c zerogw.yaml -Dport=8002 -Ddir=./run/z2 &
zerogw -c zerogw.yaml -Dport=8003 -Ddir=./run/z3 &

# because of zeromq features, and because we are using bind sockets in zerogw
# side (which is most useful use case), we can run any number of backends
# with equal configuration
# BTW, storing data in redis also helps
for i in $(seq 0 4); do
    python3 -m tabbedchat \
        --auth-connect "ipc://./run/z1/auth.sock" \
        --auth-connect "ipc://./run/z2/auth.sock" \
        --auth-connect "ipc://./run/z3/auth.sock" \
        --chat-connect "ipc://./run/z1/chat.sock" \
        --chat-connect "ipc://./run/z2/chat.sock" \
        --chat-connect "ipc://./run/z3/chat.sock" \
        --output-connect "ipc://./run/z1/output.sock" \
        --output-connect "ipc://./run/z2/output.sock" \
        --output-connect "ipc://./run/z3/output.sock" \
        &
done

echo "========================================================================"
echo "You can now browse one of the three urls:"
echo "    http://localhost:8001/"
echo "    http://localhost:8002/"
echo "    http://localhost:8003/"
echo "All of them should be same, but served with different zerogw"
echo "(although it can take few seconds to start up redis and python)"
echo "NOTE: we have put all the sockets, redis data and logs in ./run dir"
echo "========================================================================"

while ! wait; do true; done;
