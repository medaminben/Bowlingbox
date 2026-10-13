#include <Sandbox/Alley/Alley.h>
#include "Lane_impl.h"

namespace Sandbox {
namespace Alley {

Lane::Lane() : _impl(std::make_unique<LaneImpl>()) {}
Lane::~Lane() = default;

Result<> Lane::moveAim(float d) { return _impl->moveAim(d); }
Result<> Lane::rotateAim(float d) { return _impl->rotateAim(d); }
Result<> Lane::startCharge() { return _impl->startCharge(); }
Result<> Lane::release() { return _impl->release(); }
Result<> Lane::newGame() { return _impl->newGame(); }

void Lane::step(float dt) noexcept { _impl->step(dt); }

Snapshot Lane::snapshot() const { return _impl->snapshot(); }
std::vector<int> Lane::rolls() const { return _impl->game().rolls(); }
std::vector<int> Lane::frameScores() const { return _impl->game().scores(); }
int Lane::totalScore() const { return _impl->game().score(); }
bool Lane::isGameOver() const { return _impl->game().isOver(); }
int Lane::pinsLeft() const { return _impl->game().pinsLeft(); }

}  // namespace Alley
}  // namespace Sandbox
