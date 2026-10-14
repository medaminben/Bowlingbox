# BowlingDisplay: Qt Widgets score sheet (BUILD_QT_UI=ON). The form and its
# logic live in bowlingdisplay_core, so tests can open it without main().
add_library(bowlingdisplay_core STATIC
    display.h display.cpp display.ui
)
target_include_directories(bowlingdisplay_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlingdisplay_core PUBLIC
    ${CMAKE_ROOT_NAME}::Score
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
)
bowlingbox_set_warnings(bowlingdisplay_core)

create_application(
    NAME         BowlingDisplay
    ENTRY        QT_ui
    SOURCES      main.cpp
    DEPENDENCIES bowlingdisplay_core
)
