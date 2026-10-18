#ifndef BOWLINGBOX_SCORE_IBOWLINGGAME_H
#define BOWLINGBOX_SCORE_IBOWLINGGAME_H
#include <Bowlingbox/Score/Bowlingbox_Score_export.h>
#include <vector>

namespace Bowlingbox {
namespace Score {

/// The outcome of IBowlingGame::roll(). When a ball breaks several rules, GameOver wins.
enum class [[nodiscard]] RollStatus { Accepted, GameOver, Negative, TooManyPins };

/// A short English description, e.g. "more pins than are standing".
BOWLINGBOX_SCORE_API const char* describe(RollStatus status);

/// Read-only view of a bowling game. Displays depend on this, not on a
/// concrete calculator.
class BOWLINGBOX_SCORE_API IScoreSheet {
public:
    virtual ~IScoreSheet() = default;

    /// kRollSlots values: pin counts or Mark sentinels (see Notation.h).
    virtual std::vector<int> rolls() const = 0;
    /// kFrames running totals. A frame stays Mark::None until it is final
    /// (two balls for an open frame, plus the bonus balls for a strike or spare).
    virtual std::vector<int> scores() const = 0;
    virtual int score() const = 0;
    /// Running total through frameIndex (clamped to 0..9), counting balls thrown so far.
    virtual int frameScore(int frameIndex) const = 0;
    /// Pins standing for the next ball; 0 once the game is over.
    virtual int pinsLeft() const = 0;
    virtual bool isOver() const = 0;
};

/// A score sheet that also accepts rolls.
class BOWLINGBOX_SCORE_API IBowlingGame : public IScoreSheet {
public:
    /// Records a roll. Anything but Accepted changes nothing.
    virtual RollStatus roll(int pins) = 0;
    virtual void reset() = 0;
};

}  // namespace Score
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_SCORE_IBOWLINGGAME_H
