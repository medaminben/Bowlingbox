#ifndef GAMEBOX_ALLEY_H
#define GAMEBOX_ALLEY_H
#include <Gamebox/Alley/Gamebox_Alley_export.h>
#include <Gamebox/Alley/Types.h>
#include <Gamebox/Core/Core.h>
#include <Gamebox/Score/IBowlingGame.h>
#include <memory>

namespace Gamebox {
namespace Alley {

class IPhysicsWorld;
class LaneImpl;

class GAMEBOX_ALLEY_API Lane {
public:
    /// Bullet physics + Gamebox::Score::Bowlingbox.
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

private:
    std::unique_ptr<LaneImpl> _impl;
};

}  // namespace Alley
}  // namespace Gamebox
#endif  // GAMEBOX_ALLEY_H
