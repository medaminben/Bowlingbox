#include "EnvironmentDecor.h"
#include "PropBuilders.h"
#include <algorithm>
#include <array>
#include <utility>

namespace Bowlingbox {
namespace Render {
namespace {
using detail::box;
using detail::pin;
using detail::shaped;
using detail::slab;

constexpr float kStrip = 0.06f;  // neon strip thickness
constexpr float kScreenY = 2.3f;
const Material kIconWhite {{1.f, 1.f, 1.f}, true, 1.f, 0.f, 1.f, Pattern::None};

struct Tree {
    float x, z, s;
};
// Mirrored to both sides: 16 trees.
constexpr std::array<Tree, 8> kTrees {{{6.5f, -4.f, 1.0f}, {8.f, 4.f, 1.3f}, {6.f, 12.f, 0.9f}, {9.5f, 18.f, 1.4f},
                                       {7.f, 26.f, 1.1f}, {11.f, 34.f, 1.5f}, {8.5f, 44.f, 1.2f}, {12.f, 55.f, 1.6f}}};

/// x range of a layer `from`..`to` metres in from the side wall at x = side * w.
std::pair<float, float> layer(float side, float w, float from, float to) {
    const float a = side * (w - from), b = side * (w - to);
    return {std::min(a, b), std::max(a, b)};
}

void addPillars(std::vector<Prop>& out, const Theme& t, float w) {
    for (float z : {-6.f, -1.f, 4.f, 9.f, 14.f, 19.f})
        for (float side : {-1.f, 1.f}) {
            const auto [x0, x1] = layer(side, w, 0.f, 0.4f);
            out.push_back(slab({x0, kFloorTopY, z - 0.2f}, {x1, kHallHeight, z + 0.2f}, t.trim));
        }
}

void addNeonStrips(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g, float w) {
    for (float side : {-1.f, 1.f}) {
        const auto [x0, x1] = layer(side, w, 0.f, kStrip);
        out.push_back(slab({x0, 1.0f - kStrip / 2, kHallFrontZ}, {x1, 1.0f + kStrip / 2, kHallBackZ}, t.accentA));
        out.push_back(slab({x0, 3.4f - kStrip / 2, kHallFrontZ}, {x1, 3.4f + kStrip / 2, kHallBackZ}, t.accentB));
    }
    // Along the masking unit's lower front edge, centred on its face (z = laneLength + 0.05).
    const float z = g.laneLength + 0.05f;
    out.push_back(slab({-w, 0.9f - kStrip / 2, z - kStrip / 2}, {w, 0.9f + kStrip / 2, z + kStrip / 2}, t.accentA));
}

void addCeilingLights(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g) {
    const float x = lanePitch(g) / 2;
    for (int i = 0; i < 7; ++i) {
        const float z = -6.f + 4.f * static_cast<float>(i);
        for (float side : {-1.f, 1.f})
            out.push_back(box({side * x, kHallHeight - 0.2f, z}, {1.2f, 0.05f, 0.3f}, t.accentA));
    }
}

void addWallScreens(std::vector<Prop>& out, const Theme& t, float w) {
    for (float side : {-1.f, 1.f})
        for (float z : {6.f, 12.f}) {
            const auto [fx0, fx1] = layer(side, w, 0.f, 0.08f);
            out.push_back(slab({fx0, kScreenY - 0.8f, z - 1.3f}, {fx1, kScreenY + 0.8f, z + 1.3f}, t.trim));
            const auto [px0, px1] = layer(side, w, 0.08f, 0.10f);
            out.push_back(slab({px0, kScreenY - 0.7f, z - 1.2f}, {px1, kScreenY + 0.7f, z + 1.2f}, t.screen));
            Prop icon = pin({side * (w - 0.12f), kScreenY - 0.15f, z}, {0.2f, 3.f, 3.f}, kIconWhite);
            icon.pinBand = true;
            out.push_back(icon);
        }
}

void addScoreMonitor(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g) {
    const float z = g.laneLength + 0.2f;
    out.push_back(box({0.f, 2.9f, z}, {3.f, 0.9f, 0.1f}, t.trim));
    out.push_back(slab({-1.4f, 2.55f, z - 0.07f}, {1.4f, 3.25f, z - 0.05f}, t.screen));
}

void addFences(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g) {
    const float f = lanePitch(g) * static_cast<float>(t.neighbourLanesPerSide) + g.laneHalfWidth + g.gutterWidth + 1.f;
    for (float side : {-1.f, 1.f}) {
        const float x = side * f;
        for (int i = 0; i < 19; ++i) {
            const float z = -6.f + 2.f * static_cast<float>(i);
            out.push_back(slab({x - 0.05f, kFloorTopY, z - 0.05f}, {x + 0.05f, kFloorTopY + 1.f, z + 0.05f}, t.trim));
        }
        for (float y : {0.4f, 0.8f})
            out.push_back(box({x, y, 12.f}, {0.05f, 0.08f, 36.f}, t.trim));
    }
}

void addTrees(std::vector<Prop>& out, const Theme& t) {
    for (const Tree& tree : kTrees)
        for (float side : {-1.f, 1.f}) {
            const float x = side * tree.x, s = tree.s;
            out.push_back(shaped(Shape::Cylinder, {x, kFloorTopY + 0.75f * s, tree.z}, {0.3f * s, 1.5f * s, 0.3f * s},
                                 t.accentB));
            out.push_back(shaped(Shape::Cone, {x, kFloorTopY + 2.75f * s, tree.z}, {2.4f * s, 2.5f * s, 2.4f * s},
                                 t.accentA));
        }
}
}  // namespace

void addDecor(std::vector<Prop>& out, const Theme& t, const SceneGeometry& g) {
    const PropSet& p = t.props;
    if (t.indoor) {
        const float w = hallHalfWidth(t, g);
        if (p.pillars) addPillars(out, t, w);
        if (p.neonStrips) addNeonStrips(out, t, g, w);
        if (p.ceilingLights) addCeilingLights(out, t, g);
        if (p.wallScreens) addWallScreens(out, t, w);
        if (p.scoreMonitor) addScoreMonitor(out, t, g);
    } else {
        if (p.fences) addFences(out, t, g);
        if (p.trees) addTrees(out, t);
    }
}

}  // namespace Render
}  // namespace Bowlingbox
