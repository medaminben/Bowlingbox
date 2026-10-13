set(EXECUTABLE_NAME BowlingGL)
set(EXEC_LIST)
set(THIS_DIR ${CMAKE_CURRENT_LIST_DIR})

add_project(${EXECUTABLE_NAME} EXEC_LIST) # add_guard

if(BUILD_WX_UI AND BUILD_APPS)
    set(BUILD_${EXECUTABLE_NAME} ON PARENT_SCOPE)
else()
    set(BUILD_${EXECUTABLE_NAME} OFF PARENT_SCOPE)
endif()
set(${EXECUTABLE_NAME}_ENTRY
    WX_ui
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_HEADERS
    ${THIS_DIR}/InputMapper.h
    ${THIS_DIR}/SceneAdapter.h
    ${THIS_DIR}/BowlingCanvas.h
    ${THIS_DIR}/BowlingFrame.h
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_SOURCES
    ${THIS_DIR}/InputMapper.cpp
    ${THIS_DIR}/SceneAdapter.cpp
    ${THIS_DIR}/BowlingCanvas.cpp
    ${THIS_DIR}/BowlingFrame.cpp
    ${THIS_DIR}/main.cpp
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_UI PARENT_SCOPE)
set(${EXECUTABLE_NAME}_RSC PARENT_SCOPE)
set(${EXECUTABLE_NAME}_DEPS
    ${CMAKE_ROOT_NAME}::Alley
    ${CMAKE_ROOT_NAME}::Render
    wx::gl
    wx::core
    wx::base
    OpenGL::GL
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_BUILD_ARGS PARENT_SCOPE)

set(PROJECT_LIST
    ${PROJECT_LIST}
    ${EXEC_LIST}
    PARENT_SCOPE
)

# Window-free unit test for the input mapping.
if(BUILD_TESTING AND BUILD_WX_UI)
    build_gtest_executable(
        NAME     test_bowlinggl_input
        SRC      ${THIS_DIR}/InputMapper.cpp ${THIS_DIR}/test/test_InputMapper.cpp
        DEPENDS  ${CMAKE_ROOT_NAME}::Alley wx::core wx::base
                 GTest::gtest GTest::gmock GTest::gmock_main
        DISCOVER ON
    )
    target_include_directories(test_bowlinggl_input PRIVATE ${THIS_DIR})
endif()
