#include "Score_impl.h"
#include <algorithm>

namespace Gamebox {
namespace Score {

/**
 * @brief Records a ball.
 * @return false when the game is over or `pins` is not between 0 and the
 *         pins standing; the game is unchanged then.
 */
bool BowlingSet::roll(int pins) {
    const Position p = position();
    if (p.frame == kFrames || pins < 0 || pins > p.standing)
        return false;
    _balls[_count++] = pins;
    return true;
}

void BowlingSet::reset() {
    _balls.fill(0);
    _count = 0;
}

int BowlingSet::at(int index) const {
    return index < _count ? _balls[index] : 0;
}

/**
 * @brief Walks the thrown balls frame by frame to find the next ball.
 * Frames 1-9 end after a strike or two balls. The tenth frame gets a third
 * ball after a strike or a spare, and its rack is reset whenever it is cleared.
 */
BowlingSet::Position BowlingSet::position() const {
    int i = 0;
    for (int f = 0; f < kFrames - 1; ++f) {
        if (i == _count)
            return {f, kPins};
        if (_balls[i] == kPins) {
            ++i;
            continue;
        }
        if (i + 1 == _count)
            return {f, kPins - _balls[i]};
        i += 2;
    }
    const int first = at(i), second = at(i + 1);
    switch (_count - i) {
        case 0:
            return {kFrames - 1, kPins};
        case 1:
            return {kFrames - 1, first == kPins ? kPins : kPins - first};
        case 2:
            if (first == kPins)
                return {kFrames - 1, second == kPins ? kPins : kPins - second};
            if (first + second == kPins)
                return {kFrames - 1, kPins};
            return {kFrames, 0};
        default:
            return {kFrames, 0};
    }
}

int BowlingSet::pinsLeft() const {
    return position().standing;
}

bool BowlingSet::isOver() const {
    return position().frame == kFrames;
}

/**
 * @brief Running total through `frameIndex` (clamped to 0..9), counting only
 *        the balls thrown so far.
 */
int BowlingSet::frameScore(int frameIndex) const {
    const int last = std::clamp(frameIndex, 0, kFrames - 1);
    int total = 0, i = 0;
    for (int f = 0; f <= last && i < _count; ++f) {
        const bool strike = at(i) == kPins;
        const bool spare = !strike && at(i) + at(i + 1) == kPins;
        total += at(i) + at(i + 1) + ((strike || spare) ? at(i + 2) : 0);
        i += (strike && f < kFrames - 1) ? 1 : 2;
    }
    return total;
}

int BowlingSet::score() const {
    return frameScore(kFrames - 1);
}

/**
 * @brief Running totals per frame; Mark::None until a frame is final
 *        (an open frame after two balls, a strike or spare after its bonus).
 */
std::vector<int> BowlingSet::scores() const {
    std::vector<int> out(kFrames, toInt(Mark::None));
    int total = 0, i = 0;
    for (int f = 0; f < kFrames && i < _count; ++f) {
        const bool strike = _balls[i] == kPins;
        const bool spare = !strike && i + 1 < _count && _balls[i] + _balls[i + 1] == kPins;
        const int needed = (strike || spare) ? 3 : 2;  // balls that decide this frame
        if (i + needed > _count)
            break;
        total += _balls[i] + _balls[i + 1] + ((strike || spare) ? _balls[i + 2] : 0);
        out[f] = total;
        i += (strike && f < kFrames - 1) ? 1 : 2;
    }
    return out;
}

/**
 * @brief The 21 roll slots of the score sheet (see Notation.h).
 * Frames 1-9 use two slots; a strike fills the second with Mark::Empty.
 * The tenth frame uses three slots; a ball that clears a fresh rack is a
 * strike, one that clears a partly knocked rack is a spare.
 */
std::vector<int> BowlingSet::rolls() const {
    std::vector<int> out(kRollSlots, toInt(Mark::None));
    int i = 0;
    for (int f = 0; f < kFrames - 1 && i < _count; ++f) {
        const int slot = 2 * f;
        if (_balls[i] == kPins) {
            out[slot] = toInt(Mark::Strike);
            out[slot + 1] = toInt(Mark::Empty);
            ++i;
            continue;
        }
        out[slot] = _balls[i];
        if (i + 1 < _count)
            out[slot + 1] = _balls[i] + _balls[i + 1] == kPins ? toInt(Mark::Spare) : _balls[i + 1];
        i += 2;
    }
    int standing = kPins;
    bool freshRack = true;
    for (int slot = 2 * (kFrames - 1); slot < kRollSlots && i < _count; ++slot, ++i) {
        const int pins = _balls[i];
        if (freshRack && pins == kPins)
            out[slot] = toInt(Mark::Strike);
        else if (!freshRack && pins == standing)
            out[slot] = toInt(Mark::Spare);
        else
            out[slot] = pins;
        freshRack = pins == standing;
        standing = freshRack ? kPins : standing - pins;
    }
    return out;
}

}  // namespace Score
}  // namespace Gamebox
