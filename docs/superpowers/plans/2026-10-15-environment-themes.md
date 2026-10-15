# Environment Themes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BowlingGL draws a themed environment (Neon Hall, Classic Alley, Outdoor) around the lane. You pick the theme with `--theme`, the `T` key or the View ▸ Theme menu.

**Architecture:**
- Render gets a GL-free `Environment` module: theme data, the presets, shape meshes and bounds, and `buildProps()`, which lays out the decor as a list of boxes, cylinders, cones and pins.
- `SceneRenderer` draws a background gradient and the props, then the existing lane, pins and ball, all in the theme's colours. Its lit shader adds fog, glowing (emissive) materials and lane stripes.
- BowlingGL adds a `CycleTheme` command, a View ▸ Theme menu and a `--theme` option.

**Tech Stack:** C++20, CMake ≥ 3.29, OpenGL 3.3 core (GLAD), GLM, wxWidgets 3.2, GoogleTest.

**Spec:** `docs/superpowers/specs/2026-10-15-environment-themes-design.md`

## Global Constraints

- **Warnings:** every target builds with `-Wall -Wextra -Wpedantic -Werror` (`bowlingbox_set_warnings`). An aggregate initialiser that leaves out a field fails `-Wmissing-field-initializers`, so use the helper functions shown, or name every field.
- **Dependencies:** Render must not include Alley. Physics, `Alley`, `Score` and `BulletWorld` do not change.
- **Theme names:** the only name table is `Theme::name` (`"neon"`, `"classic"`, `"outdoor"`). Menu labels come from `Theme::label`.
- **GL-free code:** `buildProps`, `shapeMesh`, `localBounds` and `bounds` make no GL calls.
- **Tests:** test names follow the existing `TEST(test_<area>, what_it_checks)` style.
- **Build and test (each worktree):** run this inside the task's worktree. The dependencies are reused from the main checkout, so nothing is downloaded:

```bash
D=/home/amin/Dokumente/dev/Bowlingbox/build/_deps
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_WX_UI=ON -DGENERATE_SCAFFOLD=OFF \
      -DFETCHCONTENT_SOURCE_DIR_BULLET3=$D/bullet3-src -DFETCHCONTENT_SOURCE_DIR_GLM=$D/glm-src \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$D/googletest-src
cmake --build build -j"$(nproc)"
ctest --test-dir build --label-regex unit --output-on-failure
```

## Spec clarifications (decided here, apply to all tasks)

- **`Prop::stripes`:** `Prop` gets a `bool stripes = false` field. The spec puts lane stripes on the neighbour lane boxes, and the renderer needs a flag to know which props are lanes. A neighbour lane is a unit box scaled to its width, so its boards come out at 0.02625 m instead of 0.025 m. That difference is fine.
- **Case-insensitive names:** `themeFromName` ignores ASCII case, so `--theme=Classic` works. The names it returns stay lowercase.
- **Wall-screen icons:** these use the identity rotation and scale (0.2, 3, 3), which makes them thin along world x and flat against the side wall. A pin is rotationally symmetric about y, so this is the same as the spec's "90° turn".
- **Pillars:** they span from the floor top (y = -0.15) to the ceiling (y = 4.5).
- **Unknown-theme message:** `HudText::unknownThemeMessage(name)` builds it, so it can be unit-tested.
- **GL init errors:** `BowlingCanvas::ensureRenderer` also writes GL init errors to stderr, so a shader that fails to compile can be detected without looking at the screen.

## Review Focus

1. **`--theme=Classic` / `--theme=`:** capitals select the theme. An empty or unknown value prints `unknown theme '<x>'; use neon, classic or outdoor` and starts Neon Hall. Pinned by Task 1 (`theme_names_ignore_case_and_reject_unknown`) and Task 5 (`unknown_theme_message_lists_every_name`).
2. **Ball over the gutter at full aim offset, and pins flying off the deck:** they stay visible because no decor enters the play or sight corridor. Pinned by Task 1 (`no_prop_enters_the_corridors_or_the_camera_box`, which runs over the full decor once Task 2 lands).
3. **`T` held down or pressed during a roll:** the theme cycles and the throw is untouched. Pinned by Task 4 (`t_cycles_the_theme_in_every_phase`, `cycle_theme_leaves_the_lane_alone`) and Task 1 (`next_theme_visits_all_three_and_wraps`).
4. **A theme chosen on the command line before the GL context exists:** it must show on the first frame. This can't be unit-tested (it needs a window). Pinned by Task 5, Step 6 (smoke run with `--theme=outdoor`) and the Task 7 hand check.
5. **After the shader rewrite:** the aim guide stays translucent and unfogged, and the pin necks stay red. This can't be unit-tested. Pinned by Task 3's shader code (`uFog` off for the guide, alpha passed through, band inputs kept) and the Task 7 hand check.

## Parallel execution

| Wave | Tasks (run in parallel, each in its own worktree) | Needs |
|---|---|---|
| 1 | Task 1 (Render environment data + core layout), Task 4 (BowlingGL `CycleTheme` command) | — |
| 2 | Task 2 (decor props), Task 3 (GL drawing) | Task 1 merged |
| 3 | Task 5 (app wiring), Task 6 (docs) | Tasks 2, 3, 4 merged |
| 4 | Task 7 (completeness review) | all merged |

No two tasks in the same wave edit the same file.

---

### Task 1: Environment data, shapes and core layout (Render)

**Files:**
- Create: `libraries/Render/include/Bowlingbox/Render/Environment.h`
- Create: `libraries/Render/src/Environment.cpp`
- Create: `libraries/Render/src/PropBuilders.h`
- Create: `libraries/Render/test/src/test_Environment.cpp`
- Modify: `libraries/Render/include/Bowlingbox/Render/Render.h` (remove `Transform` and `SceneGeometry`, include `Environment.h`)
- Modify: `libraries/Render/src/Render.cpp` (remove `Transform::matrix`)
- Modify: `libraries/Render/CMakeLists.txt` (new header, sources and test)
- Modify: `applications/bowlingGame/bowlingGameGL/SceneAdapter.cpp` (copy `pinSpacing`, `headPinZ`)

**Interfaces:**
- Consumes: `boxData`, `cylinderData`, `latheData`, `pinData`, `MeshData` from `Geometry.h`.
- Produces, used by Tasks 2, 3 and 5:
  - all types and functions in `Environment.h` below
  - `detail::shaped`, `detail::box`, `detail::slab` and `detail::pin` in `src/PropBuilders.h`
  - `buildProps` (Task 2 appends a call to `addDecor`)

- [ ] **Step 1: Write the failing tests**

Create `libraries/Render/test/src/test_Environment.cpp`:

```cpp
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
```

Add the test to `libraries/Render/CMakeLists.txt`:

```cmake
set(TST_SOURCES
    test/src/test_Render.cpp
    test/src/test_Environment.cpp
)
```

- [ ] **Step 2: Run the build to see it fail**

Run the build from Global Constraints.
Expected: compile error `Bowlingbox/Render/Environment.h: No such file or directory`.

- [ ] **Step 3: Write `Environment.h`**

