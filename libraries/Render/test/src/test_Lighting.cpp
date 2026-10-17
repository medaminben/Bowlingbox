#include <Bowlingbox/Render/Lighting.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>

using namespace Bowlingbox::Render;

namespace {
// Alley::kLane, copied: Render does not depend on Alley.
const SceneGeometry kG {0.525f, 18.3f, 2.f, 0.24f, 0.05f, 0.109f, 0.38f, 0.06f, 0.12f, 0.3048f, 17.4f};

glm::vec3 toClip(const LightFrustum& f, glm::vec3 p) {
    const glm::vec4 c = f.viewProjection() * glm::vec4(p, 1.f);
    return glm::vec3(c) / c.w;
}
std::array<glm::vec3, 8> corners(const Aabb& b) {
    std::array<glm::vec3, 8> out;
    for (int i = 0; i < 8; ++i)
        out[static_cast<std::size_t>(i)] = {(i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y,
                                            (i & 4) ? b.max.z : b.min.z};
    return out;
}
}  // namespace

TEST(test_lighting, srgb_round_trips) {
    for (int i = 0; i <= 100; ++i) {
        const float c = static_cast<float>(i) / 100.f;
        EXPECT_NEAR(linearToSrgb(srgbToLinear(c)), c, 1e-5f) << c;
    }
}

TEST(test_lighting, srgb_matches_known_values) {
    EXPECT_FLOAT_EQ(srgbToLinear(0.f), 0.f);
    EXPECT_NEAR(srgbToLinear(1.f), 1.f, 1e-6f);
    EXPECT_NEAR(srgbToLinear(0.5f), 0.21404f, 1e-4f);
    const glm::vec3 v = srgbToLinear(glm::vec3(0.f, 0.5f, 1.f));
    EXPECT_NEAR(v.y, 0.21404f, 1e-4f);
    EXPECT_NEAR(linearToSrgb(glm::vec3(0.21404f)).z, 0.5f, 1e-4f);
}

TEST(test_lighting, shadow_box_covers_the_lane_deck_and_pit) {
    const Aabb b = shadowBox(kG);
    EXPECT_LE(b.min.z, -kG.approach);
    EXPECT_GE(b.max.z, kG.laneLength + kPitLength);
    EXPECT_GE(b.max.x, kG.laneHalfWidth + kG.gutterWidth);
    EXPECT_LE(b.min.x, -(kG.laneHalfWidth + kG.gutterWidth));
    EXPECT_LE(b.min.y, kPitTopY);
    EXPECT_FLOAT_EQ(b.max.y, kShadowTopY);
}

TEST(test_lighting, light_frustum_holds_the_shadow_box_for_every_theme) {
    const Aabb box = shadowBox(kG);
    for (ThemeId id : kThemes) {
        const LightFrustum f = fitLightFrustum(theme(id).lightDir, box);
        float maxX = 0.f, maxY = 0.f;
        for (const glm::vec3& c : corners(box)) {
            const glm::vec3 p = toClip(f, c);
            for (int k = 0; k < 3; ++k) {
                EXPECT_GE(p[k], -1.f - 1e-4f) << theme(id).name;
                EXPECT_LE(p[k], 1.f + 1e-4f) << theme(id).name;
            }
            maxX = std::max(maxX, std::abs(p.x));
            maxY = std::max(maxY, std::abs(p.y));
        }
        EXPECT_NEAR(maxX, 1.f, 1e-4f) << "fit is tight in x: " << theme(id).name;
        EXPECT_NEAR(maxY, 1.f, 1e-4f) << "fit is tight in y: " << theme(id).name;
    }
}

TEST(test_lighting, light_frustum_depth_grows_along_the_light) {
    const Aabb box = shadowBox(kG);
    const glm::vec3 dir = theme(ThemeId::NeonHall).lightDir;
    const LightFrustum f = fitLightFrustum(dir, box);
    const glm::vec3 c = (box.min + box.max) * 0.5f;
    EXPECT_LT(toClip(f, c).z, toClip(f, c + glm::normalize(dir) * 0.1f).z);
}

TEST(test_lighting, the_lane_runs_along_the_light_maps_long_axis) {
    const Aabb box = shadowBox(kG);
    for (ThemeId id : kThemes) {
        const LightFrustum f = fitLightFrustum(theme(id).lightDir, box);
        const glm::vec3 foul = toClip(f, {0.f, 0.f, 0.f}), down = toClip(f, {0.f, 0.f, 10.f});
        EXPECT_NEAR(foul.x, down.x, 1e-4f) << "moving down the lane stays in one map column: " << theme(id).name;
        EXPECT_GT(std::abs(down.y - foul.y), 0.1f) << theme(id).name;
    }
}

TEST(test_lighting, straight_down_light_gives_a_finite_frustum) {
    const glm::mat4 m = fitLightFrustum({0.f, -1.f, 0.f}, shadowBox(kG)).viewProjection();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_TRUE(std::isfinite(m[c][r])) << c << "," << r;
}
