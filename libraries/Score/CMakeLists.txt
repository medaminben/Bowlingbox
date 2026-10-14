# The project name is used internally by CMake, but is not relevant for
# anything else. To avoid confusion, it should however match with the
# directory name. Camel case is okay.
project(Score VERSION 1.2.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})

set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    src/Score_impl.h
    include/Bowlingbox/Score/IBowlingGame.h
    include/Bowlingbox/Score/Frames.h
    include/Bowlingbox/Score/Notation.h
    include/Bowlingbox/Score/Score.h
)

set(TARGET_SOURCES
    src/Frames.cpp
    src/Notation.cpp
    src/Score.cpp
    src/Score_impl.cpp
)

set(TARGET_RSC)

set(PRIVATE_DEPS
)
set(PUBLIC_DEPS
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Score.cpp
)

set(TEST_DEPS
)
###############################################################
# build
###############################################################
create_library(
    LIB_NAME             ${TARGET_NAME}
    LIB_FILES            ${TARGET_FILES}
    PRIVATE_DEPENDENCIES ${PRIVATE_DEPS}
    PUBLIC_DEPENDENCIES  ${PUBLIC_DEPS}
    TEST_DISCOVER        ${TST_DISCOVER}
    TEST_SOURCES         ${TST_SOURCES}
    TEST_DEPENDENCIES    ${TEST_DEPS}
    LIB_RSC              ${TARGET_RSC}
)
