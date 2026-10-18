#ifndef BOWLINGBOX_ALLEY_H
#define BOWLINGBOX_ALLEY_H
#include <Bowlingbox/Alley/Bowlingbox_Alley_export.h>
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Core/Core.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>
#include <vector>

namespace Bowlingbox {
namespace Alley {

class IPhysicsWorld;
class LaneImpl;

class BOWLINGBOX_ALLEY_API Lane {
public:
    /// Bullet physics + Bowlingbox::Score::ScoreCalculator.
    Lane();
    /// Both collaborators are injected; neither may be null.
    Lane(std::unique_ptr<IPhysicsWorld> world, std::unique_ptr<Score::IBowlingGame> game);
    ~Lane();
    Lane(const Lane&) = delete;
    Lane& operator=(const Lane&) = delete;

    // Input. Valid only in Aiming / Charging (newGame: any phase); otherwise Error.
    Result<> moveAim(float deltaOffset);
    Result<> rotateAim(float deltaAngleDeg);
    Result<> startCharge();  // Aiming  -> Charging
    Result<> release();      // Charging -> Rolling
    Result<> newGame();      // any      -> Aiming

    // Time.
    void step(float dtSeconds) noexcept;

    // Read.
    Snapshot snapshot() const;
    const Score::IScoreSheet& scoreSheet() const;
    /// New physics contacts since the last call (for sound), oldest first.
    std::vector<Impact> drainImpacts();

private:
    std::unique_ptr<LaneImpl> _impl;
};

}  // namespace Alley
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_ALLEY_H
