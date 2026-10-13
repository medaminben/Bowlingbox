#include <Gamebox/Alley/Alley.h>
#include <Gamebox/Alley/PhysicsWorld.h>
#include <Gamebox/Score/Score.h>
#include "Lane_impl.h"

namespace Gamebox {
namespace Alley {

Lane::Lane() : Lane(makeBulletWorld(), std::make_unique<Score::Bowlingbox>()) {}
Lane::Lane(std::unique_ptr<IPhysicsWorld> world, std::unique_ptr<Score::IBowlingGame> game)
    : _impl(std::make_unique<LaneImpl>(std::move(world), std::move(game))) {}
Lane::~Lane() = default;

Result<> Lane::moveAim(float d) { return _impl->moveAim(d); }
Result<> Lane::rotateAim(float d) { return _impl->rotateAim(d); }
Result<> Lane::startCharge() { return _impl->startCharge(); }
Result<> Lane::release() { return _impl->release(); }
Result<> Lane::newGame() { return _impl->newGame(); }

void Lane::step(float dt) noexcept { _impl->step(dt); }

Snapshot Lane::snapshot() const { return _impl->snapshot(); }
const Score::IScoreSheet& Lane::scoreSheet() const { return _impl->game(); }

}  // namespace Alley
}  // namespace Gamebox
