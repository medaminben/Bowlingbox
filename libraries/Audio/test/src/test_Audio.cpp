#include <Bowlingbox/Audio/Audio.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <set>
#include <string>

using namespace Bowlingbox::Audio;

TEST(test_audio_catalogue, every_sound_has_a_unique_wav_file_and_data) {
    std::set<std::string> names;
    for (SoundId id : kAllSounds) {
        const std::string name(fileName(id));
        EXPECT_TRUE(name.ends_with(".wav")) << name;
        EXPECT_TRUE(names.insert(name).second) << name;
        EXPECT_GT(embeddedData(id).size(), 44u) << name;  // more than a WAV header
    }
    EXPECT_EQ(names.size(), static_cast<std::size_t>(kSoundCount));
}

TEST(test_audio_catalogue, loops_are_roll_charge_and_the_ambiences) {
    for (SoundId id : kAllSounds) {
        const bool expected = id == SoundId::Roll || id == SoundId::ChargeTone || id == SoundId::AmbientNeon ||
                              id == SoundId::AmbientClassic || id == SoundId::AmbientOutdoor;
        EXPECT_EQ(isLoop(id), expected) << fileName(id);
    }
}

// Effects keep their highs at 44.1 kHz; the long ambience beds stay at 22.05 kHz to save space.
TEST(test_audio_catalogue, every_sound_decodes_to_mono_at_a_supported_rate_within_its_length_limit) {
    for (SoundId id : kAllSounds) {
        auto decoded = decodeInfo(id);
        ASSERT_TRUE(decoded.isValue()) << fileName(id);
        const SoundInfo info = decoded();
        EXPECT_EQ(info.channels, 1) << fileName(id);
        EXPECT_TRUE(info.sampleRate == 22050 || info.sampleRate == 44100) << fileName(id) << ": " << info.sampleRate;
        if (isLoop(id)) {
            EXPECT_GE(info.seconds, 0.5f) << fileName(id);
            EXPECT_LE(info.seconds, 45.f) << fileName(id);
        } else {
            EXPECT_GE(info.seconds, 0.05f) << fileName(id);
            EXPECT_LE(info.seconds, 3.f) << fileName(id);
        }
    }
}

TEST(test_audio_catalogue, embedded_sounds_stay_under_six_megabytes) {
    std::size_t total = 0;
    for (SoundId id : kAllSounds)
        total += embeddedData(id).size();
    EXPECT_LT(total, 6u * 1024u * 1024u);
}

TEST(test_audio_silent, silent_output_tracks_mute_and_ignores_cues) {
    auto out = makeSilentOutput();
    ASSERT_NE(out, nullptr);
    EXPECT_FALSE(out->muted());
    out->play(SoundId::Strike, Voice {});
    out->setLoop(LoopSlot::Ambient, SoundId::AmbientNeon, Voice {0.35f, 1.f, 0.f});
    out->setLoop(LoopSlot::Ambient, std::nullopt, Voice {});
    out->setMuted(true);
    EXPECT_TRUE(out->muted());
    out->setMuted(false);
    EXPECT_FALSE(out->muted());
}

TEST(test_audio_silent, silent_output_clamps_the_master_volume) {
    auto out = makeSilentOutput();
    EXPECT_FLOAT_EQ(out->volume(), 1.f);
    out->setVolume(0.4f);
    EXPECT_FLOAT_EQ(out->volume(), 0.4f);
    out->setVolume(2.f);
    EXPECT_FLOAT_EQ(out->volume(), 1.f);
    out->setVolume(-1.f);
    EXPECT_FLOAT_EQ(out->volume(), 0.f);
}
