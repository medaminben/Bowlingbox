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
    /// kFrames running totals, Mark::None for frames not reached yet.
    virtual std::vector<int> scores() const = 0;
    virtual int score() const = 0;
    virtual int frameScore(int frameIndex) const = 0;
    virtual int pinsLeft() const = 0;
    virtual bool isOver() const = 0;
};

/// A score sheet that also accepts rolls.
class GAMEBOX_SCORE_API IBowlingGame : public IScoreSheet {
public:
    /// Records a roll. Returns false when the game is already over.
    virtual bool roll(int pins) = 0;
    virtual void reset() = 0;
};

}  // namespace Score
}  // namespace Gamebox
#endif  // GAMEBOX_SCORE_IBOWLINGGAME_H
