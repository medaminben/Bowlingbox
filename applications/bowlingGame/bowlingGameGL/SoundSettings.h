#ifndef BOWLINGGL_SOUND_SETTINGS_H
#define BOWLINGGL_SOUND_SETTINGS_H
#include <algorithm>
#include "Command.h"

/// The player's sound preferences: what the Sound menu and keys change and what is saved.
struct SoundSettings {
    static constexpr int kVolumeStep = 10;  // percent per key press
    static constexpr int kDefaultVolume = 80;

    bool muted = false;
    int volume = kDefaultVolume;  // percent, 0..100
    bool ambience = true;         // the theme's background loop

    /// Moves the volume by steps * kVolumeStep, clamped to 0..100. Changing it unmutes.
    void stepVolume(int steps) {
        volume = std::clamp(volume + steps * kVolumeStep, 0, 100);
        muted = false;
    }
    /// Master gain for the audio output; squared so equal steps sound about equally loud.
    float gain() const {
        const float v = static_cast<float>(std::clamp(volume, 0, 100)) / 100.f;
        return v * v;
    }
    /// The settings after a sound command; any other command leaves them unchanged.
    SoundSettings after(Command::Type type) const {
        SoundSettings next = *this;
        switch (type) {
            case Command::Type::ToggleMute:     next.muted = !next.muted; break;
            case Command::Type::VolumeUp:       next.stepVolume(1); break;
            case Command::Type::VolumeDown:     next.stepVolume(-1); break;
            case Command::Type::ToggleAmbience: next.ambience = !next.ambience; break;
            default:                            break;
        }
        return next;
    }
    bool operator==(const SoundSettings&) const = default;
};
#endif  // BOWLINGGL_SOUND_SETTINGS_H
