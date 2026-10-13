#ifndef SANDBOX_ALLEY_WORLD_H
#define SANDBOX_ALLEY_WORLD_H
#include <Sandbox/Alley/Alley.h>
#include <array>
#include <memory>
#include <utility>

namespace Sandbox {
namespace Alley {

/// Bullet rigid-body world: static lane, gutters and pit; one ball; ten pins.
/// Knows nothing about bowling rules or phases.
class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void step(float dt);  // stepSimulation(dt, 4, 1/120)

    void placeBall(float offset);  // at rest on the approach, z = kBallStartZ
    void launchBall(float offset, float angleDeg, float speed);

    void resetRack();     // all 10 pins standing and in play
    void rebuild();       // new Bullet world (deterministic between frames)
    void removePin(int i);  // take a pin out of the world

    Pose ballPose() const;
    Pose pinPose(int i) const;
    bool isPinStanding(int i) const;
    bool isPinInPlay(int i) const;
    bool ballInPit() const;
    float ballSpeed() const;
    bool allSlow() const;  // every body in the world is (nearly) at rest

    /// Standard rack: (x, z) of pins 1..10 in metres.
    static std::array<std::pair<float, float>, 10> pinPositions();

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

}  // namespace Alley
}  // namespace Sandbox
#endif  // SANDBOX_ALLEY_WORLD_H
