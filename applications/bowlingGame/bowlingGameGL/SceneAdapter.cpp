#include "SceneAdapter.h"
#include <glm/gtc/quaternion.hpp>

using namespace Gamebox;

namespace SceneAdapter {

Render::SceneGeometry geometry(const Alley::LaneDimensions& d) {
    return {d.halfWidth, d.length, d.approach, d.gutterWidth, d.gutterDepth,
            d.ballRadius, d.pinHeight, d.pinMaxRadius, d.pinComHeight};
}

Render::Transform toTransform(const Alley::Pose& p) {
    Render::Transform t;
    t.position = {p.position[0], p.position[1], p.position[2]};
    t.rotation = glm::quat(p.rotation[3], p.rotation[0], p.rotation[1], p.rotation[2]);  // w, x, y, z
    return t;
}

Render::SceneDescription toScene(const Alley::Snapshot& s) {
    Render::SceneDescription d;
    d.ball = toTransform(s.ball);
    for (int i = 0; i < 10; ++i) {
        d.pins[static_cast<std::size_t>(i)] = toTransform(s.pins[static_cast<std::size_t>(i)]);
        d.pinVisible[static_cast<std::size_t>(i)] = s.pinInPlay[static_cast<std::size_t>(i)];
    }
    d.showAimGuide = s.phase == Alley::Phase::Aiming || s.phase == Alley::Phase::Charging;
    d.aimOffset = s.aimOffset;
    d.aimAngleDeg = s.aimAngle;
    return d;
}

}  // namespace SceneAdapter
