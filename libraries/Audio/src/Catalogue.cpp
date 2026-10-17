#include "EmbeddedSounds.h"
#include <Bowlingbox/Audio/Audio.h>
#include <miniaudio.h>
#include <array>
#include <cstddef>
#include <string>

namespace Bowlingbox::Audio {
namespace {
struct Entry {
    std::string_view file;
    const unsigned char* data;
    const std::size_t* size;
    bool loop;
};

// Same order as SoundId.
const std::array<Entry, kSoundCount> kEntries {{
    {"roll.wav", embedded::kSound_roll, &embedded::kSound_rollSize, true},
    {"gutter_drop.wav", embedded::kSound_gutter_drop, &embedded::kSound_gutter_dropSize, false},
    {"pin_hit.wav", embedded::kSound_pin_hit, &embedded::kSound_pin_hitSize, false},
    {"pin_clatter.wav", embedded::kSound_pin_clatter, &embedded::kSound_pin_clatterSize, false},
    {"charge.wav", embedded::kSound_charge, &embedded::kSound_chargeSize, true},
    {"release.wav", embedded::kSound_release, &embedded::kSound_releaseSize, false},
    {"strike.wav", embedded::kSound_strike, &embedded::kSound_strikeSize, false},
    {"spare.wav", embedded::kSound_spare, &embedded::kSound_spareSize, false},
    {"new_game.wav", embedded::kSound_new_game, &embedded::kSound_new_gameSize, false},
    {"theme_switch.wav", embedded::kSound_theme_switch, &embedded::kSound_theme_switchSize, false},
    {"ambient_neon.wav", embedded::kSound_ambient_neon, &embedded::kSound_ambient_neonSize, true},
    {"ambient_classic.wav", embedded::kSound_ambient_classic, &embedded::kSound_ambient_classicSize, true},
    {"ambient_outdoor.wav", embedded::kSound_ambient_outdoor, &embedded::kSound_ambient_outdoorSize, true},
}};

const Entry& entry(SoundId id) {
    return kEntries[static_cast<std::size_t>(id)];
}

Core::Error<> decodeError(SoundId id, ma_result r) {
    return Core::Error<>(std::string(fileName(id)) + ": " + ma_result_description(r));
}
}  // namespace

std::string_view fileName(SoundId id) {
    return entry(id).file;
}

std::span<const unsigned char> embeddedData(SoundId id) {
    const Entry& e = entry(id);
    return {e.data, *e.size};
}

bool isLoop(SoundId id) {
    return entry(id).loop;
}

Result<SoundInfo> decodeInfo(SoundId id) {
    const auto data = embeddedData(id);
    ma_decoder decoder;
    if (const ma_result r = ma_decoder_init_memory(data.data(), data.size(), nullptr, &decoder); r != MA_SUCCESS)
        return decodeError(id, r);
    ma_uint64 frames = 0;
    const ma_result r = ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    const SoundInfo info {static_cast<int>(decoder.outputChannels), static_cast<int>(decoder.outputSampleRate),
                          static_cast<float>(frames) / static_cast<float>(decoder.outputSampleRate)};
    ma_decoder_uninit(&decoder);
    if (r != MA_SUCCESS)
        return decodeError(id, r);
    return info;
}

}  // namespace Bowlingbox::Audio
