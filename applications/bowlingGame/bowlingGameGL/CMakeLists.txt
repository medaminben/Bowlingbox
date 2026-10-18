# BowlingGL: wxWidgets + OpenGL game (BUILD_WX_UI=ON). The window-free units
# live in bowlinggl_core; the executable adds the wx window classes and main().
add_library(bowlinggl_core STATIC
    Command.h
    FixedStep.h FixedStep.cpp
    InputMapper.h InputMapper.cpp
    LaneCommands.h LaneCommands.cpp
    FollowCamera.h FollowCamera.cpp
    HudText.h HudText.cpp
    Hud.h Hud.cpp
    SceneAdapter.h SceneAdapter.cpp
    SoundDirector.h SoundDirector.cpp
    SoundSettings.h
    ThemeSounds.h ThemeSounds.cpp
)
target_include_directories(bowlinggl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlinggl_core PUBLIC
    ${CMAKE_ROOT_NAME}::Alley
    ${CMAKE_ROOT_NAME}::Audio
    ${CMAKE_ROOT_NAME}::Render
    wx::core
    wx::base
)
bowlingbox_set_warnings(bowlinggl_core)

create_application(
    NAME         BowlingGL
    ENTRY        WX_ui
    COMPONENT    ${BOWLINGBOX_RUNTIME_COMPONENT}
    SOURCES      BowlingCanvas.h BowlingCanvas.cpp
                 BowlingFrame.h BowlingFrame.cpp
                 main.cpp
    DEPENDENCIES bowlinggl_core wx::gl OpenGL::GL
)

# Window-free unit tests: input mapping, lane commands, camera rig, HUD.
if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlinggl
        SRC      test/test_InputMapper.cpp test/test_Gameplay.cpp test/test_SoundDirector.cpp
        DEPENDS  bowlinggl_core GTest::gtest GTest::gmock GTest::gmock_main
        DISCOVER ON
    )
endif()
