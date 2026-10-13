# The project name is used internally by CMake, but is not relevant for
# anything else. To avoid confusion, it should however match with the
# directory name. Camel case is okay.
project(Gamebox VERSION 1.2.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})

set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    # private headers belong to "src"
    src/Gamebox_impl.h
    # Public headers belong in: include/CMAKE_ROOT_NAME/...
    # Other components and applications will add "include"
    # to their search path, and need to use  the following
    # kind of path: #include <CMAKE_ROOT_NAME/PROJECT_NAME/...>
    include/Sandbox/Gamebox/Gamebox.h
)

set(TARGET_SOURCES
    # Source files belong in "src",
    # they will never been seen or used by other components,
    src/Gamebox.cpp
    src/Gamebox_impl.cpp
)

set(TARGET_RSC
    # resources
    test/data/PreprocessorTestData.txt
)

##dependencies
#private dependencies
set(PRIVATE_DEPS
)
#public dependencies
set(PUBLIC_DEPS
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    # C++ files containing test cases should start with test_
    # to identify them more easily.
    test/src/test_Gamebox.cpp
)

# Test dependencies other than the actual library
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