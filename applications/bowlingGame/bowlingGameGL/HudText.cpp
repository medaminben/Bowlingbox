#include "HudText.h"
#include <Gamebox/Score/Notation.h>
#include <algorithm>

using Gamebox::Alley::Phase;
using Gamebox::Score::Mark;
using Gamebox::Score::toInt;

namespace {
/// The most recent roll slot that holds something (Strike / Spare marks included).
int lastRollValue(const std::vector<int>& rolls) {
    for (auto it = rolls.rbegin(); it != rolls.rend(); ++it)
        if (!Gamebox::Score::isBlank(*it))
            return *it;
    return toInt(Mark::None);
}
}  // namespace

namespace HudText {

std::string hint(Phase p) {
    switch (p) {
        case Phase::Aiming:   return "<- -> move   A D angle   hold Space to charge";
        case Phase::Charging: return "release to throw";
        case Phase::GameOver: return "N: new game   Esc: quit";
        default:              return "";
    }
}

std::string phaseName(Phase p) {
    switch (p) {
        case Phase::Aiming:    return "Aiming";
        case Phase::Charging:  return "Charging";
        case Phase::Rolling:   return "Rolling";
        case Phase::Settling:  return "Settling";
        case Phase::FrameDone: return "Frame done";
        case Phase::GameOver:  return "Game over";
    }
    return "";
}

std::string result(const Gamebox::Alley::Snapshot& s, const Gamebox::Score::IScoreSheet& sheet) {
    if (s.phase == Phase::GameOver)
        return "Game over - score " + std::to_string(sheet.score());
    if (s.phase != Phase::FrameDone || s.lastPinsDown < 0)
        return "";
    const int last = lastRollValue(sheet.rolls());
    if (last == toInt(Mark::Strike) || s.lastPinsDown == Gamebox::Score::kPins)
        return "STRIKE!";
    if (last == toInt(Mark::Spare))
        return "SPARE!";
    if (s.lastPinsDown == 0)
        return "Gutter";
    return std::to_string(s.lastPinsDown) + " pins";
}

std::string status(const Gamebox::Alley::Snapshot& s, const Gamebox::Score::IScoreSheet& sheet) {
    const auto standing = std::count(s.pinStanding.begin(), s.pinStanding.end(), true);
    return phaseName(s.phase) + "   |   score " + std::to_string(sheet.score()) +
           "   |   pins standing " + std::to_string(standing);
}

}  // namespace HudText
