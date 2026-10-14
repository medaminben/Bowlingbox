#ifndef GAMEBOX_SCORE_IBOWLINGGAME_H
#define GAMEBOX_SCORE_IBOWLINGGAME_H
#include <Gamebox/Score/Gamebox_Score_export.h>
#include <vector>

namespace Gamebox {
namespace Score {

/// Read-only view of a bowling game. Displays depend on this, not on a
/// concrete calculator.
class GAMEBOX_SCORE_API IScoreSheet {
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
class GAMEBOX_SCORE_API IBowlingGame : public IScoreSheet {
public:
    /// Records a roll. Returns false, and changes nothing, when the game is
    /// over or pins is not between 0 and pinsLeft().
    virtual bool roll(int pins) = 0;
    virtual void reset() = 0;
};

}  // namespace Score
}  // namespace Gamebox
#endif  // GAMEBOX_SCORE_IBOWLINGGAME_H
