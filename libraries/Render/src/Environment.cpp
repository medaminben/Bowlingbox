#include <Bowlingbox/Render/Environment.h>
#include "PropBuilders.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>

namespace Bowlingbox {
namespace Render {
namespace {
using detail::pin;
using detail::slab;

Material lit(float r, float g, float b) {
    return {{r, g, b}, false};
}
Material glow(float r, float g, float b) {
    return {{r, g, b}, true};
}

const Material kBall = lit(0.098f, 0.365f, 0.925f);
const Material kPinBody = lit(0.941f, 0.941f, 0.941f);
const Material kPinBand = lit(0.902f, 0.f, 0.f);
const Material kUnused = lit(0.5f, 0.5f, 0.5f);

const Theme kNeonHall {
    .id = ThemeId::NeonHall,
    .name = "neon",
    .label = "Neon Hall",
    .backgroundTop = {0.01f, 0.02f, 0.08f},
    .backgroundBottom = {0.04f, 0.05f, 0.18f},
    .fogColor = {0.03f, 0.04f, 0.14f},
    .fogStart = 12.f,
    .fogEnd = 35.f,
    .lightDir = {-0.3f, -1.f, 0.4f},
    .ambient = 0.40f,
    .lane = lit(0.831f, 0.639f, 0.451f),
    .gutter = lit(0.008f, 0.055f, 0.22f),
    .ball = kBall,
    .pinBody = kPinBody,
    .pinBand = kPinBand,
    .indoor = true,
    .neighbourLanesPerSide = 2,
    .floor = lit(0.02f, 0.03f, 0.10f),
    .wall = lit(0.03f, 0.05f, 0.20f),
    .ceiling = lit(0.02f, 0.03f, 0.12f),
    .trim = lit(0.05f, 0.08f, 0.30f),
    .accentA = glow(0.f, 0.9f, 1.f),
    .accentB = glow(0.62f, 0.2f, 1.f),
    .screen = glow(0.1f, 0.3f, 0.9f),
    .props = {.pillars = true, .neonStrips = true, .ceilingLights = true, .wallScreens = true,
              .scoreMonitor = false, .fences = false, .trees = false},
};

const Theme kClassicAlley {
    .id = ThemeId::ClassicAlley,
    .name = "classic",
    .label = "Classic Alley",
    .backgroundTop = {0.93f, 0.88f, 0.78f},
    .backgroundBottom = {0.80f, 0.72f, 0.60f},
    .fogColor = {0.85f, 0.80f, 0.70f},
    .fogStart = 20.f,
    .fogEnd = 45.f,
    .lightDir = {-0.2f, -1.f, 0.3f},
    .ambient = 0.55f,
    .lane = lit(0.86f, 0.70f, 0.50f),
    .gutter = lit(0.25f, 0.25f, 0.28f),
    .ball = kBall,
    .pinBody = kPinBody,
    .pinBand = kPinBand,
    .indoor = true,
    .neighbourLanesPerSide = 2,
    .floor = lit(0.45f, 0.30f, 0.18f),
    .wall = lit(0.88f, 0.82f, 0.70f),
    .ceiling = lit(0.95f, 0.95f, 0.92f),
    .trim = lit(0.50f, 0.32f, 0.18f),
    .accentA = glow(1.f, 0.96f, 0.85f),
    .accentB = lit(0.3f, 0.3f, 0.3f),
    .screen = glow(0.05f, 0.1f, 0.3f),
    .props = {.pillars = false, .neonStrips = false, .ceilingLights = true, .wallScreens = false,
              .scoreMonitor = true, .fences = false, .trees = false},
};

const Theme kOutdoor {
    .id = ThemeId::Outdoor,
    .name = "outdoor",
    .label = "Outdoor",
    .backgroundTop = {0.35f, 0.60f, 0.95f},
    .backgroundBottom = {0.80f, 0.90f, 1.00f},
    .fogColor = {0.80f, 0.88f, 0.98f},
    .fogStart = 25.f,
    .fogEnd = 60.f,
    .lightDir = {-0.4f, -1.f, 0.3f},
    .ambient = 0.45f,
    .lane = lit(0.831f, 0.639f, 0.451f),
    .gutter = lit(0.30f, 0.30f, 0.32f),
    .ball = kBall,
    .pinBody = kPinBody,
    .pinBand = kPinBand,
    .indoor = false,
    .neighbourLanesPerSide = 1,
    .floor = lit(0.25f, 0.55f, 0.20f),
    .wall = kUnused,
    .ceiling = kUnused,
    .trim = lit(0.55f, 0.40f, 0.25f),
    .accentA = lit(0.15f, 0.45f, 0.15f),
    .accentB = lit(0.40f, 0.26f, 0.13f),
    .screen = lit(0.1f, 0.1f, 0.1f),
    .props = {.pillars = false, .neonStrips = false, .ceilingLights = false, .wallScreens = false,
              .scoreMonitor = false, .fences = true, .trees = true},
};

constexpr float kSlab = 0.1f;             // floor and pit thickness
constexpr float kPitMargin = 0.12f;       // pit is this much wider than the gutters on each side
constexpr float kMaskBottomY = 0.9f;
constexpr float kMaskTopY = 2.4f;
constexpr float kOutdoorHalfWidth = 40.f;
constexpr float kOutdoorFrontZ = -20.f;
constexpr float kOutdoorBackZ = 80.f;

/// BulletWorld::pinPositions(): head pin first, then rows of 2, 3 and 4.
std::array<glm::vec2, 10> rack(const SceneGeometry& g) {
    const float s = g.pinSpacing, row = s * 0.8660254f, z0 = g.headPinZ;
    return {{{0.f, z0},
             {-s / 2, z0 + row}, {s / 2, z0 + row},
             {-s, z0 + 2 * row}, {0.f, z0 + 2 * row}, {s, z0 + 2 * row},
             {-1.5f * s, z0 + 3 * row}, {-s / 2, z0 + 3 * row}, {s / 2, z0 + 3 * row}, {1.5f * s, z0 + 3 * row}}};
}

/// Ground slabs (top at kFloorTopY) over [-halfWidth, halfWidth] x [front, back],
/// with a hole over the main pit, plus the dark pit box itself.
void addGroundAndPit(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g, float halfWidth,
                     float front, float back) {
    const float px = g.laneHalfWidth + g.gutterWidth + kPitMargin;
    const float y0 = kFloorTopY - kSlab, y1 = kFloorTopY;
    const float pitEnd = g.laneLength + kPitLength;
    out.push_back(slab({-halfWidth, y0, front}, {-px, y1, back}, t.floor));
    out.push_back(slab({px, y0, front}, {halfWidth, y1, back}, t.floor));
    out.push_back(slab({-px, y0, front}, {px, y1, g.laneLength}, t.floor));
    out.push_back(slab({-px, y0, pitEnd}, {px, y1, back}, t.floor));
    out.push_back(slab({-px, kPitTopY - kSlab, g.laneLength}, {px, kPitTopY, pitEnd}, t.gutter));
}

/// Floor, walls, ceiling and the masking unit over the pinsetters.
void addHall(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g) {
    const float w = hallHalfWidth(t, g), h = kHallHeight, th = kWallThickness;
    addGroundAndPit(out, t, g, w, kHallFrontZ, kHallBackZ);
    out.push_back(slab({-w - th, kFloorTopY, kHallFrontZ}, {-w, h, kHallBackZ}, t.wall));
    out.push_back(slab({w, kFloorTopY, kHallFrontZ}, {w + th, h, kHallBackZ}, t.wall));
    out.push_back(slab({-w, kFloorTopY, kHallBackZ}, {w, h, kHallBackZ + th}, t.wall));
    out.push_back(slab({-w, kFloorTopY, kHallFrontZ - th}, {w, h, kHallFrontZ}, t.wall));
    out.push_back(slab({-w - th, h, kHallFrontZ - th}, {w + th, h + th, kHallBackZ + th}, t.ceiling));
    out.push_back(slab({-w, kMaskBottomY, g.laneLength + 0.05f}, {w, kMaskTopY, g.laneLength + 0.35f}, t.trim));
}

/// A copy of the main lane centred at x0: lane with approach, two gutters, a full rack.
void addNeighbourLane(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g, float x0) {
    const float hw = g.laneHalfWidth, outer = hw + g.gutterWidth;
    Prop lane = slab({x0 - hw, -0.1f, -g.approach}, {x0 + hw, 0.f, g.laneLength}, t.lane);
    lane.stripes = true;
    out.push_back(lane);
    for (float side : {-1.f, 1.f}) {
        const float a = x0 + side * hw, b = x0 + side * outer;
        out.push_back(slab({std::min(a, b), -g.gutterDepth - 0.1f, 0.f},
                           {std::max(a, b), -g.gutterDepth, g.laneLength}, t.gutter));
    }
    for (const glm::vec2& p : rack(g)) {
        Prop standing = pin({x0 + p.x, g.pinComHeight, p.y}, glm::vec3(1.f), t.pinBody);
        standing.pinBand = true;
        out.push_back(standing);
    }
}

/// One return in the middle of every gap between adjacent lanes.
void addBallReturns(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g) {
    const int n = t.neighbourLanesPerSide;
    const float pitch = lanePitch(g);
    for (int k = -n; k < n; ++k) {
        const float x = (static_cast<float>(k) + 0.5f) * pitch;
        out.push_back(slab({x - kReturnWidth / 2, 0.f, -g.approach},
                           {x + kReturnWidth / 2, kReturnHeight, g.laneLength}, t.gutter));
    }
}
}  // namespace

glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1.f), position) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.f), scale);
}

