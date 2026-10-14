# Don't use PROJECT_NAME as target name,
# using a proper executable name gives
# more freedom

#################################################
# executable
#################################################
set(EXECUTABLE_NAME BowlingConsole)
set(EXEC_LIST)
set(THIS_DIR ${CMAKE_CURRENT_LIST_DIR})

add_project(${EXECUTABLE_NAME} EXEC_LIST) # add_guard

set(BUILD_${EXECUTABLE_NAME}
    ${BUILD_APPS}
    PARENT_SCOPE
)
# executable type
set(${EXECUTABLE_NAME}_ENTRY
    Console
    PARENT_SCOPE
)

# header files
set(${EXECUTABLE_NAME}_HEADERS
    ${THIS_DIR}/bowlingScreen.h
    ${THIS_DIR}/scoreGrid.h
    ${THIS_DIR}/screenTools.h
    ${THIS_DIR}/terminal.h
    PARENT_SCOPE
)

# source files
set(${EXECUTABLE_NAME}_SOURCES
    ${THIS_DIR}/console.cpp
    ${THIS_DIR}/bowlingScreen.cpp
    ${THIS_DIR}/scoreGrid.cpp
    ${THIS_DIR}/screenTools.cpp
    ${THIS_DIR}/terminal.cpp
    PARENT_SCOPE
)

# forme files
set(${EXECUTABLE_NAME}_UI
    PARENT_SCOPE
)

# ressources
set(${EXECUTABLE_NAME}_RSC
    PARENT_SCOPE
)
# dependencies
set(${EXECUTABLE_NAME}_DEPS
    ${CMAKE_ROOT_NAME}::Score
    PARENT_SCOPE
)
# compile arguments
set(${EXECUTABLE_NAME}_BUILD_ARGS
    PARENT_SCOPE
)
set(PROJECT_LIST
    ${PROJECT_LIST}
    ${EXEC_LIST}
    PARENT_SCOPE
)

# Window-free tests: the game loop against a scripted terminal.
if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlingconsole
        SRC      ${THIS_DIR}/bowlingScreen.cpp ${THIS_DIR}/scoreGrid.cpp ${THIS_DIR}/screenTools.cpp
                 ${THIS_DIR}/test/test_Console.cpp
        DEPENDS  ${CMAKE_ROOT_NAME}::Score GTest::gtest GTest::gtest_main
        DISCOVER ON
    )
    target_include_directories(test_bowlingconsole PRIVATE ${THIS_DIR})
endif()


