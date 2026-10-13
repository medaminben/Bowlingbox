# Bullet3 rigid-body physics, fetched and built statically with -fPIC so it
# can be linked into the shared Gamebox::Alley library.
include(FetchContent)

set(GAMEBOX_BULLET_VERSION "3.25" CACHE STRING "Bullet3 release tag")

FetchContent_Declare(
    bullet3
    URL https://github.com/bulletphysics/bullet3/archive/refs/tags/${GAMEBOX_BULLET_VERSION}.tar.gz
    SYSTEM
)

# Bullet's option() calls must not override these (CMP0077 NEW is set in Common.cmake).
# BUILD_SHARED_LIBS is restored afterwards so Gamebox libraries stay shared.
set(_sandbox_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
foreach(opt
        BUILD_BULLET2_DEMOS BUILD_CPU_DEMOS BUILD_OPENGL3_DEMOS BUILD_EXTRAS
        BUILD_UNIT_TESTS BUILD_BULLET3 BUILD_CLSOCKET BUILD_ENET BUILD_EGL
        BUILD_PYBULLET USE_GRAPHICAL_BENCHMARK USE_GLUT INSTALL_LIBS INSTALL_CMAKE_FILES)
    set(${opt} OFF)
endforeach()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(bullet3)
set(BUILD_SHARED_LIBS ${_sandbox_saved_shared})

add_library(sandbox_bullet INTERFACE)
target_link_libraries(sandbox_bullet INTERFACE BulletDynamics BulletCollision LinearMath)
target_include_directories(sandbox_bullet SYSTEM INTERFACE ${bullet3_SOURCE_DIR}/src)
add_library(Bullet::Bullet ALIAS sandbox_bullet)