const Theme& theme(ThemeId id) {
    switch (id) {
        case ThemeId::ClassicAlley: return kClassicAlley;
        case ThemeId::Outdoor:      return kOutdoor;
        case ThemeId::NeonHall:     break;
    }
    return kNeonHall;
}

std::optional<ThemeId> themeFromName(std::string_view name) {
    for (ThemeId id : kThemes) {
        const std::string_view known = theme(id).name;
        const bool same = known.size() == name.size() &&
                          std::equal(known.begin(), known.end(), name.begin(), [](char k, char n) {
                              return k == std::tolower(static_cast<unsigned char>(n));
                          });
        if (same)
            return id;
    }
    return std::nullopt;
}

ThemeId nextTheme(ThemeId id) {
    const auto it = std::find(kThemes.begin(), kThemes.end(), id);
    const auto index = static_cast<std::size_t>(it - kThemes.begin());
    return kThemes[(index + 1) % kThemes.size()];
}

float lanePitch(const SceneGeometry& g) {
    return 2.f * (g.laneHalfWidth + g.gutterWidth) + kReturnGap;
}

float hallHalfWidth(const Theme& t, const SceneGeometry& g) {
    return lanePitch(g) * (static_cast<float>(t.neighbourLanesPerSide) + 0.5f) + 0.6f;
}