```cpp
#ifndef BOWLINGBOX_RENDER_ENVIRONMENT_H
#define BOWLINGBOX_RENDER_ENVIRONMENT_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Render/Geometry.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace Bowlingbox {
namespace Render {

struct BOWLINGBOX_RENDER_API Transform {
    glm::vec3 position {0.f};
    glm::quat rotation {1.f, 0.f, 0.f, 0.f};  // w, x, y, z
    glm::vec3 scale {1.f};
    glm::mat4 matrix() const;
};

/// Physical sizes (metres); copied from Alley::kLane by the application.
struct SceneGeometry {
    float laneHalfWidth, laneLength, approach, gutterWidth, gutterDepth;
    float ballRadius, pinHeight, pinMaxRadius, pinComHeight;
    float pinSpacing, headPinZ;
};

enum class ThemeId { NeonHall, ClassicAlley, Outdoor };
inline constexpr std::array<ThemeId, 3> kThemes {ThemeId::NeonHall, ThemeId::ClassicAlley, ThemeId::Outdoor};

struct Material {
    glm::vec3 color {1.f};
    bool emissive = false;  // glows: ignores the light, fogged at half strength
};

struct PropSet {
    bool pillars = false, neonStrips = false, ceilingLights = false, wallScreens = false;
    bool scoreMonitor = false, fences = false, trees = false;
};

struct Theme {
    ThemeId id;
    std::string_view name;   // "neon", "classic", "outdoor": the only name table
    std::string_view label;  // "Neon Hall", "Classic Alley", "Outdoor"
    glm::vec3 backgroundTop, backgroundBottom;
    glm::vec3 fogColor;
    float fogStart, fogEnd;  // metres from the camera
    glm::vec3 lightDir;      // direction the light travels
    float ambient;           // 0..1
    Material lane, gutter, ball, pinBody, pinBand;
    bool indoor;             // walls + ceiling; otherwise open ground
    int neighbourLanesPerSide;
    Material floor, wall, ceiling, trim, accentA, accentB, screen;
    PropSet props;
};

/// Box, Cylinder and Cone are unit shapes (1 m across, centred on the origin), so
/// `transform.scale` is their size in metres. Pin is the real pin mesh, base at y = -pinComHeight.
enum class Shape { Box, Cylinder, Cone, Pin };

struct Prop {
    Shape shape = Shape::Box;
    Transform transform;
    Material material;
    bool pinBand = false;  // draw the theme's pinBand on the neck (Pin only)
    bool stripes = false;  // lane boards (neighbour lane boxes only)
};

struct Aabb {
    glm::vec3 min {0.f};
    glm::vec3 max {0.f};
};

// Layout constants, metres.
inline constexpr float kReturnGap = 0.30f;     // between the gutters of adjacent lanes
inline constexpr float kReturnWidth = 0.14f;   // ball-return box in the middle of that gap
inline constexpr float kReturnHeight = 0.3f;
inline constexpr float kHallHeight = 4.5f;     // ceiling's inner face
inline constexpr float kHallFrontZ = -8.f;
inline constexpr float kHallBackZ = 22.f;
inline constexpr float kWallThickness = 0.2f;
inline constexpr float kFloorTopY = -0.15f;
inline constexpr float kPitLength = 2.4f;      // BulletWorld's pit behind the deck
inline constexpr float kPitTopY = -0.25f;

BOWLINGBOX_RENDER_API const Theme& theme(ThemeId id);
/// ASCII case-insensitive; empty for an unknown name.
BOWLINGBOX_RENDER_API std::optional<ThemeId> themeFromName(std::string_view name);
BOWLINGBOX_RENDER_API ThemeId nextTheme(ThemeId id);  // Neon -> Classic -> Outdoor -> Neon
/// Distance between lane centres: 2 * (laneHalfWidth + gutterWidth) + kReturnGap.
BOWLINGBOX_RENDER_API float lanePitch(const SceneGeometry& g);
/// Inner half-width of an indoor hall: lanePitch * (neighbourLanesPerSide + 0.5) + 0.6.
BOWLINGBOX_RENDER_API float hallHalfWidth(const Theme& theme, const SceneGeometry& g);
BOWLINGBOX_RENDER_API std::vector<Prop> buildProps(const Theme& theme, const SceneGeometry& g);
/// The mesh drawn for a shape, and its local box (min/max over that mesh's vertices).
BOWLINGBOX_RENDER_API MeshData shapeMesh(Shape shape, const SceneGeometry& g);
BOWLINGBOX_RENDER_API Aabb localBounds(Shape shape, const SceneGeometry& g);
/// World box: localBounds' eight corners through transform.matrix().
BOWLINGBOX_RENDER_API Aabb bounds(const Prop& prop, const SceneGeometry& g);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_ENVIRONMENT_H
```

- [ ] **Step 4: Write `src/PropBuilders.h`**

```cpp
#ifndef BOWLINGBOX_RENDER_PROP_BUILDERS_H
#define BOWLINGBOX_RENDER_PROP_BUILDERS_H
#include <Bowlingbox/Render/Environment.h>

namespace Bowlingbox {
namespace Render {
namespace detail {

/// A shape centred at `centre`, `size` metres across (unit shapes are 1 m).
inline Prop shaped(Shape shape, glm::vec3 centre, glm::vec3 size, const Material& m) {
    Prop p;
    p.shape = shape;
    p.transform.position = centre;
    p.transform.scale = size;
    p.material = m;
    return p;
}
inline Prop box(glm::vec3 centre, glm::vec3 size, const Material& m) {
    return shaped(Shape::Box, centre, size, m);
}
/// An axis-aligned box from its low corner to its high corner.
inline Prop slab(glm::vec3 lo, glm::vec3 hi, const Material& m) {
    return box((lo + hi) * 0.5f, hi - lo, m);
}
/// A pin whose origin (pinComHeight above its base) is at `origin`.
inline Prop pin(glm::vec3 origin, glm::vec3 scale, const Material& m) {
    return shaped(Shape::Pin, origin, scale, m);
}

}  // namespace detail
}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_PROP_BUILDERS_H
```

- [ ] **Step 5: Write `src/Environment.cpp`**

```cpp
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
```

- [ ] **Step 6: Move `Transform` and `SceneGeometry` out of `Render.h`**

In `libraries/Render/include/Bowlingbox/Render/Render.h`:
- Delete the `struct BOWLINGBOX_RENDER_API Transform { … };` block.
- Delete the `/// Physical sizes …` comment and the `struct SceneGeometry { … };` block.
- Add `#include <Bowlingbox/Render/Environment.h>` after `#include <Bowlingbox/Render/Canvas2D.h>`.

In `libraries/Render/src/Render.cpp`, delete the `// ---- Transform` section, which is the `glm::mat4 Transform::matrix() const { … }` definition.

Add the new files to `libraries/Render/CMakeLists.txt`:

```cmake
set(TARGET_HEADERS
    src/GlShader.h
    src/GlMesh.h
    src/ContextData.h
    src/PropBuilders.h
    include/Bowlingbox/Render/Geometry.h
    include/Bowlingbox/Render/Canvas2D.h
    include/Bowlingbox/Render/ScoreboardLayout.h
    include/Bowlingbox/Render/ScoreboardPainter.h
    include/Bowlingbox/Render/Environment.h
    include/Bowlingbox/Render/Render.h
)
set(TARGET_SOURCES
    src/Geometry.cpp
    src/Canvas2D.cpp
    src/ScoreboardLayout.cpp
    src/ScoreboardPainter.cpp
    src/Environment.cpp
    src/GlShader.cpp
    src/GlMesh.cpp
    src/Render.cpp
)
```

- [ ] **Step 7: Copy the new geometry fields in `SceneAdapter.cpp`**

`SceneGeometry` now has two more fields. Leaving them out would fail `-Wmissing-field-initializers`. Replace `SceneAdapter::geometry` with:

