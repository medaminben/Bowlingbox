# Target helpers.

# Convert "Bowlingbox::Core" -> "bowlingbox_core" for add_library().
macro(get_raw_target_name qualified_name out_var)
    string(REPLACE "::" "_" ${out_var} ${qualified_name})
    string(TOLOWER ${${out_var}} ${out_var})
endmacro()

# ---------------------------------------------------------------------------
# create_library — shared/static library with export header + optional tests
# ---------------------------------------------------------------------------
function(create_library)
    set(options)
    set(single_value_args LIB_NAME TEST_DISCOVER)
    set(list_args LIB_FILES PRIVATE_DEPENDENCIES PUBLIC_DEPENDENCIES
                  TEST_SOURCES TEST_DEPENDENCIES LIB_RSC)
    cmake_parse_arguments(PARSE_ARGV 0 parameter
        "${options}" "${single_value_args}" "${list_args}")

    foreach(arg IN LISTS parameter_UNPARSED_ARGUMENTS)
        message(WARNING "unparsed argument: ${arg}")
    endforeach()

    if(DEFINED CMAKE_ROOT_NAME AND NOT CMAKE_ROOT_NAME STREQUAL "")
        set(LIBRARY_NAME ${CMAKE_ROOT_NAME}::${parameter_LIB_NAME})
        get_raw_target_name(${LIBRARY_NAME} LIBRARY_NAME_RAW)
    else()
        set(LIBRARY_NAME ${parameter_LIB_NAME})
        set(LIBRARY_NAME_RAW ${LIBRARY_NAME})
    endif()

    add_library(${LIBRARY_NAME_RAW} ${parameter_LIB_FILES})
    add_library(${LIBRARY_NAME} ALIAS ${LIBRARY_NAME_RAW})

    include(GenerateExportHeader)
    string(TOUPPER ${CMAKE_ROOT_NAME} ROOT_FLAG)
    string(TOUPPER ${parameter_LIB_NAME} LIB_FLAG)

    set(_export_dir
        "${CMAKE_CURRENT_BINARY_DIR}/include/${CMAKE_ROOT_NAME}/${parameter_LIB_NAME}")
    file(MAKE_DIRECTORY "${_export_dir}")

    generate_export_header(${LIBRARY_NAME_RAW}
        EXPORT_FILE_NAME
            "${_export_dir}/${CMAKE_ROOT_NAME}_${parameter_LIB_NAME}_export.h"
        EXPORT_MACRO_NAME ${ROOT_FLAG}_${LIB_FLAG}_API
        BASE_NAME ${ROOT_FLAG}_${LIB_FLAG}
        DEFINE_NO_DEPRECATED
    )

    target_include_directories(${LIBRARY_NAME_RAW}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(${LIBRARY_NAME_RAW}
        PRIVATE ${parameter_PRIVATE_DEPENDENCIES}
        PUBLIC  ${parameter_PUBLIC_DEPENDENCIES}
    )

    bowlingbox_set_warnings(${LIBRARY_NAME_RAW})

    set_target_properties(${LIBRARY_NAME_RAW} PROPERTIES
        OUTPUT_NAME "${CMAKE_ROOT_NAME}${parameter_LIB_NAME}"
        EXPORT_NAME "${parameter_LIB_NAME}"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    install(TARGETS ${LIBRARY_NAME_RAW}
        EXPORT ${CMAKE_ROOT_NAME}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT ${BOWLINGBOX_RUNTIME_COMPONENT}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    if(BUILD_TESTING AND parameter_TEST_SOURCES)
        set(TEST_NAME "test_${LIBRARY_NAME_RAW}")
        set(GTEST_DEPENDENCIES GTest::gtest GTest::gmock GTest::gmock_main)

        build_gtest_executable(
            NAME ${TEST_NAME}
            SRC  ${parameter_TEST_SOURCES}
            DEPENDS
                ${parameter_TEST_DEPENDENCIES}
                ${GTEST_DEPENDENCIES}
                ${LIBRARY_NAME}
            DISCOVER ${parameter_TEST_DISCOVER}
        )
        if(DEFINED parameter_LIB_RSC)
            foreach(item IN LISTS parameter_LIB_RSC)
                set(_rsc "${item}")
                if(NOT IS_ABSOLUTE "${_rsc}")
                    set(_rsc "${CMAKE_CURRENT_SOURCE_DIR}/${item}")
                endif()
                if(EXISTS "${_rsc}")
                    file(COPY ${_rsc}
                        DESTINATION ${PROJECT_BUILD_OUTPUT}/data/${LIBRARY_NAME_RAW})
                endif()
            endforeach()
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# create_application — executable of kind ENTRY (Console or WX_ui).
# Callers guard WX_ui apps with if(BUILD_WX_UI).
# ---------------------------------------------------------------------------
function(create_application)
    # Empty keyword args (e.g. UI with no value) are allowed for app scaffolds.
    if(POLICY CMP0174)
        cmake_policy(SET CMP0174 NEW)
    endif()

    set(options)
    set(single_value_args NAME ENTRY UI COMPONENT)
    set(list_args HEADERS SOURCES RESOURCES BUILD_ARGS DEPENDENCIES)
    cmake_parse_arguments(PARSE_ARGV 0 app
        "${options}" "${single_value_args}" "${list_args}")

    foreach(arg IN LISTS app_UNPARSED_ARGUMENTS)
        message(WARNING "unparsed argument: ${arg}")
    endforeach()

    set(_entries Console WX_ui)
    if(NOT app_ENTRY IN_LIST _entries)
        message(FATAL_ERROR "${app_NAME}: ENTRY must be one of ${_entries}, got '${app_ENTRY}'")
    endif()
    if(app_ENTRY STREQUAL "WX_ui" AND NOT BUILD_WX_UI)
        message(FATAL_ERROR "${app_NAME}: ENTRY WX_ui needs BUILD_WX_UI=ON; guard the call with if(BUILD_WX_UI)")
    endif()

    set(PROJECT_FILES ${app_HEADERS} ${app_SOURCES} ${app_RESOURCES})
    if(DEFINED app_UI AND NOT app_UI STREQUAL "")
        list(APPEND PROJECT_FILES ${app_UI})
    endif()

    add_executable(${app_NAME} ${app_BUILD_ARGS} ${PROJECT_FILES})
    target_link_libraries(${app_NAME} PRIVATE ${app_DEPENDENCIES})
    if(NOT app_ENTRY STREQUAL "Console")
        set_target_properties(${app_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
            MACOSX_BUNDLE TRUE
        )
    endif()
    bowlingbox_set_warnings(${app_NAME})
    # COMPONENT puts the app into a named install component (the Windows
    # installer stages BOWLINGBOX_RUNTIME_COMPONENT only).
    set(_component_args)
    if(app_COMPONENT)
        set(_component_args COMPONENT ${app_COMPONENT})
    endif()
    install(TARGETS ${app_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} ${_component_args}
        BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR} ${_component_args}
    )
endfunction()
