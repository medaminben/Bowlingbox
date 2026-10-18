project(Audio VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
include(EmbedFile)
set(SOUNDS_DIR ${CMAKE_SOURCE_DIR}/3rdparty/sounds)
# One entry per Audio::SoundId (src/Catalogue.cpp names each embedded symbol explicitly).
set(SOUND_NAMES
    roll gutter_drop pin_hit pin_clatter charge release strike spare
    new_game theme_switch ambient_neon ambient_classic ambient_outdoor
)
file(READ ${SOUNDS_DIR}/SOURCES.md SOUND_PROVENANCE)
file(GLOB SOUND_FILES_ON_DISK RELATIVE ${SOUNDS_DIR} ${SOUNDS_DIR}/*.wav)
foreach(wav IN LISTS SOUND_FILES_ON_DISK)
    string(FIND "${SOUND_PROVENANCE}" "`${wav}`" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "3rdparty/sounds/${wav} is not listed in 3rdparty/sounds/SOURCES.md")
    endif()
endforeach()
set(SOUND_SOURCES)
foreach(name IN LISTS SOUND_NAMES)
    if(NOT EXISTS ${SOUNDS_DIR}/${name}.wav)
        message(FATAL_ERROR "missing sound 3rdparty/sounds/${name}.wav")
    endif()
    set(out ${CMAKE_CURRENT_BINARY_DIR}/generated/Sound_${name}.cpp)
    bowlingbox_embed_file(
        INPUT     ${SOUNDS_DIR}/${name}.wav
        OUTPUT    ${out}
        SYMBOL    kSound_${name}
        NAMESPACE Bowlingbox::Audio::embedded
    )
    list(APPEND SOUND_SOURCES ${out})
endforeach()
set(TARGET_HEADERS
    src/EmbeddedSounds.h
    include/Bowlingbox/Audio/Audio.h
)
set(TARGET_SOURCES
    src/Catalogue.cpp
    src/SilentOutput.cpp
    src/DeviceOutput.cpp
    ${SOUND_SOURCES}
)
set(TARGET_RSC)
# miniaudio is an implementation detail; BUILD_INTERFACE keeps it out of the
# installed export set.
set(PRIVATE_DEPS
    $<BUILD_INTERFACE:miniaudio::miniaudio>
)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Audio.cpp
)
set(TEST_DEPS)
###############################################################
# build
###############################################################
create_library(
    LIB_NAME             ${TARGET_NAME}
    LIB_FILES            ${TARGET_FILES}
    PRIVATE_DEPENDENCIES ${PRIVATE_DEPS}
    PUBLIC_DEPENDENCIES  ${PUBLIC_DEPS}
    TEST_DISCOVER        ${TST_DISCOVER}
    TEST_SOURCES         ${TST_SOURCES}
    TEST_DEPENDENCIES    ${TEST_DEPS}
    LIB_RSC              ${TARGET_RSC}
)
