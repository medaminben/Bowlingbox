#ifndef BOWLINGBOX_AUDIO_H
#define BOWLINGBOX_AUDIO_H
#include <Bowlingbox/Audio/Bowlingbox_Audio_export.h>
#include <Bowlingbox/Core/Core.h>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace Bowlingbox::Audio {

/// Every embedded sound. Files live in 3rdparty/sounds (see SOURCES.md there).
enum class SoundId {
    Roll, GutterDrop, PinHit, PinClatter, ChargeTone, Release, Strike, Spare,
    NewGame, ThemeSwitch, AmbientNeon, AmbientClassic, AmbientOutdoor
};
inline constexpr int kSoundCount = 13;
inline constexpr std::array<SoundId, kSoundCount> kAllSounds {
    SoundId::Roll, SoundId::GutterDrop, SoundId::PinHit, SoundId::PinClatter, SoundId::ChargeTone,
    SoundId::Release, SoundId::Strike, SoundId::Spare, SoundId::NewGame,
    SoundId::ThemeSwitch, SoundId::AmbientNeon, SoundId::AmbientClassic, SoundId::AmbientOutdoor};

/// Each slot plays at most one looping sound.
enum class LoopSlot { Roll, Charge, Ambient };
inline constexpr int kLoopSlotCount = 3;

struct Voice {
    float volume = 1.f;  // [0, 1]
    float pitch = 1.f;   // playback rate, > 0
    float pan = 0.f;     // -1 left .. +1 right
    bool operator==(const Voice&) const = default;
};

struct SoundInfo {
    int channels = 0;
    int sampleRate = 0;
    float seconds = 0.f;
};

class BOWLINGBOX_AUDIO_API IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual void play(SoundId id, const Voice& voice) = 0;
    /// nullopt stops the slot; the same id updates the voice without restarting;
    /// a different id restarts the slot with that sound.
    virtual void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& voice) = 0;
    virtual void setMuted(bool muted) = 0;
    virtual bool muted() const = 0;
    /// Master gain in [0, 1] for everything; mute silences on top of it.
    virtual void setVolume(float volume) = 0;
    virtual float volume() const = 0;
};

BOWLINGBOX_AUDIO_API std::string_view fileName(SoundId id);  // "roll.wav"
BOWLINGBOX_AUDIO_API std::span<const unsigned char> embeddedData(SoundId id);
BOWLINGBOX_AUDIO_API bool isLoop(SoundId id);
/// Decodes the embedded file without an audio device.
BOWLINGBOX_AUDIO_API Result<SoundInfo> decodeInfo(SoundId id);

/// Plays through the default audio device; fails if there is none.
BOWLINGBOX_AUDIO_API Result<std::unique_ptr<IAudioOutput>> createDeviceOutput();
/// Plays nothing; used when there is no device.
BOWLINGBOX_AUDIO_API std::unique_ptr<IAudioOutput> makeSilentOutput();

}  // namespace Bowlingbox::Audio
#endif  // BOWLINGBOX_AUDIO_H
