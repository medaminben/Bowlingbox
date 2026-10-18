#ifndef BOWLINGGL_HUD_TEXT_H
#define BOWLINGGL_HUD_TEXT_H
#include "SoundSettings.h"
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <string>
#include <string_view>

/// The words shown to the player. Pure functions, no drawing.
namespace HudText {
std::string hint(Bowlingbox::Alley::Phase phase);
std::string phaseName(Bowlingbox::Alley::Phase phase);
enum class ThrowResult { None, Strike, Spare, Gutter, Pins };
/// What the last throw was, by the sheet's marks (a ten on a second ball is a
/// spare); None outside FrameDone.
ThrowResult throwResult(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet);
/// "STRIKE!", "SPARE!", "Gutter", "7 pins", "Game over - score N" or "".
std::string result(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet);
/// Status bar line.
std::string status(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet);
/// Status bar part for the sound: "volume 80%", "sound muted", plus ", no ambience".
std::string soundStatus(const SoundSettings& settings);
/// Text of the Help ▸ Controls dialog.
std::string controlsHelp();
/// Shown on stderr for --theme=<name> with an unknown name.
std::string unknownThemeMessage(std::string_view name);
}  // namespace HudText
#endif  // BOWLINGGL_HUD_TEXT_H
