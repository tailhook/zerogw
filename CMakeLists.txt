PROJECT(ZEROGW)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

ADD_SUBDIRECTORY(src/automata)

ADD_EXECUTABLE(zerogw
    src/log.c
    src/config.c
    src/main.c
    )

SET_TARGET_PROPERTIES(zerogw
    PROPERTIES
    DEPENDS automata
    LINK_FLAGS "-L${AUTOMATA_BINARY_DIR} -lyaml -lautomata"
    COMPILE_FLAGS "-I${AUTOMATA_SOURCE_DIR}/src"
    )

ADD_DEPENDENCIES(zerogw automata)

ADD_DEFINITIONS(
    -DCONFIG_ZEROGW
    -DLOG_STRIP_PATH="${ZEROGW_SOURCE_DIR}/src/"
    )
