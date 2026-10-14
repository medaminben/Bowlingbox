#ifndef GAMEBOX_SCORE_IMPL_H
#define GAMEBOX_SCORE_IMPL_H
#include <Gamebox/Score/Notation.h>
#include <array>
#include <vector>
namespace Gamebox {
namespace Score {
/// Ten-pin game state: the pins knocked down by each ball, in order.
/// Frames, bonuses and the next ball are derived from that list on demand.
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
    /// Where the next ball goes.
    struct Position {
        int frame;     // 0..9, kFrames when the game is over
        int standing;  // pins standing for the next ball, 0 when over
    };
    Position position() const;
    int at(int index) const;  // pins of ball `index`, 0 if not thrown yet

    std::array<int, kRollSlots> _balls {};
    int _count = 0;
};
}  // namespace Score
}  // namespace Gamebox
#endif  // GAMEBOX_SCORE_IMPL_H
