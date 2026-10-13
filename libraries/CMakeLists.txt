# Library roots. Add new libraries via SANDBOX_LIBRARIES in the top-level
# CMakeLists.txt (GENERATE_SCAFFOLD) or by hand here.

# Only libraries that exist in this tree. Missing kata dirs (Pot, Tools, Geo,
# Codility, …) are skipped so the project still configures.
set(_sandbox_library_dirs Gamebox Core Pot Tools Geo Codility)
foreach(_lib IN LISTS _sandbox_library_dirs)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_lib}/CMakeLists.txt")
        add_subdirectory(${_lib})
    else()
        message(STATUS "library skipped (not present): ${_lib}")
    endif()
endforeach()
