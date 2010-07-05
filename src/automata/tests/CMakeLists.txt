ADD_EXECUTABLE(test_automata
    test1.c
    )
TARGET_LINK_LIBRARIES(test_automata
    automata
    cunit)
SET_TARGET_PROPERTIES(test_automata
    PROPERTIES
    COMPILE_FLAGS "-I${AUTOMATA_SOURCE_DIR}/src")
