project(BowlingGame LANGUAGES CXX)

# BUILD_APPS is checked by the root CMakeLists.txt before this directory is added.
add_subdirectory(bowlingGameCMD)
if(BUILD_WX_UI)
    add_subdirectory(bowlingGameGL)
    add_subdirectory(bowlingGameUI)
endif()
