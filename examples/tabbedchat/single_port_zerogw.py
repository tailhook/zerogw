#!/usr/bin/env python3

"""
This script is intended to run multiple zerogw instances with single shared
port/socket. This works by creating a socket externally, and pasing it as
a file descriptor to all zerogw instances, during exec.

You must configure zerogw to use the file descriptor to run correctly
(example zerogw_fd.yaml does this)
"""

import socket
import os
import sys

# Let's open shared socket
zgwsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
zgwsock.bind(('0.0.0.0', 8000))
zgwsock.listen(100000)
zgwsock.setblocking(False)  # Crucial for zerogw with older libwebsite

# Now make it file descriptor zero
os.dup2(zgwsock.fileno(), 0)

# Now close old socket (we have dup as fd zero)
zgwsock.close()

# Each zerogw has it's own socket directory, so we use pass directory
# names as arguments and create a zerogw instance per directory
for dir in sys.argv[1:]:
    if dir != sys.argv[-1]: # last instance is our process, no more forks
        if os.fork():  # parent continues to fork processes
            continue
    os.execlp('zerogw', 'zerogw', '-c', 'zerogw_fd.yaml', '-Ddir=' + dir)
