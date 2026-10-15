project(Render VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    src/GlShader.h
    src/GlMesh.h
    src/ContextData.h
    src/PropBuilders.h
    include/Bowlingbox/Render/Geometry.h
    include/Bowlingbox/Render/Canvas2D.h
    include/Bowlingbox/Render/ScoreboardLayout.h
    include/Bowlingbox/Render/ScoreboardPainter.h
    include/Bowlingbox/Render/Environment.h
    include/Bowlingbox/Render/Render.h
)
set(TARGET_SOURCES
    src/Geometry.cpp
    src/Canvas2D.cpp
    src/ScoreboardLayout.cpp
    src/ScoreboardPainter.cpp
    src/Environment.cpp
    src/GlShader.cpp
    src/GlMesh.cpp
    src/Render.cpp
)
set(TARGET_RSC)
set(PRIVATE_DEPS
    ${CMAKE_ROOT_NAME}::Score
    $<BUILD_INTERFACE:glad::glad>
    $<BUILD_INTERFACE:stb::easy_font>
)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
    $<BUILD_INTERFACE:glm::glm>
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Render.cpp
    test/src/test_Environment.cpp
)
set(TEST_DEPS)
###############################################################
# build
###############################################################
create_library(
    LIB_NAME             ${TARGET_NAME}
    LIB_FILES            ${TARGET_FILES}
    PRIVATE_DEPENDENCIES ${PRIVATE_DEPS}
    PUBLIC_DEPENDENCIES  ${PUBLIC_DEPS}
    TEST_DISCOVER        ${TST_DISCOVER}
    TEST_SOURCES         ${TST_SOURCES}
    TEST_DEPENDENCIES    ${TEST_DEPS}
    LIB_RSC              ${TARGET_RSC}
)
