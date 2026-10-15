# BowlingDisplay: wxWidgets score sheet (BUILD_WX_UI=ON). The form and its
# logic live in bowlingdisplay_core, so tests can open it without main().
add_library(bowlingdisplay_core STATIC
    ScoreSheetFrame.h ScoreSheetFrame.cpp
)
target_include_directories(bowlingdisplay_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlingdisplay_core PUBLIC
    ${CMAKE_ROOT_NAME}::Score
    wx::core
    wx::base
)
bowlingbox_set_warnings(bowlingdisplay_core)

create_application(
    NAME         BowlingDisplay
    ENTRY        WX_ui
    SOURCES      main.cpp
    DEPENDENCIES bowlingdisplay_core
)

if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlingdisplay
        SRC      test/test_Display.cpp
        DEPENDS  bowlingdisplay_core GTest::gtest
        DISCOVER ON
        LABELS   "unit;gui"
    )
endif()
