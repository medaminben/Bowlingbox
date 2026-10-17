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

# miniaudio: playback for Bowlingbox::Audio. No null backend, so a machine
# without an audio device fails ma_engine_init and the game falls back to silence.
find_package(Threads REQUIRED)
add_library(bowlingbox_miniaudio STATIC miniaudio/miniaudio.c)
target_include_directories(bowlingbox_miniaudio SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)
target_compile_definitions(bowlingbox_miniaudio PUBLIC MA_NO_ENCODING MA_NO_GENERATION MA_NO_NULL)
target_link_libraries(bowlingbox_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(bowlingbox_miniaudio PUBLIC m)
endif()
set_target_properties(bowlingbox_miniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
add_library(miniaudio::miniaudio ALIAS bowlingbox_miniaudio)

# Dev tool that turns downloaded samples into 3rdparty/sounds/*.wav. Not shipped:
# cmake --build build --target sound_convert
add_executable(sound_convert EXCLUDE_FROM_ALL sounds/tool/sound_convert.c)
target_include_directories(sound_convert SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_link_libraries(sound_convert PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(sound_convert PRIVATE m)
endif()
