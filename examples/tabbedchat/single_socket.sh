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

# We provide each zerogw instance a separate directory for zeromq sockets
# because zeromq sockets are bound, but use special startup script for
# running them on single port
mkdir -p ./run/z{1,2,3} 2> /dev/null
# passing three dirs means start three zerogw instances one per dir
python single_port_zerogw.py ./run/z1 ./run/z2 ./run/z3 &

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
        --redis-socket "./run/redis.sock" \
        --log-file "./run/python.log" \
        &
done

echo "========================================================================"
echo "You can now browse:"
echo "    http://localhost:8000/"
echo "Depending on your luck you will hit different zerogw instance each time"
echo "NOTE: Long polling (websocket fallback) doesn't work correctly"
echo "WARNING: Danging zerogw processes may exist after stopping"
echo "========================================================================"

while ! wait; do true; done;
