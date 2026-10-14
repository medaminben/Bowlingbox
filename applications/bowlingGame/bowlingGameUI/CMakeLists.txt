set(EXECUTABLE_NAME BowlingDisplay)
set(EXEC_LIST)
set(THIS_DIR ${CMAKE_CURRENT_LIST_DIR})

add_project(${EXECUTABLE_NAME} EXEC_LIST) # add_guard#

# will be needed in case of missing 3rdparty
# dependencies to reject the build (example of
# usage: an app depends on boost and boost is
# not available also better to  discard
# building here)
set(BUILD_${EXECUTABLE_NAME}
    ${BUILD_QT_UI} AND ${BUILD_APPS}
    PARENT_SCOPE
)
#executable type
set(${EXECUTABLE_NAME}_ENTRY
    QT_ui
    PARENT_SCOPE
)
# Manage executable files
# header files
set(${EXECUTABLE_NAME}_HEADERS
    ${THIS_DIR}/display.h
    PARENT_SCOPE
)

# source files
set(${EXECUTABLE_NAME}_SOURCES
    ${THIS_DIR}/display.cpp
    ${THIS_DIR}/main.cpp
    PARENT_SCOPE
)

# forme files
set(${EXECUTABLE_NAME}_UI
    ${THIS_DIR}/display.ui
    PARENT_SCOPE
)

# ressources
set(${EXECUTABLE_NAME}_RSC
    PARENT_SCOPE
)

# Use a library name as a dependency.
# The "::" in the name forces CMake to look
# for a target instead of looking for bare
# library files on the filesystem, which can
# avoid some errors. Since CMake knows  the
# include paths and  the libraries  produced
# by the target, it will add them  automatically
# to the compiler and linker command lines.
set(${EXECUTABLE_NAME}_DEPS
    ${CMAKE_ROOT_NAME}::Score
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
    PARENT_SCOPE
)

set(${EXECUTABLE_NAME}_BUILD_ARGS
    MANUAL_FINALIZATION
    PARENT_SCOPE
)


set(PROJECT_LIST
    ${PROJECT_LIST}
    ${EXEC_LIST}
    PARENT_SCOPE
)