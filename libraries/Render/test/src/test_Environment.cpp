#include <Bowlingbox/Render/Environment.h>
#include <gtest/gtest.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using namespace Bowlingbox::Render;

namespace {
// Alley::kLane, copied: Render does not depend on Alley.
const SceneGeometry kG {0.525f, 18.3f, 2.f, 0.24f, 0.05f, 0.109f, 0.38f, 0.06f, 0.12f, 0.3048f, 17.4f};

// Volumes that must stay free of decor (spec "Coordinates").
const Aabb kPlayCorridor {{-0.82f, -0.14f, -4.f}, {0.82f, 0.8f, 19.f}};
const Aabb kSightCorridor {{-0.82f, -0.14f, -4.f}, {0.82f, 2.2f, 18.3f}};
const Aabb kCameraBox {{-0.6f, 1.4f, -4.f}, {0.6f, 2.4f, 13.5f}};

bool overlaps(const Aabb& a, const Aabb& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

Aabb meshExtent(const MeshData& m) {
    Aabb box {glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
    for (std::size_t v = 0; v < m.vertexCount(); ++v) {
        const glm::vec3 p {m.vertices[v * 6], m.vertices[v * 6 + 1], m.vertices[v * 6 + 2]};
        box.min = glm::min(box.min, p);
        box.max = glm::max(box.max, p);
    }
    return box;
}

void expectNear(const glm::vec3& actual, const glm::vec3& expected, const char* what) {
    EXPECT_NEAR(actual.x, expected.x, 1e-4f) << what;
    EXPECT_NEAR(actual.y, expected.y, 1e-4f) << what;
    EXPECT_NEAR(actual.z, expected.z, 1e-4f) << what;
}
}  // namespace

TEST(test_environment, every_theme_round_trips_its_id_and_name) {
    for (ThemeId id : kThemes) {
        EXPECT_EQ(theme(id).id, id);
        EXPECT_EQ(themeFromName(theme(id).name), id);
    }
    EXPECT_EQ(themeFromName("neon"), ThemeId::NeonHall);
    EXPECT_EQ(themeFromName("classic"), ThemeId::ClassicAlley);
    EXPECT_EQ(themeFromName("outdoor"), ThemeId::Outdoor);
}

TEST(test_environment, theme_names_ignore_case_and_reject_unknown) {
    EXPECT_EQ(themeFromName("Classic"), ThemeId::ClassicAlley);
    EXPECT_EQ(themeFromName("NEON"), ThemeId::NeonHall);
    EXPECT_FALSE(themeFromName("x").has_value());
    EXPECT_FALSE(themeFromName("").has_value());
    EXPECT_FALSE(themeFromName("neonx").has_value());
}

TEST(test_environment, next_theme_visits_all_three_and_wraps) {
    EXPECT_EQ(nextTheme(ThemeId::NeonHall), ThemeId::ClassicAlley);
    EXPECT_EQ(nextTheme(ThemeId::ClassicAlley), ThemeId::Outdoor);
    EXPECT_EQ(nextTheme(ThemeId::Outdoor), ThemeId::NeonHall);
}

TEST(test_environment, fog_and_ambient_are_in_range) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        EXPECT_LT(t.fogStart, t.fogEnd) << t.name;
        EXPECT_GE(t.ambient, 0.f) << t.name;
        EXPECT_LE(t.ambient, 1.f) << t.name;
        EXPECT_GE(t.neighbourLanesPerSide, 0) << t.name;
        EXPECT_LE(t.neighbourLanesPerSide, 3) << t.name;
    }
}

TEST(test_environment, local_bounds_match_the_drawn_meshes) {
    for (Shape shape : {Shape::Box, Shape::Cylinder, Shape::Cone, Shape::Pin}) {
        const Aabb mesh = meshExtent(shapeMesh(shape, kG));
        const Aabb local = localBounds(shape, kG);
        expectNear(local.min, mesh.min, "min");
        expectNear(local.max, mesh.max, "max");
    }
}

TEST(test_environment, bounds_of_a_turned_box_are_its_corner_extent) {
    Prop p;
    p.shape = Shape::Box;
    p.transform.position = {1.f, 2.f, 3.f};
    p.transform.rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0.f, 1.f, 0.f));
    p.transform.scale = {2.f, 1.f, 0.5f};
    const glm::mat4 m = p.transform.matrix();
    Aabb corners {glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 c {(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f};
        const glm::vec3 w = glm::vec3(m * glm::vec4(c, 1.f));
        corners.min = glm::min(corners.min, w);
        corners.max = glm::max(corners.max, w);
    }
    const Aabb b = bounds(p, kG);
    expectNear(b.min, corners.min, "min");
    expectNear(b.max, corners.max, "max");
}

