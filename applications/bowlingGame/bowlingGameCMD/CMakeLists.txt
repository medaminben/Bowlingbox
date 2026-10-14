# BowlingConsole: text front-end. Everything except main() lives in
# bowlingconsole_core, so the tests link the same code as the app.
add_library(bowlingconsole_core STATIC
    banner.h banner.cpp
    bowlingScreen.h bowlingScreen.cpp
    scoreGrid.h scoreGrid.cpp
    session.h session.cpp
    terminal.h terminal.cpp
)
target_include_directories(bowlingconsole_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlingconsole_core PUBLIC ${CMAKE_ROOT_NAME}::Score)
bowlingbox_set_warnings(bowlingconsole_core)

create_application(
    NAME         BowlingConsole
    ENTRY        Console
    SOURCES      console.cpp
    DEPENDENCIES bowlingconsole_core
)

# Window-free tests: the game loop against a scripted terminal.
if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlingconsole
        SRC      test/test_Console.cpp test/test_Session.cpp test/test_ScoreGrid.cpp
        DEPENDS  bowlingconsole_core GTest::gtest GTest::gtest_main
        DISCOVER ON
    )
endif()