std::vector<Prop> buildProps(const Theme& t, const SceneGeometry& g) {
    std::vector<Prop> props;
    if (t.indoor)
        addHall(props, t, g);
    else
        addGroundAndPit(props, t, g, kOutdoorHalfWidth, kOutdoorFrontZ, kOutdoorBackZ);
    const float pitch = lanePitch(g);
    for (int k = 1; k <= t.neighbourLanesPerSide; ++k)
        for (float side : {-1.f, 1.f})
            addNeighbourLane(props, t, g, side * pitch * static_cast<float>(k));
    addBallReturns(props, t, g);
    return props;
}

MeshData shapeMesh(Shape shape, const SceneGeometry& g) {
    switch (shape) {
        case Shape::Box:      return boxData(0.5f, 0.5f, 0.5f);
        case Shape::Cylinder: return cylinderData(0.5f, 1.f, 24);
        case Shape::Cone:     return latheData({{0.5f, -0.5f}, {0.f, 0.5f}}, 24);
        case Shape::Pin:      break;
    }
    return pinData(g.pinHeight, g.pinMaxRadius, g.pinComHeight);
}

Aabb localBounds(Shape shape, const SceneGeometry& g) {
    if (shape == Shape::Pin)
        return {{-g.pinMaxRadius, -g.pinComHeight, -g.pinMaxRadius},
                {g.pinMaxRadius, g.pinHeight - g.pinComHeight, g.pinMaxRadius}};
    return {glm::vec3(-0.5f), glm::vec3(0.5f)};
}

Aabb bounds(const Prop& prop, const SceneGeometry& g) {
    const Aabb local = localBounds(prop.shape, g);
    const glm::mat4 m = prop.transform.matrix();
    Aabb out {glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner {(i & 1) ? local.max.x : local.min.x, (i & 2) ? local.max.y : local.min.y,
                                (i & 4) ? local.max.z : local.min.z};
        const glm::vec3 w = glm::vec3(m * glm::vec4(corner, 1.f));
        out.min = glm::min(out.min, w);
        out.max = glm::max(out.max, w);
    }
    return out;
}

}  // namespace Render
}  // namespace Bowlingbox