```cpp
Render::SceneGeometry geometry(const Alley::LaneDimensions& d) {
    return {d.halfWidth, d.length, d.approach, d.gutterWidth, d.gutterDepth,
            d.ballRadius, d.pinHeight, d.pinMaxRadius, d.pinComHeight,
            d.pinSpacing, d.headPinZ};
}
```

- [ ] **Step 8: Build and run every test**

Run the build from Global Constraints.
Expected: no warnings. All tests pass, including the 13 new `test_environment.*` tests. `test_bowlinggl` still passes.

- [ ] **Step 9: Commit**

```bash
git add libraries/Render applications/bowlingGame/bowlingGameGL/SceneAdapter.cpp
git commit -m "feat(render): environment themes, shapes and core layout

Theme presets (Neon Hall, Classic Alley, Outdoor), unit shape meshes with
matching bounds, and buildProps for neighbour lanes, ball returns, the hall
or open ground and the pit. Transform and SceneGeometry move to
Environment.h; SceneGeometry gains pinSpacing and headPinZ.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `CycleTheme` command and controls help (BowlingGL)

Runs in wave 1, in parallel with Task 1.

**Files:**
- Modify: `applications/bowlingGame/bowlingGameGL/Command.h`
- Modify: `applications/bowlingGame/bowlingGameGL/InputMapper.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/LaneCommands.h`
- Modify: `applications/bowlingGame/bowlingGameGL/LaneCommands.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/HudText.cpp`
- Test: `applications/bowlingGame/bowlingGameGL/test/test_InputMapper.cpp`, `applications/bowlingGame/bowlingGameGL/test/test_Gameplay.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `Command::Type::CycleTheme`, which Task 5 handles in `BowlingCanvas::apply`.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_InputMapper.cpp`:

```cpp
TEST(test_input_mapper, t_cycles_the_theme_in_every_phase) {
    for (Phase p : {Phase::Aiming, Phase::Charging, Phase::Rolling, Phase::Settling, Phase::FrameDone,
                    Phase::GameOver}) {
        EXPECT_EQ(InputMapper::keyDown('T', false, p), cmd(Type::CycleTheme));
        EXPECT_EQ(InputMapper::keyDown('t', false, p), cmd(Type::CycleTheme));
    }
}
```

In `test/test_Gameplay.cpp`, add after `reports_rejections_and_ignores_non_lane_commands`:

```cpp
TEST(test_lane_commands, cycle_theme_leaves_the_lane_alone) {
    Alley::Lane lane;
    ASSERT_TRUE(LaneCommands::execute({Command::Type::MoveAim, 0.1f}, lane).isValue());
    const Alley::Snapshot before = lane.snapshot();
    EXPECT_TRUE(LaneCommands::execute({Command::Type::CycleTheme}, lane).isValue());
    const Alley::Snapshot after = lane.snapshot();
    EXPECT_EQ(after.phase, before.phase);
    EXPECT_FLOAT_EQ(after.aimOffset, before.aimOffset);
    EXPECT_FLOAT_EQ(after.aimAngle, before.aimAngle);
    EXPECT_FLOAT_EQ(after.power, before.power);
}
```

In the existing `controls_help_names_every_binding` test, extend the key list:

```cpp
    for (const char* key : {"Left / Right", "A / D", "Space", "N", "Esc", "Ctrl+Q", "T: change the scenery"})
```

- [ ] **Step 2: Run the tests to see them fail**

Run the build from Global Constraints.
Expected: compile error `'CycleTheme' is not a member of 'Command::Type'`.

- [ ] **Step 3: Add the command**

`Command.h`:

```cpp
    enum class Type { None, MoveAim, RotateAim, StartCharge, Release, NewGame, CycleTheme, Quit };
```

`InputMapper.cpp`, in `keyDown` right after the `NewGame` check:

```cpp
    if (keyCode == 'T' || keyCode == 't')
        return {Type::CycleTheme};
```

`LaneCommands.cpp`, in the switch:

```cpp
        case Command::Type::CycleTheme:
        case Command::Type::Quit:
        case Command::Type::None:
            break;
```

`LaneCommands.h`, update the comment:

```cpp
/// Applies a game command to the lane, using its amount for MoveAim / RotateAim.
/// CycleTheme, Quit and None are not lane commands and succeed without effect;
/// the caller handles CycleTheme and Quit.
```

`HudText.cpp`, in `controlsHelp()`:

```cpp
std::string controlsHelp() {
    return "Left / Right or mouse move: position the ball\n"
           "A / D or right-drag: aim angle\n"
           "Hold Space or left button: charge, release to throw\n"
           "T: change the scenery\n"
           "N: new game    Esc / Ctrl+Q: quit";
}
```

- [ ] **Step 4: Run the tests**

Run the build from Global Constraints.
Expected: no warnings, and all `test_bowlinggl` tests pass, including the two new ones.

- [ ] **Step 5: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL
git commit -m "feat(gl): T cycles the theme as a CycleTheme command

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Decor props (Render)

Runs in wave 2, in parallel with Task 3. Needs Task 1.

**Files:**
- Create: `libraries/Render/src/EnvironmentDecor.h`
- Create: `libraries/Render/src/EnvironmentDecor.cpp`
- Modify: `libraries/Render/src/Environment.cpp` (call `addDecor` at the end of `buildProps`)
- Modify: `libraries/Render/CMakeLists.txt` (add the two files)
- Test: `libraries/Render/test/src/test_Environment.cpp` (append)

**Interfaces:**
- Consumes, from Task 1:
  - `Theme`, `PropSet`, `Prop`, `Shape`, `SceneGeometry`, `lanePitch`, `hallHalfWidth`, `bounds`
  - the constants `kHallHeight`, `kHallFrontZ`, `kHallBackZ`, `kFloorTopY`
  - `detail::shaped`, `detail::box`, `detail::slab` and `detail::pin` (`src/PropBuilders.h`)
- Produces: `void addDecor(std::vector<Prop>& out, const Theme& theme, const SceneGeometry& g)` (internal, not exported).

- [ ] **Step 1: Write the failing tests**

Append to `libraries/Render/test/src/test_Environment.cpp`. It reuses `kG` and `bounds` from Task 1's helpers in the same file:

```cpp
namespace {
template <typename Pred>
long countProps(const std::vector<Prop>& props, Pred pred) {
    return static_cast<long>(std::count_if(props.begin(), props.end(), pred));
}
bool near(float a, float b) {
    return std::abs(a - b) < 1e-4f;
}
}  // namespace

TEST(test_environment_decor, only_neon_hall_hangs_glowing_pin_icons) {
    for (ThemeId id : kThemes) {
        const auto props = buildProps(theme(id), kG);
        const auto icons = countProps(props, [](const Prop& p) {
            return p.shape == Shape::Pin && p.transform.position.y > 1.f;
        });
        EXPECT_EQ(icons, theme(id).props.wallScreens ? 4 : 0) << theme(id).name;
        for (const Prop& p : props)
            if (p.shape == Shape::Pin && p.transform.position.y > 1.f) {
                EXPECT_TRUE(p.pinBand);
                EXPECT_TRUE(p.material.emissive);
            }
    }
}

