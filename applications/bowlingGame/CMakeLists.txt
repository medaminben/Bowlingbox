project(BowlingGame LANGUAGES CXX)

# BUILD_APPS is checked by the root CMakeLists.txt before this directory is added.
add_subdirectory(bowlingGameCMD)
# BOWLINGBOX_WX_AVAILABLE: wxWidgets and OpenGL were found (cmake/WxSupport.cmake).
if(BUILD_WX_UI AND BOWLINGBOX_WX_AVAILABLE)
    add_subdirectory(bowlingGameGL)
    add_subdirectory(bowlingGameUI)
endif()
