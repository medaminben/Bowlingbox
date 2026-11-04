# Bowlingbox project settings. Included from the root CMakeLists.txt after
# project(). The generic modules (BuildUtils, LibraryGenerator, BuildLocation,
# CompilerWarnings, TestUtils, Dependency, ...) come from buildkit, which the
# loader (cmake/buildkit.cmake) includes before project().

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
# Coverage did nothing when buildkit loaded it before project(); now the
# compiler is known.
include(Coverage)

# Installer builds link the GCC runtime into every module, so BowlingGL.exe and
# the Bowlingbox DLLs import no libstdc++/libgcc/winpthread DLLs. On PE targets
# -static only changes how -l names are searched; import libraries passed by
# full path (our libBowlingbox*.dll.a) and the system import archives still link.
option(BOWLINGBOX_STATIC_RUNTIME "Link the GCC runtime statically (MinGW GCC only)" OFF)
if(BOWLINGBOX_STATIC_RUNTIME)
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(APPEND CMAKE_EXE_LINKER_FLAGS    " -static-libgcc -static-libstdc++ -static")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -static-libgcc -static-libstdc++ -static")
    else()
        message(WARNING "BOWLINGBOX_STATIC_RUNTIME applies to MinGW GCC only; ignored for "
                        "${CMAKE_CXX_COMPILER_ID} on ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

# Install component holding exactly what the Windows installer ships:
# BowlingGL, the Bowlingbox DLLs, LICENSE and THIRD_PARTY_NOTICES.txt.
set(BOWLINGBOX_RUNTIME_COMPONENT BowlingBox)

# Windows installer: the licence files ship beside BowlingGL.exe.
if(WIN32)
    install(
        FILES
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${CMAKE_SOURCE_DIR}/installer/THIRD_PARTY_NOTICES.txt"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT ${BOWLINGBOX_RUNTIME_COMPONENT}
    )
endif()

# Fetched projects must honour our normal variables in their option() calls
# (CMP0077) and must not drop them when creating cache entries (CMP0126).
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)

if(BUILD_VERBOSE_OUTPUT)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    dump_cmake_vars(configLogStart.log)
endif()

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
    endif()
endif()

if(BUILD_TESTING)
    message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
    include(CTest)
    enable_testing()
endif()

# Third-party code comes through dev_dependency(): an installed package first,
# else a download. -DBUILDKIT_OFFLINE=ON never downloads.
include(BulletSupport)   # Bowlingbox::Alley always needs it (REQUIRED)
include(GlmSupport)      # Bowlingbox::Render always needs it (REQUIRED)
# The wx applications need wxWidgets; BowlingConsole and the libraries do not
# (OPTIONAL with BUILD_WX_UI=AUTO, REQUIRED with ON). BOWLINGBOX_WX_AVAILABLE
# tells applications/ whether to add them.
set(BOWLINGBOX_WX_AVAILABLE FALSE)
if(BUILD_WX_UI)
    include(WxSupport)
endif()
if(BUILD_VERBOSE_OUTPUT)
    dump_cmake_vars(configLogEnd.log)
endif()
