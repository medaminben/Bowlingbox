# Consistent warning levels across compilers (treat warnings as errors).
# Kept to -Wall/-Wextra/-Wpedantic/-Werror; stricter flags can be added
# per target later.

function(bowlingbox_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endfunction()
