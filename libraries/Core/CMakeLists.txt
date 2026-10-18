project(Core VERSION 2.0.0 LANGUAGES CXX)

set(TARGET_NAME Core)

set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    # private headers belong in "src"
    # src/${TARGET_NAME}_impl.h
    # Public headers belong in: include/CMAKE_ROOT_NAME/...
    # Other components and applications will add "include"
    # to their search path, and need to use the following
    # kind of path: #include <CMAKE_ROOT_NAME/PROJECT_NAME/...>
    include/${CMAKE_ROOT_NAME}/${TARGET_NAME}/${TARGET_NAME}.h
)

set(TARGET_SOURCES
    # Source files belong in "src",
    # they will never be seen or used by other components,
    src/${TARGET_NAME}.cpp
    #src/${TARGET_NAME}_impl.cpp
)

set(TARGET_RSC
    # resources
    #test/data/${TARGET_NAME}TestData.txt
)

##dependencies
#private dependencies
set(PRIVATE_DEPS
)
#public dependencies
set(PUBLIC_DEPS
)
set(TARGET_FILES
    ${TARGET_HEADERS}
    ${TARGET_SOURCES}
)
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    # C++ files containing test cases should start with test_
    # to identify them more easily.
    test/src/test_${TARGET_NAME}.cpp
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
