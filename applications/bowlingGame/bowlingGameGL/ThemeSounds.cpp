#include "ThemeSounds.h"

using Bowlingbox::Audio::SoundId;
using Bowlingbox::Render::ThemeId;

SoundId ambientFor(ThemeId id) {
    switch (id) {
        case ThemeId::NeonHall:     return SoundId::AmbientNeon;
        case ThemeId::ClassicAlley: return SoundId::AmbientClassic;
        case ThemeId::Outdoor:      return SoundId::AmbientOutdoor;
    }
    return SoundId::AmbientNeon;
}
