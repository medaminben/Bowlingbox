# Outputs live in the build tree, so separate build directories under build/
# (build/debug, build/release, worktrees, CI) never overwrite each other's binaries.
set(PROJECT_BUILD_OUTPUT              ${CMAKE_BINARY_DIR})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY   ${PROJECT_BUILD_OUTPUT}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY   ${PROJECT_BUILD_OUTPUT}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY   ${PROJECT_BUILD_OUTPUT}/bin)
if(UNIX)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BUILD_OUTPUT}/lib)
endif()
