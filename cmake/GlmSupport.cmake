# GLM maths, header-only.
include(FetchContent)

set(BOWLINGBOX_GLM_VERSION "1.0.1" CACHE STRING "GLM release tag")

FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/${BOWLINGBOX_GLM_VERSION}.tar.gz
    SYSTEM
)
set(GLM_BUILD_LIBRARY OFF)
set(GLM_BUILD_TESTS OFF)
set(GLM_BUILD_INSTALL OFF)
set(GLM_ENABLE_CXX_20 ON)
FetchContent_MakeAvailable(glm)
