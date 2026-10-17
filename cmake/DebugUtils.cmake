# Dump all CMake variables to a text file for troubleshooting purposes
# The file is called CMakeVariablesDump.txt and it is located in the
# "CMakeFiles" directory corresponding to the binary directory which
# is active at the point where the function is called.
function(dump_cmake_vars)
    set(LOG_OUTPUT ${PROJECT_BUILD_OUTPUT}/log)
    set (args ${ARGN})

    # if optional args?
    set(NO_NAME ON)
    foreach(arg ${args})
        if("${arg}" MATCHES "\.txt$" OR "${arg}" MATCHES "\.log$")
            set(VARIABLESDUMP_FILE ${LOG_OUTPUT}/${arg})
            set(NO_NAME OFF)
            break()
        endif()
    endforeach()
    if(NO_NAME)
        set(VARIABLESDUMP_FILE ${LOG_OUTPUT}/cmake_vars_dump.log)
    endif()
    unset(NO_NAME)

    get_cmake_property(_variableNames VARIABLES)
    list(SORT _variableNames)
    list(REMOVE_DUPLICATES _variableNames) # variables appear twice when set from set_variables_from_env
    file(WRITE "${VARIABLESDUMP_FILE}" "") # Remove existing content
    foreach (_variableName ${_variableNames})
        file(APPEND "${VARIABLESDUMP_FILE}" "${_variableName}= ${${_variableName}}\n")
    endforeach()
    message(STATUS "Dumped all CMake variables to ${VARIABLESDUMP_FILE}")
endfunction()

# Print the name and the value of a variable
# Useful for a quick check.
function(print)
    set (args ${ARGN})
    foreach(arg ${args})
        message(">>>>>> ${arg} = ${${arg}}")
    endforeach()
endfunction()


# Print a message in verbose mode.
function(prompt)
    if(NOT BUILD_VERBOSE_OUTPUT)
        return()
    endif()
    set(options WARNING STATUS)
    set(single_value_args)
    set(list_args)

    cmake_parse_arguments( PARSE_ARGV 0 prom
        "${options}" "${single_value_args}" "${list_args}"
    )
    if(prom_WARNING)
        message(WARNING " /!\\  ${prom_UNPARSED_ARGUMENTS}")
    else()
        message(STATUS " >>>>>> ${prom_UNPARSED_ARGUMENTS}")
    endif()
endfunction()
