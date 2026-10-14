#include "FollowCamera.h"
#include <algorithm>

using Bowlingbox::Alley::Phase;

void FollowCamera::update(const Bowlingbox::Alley::Snapshot& s, float dt) {
    float target = kHomeZ;
    if (s.phase == Phase::Rolling || s.phase == Phase::Settling || s.phase == Phase::FrameDone)
        target = std::clamp(s.ball.position[2] - 3.5f, kHomeZ, Bowlingbox::Alley::kLane.length - 5.5f);
    _z += (target - _z) * std::min(1.f, 6.f * dt);
}

void FollowCamera::applyTo(Bowlingbox::Render::Camera& camera) const {
    camera.lookAt({0.f, kHeight, _z}, {0.f, 0.f, _z + kLookAhead});
}
