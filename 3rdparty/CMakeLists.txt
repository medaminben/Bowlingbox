# In-tree third-party code (no network needed).
enable_language(C)

add_library(bowlingbox_glad STATIC glad/src/gl.c)
target_include_directories(bowlingbox_glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
target_link_libraries(bowlingbox_glad PUBLIC ${CMAKE_DL_LIBS})
set_target_properties(bowlingbox_glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(glad::glad ALIAS bowlingbox_glad)

add_library(bowlingbox_stb INTERFACE)
target_include_directories(bowlingbox_stb SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
add_library(stb::easy_font ALIAS bowlingbox_stb)
add_library(stb::truetype ALIAS bowlingbox_stb)
