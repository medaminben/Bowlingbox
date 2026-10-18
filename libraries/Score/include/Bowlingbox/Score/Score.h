#ifndef BOWLINGBOX_SCORE_H
#define BOWLINGBOX_SCORE_H
#include <Bowlingbox/Score/Bowlingbox_Score_export.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <Bowlingbox/Score/Frames.h>
#include <Bowlingbox/Score/Notation.h>
#include <memory>
#include <vector>
namespace Bowlingbox {
namespace Score {
struct BowlingSet;

/// Ten-pin score calculator (codingdojo bowling kata).
class BOWLINGBOX_SCORE_API ScoreCalculator final : public IBowlingGame {
public:
    ScoreCalculator();
    ~ScoreCalculator() override;

public:
    RollStatus roll(int pins) override;
    void reset() override;

    std::vector<int> rolls() const override;
    std::vector<int> scores() const override;
    int score() const override;
    int frameScore(int frameIndex) const override;
    int pinsLeft() const override;
    bool isOver() const override;

private:
    std::unique_ptr<BowlingSet> _gameSet;
};
}  // namespace Score
}  // namespace Bowlingbox

#endif  // BOWLINGBOX_SCORE_H
