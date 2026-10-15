#include "HudText.h"
#include <Bowlingbox/Render/Environment.h>
#include <Bowlingbox/Score/Notation.h>
#include <algorithm>
#include <cstddef>

using Bowlingbox::Alley::Phase;
using Bowlingbox::Score::Mark;
using Bowlingbox::Score::toInt;

namespace {
/// The most recent roll slot that holds something (Strike / Spare marks included).
int lastRollValue(const std::vector<int>& rolls) {
    for (auto it = rolls.rbegin(); it != rolls.rend(); ++it)
        if (!Bowlingbox::Score::isBlank(*it))
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

std::string result(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet) {
    if (s.phase == Phase::GameOver)
        return "Game over - score " + std::to_string(sheet.score());
    if (s.phase != Phase::FrameDone || s.lastPinsDown < 0)
        return "";
    const int last = lastRollValue(sheet.rolls());
    // The sheet's mark decides: ten pins on a second ball are a spare.
    if (last == toInt(Mark::Spare))
        return "SPARE!";
    if (last == toInt(Mark::Strike) || s.lastPinsDown == Bowlingbox::Score::kPins)
        return "STRIKE!";
    if (s.lastPinsDown == 0)
        return "Gutter";
    return std::to_string(s.lastPinsDown) + " pins";
}

std::string status(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet) {
    const auto standing = std::count(s.pinStanding.begin(), s.pinStanding.end(), true);
    return phaseName(s.phase) + "   |   score " + std::to_string(sheet.score()) +
           "   |   pins standing " + std::to_string(standing);
}

std::string controlsHelp() {
    return "Left / Right or mouse move: position the ball\n"
           "A / D or right-drag: aim angle\n"
           "Hold Space or left button: charge, release to throw\n"
           "T: change the scenery\n"
           "N: new game    Esc / Ctrl+Q: quit";
}

std::string unknownThemeMessage(std::string_view name) {
    std::string names;
    const auto& ids = Bowlingbox::Render::kThemes;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0)
            names += i + 1 == ids.size() ? " or " : ", ";
        names += Bowlingbox::Render::theme(ids[i]).name;
    }
    return "unknown theme '" + std::string(name) + "'; use " + names;
}

}  // namespace HudText
