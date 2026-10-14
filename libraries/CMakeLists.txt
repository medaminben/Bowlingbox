# Library roots. Add new libraries via GAMEBOX_LIBRARIES in the top-level
# CMakeLists.txt (GENERATE_SCAFFOLD) or by hand here.

# Directories listed here but missing on disk are skipped so the project
# still configures.
set(_gamebox_library_dirs Score Core Alley Render)
foreach(_lib IN LISTS _gamebox_library_dirs)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_lib}/CMakeLists.txt")
        add_subdirectory(${_lib})
    else()
        message(STATUS "library skipped (not present): ${_lib}")
    endif()
endforeach()
