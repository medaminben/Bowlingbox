# In-tree third-party code (no network needed).
enable_language(C)

add_library(sandbox_glad STATIC glad/src/gl.c)
target_include_directories(sandbox_glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
target_link_libraries(sandbox_glad PUBLIC ${CMAKE_DL_LIBS})
set_target_properties(sandbox_glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(glad::glad ALIAS sandbox_glad)

add_library(sandbox_stb INTERFACE)
target_include_directories(sandbox_stb SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
add_library(stb::easy_font ALIAS sandbox_stb)
