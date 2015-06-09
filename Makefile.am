bin_PROGRAMS = zerogw openport zerogwctl
ACLOCAL_AMFLAGS=-I m4
AM_CFLAGS=-std=c99 -I/usr/include
#AM_LDFLAGS=-L /usr/local/lib
include_HEADERS=src/request.h src/pool.h src/polling.h src/resolve.h src/http.h src/uidgen.h src/commands.h src/main.h src/zutils.h src/disk.h src/sieve.h src/log.h src/msgqueue.h src/websocket.h src/namedoutput.h 

zerogw_LDFLAGS = -L/usr/local/lib
zerogw_LDADD = -lzmq -lev -lpthread /usr/local/lib/libwebsite.la /usr/local/lib/libcoyaml.la -lyaml -lcrypto
zerogw_SOURCES = src/commands.c src/disk.c src/http.c src/log.c src/main.c src/msgqueue.c src/polling.c src/pool.c src/request.c src/resolve.c src/sieve.c src/uidgen.c src/websocket.c  src/zutils.c src/config.c

zerogwctl_LDADD = -lzmq -lev  /usr/local/lib/libcoyaml.la -lyaml
zerogwctl_LDFLAGS = -L/usr/local/lib
zerogwctl_SOURCES = src/zerogwctl.c src/config.c

openport_LDADD = -lzmq -lev
openport_SOURCES = src/openport.c