TEST(test_environment_decor, neon_hall_has_pillars_strips_lights_and_screens) {
    const auto props = buildProps(theme(ThemeId::NeonHall), kG);
    EXPECT_EQ(countProps(props, [](const Prop& p) {
                  return p.shape == Shape::Box && near(p.transform.scale.x, 0.4f) && near(p.transform.scale.z, 0.4f);
              }),
              12);  // pillars: 6 per side wall
    // 4 wall strips + 1 masking strip + 14 ceiling panels + 4 screen panels
    EXPECT_EQ(countProps(props, [](const Prop& p) { return p.shape == Shape::Box && p.material.emissive; }), 23);
    EXPECT_EQ(countProps(props, [](const Prop& p) { return p.shape == Shape::Cone; }), 0);
}

TEST(test_environment_decor, classic_alley_has_ceiling_lights_and_a_score_monitor) {
    const auto props = buildProps(theme(ThemeId::ClassicAlley), kG);
    // 14 ceiling panels + 1 monitor screen
    EXPECT_EQ(countProps(props, [](const Prop& p) { return p.shape == Shape::Box && p.material.emissive; }), 15);
    EXPECT_EQ(countProps(props, [](const Prop& p) {
                  return p.shape == Shape::Box && near(p.transform.position.x, 0.f) &&
                         near(p.transform.position.y, 2.9f) && near(p.transform.position.z, kG.laneLength + 0.2f);
              }),
              1);
    EXPECT_EQ(countProps(props, [](const Prop& p) {
                  return p.shape == Shape::Box && near(p.transform.scale.x, 0.4f) && near(p.transform.scale.z, 0.4f);
              }),
              0);  // no pillars
}

TEST(test_environment_decor, outdoor_terrace_has_fences_and_sixteen_trees_below_six_and_a_half_metres) {
    const auto props = buildProps(theme(ThemeId::Outdoor), kG);
    EXPECT_EQ(countProps(props, [](const Prop& p) { return p.shape == Shape::Cylinder; }), 16);
    EXPECT_EQ(countProps(props, [](const Prop& p) { return p.shape == Shape::Cone; }), 16);
    EXPECT_EQ(countProps(props, [](const Prop& p) {
                  return p.shape == Shape::Box && near(p.transform.scale.x, 0.1f) && near(p.transform.scale.y, 1.f);
              }),
              38);  // 19 posts per side
    EXPECT_EQ(countProps(props, [](const Prop& p) {
                  return p.shape == Shape::Box && near(p.transform.scale.z, 36.f);
              }),
              4);  // two rails per side
    EXPECT_EQ(countProps(props, [](const Prop& p) { return p.material.emissive; }), 0);
    float top = std::numeric_limits<float>::lowest();
    for (const Prop& p : props)
        top = std::max(top, bounds(p, kG).max.y);
    EXPECT_LE(top, 6.5f);
    EXPECT_NEAR(top, 6.25f, 1e-3f);  // the tallest tree
}
```

- [ ] **Step 2: Run the tests to see them fail**

Run the build from Global Constraints.
Expected: the four `test_environment_decor.*` tests fail (no icons, pillars, lights, monitor, fences or trees yet). The Task 1 tests still pass.

- [ ] **Step 3: Write `src/EnvironmentDecor.h`**

```cpp
#ifndef BOWLINGBOX_RENDER_ENVIRONMENT_DECOR_H
#define BOWLINGBOX_RENDER_ENVIRONMENT_DECOR_H
#include <Bowlingbox/Render/Environment.h>
#include <vector>

