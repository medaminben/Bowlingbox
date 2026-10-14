# In-tree third-party code (no network needed).
enable_language(C)

add_library(gamebox_glad STATIC glad/src/gl.c)
target_include_directories(gamebox_glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
target_link_libraries(gamebox_glad PUBLIC ${CMAKE_DL_LIBS})
set_target_properties(gamebox_glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(glad::glad ALIAS gamebox_glad)

add_library(gamebox_stb INTERFACE)
target_include_directories(gamebox_stb SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
add_library(stb::easy_font ALIAS gamebox_stb)
