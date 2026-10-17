#ifndef BOWLINGBOX_RENDER_LIGHTING_H
#define BOWLINGBOX_RENDER_LIGHTING_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Render/Environment.h>
#include <glm/glm.hpp>

namespace Bowlingbox {
namespace Render {

/// sRGB transfer function per channel, for values in [0, 1].
BOWLINGBOX_RENDER_API float srgbToLinear(float c);
BOWLINGBOX_RENDER_API float linearToSrgb(float c);
BOWLINGBOX_RENDER_API glm::vec3 srgbToLinear(glm::vec3 c);
BOWLINGBOX_RENDER_API glm::vec3 linearToSrgb(glm::vec3 c);

/// Top of the shadow box: the masking unit (up to 2.4 m) casts, the ceiling (4.5 m) does not.
inline constexpr float kShadowTopY = 2.5f;

struct LightFrustum {
    glm::mat4 view {1.f};
    glm::mat4 projection {1.f};
    glm::mat4 viewProjection() const { return projection * view; }
};

/// Orthographic light camera looking along `lightDir` whose volume is the tightest fit
/// around `box`. It depends only on the light and the box, never on the player's camera.
/// Its up vector is the lane direction (+z), so the lane runs along the map's y axis.
BOWLINGBOX_RENDER_API LightFrustum fitLightFrustum(glm::vec3 lightDir, const Aabb& box);
/// What the key light's shadow map covers: lane, gutters, approach, deck and pit, up to kShadowTopY.
BOWLINGBOX_RENDER_API Aabb shadowBox(const SceneGeometry& g);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_LIGHTING_H
