#ifndef BOWLINGBOX_ALLEY_LANE_IMPL_H
#define BOWLINGBOX_ALLEY_LANE_IMPL_H
#include <Bowlingbox/Alley/Alley.h>
#include <Bowlingbox/Alley/PhysicsWorld.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>

namespace Bowlingbox {
namespace Alley {

/// Throw state machine. Talks to physics and scoring only through their interfaces.
class LaneImpl {
public:
    LaneImpl(std::unique_ptr<IPhysicsWorld> world, std::unique_ptr<Score::IBowlingGame> game);

    Result<> moveAim(float delta);
    Result<> rotateAim(float delta);
    Result<> startCharge();
    Result<> release();
    Result<> newGame();

    void step(float dt) noexcept;
    Snapshot snapshot() const;
    std::vector<Impact> drainImpacts() { return _world->drainImpacts(); }
    const Score::IScoreSheet& game() const { return *_game; }

private:
    bool canAim() const { return _phase == Phase::Aiming || _phase == Phase::Charging; }
    void enter(Phase p);
    void finishThrow();      // Settling -> FrameDone (or GameOver on rejected roll)
    void prepareNextBall();  // FrameDone -> Aiming

    int fallenPins() const;  // knocked over but still in the world

    std::unique_ptr<IPhysicsWorld> _world;
    std::unique_ptr<Score::IBowlingGame> _game;
    Phase _phase = Phase::Aiming;
    float _aimOffset = 0.f;
    float _aimAngle = 0.f;
    float _power = 0.f;
    float _phaseTime = 0.f;
    int _lastPinsDown = -1;
};

}  // namespace Alley
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_ALLEY_LANE_IMPL_H
