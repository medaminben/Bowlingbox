# Consistent warning levels across compilers (treat warnings as errors).
# Kept to -Wall/-Wextra/-Wpedantic/-Werror so kata-style code (Codility,
# Gamebox casts/conversions) builds cleanly; stricter flags can return later.

function(sandbox_set_warnings target)
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
