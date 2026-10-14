#ifndef BOWLINGGL_HUD_TEXT_H
#define BOWLINGGL_HUD_TEXT_H
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <string>

/// The words shown to the player. Pure functions, no drawing.
namespace HudText {
std::string hint(Bowlingbox::Alley::Phase phase);
std::string phaseName(Bowlingbox::Alley::Phase phase);
/// "STRIKE!", "SPARE!", "Gutter", "7 pins", "Game over - score N" or "".
std::string result(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet);
/// Status bar line.
std::string status(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet);
}  // namespace HudText
#endif  // BOWLINGGL_HUD_TEXT_H
