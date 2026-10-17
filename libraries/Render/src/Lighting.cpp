#include <Bowlingbox/Render/Lighting.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <limits>

namespace Bowlingbox {
namespace Render {

float srgbToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
float linearToSrgb(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
}
glm::vec3 srgbToLinear(glm::vec3 c) {
    return {srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b)};
}
glm::vec3 linearToSrgb(glm::vec3 c) {
    return {linearToSrgb(c.r), linearToSrgb(c.g), linearToSrgb(c.b)};
}

LightFrustum fitLightFrustum(glm::vec3 lightDir, const Aabb& box) {
    const glm::vec3 dir = glm::normalize(lightDir);
    const glm::vec3 centre = (box.min + box.max) * 0.5f;
    // Up = down the lane: the light-space x axis then has no z part, so the long, thin lane fills
    // the map's long (y) axis. A light along the lane can't use that up; it never happens in a theme.
    const glm::vec3 up = std::abs(dir.z) > 0.99f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(0.f, 0.f, 1.f);
    const glm::mat4 view = glm::lookAt(centre - dir, centre, up);
    glm::vec3 lo(std::numeric_limits<float>::max()), hi(std::numeric_limits<float>::lowest());
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner {(i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y,
                                (i & 4) ? box.max.z : box.min.z};
        const glm::vec3 p = glm::vec3(view * glm::vec4(corner, 1.f));
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    // The view looks down -z, so the corner nearest the light has the largest z.
    return {view, glm::ortho(lo.x, hi.x, lo.y, hi.y, -hi.z, -lo.z)};
}

Aabb shadowBox(const SceneGeometry& g) {
    const float x = g.laneHalfWidth + g.gutterWidth + 0.1f;
    return {{-x, kPitTopY - 0.1f, -g.approach}, {x, kShadowTopY, g.laneLength + kPitLength}};
}

std::vector<std::size_t> propsInBox(const std::vector<Prop>& props, const SceneGeometry& g, const Aabb& box) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < props.size(); ++i) {
        const Aabb b = bounds(props[i], g);
        if (b.min.x < box.max.x && b.max.x > box.min.x && b.min.y < box.max.y && b.max.y > box.min.y &&
            b.min.z < box.max.z && b.max.z > box.min.z)
            out.push_back(i);
    }
    return out;
}

}  // namespace Render
}  // namespace Bowlingbox
