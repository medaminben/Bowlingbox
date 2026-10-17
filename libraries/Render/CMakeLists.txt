project(Render VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
include(EmbedFile)
set(RUBIK_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/RubikMedium.cpp)
bowlingbox_embed_file(
    INPUT  ${CMAKE_SOURCE_DIR}/3rdparty/fonts/Rubik-Medium.ttf
    OUTPUT ${RUBIK_CPP}
    SYMBOL kRubikMedium
)
set(TARGET_HEADERS
    src/EmbeddedFont.h
    src/GlShader.h
    src/GlMesh.h
    src/ContextData.h
    src/PropBuilders.h
    src/EnvironmentDecor.h
    include/Bowlingbox/Render/Geometry.h
    include/Bowlingbox/Render/Canvas2D.h
    include/Bowlingbox/Render/ScoreboardLayout.h
    include/Bowlingbox/Render/ScoreboardPainter.h
    include/Bowlingbox/Render/Environment.h
    include/Bowlingbox/Render/Lighting.h
    include/Bowlingbox/Render/FontAtlas.h
    include/Bowlingbox/Render/Render.h
)
set(TARGET_SOURCES
    src/Geometry.cpp
    src/Canvas2D.cpp
    src/ScoreboardLayout.cpp
    src/ScoreboardPainter.cpp
    src/Environment.cpp
    src/EnvironmentDecor.cpp
    src/Lighting.cpp
    src/GlShader.cpp
    src/GlMesh.cpp
    src/FontAtlas.cpp
    src/Render.cpp
    ${RUBIK_CPP}
)
set(TARGET_RSC)
set(PRIVATE_DEPS
    ${CMAKE_ROOT_NAME}::Score
    $<BUILD_INTERFACE:glad::glad>
    $<BUILD_INTERFACE:stb::easy_font>
    $<BUILD_INTERFACE:stb::truetype>
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
    test/src/test_Lighting.cpp
    test/src/test_FontAtlas.cpp
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
