#ifndef SCORE_H
#define SCORE_H
#include <Gamebox/Score/Gamebox_Score_export.h>
#include <Gamebox/Score/IBowlingGame.h>
#include <Gamebox/Score/Notation.h>
#include <memory>
#include <vector>
namespace Gamebox {
namespace Score {
struct BowlingSet;

/// Ten-pin score calculator (codingdojo bowling kata).
class GAMEBOX_SCORE_API Bowlingbox final : public IBowlingGame {
public:
    Bowlingbox();
    ~Bowlingbox() override;

public:
    bool roll(int pins) override;
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
}  // namespace Gamebox

#endif  // SCORE_H
