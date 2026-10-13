#include "Lane_impl.h"
#include <algorithm>
#include <cmath>

namespace Sandbox {
namespace Alley {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kChargeHz = 1.0f;
constexpr float kMinSpeed = 4.0f;
constexpr float kSpeedRange = 8.0f;
constexpr float kRollingTimeout = 8.0f;
constexpr float kBallStoppedSpeed = 0.1f;
constexpr float kSettleTimeout = 3.0f;
constexpr float kFrameDoneDuration = 1.5f;

Result<> ok() {
    return Result<>(std::monostate {});
}
Result<> fail(const char* message) {
    return Result<>(Sc::Error<>(std::string(message)));
}
}  // namespace

LaneImpl::LaneImpl() : _game(std::make_unique<Gamebox::Bowlingbox>()) {
    _game->reset();  // Bowlingbox does not initialise its roll array itself
    _world.resetRack();
    _world.placeBall(0.f);
}

Result<> LaneImpl::moveAim(float delta) {
    if (!canAim())
        return fail("ball in motion");
    _aimOffset = std::clamp(_aimOffset + delta, -kMaxAimOffset, kMaxAimOffset);
    _world.placeBall(_aimOffset);
    return ok();
}

Result<> LaneImpl::rotateAim(float delta) {
    if (!canAim())
        return fail("ball in motion");
    _aimAngle = std::clamp(_aimAngle + delta, -kMaxAimAngle, kMaxAimAngle);
    return ok();
}

Result<> LaneImpl::startCharge() {
    if (_phase != Phase::Aiming)
        return fail("not aiming");
    _power = 0.f;
    enter(Phase::Charging);
    return ok();
}

Result<> LaneImpl::release() {
    if (_phase != Phase::Charging)
        return fail("not charging");
    _world.launchBall(_aimOffset, _aimAngle, kMinSpeed + kSpeedRange * _power);
    enter(Phase::Rolling);
    return ok();
}

Result<> LaneImpl::newGame() {
    _game->reset();
    _world.rebuild();
    _aimOffset = 0.f;
    _aimAngle = 0.f;
    _power = 0.f;
    _lastPinsDown = -1;
    _world.placeBall(0.f);
    enter(Phase::Aiming);
    return ok();
}

void LaneImpl::step(float dt) noexcept {
    _phaseTime += dt;
    switch (_phase) {
        case Phase::Charging:
            _power = 0.5f * (1.f - std::cos(2.f * kPi * kChargeHz * _phaseTime));
            break;
        case Phase::Rolling:
            _world.step(dt);
            if (_world.ballInPit() || (_phaseTime > 1.f && _world.ballSpeed() < kBallStoppedSpeed) ||
                _phaseTime > kRollingTimeout)
                enter(Phase::Settling);
            break;
        case Phase::Settling:
            _world.step(dt);
            if (_world.allSlow() || _phaseTime > kSettleTimeout)
                finishThrow();
            break;
        case Phase::FrameDone:
            if (_phaseTime >= kFrameDoneDuration) {
                if (_game->isOver())
                    enter(Phase::GameOver);
                else
                    prepareNextBall();
            }
            break;
        case Phase::Aiming:
        case Phase::GameOver:
            break;
    }
}

void LaneImpl::enter(Phase p) {
    _phase = p;
    _phaseTime = 0.f;
}

void LaneImpl::finishThrow() {
    int down = 0;
    for (int i = 0; i < 10; ++i)
        if (_world.isPinInPlay(i) && !_world.isPinStanding(i))
            ++down;
    if (!_game->roll(down)) {
        // Gamebox refused the roll (only possible when the game is already
        // over, which the FrameDone transition prevents). Fail safe.
        _lastPinsDown = -1;
        enter(Phase::GameOver);
        return;
    }
    _lastPinsDown = down;
    enter(Phase::FrameDone);
}

void LaneImpl::prepareNextBall() {
    if (_game->pinsLeft() == 10) {
        _world.rebuild();
    } else {
        for (int i = 0; i < 10; ++i)
            if (_world.isPinInPlay(i) && !_world.isPinStanding(i))
                _world.removePin(i);
    }
    _world.placeBall(_aimOffset);
    enter(Phase::Aiming);
}

Snapshot LaneImpl::snapshot() const {
    Snapshot s;
    s.phase = _phase;
    s.ball = _world.ballPose();
    for (int i = 0; i < 10; ++i) {
        s.pins[i] = _world.pinPose(i);
        s.pinStanding[i] = _world.isPinStanding(i);
        s.pinInPlay[i] = _world.isPinInPlay(i);
    }
    s.aimOffset = _aimOffset;
    s.aimAngle = _aimAngle;
    s.power = _power;
    s.lastPinsDown = _lastPinsDown;
    return s;
}

}  // namespace Alley
}  // namespace Sandbox
