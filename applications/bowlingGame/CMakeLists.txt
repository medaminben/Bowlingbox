project(BowlingGame LANGUAGES CXX)
set(PROJECT_LIST)
#################################################
# add all the subprojects
#################################################
add_subdirectory(bowlingGameCMD)
add_subdirectory(bowlingGameUI)
#################################################
# Build all the subprojects
#################################################
foreach(item IN LISTS PROJECT_LIST)

    if(BUILD_${item})
        # create_application(
        #     NAME  "PROJECT_x_NAME"
        #     ENTRY ["Console" "QT_ui" "WX_ui" "Service"]
        #     HEADERS "<header.h ...>""
        #     SOURCES ">source.cpp ...>"
        #     RESOURCES "<file.txt ...>"
        #     UI "form.ui"
        #     BUILD_ARGS "<WIN32 ...>"
        #     DEPENDENCIES "internal::dependencies" "third_party::dependencies"
        # )
        create_application(
            NAME         ${item}
            ENTRY        ${${item}_ENTRY}
            HEADERS      ${${item}_HEADERS}
            SOURCES      ${${item}_SOURCES}
            RESOURCES    ${${item}_RSC}
            UI           ${${item}_UI}
            BUILD_ARGS   ${${item}_BUILD_ARGS}
            DEPENDENCIES ${${item}_DEPS}
        )
    endif()

endforeach()