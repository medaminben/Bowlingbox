#ifndef BOWLINGBOX_ALLEY_PHYSICS_WORLD_H
#define BOWLINGBOX_ALLEY_PHYSICS_WORLD_H
#include <Bowlingbox/Alley/Bowlingbox_Alley_export.h>
#include <Bowlingbox/Alley/Types.h>
#include <memory>

namespace Bowlingbox {
namespace Alley {

/// Rigid-body world: static lane, gutters and pit; one ball; ten pins.
/// Knows nothing about bowling rules or phases. The Lane drives it through
/// this interface so the physics engine can be swapped (or faked in tests).
class BOWLINGBOX_ALLEY_API IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    virtual void step(float dt) = 0;

    virtual void placeBall(float offset) = 0;  // at rest on the approach, z = kBallStartZ
    virtual void launchBall(float offset, float angleDeg, float speed) = 0;

    virtual void resetRack() = 0;     // all 10 pins standing and in play
    virtual void rebuild() = 0;       // fresh world (deterministic between frames)
    virtual void removePin(int i) = 0;  // take a pin out of the world

    virtual Pose ballPose() const = 0;
    virtual Pose pinPose(int i) const = 0;
    virtual bool isPinStanding(int i) const = 0;
    virtual bool isPinInPlay(int i) const = 0;
    virtual bool ballInPit() const = 0;
    virtual float ballSpeed() const = 0;
    virtual bool allSlow() const = 0;  // every body in the world is (nearly) at rest
};

/// The default Bullet3 implementation.
BOWLINGBOX_ALLEY_API std::unique_ptr<IPhysicsWorld> makeBulletWorld();

}  // namespace Alley
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_ALLEY_PHYSICS_WORLD_H
