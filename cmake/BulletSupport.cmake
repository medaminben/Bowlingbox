# Bullet3 rigid-body physics through dev_dependency(): an installed Bullet
# (e.g. Debian/Ubuntu libbullet-dev, MSYS2 mingw-w64-ucrt-x86_64-bullet) first,
# else release 3.25 fetched and built statically with -fPIC so it can be
# linked into the shared Bowlingbox::Alley library. Every path provides
# Bullet::Bullet. No minimum version: the distributions ship 3.24.

# Only read when Bullet is fetched. Bullet's option() calls must not override
# these (CMP0077 NEW is set in Common.cmake). BUILD_SHARED_LIBS is restored
# afterwards so the Bowlingbox libraries stay shared.
set(_bowlingbox_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
foreach(opt
        BUILD_BULLET2_DEMOS BUILD_CPU_DEMOS BUILD_OPENGL3_DEMOS BUILD_EXTRAS
        BUILD_UNIT_TESTS BUILD_BULLET3 BUILD_CLSOCKET BUILD_ENET BUILD_EGL
        BUILD_PYBULLET USE_GRAPHICAL_BENCHMARK USE_GLUT INSTALL_LIBS INSTALL_CMAKE_FILES)
    set(${opt} OFF)
endforeach()
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

dev_dependency(Bullet REQUIRED
    FETCH_URL https://github.com/bulletphysics/bullet3/archive/refs/tags/3.25.tar.gz
    INSTALL_HINT "Debian/Ubuntu: apt install libbullet-dev; MSYS2: pacman -S mingw-w64-ucrt-x86_64-bullet")

unset(CMAKE_POLICY_VERSION_MINIMUM)
set(BUILD_SHARED_LIBS ${_bowlingbox_saved_shared})

add_library(bowlingbox_bullet INTERFACE)
add_library(Bullet::Bullet ALIAS bowlingbox_bullet)
if(TARGET BulletDynamics)
    # Fetched: Bullet's own targets and its source tree as include dir.
    FetchContent_GetProperties(Bullet SOURCE_DIR _bowlingbox_bullet_src)
    target_link_libraries(bowlingbox_bullet INTERFACE BulletDynamics BulletCollision LinearMath)
    target_include_directories(bowlingbox_bullet SYSTEM INTERFACE ${_bowlingbox_bullet_src}/src)
else()
    # Installed: CMake's FindBullet gives absolute paths; a BulletConfig.cmake
    # (Debian) gives paths relative to BULLET_ROOT_DIR and bare library names.
    foreach(_dir IN LISTS BULLET_INCLUDE_DIRS)
        if(NOT IS_ABSOLUTE "${_dir}")
            set(_dir "${BULLET_ROOT_DIR}/${_dir}")
        endif()
        target_include_directories(bowlingbox_bullet SYSTEM INTERFACE "${_dir}")
    endforeach()
    foreach(_dir IN LISTS BULLET_LIBRARY_DIRS)
        if(NOT IS_ABSOLUTE "${_dir}")
            set(_dir "${BULLET_ROOT_DIR}/${_dir}")
        endif()
        target_link_directories(bowlingbox_bullet INTERFACE "${_dir}")
    endforeach()
    target_link_libraries(bowlingbox_bullet INTERFACE ${BULLET_LIBRARIES})
    if(BULLET_DEFINITIONS)
        string(REGEX REPLACE "(^| )-D" "\\1" _defs "${BULLET_DEFINITIONS}")
        separate_arguments(_defs)
        target_compile_definitions(bowlingbox_bullet INTERFACE ${_defs})
    endif()
endif()
