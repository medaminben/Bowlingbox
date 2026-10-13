#ifndef SANDBOX_ALLEY_LANE_IMPL_H
#define SANDBOX_ALLEY_LANE_IMPL_H
#include <Sandbox/Alley/Alley.h>
#include <Sandbox/Gamebox/Gamebox.h>
#include "World.h"
#include <memory>

namespace Sandbox {
namespace Alley {

/// Throw state machine + bridge to the Gamebox score calculator.
class LaneImpl {
public:
    LaneImpl();

    Result<> moveAim(float delta);
    Result<> rotateAim(float delta);
    Result<> startCharge();
    Result<> release();
    Result<> newGame();

    void step(float dt) noexcept;
    Snapshot snapshot() const;
    Gamebox::Bowlingbox& game() const { return *_game; }

private:
    bool canAim() const { return _phase == Phase::Aiming || _phase == Phase::Charging; }
    void enter(Phase p);
    void finishThrow();      // Settling -> FrameDone (or GameOver on rejected roll)
    void prepareNextBall();  // FrameDone -> Aiming

    std::unique_ptr<Gamebox::Bowlingbox> _game;
    World _world;
    Phase _phase = Phase::Aiming;
    float _aimOffset = 0.f;
    float _aimAngle = 0.f;
    float _power = 0.f;
    float _phaseTime = 0.f;
    int _lastPinsDown = -1;
};

}  // namespace Alley
}  // namespace Sandbox
#endif  // SANDBOX_ALLEY_LANE_IMPL_H
