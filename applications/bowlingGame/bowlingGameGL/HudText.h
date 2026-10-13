#ifndef BOWLINGGL_HUD_TEXT_H
#define BOWLINGGL_HUD_TEXT_H
#include <Gamebox/Alley/Types.h>
#include <Gamebox/Score/IBowlingGame.h>
#include <string>

/// The words shown to the player. Pure functions, no drawing.
namespace HudText {
std::string hint(Gamebox::Alley::Phase phase);
std::string phaseName(Gamebox::Alley::Phase phase);
/// "STRIKE!", "SPARE!", "Gutter", "7 pins", "Game over - score N" or "".
std::string result(const Gamebox::Alley::Snapshot& s, const Gamebox::Score::IScoreSheet& sheet);
/// Status bar line.
std::string status(const Gamebox::Alley::Snapshot& s, const Gamebox::Score::IScoreSheet& sheet);
}  // namespace HudText
#endif  // BOWLINGGL_HUD_TEXT_H
