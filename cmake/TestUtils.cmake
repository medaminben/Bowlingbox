include(GTestSupport)

# build_gtest_executable(
#   NAME test_gamebox_core
#   SRC  test/src/test_Core.cpp
#   DEPENDS Gamebox::Core GTest::gtest ...
#   DISCOVER ON
# )
function(build_gtest_executable)
    set(options)
    set(single_value_args NAME DISCOVER)
    set(list_args SRC DEPENDS)
    cmake_parse_arguments(PARSE_ARGV 0 test
        "${options}" "${single_value_args}" "${list_args}")

    foreach(arg IN LISTS test_UNPARSED_ARGUMENTS)
        message(WARNING "unparsed argument: ${arg}")
    endforeach()

    if(NOT "${test_NAME}" MATCHES "^test_")
        message(FATAL_ERROR "${test_NAME}: test executable name must start with test_")
    endif()

    if("${test_NAME}" MATCHES "[A-Z]")
        message(FATAL_ERROR "${test_NAME}: test executable name must be lowercase")
    endif()

    if("${test_SRC}" STREQUAL "")
        message(FATAL_ERROR "${test_NAME}: missing SRC files")
    endif()

    add_executable(${test_NAME} ${test_SRC})
    target_link_libraries(${test_NAME} PRIVATE ${test_DEPENDS})
    # Keep tests building against third-party headers without treating their
    # diagnostics as project errors.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_NAME} PRIVATE -Wall -Wextra -Wno-error)
    endif()

    if(test_DISCOVER)
        # Tests (e.g. Tools) resolve fixtures via ../data/<target>/ relative to
        # the runtime output dir where binaries and copied resources live.
        set(_test_wd ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        if(NOT _test_wd)
            set(_test_wd ${PROJECT_BUILD_OUTPUT}/bin)
        endif()
        gtest_discover_tests(${test_NAME}
            WORKING_DIRECTORY ${_test_wd}
            PROPERTIES LABELS "unit"
        )
    else()
        add_test(NAME ${test_NAME} COMMAND ${test_NAME})
    endif()
endfunction()
