#ifndef GAMEBOX_ALLEY_BULLET_WORLD_H
#define GAMEBOX_ALLEY_BULLET_WORLD_H
#include <Gamebox/Alley/PhysicsWorld.h>
#include <array>
#include <memory>
#include <utility>

namespace Gamebox {
namespace Alley {

/// Bullet3 implementation of IPhysicsWorld.
class BulletWorld final : public IPhysicsWorld {
public:
    BulletWorld();
    ~BulletWorld() override;
    BulletWorld(const BulletWorld&) = delete;
    BulletWorld& operator=(const BulletWorld&) = delete;

    void step(float dt) override;  // stepSimulation(dt, 4, 1/120)

    void placeBall(float offset) override;
    void launchBall(float offset, float angleDeg, float speed) override;

    void resetRack() override;
    void rebuild() override;  // new Bullet world
    void removePin(int i) override;

    Pose ballPose() const override;
    Pose pinPose(int i) const override;
    bool isPinStanding(int i) const override;
    bool isPinInPlay(int i) const override;
    bool ballInPit() const override;
    float ballSpeed() const override;
    bool allSlow() const override;

    /// Standard rack: (x, z) of pins 1..10 in metres.
    static std::array<std::pair<float, float>, 10> pinPositions();

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

}  // namespace Alley
}  // namespace Gamebox
#endif  // GAMEBOX_ALLEY_BULLET_WORLD_H