namespace Bowlingbox {
namespace Render {
/// Appends the theme's optional props (pillars, neon strips, ceiling lights, wall screens,
/// score monitor, fences, trees) as selected by theme.props.
void addDecor(std::vector<Prop>& out, const Theme& theme, const SceneGeometry& g);
}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_ENVIRONMENT_DECOR_H
```

- [ ] **Step 4: Write `src/EnvironmentDecor.cpp`**

```cpp
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
const Material kIconWhite {{1.f, 1.f, 1.f}, true};

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
```

- [ ] **Step 5: Call it from `buildProps`, and register the files**

In `src/Environment.cpp`, add `#include "EnvironmentDecor.h"` after `#include "PropBuilders.h"`. Then make the end of `buildProps` read:

```cpp
    addBallReturns(props, t, g);
    addDecor(props, t, g);
    return props;
```

In `libraries/Render/CMakeLists.txt`:
- Add `src/EnvironmentDecor.h` to `TARGET_HEADERS`, after `src/PropBuilders.h`.
- Add `src/EnvironmentDecor.cpp` to `TARGET_SOURCES`, after `src/Environment.cpp`.

- [ ] **Step 6: Run every test**

Run the build from Global Constraints.
Expected: no warnings, and every test passes. In particular, Task 1's `no_prop_enters_the_corridors_or_the_camera_box` now covers the decor too, and must still pass.

- [ ] **Step 7: Commit**

```bash
git add libraries/Render
git commit -m "feat(render): theme decor - pillars, neon, lights, screens, monitor, fences, trees

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Draw the theme (Render GL)

Runs in wave 2, in parallel with Task 2. Needs Task 1.

**Files:**
- Modify: `libraries/Render/src/GlShader.h`, `libraries/Render/src/GlShader.cpp` (add `setFloat`, `setBool`)
- Modify: `libraries/Render/src/ContextData.h` (add the `sky` shader)
- Modify: `libraries/Render/src/Render.cpp` (shaders, `Context::create`, far plane, `SceneRenderer`)
- Modify: `libraries/Render/include/Bowlingbox/Render/Render.h` (`SceneRenderer` constructor and `setTheme`)
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp` (stderr line in `ensureRenderer`)

**Interfaces:**
- Consumes, from Task 1:
  - `Theme`, `ThemeId`, `theme()`, `buildProps()`, `shapeMesh()`
  - `Prop` (with `stripes` and `pinBand`), `Shape`, `Material`
- Produces, used by Task 5:
  - `SceneRenderer(Context& ctx, const SceneGeometry& geometry, ThemeId theme = ThemeId::NeonHall)`
  - `void SceneRenderer::setTheme(ThemeId theme)`, which makes no GL calls

GL drawing can't be unit-tested in this project, because no GL context exists in the tests. This task is checked by a clean build, the existing tests, and a smoke run that catches shader compile errors through stderr (Step 6).

- [ ] **Step 1: Add the uniform setters**

`GlShader.h`, after `setVec4`:

```cpp
    void setFloat(const char* name, float v) const;
    void setBool(const char* name, bool v) const;
```

`GlShader.cpp`, after `setVec4`:

```cpp
void GlShader::setFloat(const char* name, float v) const {
    glUniform1f(glGetUniformLocation(_program, name), v);
}
void GlShader::setBool(const char* name, bool v) const {
    glUniform1i(glGetUniformLocation(_program, name), v ? 1 : 0);
}
```

- [ ] **Step 2: Add the sky shader slot**

`ContextData.h`:

```cpp
struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, lit or emissive, colour band, fog, lane stripes
    std::unique_ptr<GlShader> flat;  // 2D overlay
    std::unique_ptr<GlShader> sky;   // background gradient behind the 3D scene
    int width = 1;
    int height = 1;
};
```

- [ ] **Step 3: Replace the lit shader and add the sky shader in `Render.cpp`**

Replace `kLitVs` and `kLitFs` with the following, and add `kSkyVs` and `kSkyFs` after `kFlatFs`:

```cpp
const char* kLitVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj; uniform mat3 uNormalMat;
out vec3 vNormal; out float vLocalY; out float vLocalX; out float vViewDist;
void main() {
    vNormal = normalize(uNormalMat * aNormal);
    vLocalY = aPos.y;
    vLocalX = aPos.x;
    vec4 viewPos = uView * uModel * vec4(aPos, 1.0);
    vViewDist = length(viewPos.xyz);
    gl_Position = uProj * viewPos;
})";

const char* kLitFs = R"(#version 330 core
in vec3 vNormal; in float vLocalY; in float vLocalX; in float vViewDist;
uniform vec4 uColor; uniform vec4 uBandColor; uniform vec2 uBandRange; uniform vec3 uLightDir;
uniform float uAmbient; uniform bool uEmissive; uniform bool uFog; uniform bool uStripes;
uniform vec3 uFogColor; uniform float uFogStart; uniform float uFogEnd;
out vec4 FragColor;
void main() {
    vec4 base = (vLocalY >= uBandRange.x && vLocalY <= uBandRange.y) ? uBandColor : uColor;
    if (uStripes)  // boards 0.025 m wide, alternating +-6 %
        base.rgb *= mod(floor(vLocalX / 0.025), 2.0) < 1.0 ? 1.06 : 0.94;
    vec3 rgb;
    if (uEmissive) {
        rgb = base.rgb;
    } else {
        float diffuse = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
        rgb = base.rgb * (uAmbient + (1.0 - uAmbient) * diffuse);
    }
    if (uFog) {
        float f = clamp((vViewDist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        rgb = mix(rgb, uFogColor, uEmissive ? 0.5 * f : f);
    }
    FragColor = vec4(rgb, base.a);
})";
```

```cpp
// One triangle that covers the viewport; vT runs 0 (bottom) to 1 (top).
const char* kSkyVs = R"(#version 330 core
out float vT;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    vT = (p.y + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";

const char* kSkyFs = R"(#version 330 core
in float vT;
uniform vec3 uTop; uniform vec3 uBottom;
out vec4 FragColor;
void main() { FragColor = vec4(mix(uBottom, uTop, clamp(vT, 0.0, 1.0)), 1.0); })";
```

Delete the line `const glm::vec3 kLightDir {-0.3f, -1.f, 0.4f};`. The light direction now comes from the theme.

- [ ] **Step 4: Build the sky shader and widen the far plane**

In `Context::create`, after the `flat` shader is built:

```cpp
    auto sky = GlShader::build(kSkyVs, kSkyFs);
    if (sky.isError())
        return Bowlingbox::Core::Error<>("sky shader: " + sky.error());
```

Then, next to `ctx->_d->flat = flat();`, add:

```cpp
    ctx->_d->sky = sky();
```

In `Camera::projection`, change the far plane from `100.f` to `150.f`:

```cpp
    return glm::perspective(glm::radians(45.f), _aspect, 0.1f, 150.f);
```

- [ ] **Step 5: Replace `SceneRenderer`**

In `Render.h`, replace the `SceneRenderer` class with:

```cpp
/// Background gradient, theme props, lane, gutters, ball, pins, aim guide.
class BOWLINGBOX_RENDER_API SceneRenderer {
public:
    SceneRenderer(Context& ctx, const SceneGeometry& geometry, ThemeId theme = ThemeId::NeonHall);
    ~SceneRenderer();
    /// Switches decor and colours. Rebuilds the prop list on the CPU; makes no GL calls.
    void setTheme(ThemeId theme);
    void draw(const Camera& camera, const SceneDescription& scene, int viewportY, int viewportH);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};
```

In `Render.cpp`, replace everything from `// ---- SceneRenderer` up to (not including) `// ---- Overlay` with:

```cpp
// ---------------------------------------------------------------- SceneRenderer
struct SceneRenderer::Impl {
    Context& ctx;
    SceneGeometry g;
    const Theme* current = nullptr;
    std::vector<Prop> props;
    GlMesh lane, gutter, wall, ball, pin, guide, unitBox, unitCylinder, unitCone;
    unsigned skyVao = 0;  // core profile needs a bound VAO even for attribute-less draws

    Impl(Context& c, const SceneGeometry& geo, ThemeId id)
        : ctx(c), g(geo),
          lane(boxData(geo.laneHalfWidth, 0.05f, (geo.laneLength + geo.approach) / 2)),
          gutter(boxData(geo.gutterWidth / 2, 0.05f, geo.laneLength / 2)),
          wall(boxData(0.02f, 0.15f, geo.laneLength / 2)),
          ball(sphereData(geo.ballRadius, 32)),
          pin(shapeMesh(Shape::Pin, geo)),
          guide(boxData(0.01f, 0.003f, 0.5f)),
          unitBox(shapeMesh(Shape::Box, geo)),
          unitCylinder(shapeMesh(Shape::Cylinder, geo)),
          unitCone(shapeMesh(Shape::Cone, geo)) {
        glGenVertexArrays(1, &skyVao);
        setTheme(id);
    }
    ~Impl() { glDeleteVertexArrays(1, &skyVao); }

    void setTheme(ThemeId id) {
        current = &theme(id);
        props = buildProps(*current, g);
    }

    const GlMesh& meshFor(Shape shape) const {
        switch (shape) {
            case Shape::Box:      return unitBox;
            case Shape::Cylinder: return unitCylinder;
            case Shape::Cone:     return unitCone;
            case Shape::Pin:      break;
        }
        return pin;
    }

    void drawMesh(const GlMesh& mesh, const glm::mat4& model, glm::vec4 color, bool emissive = false,
                  bool stripes = false, glm::vec2 band = kNoBand) {
        GlShader& s = *ctx.data().lit;
        s.setMat4("uModel", model);
        s.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        s.setVec4("uColor", color);
        s.setVec2("uBandRange", band);
        s.setBool("uEmissive", emissive);
        s.setBool("uStripes", stripes);
        mesh.draw();
    }
    void drawMaterial(const GlMesh& mesh, const glm::mat4& model, const Material& m, bool stripes = false,
                      glm::vec2 band = kNoBand) {
        drawMesh(mesh, model, glm::vec4(m.color, 1.f), m.emissive, stripes, band);
    }
    static glm::mat4 at(float x, float y, float z) {
        return glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
    }
};

SceneRenderer::SceneRenderer(Context& ctx, const SceneGeometry& geometry, ThemeId theme)
    : _p(std::make_unique<Impl>(ctx, geometry, theme)) {}
SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::setTheme(ThemeId theme) {
    _p->setTheme(theme);
}

void SceneRenderer::draw(const Camera& camera, const SceneDescription& scene, int viewportY,
                         int viewportH) {
    Impl& p = *_p;
    const SceneGeometry& g = p.g;
    const Theme& t = *p.current;
    glViewport(0, viewportY, p.ctx.width(), viewportH);
    glDisable(GL_BLEND);

    // Background gradient behind everything, in the 3D band only.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    GlShader& sky = *p.ctx.data().sky;
    sky.use();
    sky.setVec3("uTop", t.backgroundTop);
    sky.setVec3("uBottom", t.backgroundBottom);
    glBindVertexArray(p.skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    GlShader& s = *p.ctx.data().lit;
    s.use();
    s.setMat4("uView", camera.view());
    s.setMat4("uProj", camera.projection());
    s.setVec3("uLightDir", t.lightDir);
    s.setFloat("uAmbient", t.ambient);
    s.setBool("uFog", true);
    s.setVec3("uFogColor", t.fogColor);
    s.setFloat("uFogStart", t.fogStart);
    s.setFloat("uFogEnd", t.fogEnd);
    s.setVec4("uBandColor", glm::vec4(t.pinBand.color, 1.f));

    const glm::vec2 neck {0.55f * g.pinHeight - g.pinComHeight, 0.65f * g.pinHeight - g.pinComHeight};
    for (const Prop& prop : p.props)
        p.drawMaterial(p.meshFor(prop.shape), prop.transform.matrix(), prop.material, prop.stripes,
                       prop.pinBand ? neck : kNoBand);

    const float outerX = g.laneHalfWidth + g.gutterWidth;
    p.drawMaterial(p.lane, Impl::at(0.f, -0.05f, (g.laneLength - g.approach) / 2), t.lane, true);
    for (float sx : {-1.f, 1.f}) {
        p.drawMaterial(p.gutter, Impl::at(sx * (g.laneHalfWidth + g.gutterWidth / 2), -g.gutterDepth - 0.05f, g.laneLength / 2),
                       t.gutter);
        p.drawMaterial(p.wall, Impl::at(sx * (outerX + 0.02f), 0.05f, g.laneLength / 2), t.gutter);
    }
    for (int i = 0; i < 10; ++i)
        if (scene.pinVisible[i])
            p.drawMaterial(p.pin, scene.pins[i].matrix(), t.pinBody, false, neck);
    p.drawMaterial(p.ball, scene.ball.matrix(), t.ball);

    if (scene.showAimGuide) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        s.setBool("uFog", false);  // the guide keeps its colour and 0.40 alpha
        const float length = g.laneLength + 1.f;  // from the ball start (z = -1) to the deck end
        const float a = glm::radians(scene.aimAngleDeg);
        const glm::vec3 dir(std::sin(a), 0.f, std::cos(a));
        const glm::vec3 start(scene.aimOffset, 0.004f, -1.f);
        glm::mat4 model = glm::translate(glm::mat4(1.f), start + dir * (length / 2)) *
                          glm::rotate(glm::mat4(1.f), a, glm::vec3(0.f, 1.f, 0.f)) *
                          glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, length));
        p.drawMesh(p.guide, model, {0.f, 0.898f, 1.f, 0.40f});
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}
```

- [ ] **Step 6: Report GL init failures on stderr, then build and smoke-run**

In `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp`, add `#include <cstdio>`. In `ensureRenderer`, inside `if (created.isError()) {`, add this as the first line:

```cpp
        std::fprintf(stderr, "OpenGL initialisation failed: %s\n", created.error().c_str());
```

Run the build from Global Constraints. Expected: no warnings, and every test passes.

Smoke run, which needs a display. A shader that doesn't compile makes `Context::create` fail, and the new line prints the GLSL error:

```bash
timeout 6 ./build/bin/BowlingGL 2> build/gl-smoke.err; echo "exit=$?"
cat build/gl-smoke.err
grep -c "OpenGL initialisation failed" build/gl-smoke.err
```

Expected: `exit=124` (still running when the timeout stops it) and a count of `0`. If the count is 1, fix the shader that the message names.

- [ ] **Step 7: Commit**

```bash
git add libraries/Render applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp
git commit -m "feat(render): draw theme background, props, fog, glow and lane stripes

The lit shader keeps the pin band and alpha, adds emissive, fog and
stripes; a sky shader draws the background gradient. SceneRenderer takes
a ThemeId and setTheme rebuilds its props. Far plane 150 m.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Theme choice in the app (BowlingGL wiring)

Runs in wave 3, in parallel with Task 6. Needs Tasks 3 and 4.

**Files:**
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.h`, `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingFrame.h`, `applications/bowlingGame/bowlingGameGL/BowlingFrame.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/main.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/HudText.h`, `applications/bowlingGame/bowlingGameGL/HudText.cpp`
- Test: `applications/bowlingGame/bowlingGameGL/test/test_Gameplay.cpp`

**Interfaces:**
- Consumes:
  - from Task 3: `SceneRenderer(Context&, const SceneGeometry&, ThemeId)` and `SceneRenderer::setTheme(ThemeId)`
  - from Task 1: `ThemeId`, `kThemes`, `theme()`, `themeFromName()`, `nextTheme()`
  - from Task 4: `Command::Type::CycleTheme`
- Produces:
  - `BowlingCanvas::setTheme(ThemeId)`, `BowlingCanvas::theme()` and the `BowlingCanvas::onThemeChanged` callback
  - the `BowlingFrame(ThemeId)` constructor
  - `HudText::unknownThemeMessage(std::string_view)`

- [ ] **Step 1: Write the failing test**

Append to `test/test_Gameplay.cpp`:

```cpp
TEST(test_hud_text, unknown_theme_message_lists_every_name) {
    EXPECT_EQ(HudText::unknownThemeMessage("space"), "unknown theme 'space'; use neon, classic or outdoor");
    EXPECT_EQ(HudText::unknownThemeMessage(""), "unknown theme ''; use neon, classic or outdoor");
}
```

- [ ] **Step 2: Run it to see it fail**

Run the build from Global Constraints.
Expected: compile error `'unknownThemeMessage' is not a member of 'HudText'`.

- [ ] **Step 3: Add the message**

`HudText.h`, add `#include <string_view>` and, after `controlsHelp()`:

```cpp
/// Shown on stderr for --theme=<name> with an unknown name.
std::string unknownThemeMessage(std::string_view name);
```

`HudText.cpp`, add `#include <Bowlingbox/Render/Environment.h>` and, after `controlsHelp()`:

```cpp
std::string unknownThemeMessage(std::string_view name) {
    std::string names;
    const auto& ids = Bowlingbox::Render::kThemes;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0)
            names += i + 1 == ids.size() ? " or " : ", ";
        names += Bowlingbox::Render::theme(ids[i]).name;
    }
    return "unknown theme '" + std::string(name) + "'; use " + names;
}
```

If `HudText.cpp` does not include it yet, add `#include <cstddef>`.

Run the build. Expected: `unknown_theme_message_lists_every_name` passes.

- [ ] **Step 4: Canvas: store, apply and announce the theme**

`BowlingCanvas.h`:
- Add `#include <Bowlingbox/Render/Environment.h>`.
- In `public:`, after `newGame()`:

```cpp
    /// Stores the theme; applies it now if the renderer exists, else when it is created.
    void setTheme(Bowlingbox::Render::ThemeId id);
    Bowlingbox::Render::ThemeId theme() const { return _theme; }
    std::function<void(Bowlingbox::Render::ThemeId)> onThemeChanged;  // menu sync
```

- In `private:`, next to `_failed`:

```cpp
    Bowlingbox::Render::ThemeId _theme = Bowlingbox::Render::ThemeId::NeonHall;
```

`BowlingCanvas.cpp`:
- After `newGame()`:

```cpp
void BowlingCanvas::setTheme(Render::ThemeId id) {
    _theme = id;
    if (_scene)
        _scene->setTheme(id);  // CPU only, no GL context needed
    if (onThemeChanged)
        onThemeChanged(id);
    Refresh(false);
}
```

- In `ensureRenderer`, build the scene with the stored theme:

```cpp
    _scene = std::make_unique<Render::SceneRenderer>(*_ctx, SceneAdapter::geometry(Alley::kLane), _theme);
```

- In `apply`, after the `Quit` branch:

```cpp
    if (command.type == Command::Type::CycleTheme) {
        setTheme(Render::nextTheme(_theme));
        return;
    }
```

- [ ] **Step 5: Frame: View ▸ Theme menu and the initial theme**

`BowlingFrame.h`:

```cpp
#ifndef BOWLINGGL_BOWLING_FRAME_H
#define BOWLINGGL_BOWLING_FRAME_H
#include <Bowlingbox/Render/Environment.h>
#include <wx/wx.h>

class BowlingCanvas;

class BowlingFrame : public wxFrame {
public:
    explicit BowlingFrame(Bowlingbox::Render::ThemeId initialTheme = Bowlingbox::Render::ThemeId::NeonHall);

private:
    void OnNewGame(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnControls(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);
    void OnTheme(wxCommandEvent&);

    BowlingCanvas* _canvas = nullptr;
};
#endif  // BOWLINGGL_BOWLING_FRAME_H
```

`BowlingFrame.cpp`:
- Add `#include <cstddef>` and `#include <string>`.
- Above the constructor:

```cpp
namespace {
constexpr int kThemeMenuBase = wxID_HIGHEST + 1;  // one id per entry of Render::kThemes

int themeMenuId(Bowlingbox::Render::ThemeId id) {
    const auto& ids = Bowlingbox::Render::kThemes;
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == id)
            return kThemeMenuBase + static_cast<int>(i);
    return kThemeMenuBase;
}
}  // namespace
```

- Change the constructor's first line to `BowlingFrame::BowlingFrame(Bowlingbox::Render::ThemeId initialTheme)`.
- Build the View menu between Game and Help:

```cpp
    auto* themes = new wxMenu;
    for (std::size_t i = 0; i < Bowlingbox::Render::kThemes.size(); ++i) {
        const auto label = Bowlingbox::Render::theme(Bowlingbox::Render::kThemes[i]).label;
        themes->AppendRadioItem(kThemeMenuBase + static_cast<int>(i), wxString::FromUTF8(std::string(label)));
    }
    auto* view = new wxMenu;
    view->AppendSubMenu(themes, "&Theme");
```

- Replace the menu bar lines with:

```cpp
    auto* bar = new wxMenuBar;
    bar->Append(game, "&Game");
    bar->Append(view, "&View");
    bar->Append(help, "&Help");
    SetMenuBar(bar);
```

- After the other `Bind` calls:

```cpp
    Bind(wxEVT_MENU, &BowlingFrame::OnTheme, this, kThemeMenuBase,
         kThemeMenuBase + static_cast<int>(Bowlingbox::Render::kThemes.size()) - 1);
```

- After `_canvas->onQuit = …;`:

```cpp
    _canvas->onThemeChanged = [this](Bowlingbox::Render::ThemeId id) {
        GetMenuBar()->Check(themeMenuId(id), true);
    };
    _canvas->setTheme(initialTheme);
```

- Add the handler:

```cpp
void BowlingFrame::OnTheme(wxCommandEvent& e) {
    const auto index = static_cast<std::size_t>(e.GetId() - kThemeMenuBase);
    _canvas->setTheme(Bowlingbox::Render::kThemes[index]);
}
```

- [ ] **Step 6: App: `--theme` option**

`main.cpp`:

```cpp
#include "BowlingFrame.h"
#include "HudText.h"
#include <Bowlingbox/Render/Environment.h>
#include <wx/cmdline.h>
#include <wx/wx.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

class BowlingApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit())  // parses the command line
            return false;
        auto* frame = new BowlingFrame(_theme);
        frame->Show(true);
        return true;
    }

    void OnInitCmdLine(wxCmdLineParser& parser) override {
        wxApp::OnInitCmdLine(parser);
        parser.AddLongOption("theme", "scenery: neon, classic or outdoor", wxCMD_LINE_VAL_STRING);
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override {
        if (!wxApp::OnCmdLineParsed(parser))
            return false;
        wxString value;
        if (parser.Found("theme", &value)) {
            const std::string name(value.ToUTF8().data());
            if (const auto id = Bowlingbox::Render::themeFromName(name))
                _theme = *id;
            else
                std::fprintf(stderr, "%s\n", HudText::unknownThemeMessage(name).c_str());
        }
        return true;
    }

private:
    Bowlingbox::Render::ThemeId _theme = Bowlingbox::Render::ThemeId::NeonHall;
};

wxIMPLEMENT_APP_NO_MAIN(BowlingApp);
```

Keep the existing `int main(…)` with the `GDK_BACKEND` block unchanged.

Run the build from Global Constraints. Expected: no warnings, and every test passes.

Smoke runs, which need a display:

```bash
for t in neon classic outdoor Classic; do
  timeout 6 ./build/bin/BowlingGL --theme=$t 2> build/theme-$t.err; echo "$t exit=$?"; cat build/theme-$t.err
done
timeout 6 ./build/bin/BowlingGL --theme=space 2> build/theme-space.err; cat build/theme-space.err
```

Expected:
- Each of `neon`, `classic`, `outdoor` and `Classic` prints `exit=124` and leaves stderr empty.
- `space` prints `unknown theme 'space'; use neon, classic or outdoor` and keeps running.

- [ ] **Step 7: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL
git commit -m "feat(gl): choose the theme with --theme, T or View > Theme

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Documentation

Runs in wave 3, in parallel with Task 5.

**Files:**
- Modify: `applications/bowlingGame/README.md`
- Modify: `docs/MANUAL.md`
- Modify: `docs/ARCHITECTURE.md`

**Interfaces:**
- Consumes: the names, keys and options from Tasks 1–5, as listed in this plan.
- Produces: docs only.

- [ ] **Step 1: App README**

In `applications/bowlingGame/README.md`, add this row to the BowlingGL controls table, before the `Quit` row:

```markdown
| Change scenery | T | View ▸ Theme |
```

After the run commands block (the one ending in `./build/bin/BowlingGL`), add:

````markdown
### Scenery

Three themes surround the lane. Pick one at start or switch while playing (`T` or View ▸ Theme):

| Theme | `--theme=` | Look |
|---|---|---|
| Neon Hall (default) | `neon` | Dark-blue hall, cyan and purple neon, glowing pin screens, two lanes on each side |
| Classic Alley | `classic` | Warm, bright hall with ceiling lights and a score monitor above the pins |
| Outdoor | `outdoor` | Terrace under the sky with fences and trees, one lane on each side |

```bash
./build/bin/BowlingGL --theme=classic
```

An unknown name prints the valid ones and starts Neon Hall. The decor is drawn only; it never
touches the physics.
````

Append to the manual checklist:

```markdown
7. `--theme=neon`, `--theme=classic`, `--theme=outdoor`: each starts in that scenery; `--theme=space` warns and starts Neon Hall.
8. Press `T` three times: Neon → Classic → Outdoor → Neon, and View ▸ Theme follows; choosing from the menu works too.
9. In every theme: a gutter ball at full offset and flying pins stay visible; the aim guide is translucent and the pin necks are red; the frame rate stays smooth.
```

In the BowlingGL paragraph of `## Architecture`, append:

```markdown
`Render::buildProps()` lays out each theme's decor as plain data (`Environment.h`), so the
corridor and camera clearances are tested without GL; `SceneRenderer` draws it with fog and glow.
```

- [ ] **Step 2: Manual**

In `docs/MANUAL.md`, section `### Controls` under `## 3. Playing BowlingGL`, add this row before `Show controls`:

```markdown
| Change scenery | T | View ▸ Theme |
```

After the `### Tips` list, add:

````markdown
### Scenery

BowlingGL draws one of three themes around the lane: Neon Hall (default), Classic Alley and
Outdoor. Press `T` or use View ▸ Theme to switch. Start in a theme with:

```bash
./build/bin/BowlingGL --theme=outdoor     # neon, classic or outdoor; case does not matter
```

An unknown name prints `unknown theme '<name>'; use neon, classic or outdoor` and starts Neon Hall.
````

- [ ] **Step 3: Architecture**

In `docs/ARCHITECTURE.md`, `### Bowlingbox::Render` table:
- In the `Render.h` row, change the end from `plain data SceneGeometry / SceneDescription / Transform` to `plain data SceneDescription`. Also change `SceneRenderer (lane, gutters, ball, pins, aim guide)` to `SceneRenderer (background, theme props, lane, gutters, ball, pins, aim guide)`.
- Add a row after `Render.h`:

```markdown
| `Environment.h` | Themes (`ThemeId`, `Theme`, `theme()`, `themeFromName()`, `nextTheme()`), `SceneGeometry`, `Transform`, and the GL-free decor layout: `buildProps()`, `shapeMesh()`, `localBounds()`, `bounds()` |
```

After the paragraph that starts `Render does not include Alley`, add:

```markdown
The lit shader adds per-theme ambient light, distance fog, emissive (glowing) materials and lane
board stripes on top of the pin colour band; a separate sky shader draws the background gradient.
Theme props are unit boxes, cylinders and cones scaled by their `Transform`, so one mesh per shape
serves every prop, and `bounds()` uses the same extents as the meshes.
```

In the tests table, replace the `test_bowlingbox_render` row with:

```markdown
| `test_bowlingbox_render` | Mesh generators, scoreboard layout, `paintScoreboard` on a fake canvas, theme data, shape bounds, and decor that never enters the play corridor, sight line or camera path |
```

- [ ] **Step 4: Check the links and commit**

Run `grep -n "Scenery\|--theme\|Environment.h" applications/bowlingGame/README.md docs/MANUAL.md docs/ARCHITECTURE.md`.
Expected: matches in all three files.

```bash
git add applications/bowlingGame/README.md docs/MANUAL.md docs/ARCHITECTURE.md
git commit -m "docs: environment themes, T key and --theme

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Completeness review

Runs after Tasks 1–6 are merged into `feature/environment`, as a fresh reviewer that reads the spec and this plan. It changes no code. It reports gaps and points each one at the task that owns it.

- [ ] **Step 1: Fresh build and full test run**

In a new directory `build/env-review`, using the dependency options from Global Constraints:

```bash
D=/home/amin/Dokumente/dev/Bowlingbox/build/_deps
cmake -S . -B build/env-review -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_WX_UI=ON -DGENERATE_SCAFFOLD=OFF \
      -DFETCHCONTENT_SOURCE_DIR_BULLET3=$D/bullet3-src -DFETCHCONTENT_SOURCE_DIR_GLM=$D/glm-src \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$D/googletest-src
cmake --build build/env-review -j"$(nproc)" 2>&1 | tee build/env-review.log | grep -c -E "warning:|error:"
ctest --test-dir build/env-review --label-regex unit --output-on-failure | tail -3
cmake -S . -B build/env-review-nowx -DBUILD_WX_UI=OFF -DGENERATE_SCAFFOLD=OFF \
      -DFETCHCONTENT_SOURCE_DIR_BULLET3=$D/bullet3-src -DFETCHCONTENT_SOURCE_DIR_GLM=$D/glm-src \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$D/googletest-src && cmake --build build/env-review-nowx -j"$(nproc)"
```

Expected:
- the warning/error count is `0`
- `100% tests passed`
- the `BUILD_WX_UI=OFF` build succeeds, which shows that Render still builds without wx

- [ ] **Step 2: Spec-to-code checklist**

Open the spec and check each line against the merged code. Mark it ✅, or ❌ with the file and the missing piece:

| Spec item | Where to look |
|---|---|
| Three presets with the exact table values (backgrounds, fog, light, ambient, materials, indoor, neighbour lanes, props) | `src/Environment.cpp` theme constants |
| `Transform`/`SceneGeometry` in `Environment.h`; `Render.h` includes it; no cycle | both headers |
| `SceneGeometry` has `pinSpacing`, `headPinZ`; `SceneAdapter` copies them | `Environment.h`, `SceneAdapter.cpp` |
| Shape table: meshes and `localBounds` as specified | `shapeMesh`, `localBounds` |
| Neighbour lanes: lane with approach, two gutters, rack at `pinComHeight`, `pinBand`, no low walls | `addNeighbourLane` |
| Ball returns 0.14 × 0.3, gutter material, in every gap | `addBallReturns` |
| Indoor: W formula, wall/ceiling faces and 0.2 m thickness, floor top −0.15 with pit hole, pit box top −0.25, masking unit z laneLength+0.05…+0.35, y 0.9…2.4 | `addHall`, `addGroundAndPit` |
| Pillars z −6…19 step 5; neon strips y 1.0 / 3.4 + masking strip; ceiling lights x ±pitch/2, z −6…18 step 4; wall screens z 6/12, frame/panel/icon; score monitor | `EnvironmentDecor.cpp` |
| Outdoor: ground ±40, z −20…80, pit; fences F formula, posts every 2 m z −6…30, rails 0.4/0.8; 16 trees from the table | `EnvironmentDecor.cpp`, `addGroundAndPit` |
| Shader: band kept, alpha passed, emissive, ambient, fog (half for emissive), stripes, today's light convention | `Render.cpp` `kLitFs` |
| Sky gradient shader in `ContextData`, drawn first with depth off | `Render.cpp`, `ContextData.h` |
| Draw order background → props → lane → gutters/walls (gutter material) → pins → ball → guide (unfogged) | `SceneRenderer::draw` |
| Far plane 150 m | `Camera::projection` |
| `CycleTheme`: `T`/`t` in every phase; `LaneCommands` no-op; canvas `setTheme(nextTheme(theme()))` | `InputMapper.cpp`, `LaneCommands.cpp`, `BowlingCanvas.cpp` |
| Canvas stores the theme before GL exists; `ensureRenderer` uses it | `BowlingCanvas.cpp` |
| `BowlingFrame(ThemeId)`, View ▸ Theme radio items from `label`, `onThemeChanged` checks the item, then `setTheme(initial)` | `BowlingFrame.cpp` |
| `--theme` through `OnInitCmdLine`/`OnCmdLineParsed`, `themeFromName`, message from `kThemes`, default Neon | `main.cpp`, `HudText.cpp` |
| `controlsHelp()` has `T: change the scenery`; README controls row | `HudText.cpp`, README |
| Every test listed in the spec's Tests section exists | `test_Environment.cpp`, `test_InputMapper.cpp`, `test_Gameplay.cpp` |
| Docs: README themes section, checklist, MANUAL, ARCHITECTURE | the three docs |
| Out of scope respected: no change under `libraries/Alley`, `libraries/Score` | `git diff --stat origin/main...HEAD -- libraries/Alley libraries/Score` is empty |

- [ ] **Step 3: Smoke runs**

Do Task 5, Step 6's smoke runs against `build/env-review`.
Expected:
- Each of `neon`, `classic`, `outdoor` and `Classic` exits with `124` and leaves stderr empty.
- `space` prints the unknown-theme message.

- [ ] **Step 4: Report**

Report back with:
- the checklist table, filled in
- the warning count
- the test summary
- the smoke-run results
- every ❌, with the task it belongs to

Do not fix anything in this task.

**After the review (by hand, for the user):**
- Run the README checklist items 7–9 on screen.
- Take one screenshot per theme and add them to the README's Scenery section.
