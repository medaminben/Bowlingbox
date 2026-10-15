# Initial cache for the Windows installer build (scripts/build_windows_installer.ps1):
#   cmake -S . -B build/windows -G Ninja -C cmake/windows-installer.cmake
# Third-party code static (wx from source, GCC runtime), Bowlingbox libraries shared.
set(CMAKE_BUILD_TYPE          Release CACHE STRING "")
set(BUILD_SHARED_LIBS         ON      CACHE BOOL "")
set(BUILD_APPS                ON      CACHE BOOL "")
set(BUILD_WX_UI               ON      CACHE BOOL "")
set(BUILD_TESTING             ON      CACHE BOOL "")
set(GENERATE_SCAFFOLD         OFF     CACHE BOOL "")
set(BOWLINGBOX_USE_SYSTEM_WX  OFF     CACHE BOOL "")
set(BOWLINGBOX_STATIC_RUNTIME ON      CACHE BOOL "")
