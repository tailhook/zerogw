Zerogw
======

Zerogw is a http to zeromq gateway. Which means it listens HTTP, parses
request and sends it using zeromq socket (ZMQ_REQ). Then waits for the reply
and responds with data received from zeromq socket.

Starting with v0.3 zerogw also supports WebSockets. Websockets are implemented
by forwarding incoming messages using ZMQ_PUB socket, and listening commands
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


Resources
---------

* Home page: http://zerogw.com
* Mailing list: http://groups.google.com/group/zerogw
* Documentation: http://docs.zerogw.com
* Online chat example: http://tabbedchat.zerogw.com


Installing
----------

Ubuntu::

    sudo add-apt-repository ppa:tailhook/zerogw
    sudo apt-get update
    sudo apt-get install zerogw

ArchLinux::

    yaourt -S zerogw

NetBsd::

   git clone git@github.com:h4ck3rm1k3/zerogw.git
   git clone git@github.com:h4ck3rm1k3/libwebsite.git
   git clone git@github.com:h4ck3rm1k3/coyaml.git



Installation of the following packages (some are not needed)
  * zeromq-4.0.5        The ZeroMQ messaging library
  * libsodium-1.0.0     Library for build higher-level cryptographic tools
  * libev-4.15          Full-featured and high-performance event loop
  * libyaml-0.1.6nb1    YAML 1.1 parser and emitter written in C
  * py34-expat-3.4.2    Python interface to expat
  * py34-pip-1.5.6      Installs Python packages as an easy_install replacement
  * py34-setuptools-8.0.1 New Python packaging system
  * python34-3.4.2      Interpreted, interactive, object-oriented programming language
  * pkg-config-0.28     System for managing library compile/link flags
  * autoconf-2.69nb5    Generates automatic source code configuration scripts
  * automake-1.14.1nb1  GNU Standards-compliant Makefile generator
  * curl-7.39.0nb1      Client that groks URLs
  * gettext-0.19.3      Tools for providing messages in different languages
  * gettext-asprintf-0.19.3 Provides a printf-like interface for C++
  * gettext-lib-0.19.3  Internationalized Message Handling Library (libintl)
  * gettext-m4-0.19.3   Autoconf/automake m4 files for GNU NLS library
  * gettext-tools-0.19.3 Tools for providing messages in different languages
  * git-2.2.1           GIT version control suite meta-package
  * git-base-2.2.1      GIT Tree History Storage Tool (base package)
  * git-docs-2.2.1      GIT Tree History Storage Tool (documentation)
  * git-gitk-2.2.1      GIT Tree History Storage Tool (gitk)
  * libtool-2.4.2nb2    Generic shared library support script
  * libtool-base-2.4.2nb9 Generic shared library support script (the script itself)
  * libtool-fortran-2.4.2nb5 Generic shared library support script (the script itself, incl. Fortran)
  * libtool-info-2.4.2  Generic shared library support script - info pages
  * m4-1.4.17           GNU version of UNIX m4 macro language processor


Usage :

    export LD_LIBRARY_PATH=/usr/pkg/lib/ev:/usr/pkg/lib:/usr/local/lib
    export PATH=/usr/local/bin/:$PATH

Libs :
    ldd /usr/local/bin/zerogw
    /usr/local/bin/zerogw:
	-lyaml-0.2 => /usr/pkg/lib/libyaml-0.so.2
	-lc.12 => /usr/lib/libc.so.12
	-lzmq.4 => /usr/pkg/lib/libzmq.so.4
	-lsodium.13 => /usr/pkg/lib/libsodium.so.13
	-lrt.1 => /usr/lib/librt.so.1
	-lgcc_s.1 => /lib/libgcc_s.so.1
	-lpthread.1 => /usr/lib/libpthread.so.1
	-lstdc++.7 => /usr/lib/libstdc++.so.7
	-lm.0 => /usr/lib/libm.so.0
	-lev.4 => /usr/pkg/lib/ev/libev.so.4
	-lcrypto.8 => /usr/lib/libcrypto.so.8
	-lcrypt.1 => /lib/libcrypt.so.1


For other distributions refer to Compiling section.


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

The two libs libwebsite and coyaml need to be checked out or symlinked into the subdirectories of
this project. Alternatively you can use ``git submodule init`` and ``git submodule update`` to get the packages as submodules.

::

    ./waf configure --prefix=/usr
    ./waf build
    ./waf install