TEST(test_environment, bounds_of_a_pin_at_the_origin) {
    Prop p;
    p.shape = Shape::Pin;
    const Aabb b = bounds(p, kG);
    expectNear(b.min, {-kG.pinMaxRadius, -kG.pinComHeight, -kG.pinMaxRadius}, "min");
    expectNear(b.max, {kG.pinMaxRadius, kG.pinHeight - kG.pinComHeight, kG.pinMaxRadius}, "max");
}

TEST(test_environment, lane_pitch_and_hall_width_follow_the_geometry) {
    EXPECT_NEAR(lanePitch(kG), 2.f * (kG.laneHalfWidth + kG.gutterWidth) + 0.30f, 1e-5f);
    EXPECT_NEAR(lanePitch(kG), 1.83f, 1e-5f);
    EXPECT_NEAR(hallHalfWidth(theme(ThemeId::NeonHall), kG), 1.83f * 2.5f + 0.6f, 1e-4f);
}

TEST(test_environment, no_prop_enters_the_corridors_or_the_camera_box) {
    for (ThemeId id : kThemes) {
        const auto props = buildProps(theme(id), kG);
        EXPECT_FALSE(props.empty()) << theme(id).name;
        for (std::size_t i = 0; i < props.size(); ++i) {
            const Aabb b = bounds(props[i], kG);
            EXPECT_FALSE(overlaps(b, kPlayCorridor)) << theme(id).name << " prop " << i;
            EXPECT_FALSE(overlaps(b, kSightCorridor)) << theme(id).name << " prop " << i;
            EXPECT_FALSE(overlaps(b, kCameraBox)) << theme(id).name << " prop " << i;
        }
    }
}

TEST(test_environment, ball_return_beside_the_main_lane_clears_the_corridor) {
    float nearest = std::numeric_limits<float>::max();
    for (const Prop& p : buildProps(theme(ThemeId::NeonHall), kG))
        if (p.shape == Shape::Box && std::abs(p.transform.scale.x - kReturnWidth) < 1e-5f &&
            p.transform.position.x > 0.f)
            nearest = std::min(nearest, bounds(p, kG).min.x);
    EXPECT_NEAR(nearest, 0.845f, 1e-4f);
    EXPECT_GE(nearest, 0.82f);
}

TEST(test_environment, indoor_halls_are_closed_by_walls_ceiling_and_back_wall) {
    for (ThemeId id : {ThemeId::NeonHall, ThemeId::ClassicAlley}) {
        const Theme& t = theme(id);
        const float w = hallHalfWidth(t, kG);
        bool left = false, right = false, ceiling = false, back = false;
        for (const Prop& p : buildProps(t, kG)) {
            const Aabb b = bounds(p, kG);
            left = left || b.min.x <= -w;
            right = right || b.max.x >= w;
            ceiling = ceiling || b.max.y >= kHallHeight;
            back = back || b.max.z >= kHallBackZ;
        }
        EXPECT_TRUE(left) << t.name;
        EXPECT_TRUE(right) << t.name;
        EXPECT_TRUE(ceiling) << t.name;
        EXPECT_TRUE(back) << t.name;
    }
}

TEST(test_environment, neighbour_lanes_carry_full_racks_standing_on_the_lane) {
    const float pitch = lanePitch(kG);
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        int rack = 0;
        for (const Prop& p : buildProps(t, kG)) {
            if (p.shape != Shape::Pin || std::abs(p.transform.position.y - kG.pinComHeight) > 1e-5f)
                continue;
            ++rack;
            EXPECT_TRUE(p.pinBand) << t.name;
            const float lane = std::round(p.transform.position.x / pitch);
            EXPECT_NE(lane, 0.f) << "decor pins never stand on the main lane";
            EXPECT_LE(std::abs(lane), static_cast<float>(t.neighbourLanesPerSide)) << t.name;
            EXPECT_LE(std::abs(p.transform.position.x - lane * pitch), kG.laneHalfWidth) << t.name;
            EXPECT_GE(p.transform.position.z, kG.headPinZ - 0.1f) << t.name;
            EXPECT_LE(p.transform.position.z, kG.laneLength) << t.name;
        }
        EXPECT_EQ(rack, 10 * 2 * t.neighbourLanesPerSide) << t.name;
    }
}

TEST(test_environment, neighbour_lanes_have_board_stripes) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        const auto props = buildProps(t, kG);
        const auto striped = std::count_if(props.begin(), props.end(), [](const Prop& p) { return p.stripes; });
        EXPECT_EQ(striped, 2 * t.neighbourLanesPerSide) << t.name;
    }
}
