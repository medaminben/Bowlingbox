cmake_minimum_required(VERSION 3.29)

# buildkit provides the CMake modules (create_library, dev_dependency, ...).
# Set BUILDKIT_DIR to a local checkout; otherwise the loader fetches its tag.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/buildkit.cmake)

# The version comes from the nearest vX.Y.Z git tag. Without git or tags the
# fallback below is used; -DBOWLINGBOX_VERSION=X.Y.Z sets it by hand.
if(NOT BOWLINGBOX_VERSION)
    # ci/next-version.sh reads the FALLBACK line below; keep it on a line of its own.
    buildkit_git_version(${CMAKE_CURRENT_SOURCE_DIR} BOWLINGBOX_VERSION BOWLINGBOX_VERSION_FULL
        FALLBACK 1.4.0)
else()
    set(BOWLINGBOX_VERSION_FULL ${BOWLINGBOX_VERSION})
endif()
message(STATUS "Bowlingbox version: ${BOWLINGBOX_VERSION_FULL}")

# Project names are only relevant for humans; camel case is okay.
project(Bowlingbox VERSION ${BOWLINGBOX_VERSION} LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Bowlingbox needs a 64-bit toolchain (CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P})")
endif()

# Global project name used by library aliases (Bowlingbox::Core)
set(CMAKE_ROOT_NAME ${PROJECT_NAME})

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(BUILD_SHARED_LIBS     "Build project libraries as shared libraries" ON)
option(BUILD_APPS            "Build applications"                          ON)
# BUILD_WX_UI: AUTO (default) builds the wx applications when wxWidgets is found,
# ON requires wxWidgets, OFF leaves them out.
set(BUILD_WX_UI AUTO CACHE STRING "Build wxWidgets based applications (AUTO, ON, OFF)")
set_property(CACHE BUILD_WX_UI PROPERTY STRINGS AUTO ON OFF)
option(BUILD_VERBOSE_OUTPUT  "Build with verbose output"                   OFF)
option(BUILD_TESTING         "Build tests"                                 ON)
option(ENABLE_COVERAGE       "Enable code coverage instrumentation"        OFF)
option(ENABLE_CLANG_TIDY     "Run clang-tidy during compilation"           OFF)
option(GENERATE_SCAFFOLD     "Generate missing libraries from templates"   ON)

# Bowlingbox's cmake/ before buildkit's folders, which the loader appended.
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(Common)

# Libraries to generate / include. Add new names here for scaffold generation.
set(BOWLINGBOX_LIBRARIES
    Score
    Core
    Alley
    Audio
    Render
)

set(libraries_dir    ${CMAKE_SOURCE_DIR}/libraries)
set(applications_dir ${CMAKE_SOURCE_DIR}/applications)

if(GENERATE_SCAFFOLD)
    # Existing kata apps stay topic-based; scaffold only creates apps for *new* libs.
    # The templates are buildkit's (template/).
    generate_libraries(
        LIBRARIES_LIST     ${BOWLINGBOX_LIBRARIES}
        LIBRARIES_LOCATION ${libraries_dir}
        APPS_LOCATION      ${applications_dir}
    )
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty)

if(EXISTS "${libraries_dir}/CMakeLists.txt")
    add_subdirectory(${libraries_dir})
endif()

if(BUILD_APPS AND EXISTS "${applications_dir}/CMakeLists.txt")
    add_subdirectory(${applications_dir})
endif()

include(InstallConfig)

# Install smoke test: installs this build tree and builds a consumer project
# against the package (ci/install-smoke.sh). Linux only (bash, LD_LIBRARY_PATH).
if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    get_filename_component(_bowlingbox_cmake_bin "${CMAKE_COMMAND}" DIRECTORY)
    add_test(NAME install_smoke
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ci/install-smoke.sh ${CMAKE_BINARY_DIR})
    set_tests_properties(install_smoke PROPERTIES
        LABELS install
        # The cmake of this build, and ASan builds: the consumer is not
        # instrumented, so the ASan runtime is not first in its link order.
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_bowlingbox_cmake_bin};ASAN_OPTIONS=string_append::verify_asan_link_order=0")
endif()

if(BUILD_VERBOSE_OUTPUT)
    dump_cmake_vars(BuildLogEnd.log)
endif()
