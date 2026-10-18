#ifndef BOWLINGGL_THEME_SOUNDS_H
#define BOWLINGGL_THEME_SOUNDS_H
#include <Bowlingbox/Audio/Audio.h>
#include <Bowlingbox/Render/Environment.h>

/// The ambience loop of each theme: the one place where Render themes meet Audio sounds.
Bowlingbox::Audio::SoundId ambientFor(Bowlingbox::Render::ThemeId id);
#endif  // BOWLINGGL_THEME_SOUNDS_H
