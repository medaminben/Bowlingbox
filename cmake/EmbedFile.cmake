# bowlingbox_embed_file(INPUT <file> OUTPUT <file.cpp> SYMBOL <name> [NAMESPACE <ns>])
#
# Writes a C++ source that defines, in namespace <ns> (default Bowlingbox::Render::embedded),
#   extern const unsigned char <name>[];
#   extern const std::size_t   <name>Size;
# holding the bytes of <file>. Runs at configure time; editing <file> re-runs configure.
function(bowlingbox_embed_file)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "INPUT;OUTPUT;SYMBOL;NAMESPACE" "")
    if(NOT arg_NAMESPACE)
        set(arg_NAMESPACE Bowlingbox::Render::embedded)
    endif()
    file(READ "${arg_INPUT}" hex HEX)
    string(LENGTH "${hex}" hex_length)
    math(EXPR size "${hex_length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "((0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,))"
           "\\1\n" bytes "${bytes}")
    file(WRITE "${arg_OUTPUT}.tmp"
        "// Generated from ${arg_INPUT} by cmake/EmbedFile.cmake. Do not edit.\n"
        "#include <cstddef>\n"
        "namespace ${arg_NAMESPACE} {\n"
        "extern const unsigned char ${arg_SYMBOL}[] = {\n${bytes}};\n"
        "extern const std::size_t ${arg_SYMBOL}Size = ${size};\n"
        "}  // namespace ${arg_NAMESPACE}\n")
    file(COPY_FILE "${arg_OUTPUT}.tmp" "${arg_OUTPUT}" ONLY_IF_DIFFERENT)
    file(REMOVE "${arg_OUTPUT}.tmp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${arg_INPUT}")
endfunction()
