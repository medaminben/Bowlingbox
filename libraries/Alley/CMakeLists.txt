project(Alley VERSION 2.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    src/BulletWorld.h
    src/Lane_impl.h
    include/Bowlingbox/Alley/Types.h
    include/Bowlingbox/Alley/PhysicsWorld.h
    include/Bowlingbox/Alley/Alley.h
)
set(TARGET_SOURCES
    src/BulletWorld.cpp
    src/Lane_impl.cpp
    src/Alley.cpp
)
set(TARGET_RSC)
# Bullet is an implementation detail; BUILD_INTERFACE keeps it out of the
# installed export set.
set(PRIVATE_DEPS
    $<BUILD_INTERFACE:Bullet::Bullet>
)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
    ${CMAKE_ROOT_NAME}::Score
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Alley.cpp
)
set(TEST_DEPS)
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
