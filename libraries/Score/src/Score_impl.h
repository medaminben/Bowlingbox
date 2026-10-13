#ifndef SCORE_IMPL_H
#define SCORE_IMPL_H
#include <Gamebox/Score/Notation.h>
#include <vector>
namespace Gamebox {
namespace Score {
struct BowlingSet {
    BowlingSet() { reset(); }

    bool roll(int pins);
    void reset();
    std::vector<int> rolls() const;
    std::vector<int> scores() const;
    int pinsLeft() const;
    int score() const;
    bool isOver() const;
    int frameScore(int frameIndex) const;

private:
    bool isStrike(int index) const;
    bool isSpare(int index) const;
    bool isMiss(int index) const;
    bool managePins(int pins);
    int manageCursor() const;
    static int validateRoll(int value);

private:
    int _rolls[kRollSlots];
    int _rollIndex = 0;
    int _pinsLeft = kPins;
};
}  // namespace Score
}  // namespace Gamebox
#endif  // SCORE_IMPL_H
