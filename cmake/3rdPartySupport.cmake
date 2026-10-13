#
# read_from_ini_file( CONF_NAME "file_name"
#                     POSTFIX   "_something_to_tag_the_values"
#                     KEYS      "specefic" "value_keys" "if_no_given_will_parse_all"
# )
function(read_from_ini_file)
    # parse function arguments
    set(options)
    set(single_value_args CONF_NAME POSTFIX)
    set(list_args KEYS)
    cmake_parse_arguments(PARSE_ARGV 0 ini
        "${options}"
        "${single_value_args}"
        "${list_args}"
    )
    foreach(arg IN LISTS ini_UNPARSED_ARGUMENTS)
        prompt("unparsed argumemnt: ${arg}")
    endforeach()

    if(NOT DEFINED ini_CONF_NAME)
        prompt("config file name not defined")
        return()
    endif()

    prompt("call of ini parser")
    if(NOT DEFINED ini_POSTFIX)
        prompt("special postfix for ini file is missing so it will be set to default \"INI_VALUE\"")
        set(postfix "INI_VALUE")
    else()
        set(postfix ${ini_POSTFIX})
    endif()

    file(READ ${ini_CONF_NAME} config_file_text_stream)
    #message("${ini_CONF_NAME} : ${config_file_text_stream}")
    # Turn the contents into a list of strings, each ending with a ";".
    string(REPLACE "\n" ";" config_file_lines "${config_file_text_stream}")

    if(NOT DEFINED ini_KEYS)
        # get all the values in the ini file
        foreach(line ${config_file_lines})
            if(${line} MATCHES "=")  # key=value
                string(REGEX REPLACE "^(.*)=" "" value "${line}")
                string(REPLACE "=${value}" "" key "${line}")
                # make available to caller
                set(${key}_${postfix} ${value} PARENT_SCOPE)
            endif()
        endforeach()
    else()
        # loop through the key list specified by the consumer
        foreach(key IN LISTS ini_KEYS)
            # loop through each line in the file
            foreach(line ${config_file_lines})
                if(${line} MATCHES "${key}")
                    #remove the key, leave the content untouch
                    string(REPLACE "${key}=" "" value "${line}")
                    # make available to caller
                    set(${key}_${postfix} ${value} PARENT_SCOPE)
                endif()
            endforeach()
        endforeach()
    endif()
endfunction()
#
# import_3rd_party_properties(
#                  CONF_NAME "file_name"
#                  POSTFIX   "_something_to_tag_the_values"
#                  CONF_DIR  "directory/of/the/file/if/not/set/will/check/in/cmake/configs"
#                  KEYS      "specefic" "value_keys" "if_no_given_will_parse_all"
# )
# This import the 3rd party properties such external library directories
# it s useful for huge external dependencies such as wxWidgets OpenCV and others
function(import_3rd_party_properties)
    # parse function arguments
    set(options)
    set(single_value_args CONF_NAME CONF_DIR POSTFIX)
    set(list_args CONF_TYPES KEYS)
    cmake_parse_arguments(PARSE_ARGV 0 params "${options}" "${single_value_args}" "${list_args}")

    if(DEFINED params_UNPARSED_ARGUMENTS)
        prompt("unparsed argumemnt: ${arg}")
    endif()
    # error check
    # no file nname than return
    if(NOT DEFINED params_CONF_NAME)
        prompt(WARNING "reading file fails that the CONF_NAME is missing")
        return()
    endif()
    if(NOT DEFINED params_POSTFIX)
        prompt("special postfix (flag to identify the values) for ini file is missing, so it will be set to default \"INI_VALUE\"")
        set(postfix "INI_VALUE")
    else()
        set(postfix ${params_POSTFIX})
    endif()
    if(DEFINED params_CONF_DIR)
        set(config_dir "${params_CONF_DIR}")
    else()
        prompt("file directory is missing, it will be set to the default: ${CMAKE_SOURCE_DIR}/cmake/configs")
        set(config_dir ${CMAKE_SOURCE_DIR}/cmake/configs)
    endif()

    # get all files in the config dir
    file(GLOB_RECURSE  config_files  "${config_dir}/*")
    # select just a specific extensions if defined
    # else just sselect all
    if(NOT DEFINED params_CONF_TYPES)
        foreach(f ${config_files})
            # add without condition if is not added yet
            add_unique_to_list(${f} configs "")
        endforeach()
    else()
        foreach(t ${params_CONF_TYPES})
            string(TOLOWER ${t} tl)
            foreach(f ${config_files})
                # condition: if f matches the tl extension
                # will be added if not already
                add_unique_to_list(${f} configs "\\.(${tl})$")
            endforeach()
        endforeach()
    endif()

    foreach(conf ${configs})
        if(${conf} MATCHES "${params_CONF_NAME}")
            if(${conf} MATCHES "\\.ini$")
                if(DEFINED params_KEYS)
                    read_from_ini_file( CONF_NAME ${conf}
                                        KEYS      ${params_KEYS}
                                        POSTFIX   ${postfix}
                    )
                    # make available to caller
                    foreach(key ${params_KEYS})
                        set(${key}_${postfix} ${${key}_${postfix}} PARENT_SCOPE)
                    endforeach()
                else()
                    read_from_ini_file( CONF_NAME ${conf}
                                        POSTFIX   ${postfix}
                    )
                    get_Variables(var_list ${postfix})
                    # make available to caller
                    foreach(key ${var_list})
                        set(${key} ${${key}} PARENT_SCOPE)
                    endforeach()
                endif()
            # elseif(${conf} MATCHES "\\.json$")
            #     read_from_json_config(CONF_NAME ${conf})
            # elseif(${conf} MATCHES "\\.csv$")
            #     read_from_csv_file(CONF_NAME ${conf})
            endif()
        endif()
    endforeach()
endfunction()

# #work in progress to parse a env variable from a jsom file
# function(read_from_json_config)
#     # parse function arguments
#     set(options)
#     set(single_value_args CONF_NAME)
#     set(list_args KEYS)
#     cmake_parse_arguments(PARSE_ARGV 0 json "${options}"  "${single_value_args}" "${list_args}")
#
#     foreach(arg IN LISTS json_UNPARSED_ARGUMENTS)
#         prompt(" >>>>>> unparsed argumemnt: ${arg}")
#     endforeach()
#     if(NOT DEFINED json_CONF_NAME)
#         prompt("config file name not defined")
#     endif()
#     prompt(" >>>>>> call of json parser")
# endfunction(read_from_json_config)

# #work in progress to parse a env variable from a csv file
# function(read_from_csv_file)
#     # parse function arguments
#     set(options)
#     set(single_value_args CONF_NAME)
#     set(list_args KEYS)
#     cmake_parse_arguments(PARSE_ARGV 0 csv "${options}" "${single_value_args}" "${list_args}")
#
#     foreach(arg IN LISTS csv_UNPARSED_ARGUMENTS)
#         prompt(" >>>>>> unparsed argumemnt: ${arg}")
#     endforeach()
#     if(NOT DEFINED csv_CONF_NAME)
#         prompt("config file name not defined")
#     endif()
#     prompt(" >>>>>> call of csv parser")
# endfunction(read_from_csv_file)