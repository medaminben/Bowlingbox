# Modern Graphics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BowlingGL stops looking like an 80s console game. The ball and pins get gloss, shadows and an oiled lane that reflects, neon glows, and the HUD uses a real font, all at 60 fps on integrated GPUs.

**Architecture:**
- Forward rendering with post-processing, all inside the Render library.
- `SceneRenderer::draw` runs a shadow pass, a lane-reflection pass and an HDR main pass (RGBA16F, 4× MSAA), then bloom and ACES tone mapping into the window.
- A *basic path* (the same new lighting, tone-mapped in the lit shader, with no offscreen targets) runs when the GPU can't do the full pipeline, or when `BOWLINGBOX_RENDER=basic`.
- The CPU-side pieces are pure, exported functions with unit tests: colour space, light frustum fitting, caster selection, frame sizes, render options and the font atlas.

**Tech Stack:** C++20, CMake ≥ 3.29, OpenGL 3.3 core (GLAD), GLM, stb_truetype 1.26, Rubik Medium (OFL), wxWidgets 3.2, GoogleTest.

**Spec:** `docs/superpowers/specs/2026-10-17-modern-graphics-design.md`

## Global Constraints

- **Hardware floor:** OpenGL 3.3 core on integrated GPUs, 60 fps. Use only GL 3.3 core features: `GL_DEPTH_CLAMP`, `gl_ClipDistance`, `sampler2DShadow`, multisample textures (`glTexImage2DMultisample`, `sampler2DMS`) and `GL_TIME_ELAPSED` are all core. No extensions.
- **Warnings:** every target builds with `-Wall -Wextra -Wpedantic -Werror` (`bowlingbox_set_warnings`).
  - An aggregate initialiser that leaves out a field fails `-Wmissing-field-initializers`. Name every field, or use the helper functions shown.
- **Public API:** `Context`, `Camera`, `SceneRenderer`, `Overlay` and `ICanvas2D` keep their signatures. Additions are allowed (`Camera::eye()`).
- **Visibility:** Render is built with `CXX_VISIBILITY_PRESET hidden`. Anything a test calls must be declared in `include/Bowlingbox/Render/*.h` and marked `BOWLINGBOX_RENDER_API`.
- **No new runtime files:** shaders are embedded strings. The font is embedded as a byte array. Nothing new is installed.
- **Dependencies:** Render must not include Alley. Physics, `Alley`, `Score`, the camera rig and the HUD layout (`Hud.cpp`, `ScoreboardPainter.cpp`) don't change.
- **GL-free theme switching:** `SceneRenderer::setTheme` makes no GL calls (it may be called before a context exists).
- **Fallback:** the game never refuses to start over a missing effect. Only a failing *basic* shader makes `Context::create()` return an error.
- **Tests:** names follow `TEST(test_<area>, what_it_checks)`. Tests live in `libraries/Render/test/src/` and are listed in `TST_SOURCES` in `libraries/Render/CMakeLists.txt`.
- **Build folder:** everything is built under `<repo>/build/`, never `/tmp`. The `build/` folder is already configured (Ninja, Debug, `BUILD_WX_UI=ON`).
- **Build and test:**

```bash
cd /home/amin/Dokumente/dev/Bowlingbox
cmake --build build -j"$(nproc)"
ctest --test-dir build --label-regex unit --output-on-failure
```

- **Smoke run (GL tasks):** this starts each theme, takes a screenshot and reports every `render:` line printed to stderr. Debug builds print `render: GL error 0x....` from Task 5 on. GTK and wxWidgets may print their own warnings, so only `render:` lines count.

```bash
mkdir -p build/shots
for t in neon classic outdoor; do
  build/bin/BowlingGL --theme=$t 2> build/shots/$t.err & pid=$!
  sleep 4; import -window root build/shots/$TASK-$t.png; kill $pid; wait $pid 2>/dev/null
  if grep -q '^render:' build/shots/$t.err; then echo "== $t"; grep '^render:' build/shots/$t.err; fi
done
```

"No stderr output" in the tasks below means no `render:` line.

(Set `TASK=task5` etc. first. Look at the PNGs: `build/shots/` is in the build tree, so it's never committed.)

## Decisions taken while planning (all recorded in the spec)

- **Licence files** go in `licenses/` at the repo root, where the existing ones are, not `3rdparty/licenses/`.
- **Testable functions are public.** Because of hidden visibility, `Lighting.h`, `RenderOptions.h` and `FontAtlas.h` are public exported headers. `Lighting.h` holds the sRGB functions; there's no separate `ColorSpace.h`.
- **Shadow map fit.** The shadow frustum is fitted to a fixed box (`shadowBox`), so it never depends on the camera and needs no texel snapping. The light view uses the lane direction (+z) as its up vector, so the lane lies along the map's long axis. The map is 1024×4096: the same memory as 2048², with about 5–6 mm per texel along the lane instead of about 1 cm.
- **Shadow box height.** The shadow box reaches up to `kShadowTopY = 2.5 m`, so the masking unit over the pinsetters (y 0.9–2.4) casts a shadow. Ceilings (4.5 m) and ceiling lights fall outside it and never cast. Without that cut, the whole lane would sit in the ceiling's shadow.
- **Pin profile.** It has 45 points (4 samples per Catmull-Rom span over the 11 spans). The worst step between neighbouring normals is about 19°, under the test's 25° bound.
- **HUD text** uses a coverage atlas with 4 px glyph padding and mipmaps (levels 0–2) with trilinear filtering. The `smoothstep` edge trick belongs to distance-field fonts and isn't used.
- **Material colours** are converted sRGB → linear in each draw call. It's three `pow`s per draw, about 150 draws, so it's negligible.
- **Aim guide.** It's now an emissive cyan with alpha 0.40, so it reads the same under every theme's lighting.
- **Lane boards.** There are 39 across, as on a real lane (≈ 26.9 mm each), counted from the lane edge, so the arrows on boards 5…35 sit symmetrically about board 20, the middle board.
- **Glow.** Neon Hall's emissives are strength 4–5. Classic Alley's ceiling panels (`accentA`) are 1.5, so they get a faint glow. Outdoor has nothing above 1.
- **Highlights.** The MSAA resolve weights each sample by `1 / (1 + max(c))`, and the bright pass clamps to 16, so the ball's sharp highlight neither aliases nor makes the bloom flicker.
- **Reflection** is sampled only by the main lane. The texture is premultiplied (cleared to transparent black) and blended as `rgb × (1 − k·a) + k·refl.rgb`, so edges don't darken and areas with nothing reflected are untouched. Neighbour lanes don't reflect.
- **Lane arrows and dots** are painted on the main lane only (|x| < laneHalfWidth).
- **Minimised window.** When the 3D band is under 1 px tall, `SceneRenderer::draw` returns early. `frameSizes` still clamps every size to ≥ 1.
- **Uniforms.** `GlShader` caches uniform locations (Task 5), since the extra passes roughly triple the uniform calls per frame.
- **GPU timing** keeps three frames of queries in flight and reads a result only when it's available, so stats mode never stalls.

## Review Focus

1. **Window minimised or resized until the 3D band is 0 px tall.** There should be no 0-size framebuffer, no GL error, and normal drawing after restoring. Pinned by Task 7 (`frame_sizes_never_reach_zero`, the early return in `draw`, smoke Step 8 resize).
2. **`BOWLINGBOX_RENDER=Basic`, `=full`, `=` (empty) or unset; `BOWLINGBOX_RENDER_STATS=0`.** Only `basic` (any case) forces the basic path. Only `1` turns stats on. Pinned by Task 7 (`render_options_read_the_environment_strictly`).
3. **HUD text with characters the font doesn't have** (é in UTF-8, control characters). They draw and measure as `?`, with no crash or garbage. Pinned by Task 4 (`characters_outside_ascii_draw_as_question_marks`).
4. **Pressing T mid-roll with the full pipeline running.** The casters and light frustum are rebuilt for the new theme in `setTheme` (CPU only), so every theme has sensible casters. Pinned by Task 9 (`shadow_casters_skip_the_ceiling_and_keep_the_ball_returns`) and Task 10 (`reflection_casters_skip_the_floor_and_the_neighbour_lanes`). Both loop over every theme.
5. **A GPU that can't render RGBA16F or 4× MSAA.** It falls back to the basic path and the game runs. This can't be unit-tested. Pinned by Task 7 (samples clamped to `GL_MAX_COLOR_TEXTURE_SAMPLES` and `GL_MAX_DEPTH_TEXTURE_SAMPLES`, the probe framebuffer, and smoke Step 8 with `BOWLINGBOX_RENDER=basic`).

## Execution order

Run the tasks in order, 1 → 11. Tasks 1, 3 and 4 are independent of each other. Every task from 5 on edits `Render.cpp`, so those tasks run strictly in sequence.

---

### Task 1: Colour space and light frustum maths

**Files:**
- Create: `libraries/Render/include/Bowlingbox/Render/Lighting.h`
- Create: `libraries/Render/src/Lighting.cpp`
- Create: `libraries/Render/test/src/test_Lighting.cpp`
- Modify: `libraries/Render/CMakeLists.txt` (header, source, test lists)

**Interfaces:**
- Consumes: `Aabb`, `SceneGeometry`, `kPitTopY`, `kPitLength`, `ThemeId`, `kThemes`, `theme()` from `Environment.h`.
- Produces:
  - `float srgbToLinear(float)`, `float linearToSrgb(float)`, and `glm::vec3` overloads of both.
  - `struct LightFrustum { glm::mat4 view, projection; glm::mat4 viewProjection() const; }`.
  - `LightFrustum fitLightFrustum(glm::vec3 lightDir, const Aabb& box)`.
  - `Aabb shadowBox(const SceneGeometry&)`.
  - `inline constexpr float kShadowTopY = 2.5f`.

- [ ] **Step 1: Write the failing tests**

Create `libraries/Render/test/src/test_Lighting.cpp`:

```cpp
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
```

Add it to `TST_SOURCES` in `libraries/Render/CMakeLists.txt`:

```cmake
set(TST_SOURCES
    test/src/test_Render.cpp
    test/src/test_Environment.cpp
    test/src/test_Lighting.cpp
)
```

- [ ] **Step 2: Run the build to verify it fails**

Run: `cmake --build build -j"$(nproc)" 2>&1 | grep -m3 error`
Expected: FAIL with `Bowlingbox/Render/Lighting.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `libraries/Render/include/Bowlingbox/Render/Lighting.h`:

```cpp
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
```

- [ ] **Step 4: Write the implementation**

Create `libraries/Render/src/Lighting.cpp`:

```cpp
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

}  // namespace Render
}  // namespace Bowlingbox
```

Register both in `libraries/Render/CMakeLists.txt`. Add `include/Bowlingbox/Render/Lighting.h` to `TARGET_HEADERS` (after `Environment.h`) and `src/Lighting.cpp` to `TARGET_SOURCES` (after `src/EnvironmentDecor.cpp`).

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build -R test_lighting --output-on-failure`
Expected: PASS, 7 tests.

- [ ] **Step 6: Commit**

```bash
git add libraries/Render/include/Bowlingbox/Render/Lighting.h libraries/Render/src/Lighting.cpp \
        libraries/Render/test/src/test_Lighting.cpp libraries/Render/CMakeLists.txt
git commit -m "feat(render): sRGB conversion and light frustum fitting"
```

---

### Task 2: Materials, lights and theme data

**Files:**
- Modify: `libraries/Render/include/Bowlingbox/Render/Environment.h` (`Material`, `Pattern`, `PointLight`, `Theme`, `Prop`)
- Modify: `libraries/Render/src/Environment.cpp:16-120` (helpers, three theme tables, `addNeighbourLane`)
- Modify: `libraries/Render/src/EnvironmentDecor.cpp:17` (`kIconWhite`)
- Modify: `libraries/Render/src/Render.cpp` (two call sites; a transitional ambient value)
- Test: `libraries/Render/test/src/test_Environment.cpp`

**Interfaces:**
- Consumes: `linearToSrgb(float)` from Task 1.
- Produces:
  - `enum class Pattern { None, Wood, BallSwirl }`, with values 0, 1 and 2. The shader relies on these numbers.
  - `Material { color, emissive, roughness, specular, emissiveStrength, pattern }`.
  - `PointLight { position, color, intensity, range }` and `kMaxPointLights = 4`.
  - New `Theme` fields `keyColor, keyIntensity, skyAmbient, groundAmbient, pointLights, pointLightCount, exposure, bloomStrength`. `Theme::ambient` and `Prop::stripes` are removed.

- [ ] **Step 1: Update the tests**

In `libraries/Render/test/src/test_Environment.cpp`, replace `TEST(test_environment, fog_and_ambient_are_in_range)` with:

```cpp
TEST(test_environment, fog_and_lighting_are_in_range) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        EXPECT_LT(t.fogStart, t.fogEnd) << t.name;
        EXPECT_GE(t.neighbourLanesPerSide, 0) << t.name;
        EXPECT_LE(t.neighbourLanesPerSide, 3) << t.name;
        EXPECT_GT(t.keyIntensity, 0.f) << t.name;
        for (int k = 0; k < 3; ++k) {
            EXPECT_GE(t.skyAmbient[k], 0.f) << t.name;
            EXPECT_LE(t.skyAmbient[k], 1.f) << t.name;
            EXPECT_GE(t.groundAmbient[k], 0.f) << t.name;
            EXPECT_LE(t.groundAmbient[k], 1.f) << t.name;
        }
        EXPECT_GE(t.pointLightCount, 0) << t.name;
        EXPECT_LE(t.pointLightCount, kMaxPointLights) << t.name;
        for (int i = 0; i < t.pointLightCount; ++i) {
            const PointLight& l = t.pointLights[static_cast<std::size_t>(i)];
            EXPECT_GT(l.intensity, 0.f) << t.name << " light " << i;
            EXPECT_GT(l.range, 0.f) << t.name << " light " << i;
        }
        EXPECT_GT(t.exposure, 0.f) << t.name;
        EXPECT_GE(t.bloomStrength, 0.f) << t.name;
        EXPECT_LE(t.bloomStrength, 1.f) << t.name;
    }
}
```

Replace `TEST(test_environment, neighbour_lanes_have_board_stripes)` with:

```cpp
TEST(test_environment, neighbour_lanes_are_wood) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        const auto props = buildProps(t, kG);
        const auto wooden = std::count_if(props.begin(), props.end(),
                                          [](const Prop& p) { return p.material.pattern == Pattern::Wood; });
        EXPECT_EQ(wooden, 2 * t.neighbourLanesPerSide) << t.name;
    }
}
```

Append at the end of the file:

```cpp
namespace {
std::vector<Material> allMaterials(const Theme& t) {
    std::vector<Material> out {t.lane, t.gutter, t.ball, t.pinBody, t.pinBand, t.floor, t.wall,
                               t.ceiling, t.trim, t.accentA, t.accentB, t.screen};
    for (const Prop& p : buildProps(t, kG))
        out.push_back(p.material);
    return out;
}
}  // namespace

TEST(test_environment, every_material_has_roughness_and_specular_in_range) {
    for (ThemeId id : kThemes)
        for (const Material& m : allMaterials(theme(id))) {
            EXPECT_GE(m.roughness, 0.f) << theme(id).name;
            EXPECT_LE(m.roughness, 1.f) << theme(id).name;
            EXPECT_GE(m.specular, 0.f) << theme(id).name;
            EXPECT_LE(m.specular, 1.f) << theme(id).name;
            EXPECT_GE(m.emissiveStrength, 1.f) << theme(id).name;
        }
}

namespace {
/// The strongest glow in a theme (1 if nothing glows harder than its plain colour).
float hottestGlow(const Theme& t) {
    float hottest = 1.f;
    for (const Material& m : allMaterials(t))
        if (m.emissive)
            hottest = std::max(hottest, m.emissiveStrength);
    return hottest;
}
}  // namespace

TEST(test_environment, neon_hall_glows_hottest_and_outdoor_not_at_all) {
    const float neon = hottestGlow(theme(ThemeId::NeonHall));
    const float classic = hottestGlow(theme(ThemeId::ClassicAlley));
    EXPECT_GT(neon, classic);
    EXPECT_GT(classic, 1.f) << "the bright pass keeps only what exceeds 1: Classic's ceiling panels glow faintly";
    EXPECT_FLOAT_EQ(hottestGlow(theme(ThemeId::Outdoor)), 1.f);
}

TEST(test_environment, the_lane_is_wood_and_the_ball_swirls) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        EXPECT_EQ(t.lane.pattern, Pattern::Wood) << t.name;
        EXPECT_EQ(t.ball.pattern, Pattern::BallSwirl) << t.name;
        EXPECT_LT(t.ball.roughness, t.floor.roughness) << t.name;
    }
}
```

- [ ] **Step 2: Run the build to verify it fails**

Run: `cmake --build build -j"$(nproc)" 2>&1 | grep -m3 error`
Expected: FAIL with `'const struct Bowlingbox::Render::Theme' has no member named 'keyIntensity'`.

- [ ] **Step 3: Change the types in `Environment.h`**

Replace the `Material` struct with:

```cpp
/// Surface pattern drawn by the lit shader. The values are the shader's uPattern.
enum class Pattern { None = 0, Wood = 1, BallSwirl = 2 };

struct Material {
    glm::vec3 color {1.f};          // sRGB as authored; the renderer converts to linear
    bool emissive = false;          // glows: ignores the lights, fogged at half strength
    float roughness = 0.6f;         // 0 = sharp highlight, 1 = chalky
    float specular = 0.04f;         // reflectance at normal incidence
    float emissiveStrength = 1.f;   // > 1 pushes the glow over the bloom threshold
    Pattern pattern = Pattern::None;
};

/// A light without shadows; falls off with distance and stops at `range`.
struct PointLight {
    glm::vec3 position {0.f};
    glm::vec3 color {1.f};  // linear
    float intensity = 0.f;
    float range = 1.f;      // metres
};
inline constexpr int kMaxPointLights = 4;
```

In `struct Theme`, replace the line `float ambient;           // 0..1` with:

```cpp
    glm::vec3 keyColor;      // linear; the key light casts the shadows
    float keyIntensity;
    glm::vec3 skyAmbient, groundAmbient;  // linear; hemisphere ambient from above / below
    std::array<PointLight, kMaxPointLights> pointLights;
    int pointLightCount;     // 0 .. kMaxPointLights
    float exposure;          // > 0
    float bloomStrength;     // 0 .. 1
```

In `struct Prop`, delete the line `bool stripes = false;  // lane boards (neighbour lane boxes only)`.

- [ ] **Step 4: Change the theme tables in `Environment.cpp`**

Replace the helpers and shared materials (lines 16–26) with:

```cpp
Material lit(float r, float g, float b) {
    return {{r, g, b}, false, 0.8f, 0.04f, 1.f, Pattern::None};
}
Material glow(float r, float g, float b, float strength = 1.f) {
    return {{r, g, b}, true, 1.f, 0.f, strength, Pattern::None};
}
Material wood(float r, float g, float b) {
    return {{r, g, b}, false, 0.25f, 0.04f, 1.f, Pattern::Wood};
}
Material metal(float r, float g, float b) {
    return {{r, g, b}, false, 0.40f, 0.50f, 1.f, Pattern::None};
}

const Material kBall {{0.098f, 0.365f, 0.925f}, false, 0.15f, 0.06f, 1.f, Pattern::BallSwirl};
const Material kPinBody {{0.941f, 0.941f, 0.941f}, false, 0.30f, 0.05f, 1.f, Pattern::None};
const Material kPinBand {{0.902f, 0.f, 0.f}, false, 0.30f, 0.05f, 1.f, Pattern::None};
const Material kUnused = lit(0.5f, 0.5f, 0.5f);
```

In `kNeonHall`, replace `.ambient = 0.40f,` with the block below, and change the listed materials:

```cpp
    .keyColor = {0.80f, 0.85f, 1.00f},
    .keyIntensity = 2.5f,
    .skyAmbient = {0.05f, 0.06f, 0.16f},
    .groundAmbient = {0.02f, 0.02f, 0.05f},
    .pointLights = {{
        {{-4.5f, 1.2f, 3.f}, {0.f, 0.79f, 1.f}, 12.f, 9.f},     // cyan, along the low strips
        {{4.5f, 1.2f, 3.f}, {0.f, 0.79f, 1.f}, 12.f, 9.f},
        {{-4.5f, 3.2f, 12.f}, {0.34f, 0.03f, 1.f}, 12.f, 9.f},  // violet, along the high strips
        {{4.5f, 3.2f, 12.f}, {0.34f, 0.03f, 1.f}, 12.f, 9.f},
    }},
    .pointLightCount = 4,
    .exposure = 1.f,
    .bloomStrength = 0.6f,
```

- `.lane = wood(0.831f, 0.639f, 0.451f),`
- `.gutter = metal(0.008f, 0.055f, 0.22f),`
- `.accentA = glow(0.f, 0.9f, 1.f, 5.f),`
- `.accentB = glow(0.62f, 0.2f, 1.f, 5.f),`
- `.screen = glow(0.1f, 0.3f, 0.9f, 4.f),`

In `kClassicAlley`, replace `.ambient = 0.55f,` with:

```cpp
    .keyColor = {1.00f, 0.92f, 0.80f},
    .keyIntensity = 3.0f,
    .skyAmbient = {0.35f, 0.32f, 0.28f},
    .groundAmbient = {0.12f, 0.09f, 0.06f},
    .pointLights = {{
        {{0.f, 4.2f, 2.f}, {1.f, 0.82f, 0.6f}, 10.f, 8.f},  // warm, under the ceiling lights
        {{0.f, 4.2f, 7.f}, {1.f, 0.82f, 0.6f}, 10.f, 8.f},
        {{0.f, 4.2f, 12.f}, {1.f, 0.82f, 0.6f}, 10.f, 8.f},
        {{0.f, 4.2f, 17.f}, {1.f, 0.82f, 0.6f}, 10.f, 8.f},
    }},
    .pointLightCount = 4,
    .exposure = 1.f,
    .bloomStrength = 0.15f,
```

- `.lane = wood(0.86f, 0.70f, 0.50f),`
- `.gutter = metal(0.25f, 0.25f, 0.28f),`
- `.accentA = glow(1.f, 0.96f, 0.85f, 1.5f),` (the ceiling panels: just over the bloom threshold, a faint glow)

In `kOutdoor`, replace `.ambient = 0.45f,` with:

```cpp
    .keyColor = {1.00f, 0.96f, 0.88f},
    .keyIntensity = 3.5f,
    .skyAmbient = {0.30f, 0.42f, 0.60f},
    .groundAmbient = {0.10f, 0.14f, 0.06f},
    .pointLights = {},
    .pointLightCount = 0,
    .exposure = 1.f,
    .bloomStrength = 0.05f,
```

- `.lane = wood(0.831f, 0.639f, 0.451f),`
- `.gutter = metal(0.30f, 0.30f, 0.32f),`

In `addNeighbourLane`, the lane is now wood through `t.lane`. Replace:

```cpp
    Prop lane = slab({x0 - hw, -0.1f, -g.approach}, {x0 + hw, 0.f, g.laneLength}, t.lane);
    lane.stripes = true;
    out.push_back(lane);
```

with:

```cpp
    out.push_back(slab({x0 - hw, -0.1f, -g.approach}, {x0 + hw, 0.f, g.laneLength}, t.lane));
```

In `EnvironmentDecor.cpp:17`, name every field:

```cpp
const Material kIconWhite {{1.f, 1.f, 1.f}, true, 1.f, 0.f, 1.f, Pattern::None};
```

- [ ] **Step 5: Keep `Render.cpp` compiling (this is replaced in Task 5)**

Add `#include <Bowlingbox/Render/Lighting.h>` after `#include "GlMesh.h"`. Then change:

- `s.setFloat("uAmbient", t.ambient);` → `s.setFloat("uAmbient", linearToSrgb(glm::dot(t.skyAmbient, glm::vec3(0.2126f, 0.7152f, 0.0722f))));`
- in the prop loop, `prop.material, prop.stripes,` → `prop.material, prop.material.pattern == Pattern::Wood,`

- [ ] **Step 6: Run the tests to verify they pass**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all, including the 5 new or renamed `test_environment` tests.

- [ ] **Step 7: Commit**

```bash
git add libraries/Render/include/Bowlingbox/Render/Environment.h libraries/Render/src/Environment.cpp \
        libraries/Render/src/EnvironmentDecor.cpp libraries/Render/src/Render.cpp \
        libraries/Render/test/src/test_Environment.cpp
git commit -m "feat(render): roughness, specular, patterns and per-theme lights"
```

---

### Task 3: Smooth pin silhouette

**Files:**
- Modify: `libraries/Render/src/Geometry.cpp` (`pinData`; new `catmullRom` helper)
- Modify: `libraries/Render/include/Bowlingbox/Render/Geometry.h` (default `segments` 24 → 40)
- Test: `libraries/Render/test/src/test_Render.cpp`

**Interfaces:**
- Consumes: `latheData`.
- Produces: `pinData(height, maxRadius, comHeight, segments = 40)`, with the same signature and the same extremes (height, max radius, base at `-comHeight`).

- [ ] **Step 1: Write the failing test**

Append to `libraries/Render/test/src/test_Render.cpp`:

```cpp
TEST(test_render_geometry, pin_profile_is_smooth) {
    const int segments = 40;
    const float height = 0.38f, com = 0.12f;
    const auto m = pinData(height, 0.06f, com, segments);
    const std::size_t stride = static_cast<std::size_t>(segments) + 1;
    const std::size_t rings = m.vertexCount() / stride;
    ASSERT_GE(rings, 40u);
    const float maxStep = 25.f * 3.14159265f / 180.f;  // the worst step is about 19°
    bool havePrev = false;
    float prev = 0.f;
    for (std::size_t i = 0; i < rings; ++i) {
        const float* v = &m.vertices[i * stride * 6];  // segment 0: x = radius, z = 0
        const float r = v[0], y = v[1];
        if (r < 1e-6f || y < -com + 0.01f * height) {  // poles and the flat base
            havePrev = false;
            continue;
        }
        const float angle = std::atan2(v[4], v[3]);  // profile normal (n_r, n_y)
        if (havePrev)
            EXPECT_LT(std::abs(angle - prev), maxStep) << "ring " << i << " at y = " << y;
        prev = angle;
        havePrev = true;
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build -R pin_profile_is_smooth --output-on-failure`
Expected: FAIL. `rings` is 12 (`Expected: (rings) >= (40u)`).

- [ ] **Step 3: Implement**

In `Geometry.h`, change the declaration's default to `int segments = 40);`.

In `Geometry.cpp`, add `#include <algorithm>` and `#include <iterator>`, then add to the anonymous namespace:

```cpp
using Point2 = std::pair<float, float>;

/// Catmull-Rom between b (t = 0) and c (t = 1), with neighbours a and d.
Point2 catmullRom(Point2 a, Point2 b, Point2 c, Point2 d, float t) {
    const auto f = [t](float p0, float p1, float p2, float p3) {
        return 0.5f * (2.f * p1 + (-p0 + p2) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t * t +
                       (-p0 + 3.f * p1 - 3.f * p2 + p3) * t * t * t);
    };
    return {f(a.first, b.first, c.first, d.first), f(a.second, b.second, c.second, d.second)};
}
```

Replace `pinData` with:

```cpp
MeshData pinData(float height, float maxRadius, float comHeight, int segments) {
    // Silhouette control points (radius / maxRadius, y / height), base to top.
    const Point2 shape[] = {
        {0.0f, 0.00f}, {0.45f, 0.00f}, {0.70f, 0.05f}, {0.95f, 0.20f}, {1.00f, 0.30f},
        {0.90f, 0.45f}, {0.60f, 0.60f}, {0.45f, 0.70f}, {0.50f, 0.80f}, {0.55f, 0.90f},
        {0.35f, 0.98f}, {0.0f, 1.00f},
    };
    constexpr int kSamplesPerSpan = 4;
    const int n = static_cast<int>(std::size(shape));
    const auto at = [&](int i) { return shape[std::clamp(i, 0, n - 1)]; };
    std::vector<Point2> profile;
    for (int i = 0; i < n - 1; ++i)
        for (int s = 0; s < kSamplesPerSpan; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(kSamplesPerSpan);
            const auto [r, y] = catmullRom(at(i - 1), at(i), at(i + 1), at(i + 2), t);
            // Clamping keeps the base flat and the belly at exactly maxRadius.
            profile.emplace_back(std::clamp(r, 0.f, 1.f) * maxRadius, std::clamp(y, 0.f, 1.f) * height - comHeight);
        }
    profile.emplace_back(shape[n - 1].first * maxRadius, shape[n - 1].second * height - comHeight);
    return latheData(profile, segments);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all, including `pin_spans_from_base_to_top_relative_to_com` and `local_bounds_match_the_drawn_meshes`.

If `pin_profile_is_smooth` fails *only* on the ring next to the top pole, raise `kSamplesPerSpan` to 6 and rerun.

- [ ] **Step 5: Commit**

```bash
git add libraries/Render/src/Geometry.cpp libraries/Render/include/Bowlingbox/Render/Geometry.h \
        libraries/Render/test/src/test_Render.cpp
git commit -m "feat(render): smooth pin silhouette from a Catmull-Rom profile"
```

---

### Task 4: Embedded font and CPU font atlas

**Files:**
- Create: `3rdparty/stb/stb_truetype.h` (download)
- Create: `3rdparty/fonts/Rubik-Medium.ttf` (download)
- Create: `licenses/Rubik-OFL.txt` (download)
- Create: `cmake/EmbedFile.cmake`
- Create: `libraries/Render/src/EmbeddedFont.h`
- Create: `libraries/Render/include/Bowlingbox/Render/FontAtlas.h`
- Create: `libraries/Render/src/FontAtlas.cpp`
- Create: `libraries/Render/test/src/test_FontAtlas.cpp`
- Modify: `libraries/Render/CMakeLists.txt`, `3rdparty/CMakeLists.txt`, `licenses/README.md`, `installer/THIRD_PARTY_NOTICES.txt`

**Interfaces:**
- Consumes: `Result`, `Bowlingbox::Core::Error<>` from `Bowlingbox/Core/Core.h`.
- Produces:
  - `struct GlyphQuad { glm::vec2 p0, p1, uv0, uv1; }`.
  - `class FontAtlas`, with `kSize = 512`, `kBakePx = 48`, `kPadding = 4`, `kMaxMipLevel = 2`, and these members:
    - `static Result<FontAtlas> bake(const unsigned char*, std::size_t)`
    - `static Result<FontAtlas> bakeEmbedded()`
    - `const std::vector<unsigned char>& pixels() const`
    - `float pixelHeight(float scale) const`
    - `glm::vec2 measure(std::string_view, float pixelHeight) const`
    - `std::vector<GlyphQuad> layout(std::string_view, glm::vec2 topLeft, float pixelHeight) const`
    - `AtlasRect atlasRect(char) const`
  - Target alias `stb::truetype`.

- [ ] **Step 1: Fetch the third-party files (pinned commits) and check them**

```bash
cd /home/amin/Dokumente/dev/Bowlingbox
mkdir -p 3rdparty/fonts
curl -sSfL -o 3rdparty/stb/stb_truetype.h \
  https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_truetype.h
curl -sSfL -o 3rdparty/fonts/Rubik-Medium.ttf \
  https://raw.githubusercontent.com/googlefonts/rubik/e337a5f69a9bea30e58d05bd40184d79cc099628/old/version-2/fonts/ttf/Rubik-Medium.ttf
curl -sSfL -o licenses/Rubik-OFL.txt \
  https://raw.githubusercontent.com/googlefonts/rubik/e337a5f69a9bea30e58d05bd40184d79cc099628/OFL.txt
head -1 3rdparty/stb/stb_truetype.h
sha256sum 3rdparty/fonts/Rubik-Medium.ttf
head -3 licenses/Rubik-OFL.txt
```

Expected:
- `// stb_truetype.h - v1.26 - public domain`
- `abf4bd674237e66ee6382c434756d66cc507af61f5d37177df37e89874c174cf  3rdparty/fonts/Rubik-Medium.ttf`
- an OFL header naming the Rubik Project Authors

Stop if any of these differ.

- [ ] **Step 2: Add the stb_truetype target and the embed helper**

Append to `3rdparty/CMakeLists.txt` (it's the same include dir; the alias names the use):

```cmake
add_library(stb::truetype ALIAS bowlingbox_stb)
```

Create `cmake/EmbedFile.cmake`:

```cmake
# bowlingbox_embed_file(INPUT <file> OUTPUT <file.cpp> SYMBOL <name>)
#
# Writes a C++ source that defines, in namespace Bowlingbox::Render::embedded,
#   extern const unsigned char <name>[];
#   extern const std::size_t   <name>Size;
# holding the bytes of <file>. Runs at configure time; editing <file> re-runs configure.
function(bowlingbox_embed_file)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "INPUT;OUTPUT;SYMBOL" "")
    file(READ "${arg_INPUT}" hex HEX)
    string(LENGTH "${hex}" hex_length)
    math(EXPR size "${hex_length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "((0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,)(0x..,))"
           "\\1\n" bytes "${bytes}")
    file(WRITE "${arg_OUTPUT}.tmp"
        "// Generated from ${arg_INPUT} by cmake/EmbedFile.cmake. Do not edit.\n"
        "#include <cstddef>\n"
        "namespace Bowlingbox::Render::embedded {\n"
        "extern const unsigned char ${arg_SYMBOL}[] = {\n${bytes}};\n"
        "extern const std::size_t ${arg_SYMBOL}Size = ${size};\n"
        "}  // namespace Bowlingbox::Render::embedded\n")
    file(COPY_FILE "${arg_OUTPUT}.tmp" "${arg_OUTPUT}" ONLY_IF_DIFFERENT)
    file(REMOVE "${arg_OUTPUT}.tmp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${arg_INPUT}")
endfunction()
```

Create `libraries/Render/src/EmbeddedFont.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_EMBEDDED_FONT_H
#define BOWLINGBOX_RENDER_EMBEDDED_FONT_H
#include <cstddef>

namespace Bowlingbox::Render::embedded {
/// 3rdparty/fonts/Rubik-Medium.ttf (SIL OFL 1.1), generated by cmake/EmbedFile.cmake.
extern const unsigned char kRubikMedium[];
extern const std::size_t kRubikMediumSize;
}  // namespace Bowlingbox::Render::embedded
#endif  // BOWLINGBOX_RENDER_EMBEDDED_FONT_H
```

In `libraries/Render/CMakeLists.txt`, add these lines before `set(TARGET_HEADERS`:

```cmake
include(EmbedFile)
set(RUBIK_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/RubikMedium.cpp)
bowlingbox_embed_file(
    INPUT  ${CMAKE_SOURCE_DIR}/3rdparty/fonts/Rubik-Medium.ttf
    OUTPUT ${RUBIK_CPP}
    SYMBOL kRubikMedium
)
```

Then make these additions:
- `TARGET_HEADERS`: `src/EmbeddedFont.h` and `include/Bowlingbox/Render/FontAtlas.h`.
- `TARGET_SOURCES`: `src/FontAtlas.cpp` and `${RUBIK_CPP}`.
- `PRIVATE_DEPS`: `$<BUILD_INTERFACE:stb::truetype>`.
- `TST_SOURCES`: `test/src/test_FontAtlas.cpp`.

- [ ] **Step 3: Write the failing tests**

Create `libraries/Render/test/src/test_FontAtlas.cpp`:

```cpp
#include <Bowlingbox/Render/FontAtlas.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Bowlingbox::Render;

namespace {
const FontAtlas& font() {
    static const FontAtlas atlas = [] {
        auto baked = FontAtlas::bakeEmbedded();
        if (baked.isError())
            throw std::runtime_error(baked.error());
        return baked();
    }();
    return atlas;
}
}  // namespace

TEST(test_font_atlas, embedded_font_bakes_into_the_atlas) {
    const auto& px = font().pixels();
    ASSERT_EQ(px.size(), static_cast<std::size_t>(FontAtlas::kSize) * FontAtlas::kSize);
    EXPECT_GT(std::count_if(px.begin(), px.end(), [](unsigned char v) { return v > 128; }), 1000);
}

TEST(test_font_atlas, junk_is_not_a_font) {
    const std::vector<unsigned char> junk(64, 0);
    EXPECT_TRUE(FontAtlas::bake(junk.data(), junk.size()).isError());
    EXPECT_TRUE(FontAtlas::bake(nullptr, 0).isError());
}

TEST(test_font_atlas, width_grows_with_every_character) {
    const std::string s = "Strike! 9/ X";
    const float h = font().pixelHeight(2.f);
    float last = 0.f;
    for (std::size_t n = 1; n <= s.size(); ++n) {
        const float w = font().measure(s.substr(0, n), h).x;
        EXPECT_GT(w, last) << "after " << n << " characters";
        last = w;
    }
}

TEST(test_font_atlas, line_height_is_the_pixel_height) {
    EXPECT_FLOAT_EQ(font().measure("Ag", 30.f).y, 30.f);
}

TEST(test_font_atlas, glyphs_do_not_overlap_in_the_atlas) {
    for (char a = '!'; a < '~'; ++a)
        for (char b = static_cast<char>(a + 1); b <= '~'; ++b) {
            const auto r1 = font().atlasRect(a), r2 = font().atlasRect(b);
            // Grow one rectangle by the padding minus one pixel: the gap must survive two mip halvings.
            const int pad = FontAtlas::kPadding - 1;
            const bool overlap = r1.x0 - pad < r2.x1 && r2.x0 < r1.x1 + pad && r1.y0 - pad < r2.y1 &&
                                 r2.y0 < r1.y1 + pad;
            EXPECT_FALSE(overlap) << a << " and " << b;
        }
}

TEST(test_font_atlas, characters_outside_ascii_draw_as_question_marks) {
    const float h = 20.f;
    EXPECT_FLOAT_EQ(font().measure("\x01", h).x, font().measure("?", h).x);
    EXPECT_FLOAT_EQ(font().measure("\xc3\xa9", h).x, font().measure("??", h).x);  // é in UTF-8
    const auto odd = font().layout("\x7f", {0.f, 0.f}, h);
    const auto question = font().layout("?", {0.f, 0.f}, h);
    ASSERT_EQ(odd.size(), question.size());
    EXPECT_FLOAT_EQ(odd[0].uv0.x, question[0].uv0.x);
}

TEST(test_font_atlas, scale_maps_to_cap_height) {
    EXPECT_LT(font().pixelHeight(1.f), font().pixelHeight(2.f));
    for (float scale : {1.f, 2.f, 3.5f}) {
        const auto q = font().layout("H", {0.f, 0.f}, font().pixelHeight(scale));
        ASSERT_EQ(q.size(), 1u);
        EXPECT_NEAR(q[0].p1.y - q[0].p0.y, 7.f * scale, 1.5f) << "scale " << scale;
    }
}

TEST(test_font_atlas, quads_run_left_to_right_inside_the_atlas) {
    const auto q = font().layout("AB", {10.f, 20.f}, 30.f);
    ASSERT_EQ(q.size(), 2u);
    EXPECT_LT(q[0].p0.x, q[1].p0.x);
    for (const GlyphQuad& g : q) {
        EXPECT_GE(g.uv0.x, 0.f);
        EXPECT_LE(g.uv1.x, 1.f);
        EXPECT_GE(g.uv0.y, 0.f);
        EXPECT_LE(g.uv1.y, 1.f);
        EXPECT_GE(g.p0.y, 20.f - 1.f);  // nothing above the line's top
    }
}
```

- [ ] **Step 4: Run the build to verify it fails**

Run: `cmake -S . -B build && cmake --build build -j"$(nproc)" 2>&1 | grep -m3 error`
Expected: FAIL with `Bowlingbox/Render/FontAtlas.h: No such file or directory`. Configure must succeed and create `build/libraries/Render/generated/RubikMedium.cpp`.

- [ ] **Step 5: Write the header**

Create `libraries/Render/include/Bowlingbox/Render/FontAtlas.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_FONT_ATLAS_H
#define BOWLINGBOX_RENDER_FONT_ATLAS_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Core/Core.h>
#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace Bowlingbox {
namespace Render {

/// One glyph: screen pixels (origin top-left, y down) and atlas UVs.
struct GlyphQuad {
    glm::vec2 p0, p1;
    glm::vec2 uv0, uv1;
};

/// ASCII 32..126 of a TrueType font baked into one 8-bit coverage texture. CPU only:
/// the Overlay uploads pixels() and draws the quads from layout().
class BOWLINGBOX_RENDER_API FontAtlas {
public:
    static constexpr int kSize = 512;       // atlas width and height, pixels
    static constexpr float kBakePx = 48.f;  // ascent - descent at bake time
    static constexpr char kFirst = 32, kLast = 126;
    /// Empty pixels between glyphs. The Overlay samples mip levels 0..kMaxMipLevel, and
    /// 4 px at level 0 is still 1 px at level 2, so glyphs never bleed into each other.
    static constexpr int kPadding = 4;
    static constexpr int kMaxMipLevel = 2;

    struct AtlasRect {
        int x0, y0, x1, y1;
    };

    /// Error if `ttf` is not a TrueType font or the glyphs do not fit the atlas.
    static Result<FontAtlas> bake(const unsigned char* ttf, std::size_t size);
    /// The embedded HUD font, Rubik Medium.
    static Result<FontAtlas> bakeEmbedded();

    const std::vector<unsigned char>& pixels() const { return _pixels; }
    /// Font size for an ICanvas2D text scale: capitals are 7 * scale pixels tall, as with stb_easy_font.
    float pixelHeight(float scale) const;
    /// Advance width and line height (= pixelHeight) of `text`.
    glm::vec2 measure(std::string_view text, float pixelHeight) const;
    /// Quads with the line's top-left corner at `topLeft`. Characters outside 32..126 draw as '?'.
    std::vector<GlyphQuad> layout(std::string_view text, glm::vec2 topLeft, float pixelHeight) const;
    /// Where a glyph sits in pixels().
    AtlasRect atlasRect(char c) const;

private:
    struct Glyph {
        AtlasRect rect;
        float xoff, yoff, xoff2, yoff2, advance;  // at kBakePx, relative to the pen on the baseline
    };
    static constexpr int kCount = kLast - kFirst + 1;

    FontAtlas() = default;
    const Glyph& glyph(char c) const;

    std::vector<unsigned char> _pixels;
    std::array<Glyph, kCount> _glyphs {};
    float _ascent = 0.f;     // at kBakePx
    float _capHeight = 1.f;  // at kBakePx
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_FONT_ATLAS_H
```

- [ ] **Step 6: Write the implementation**

Create `libraries/Render/src/FontAtlas.cpp`:

```cpp
#include <Bowlingbox/Render/FontAtlas.h>
#include "EmbeddedFont.h"
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <utility>

namespace Bowlingbox {
namespace Render {

Result<FontAtlas> FontAtlas::bake(const unsigned char* ttf, std::size_t size) {
    if (ttf == nullptr || size < 12)
        return Bowlingbox::Core::Error<>("font data is too short");
    const int offset = stbtt_GetFontOffsetForIndex(ttf, 0);
    stbtt_fontinfo info;
    if (offset < 0 || !stbtt_InitFont(&info, ttf, offset))
        return Bowlingbox::Core::Error<>("not a TrueType font");

    FontAtlas atlas;
    atlas._pixels.assign(static_cast<std::size_t>(kSize) * kSize, 0);
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, atlas._pixels.data(), kSize, kSize, 0, kPadding, nullptr))
        return Bowlingbox::Core::Error<>("could not start packing glyphs");
    std::array<stbtt_packedchar, kCount> packed {};
    const int ok = stbtt_PackFontRange(&pc, ttf, 0, kBakePx, kFirst, kCount, packed.data());
    stbtt_PackEnd(&pc);
    if (!ok)
        return Bowlingbox::Core::Error<>("glyphs do not fit a 512x512 atlas");
    for (std::size_t i = 0; i < packed.size(); ++i) {
        const stbtt_packedchar& p = packed[i];
        atlas._glyphs[i] = {{p.x0, p.y0, p.x1, p.y1}, p.xoff, p.yoff, p.xoff2, p.yoff2, p.xadvance};
    }

    const float scale = stbtt_ScaleForPixelHeight(&info, kBakePx);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    atlas._ascent = static_cast<float>(ascent) * scale;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!stbtt_GetCodepointBox(&info, 'H', &x0, &y0, &x1, &y1) || y1 <= y0)
        return Bowlingbox::Core::Error<>("font has no 'H'");
    atlas._capHeight = static_cast<float>(y1 - y0) * scale;
    return Result<FontAtlas>(std::move(atlas));
}

Result<FontAtlas> FontAtlas::bakeEmbedded() {
    return bake(embedded::kRubikMedium, embedded::kRubikMediumSize);
}

float FontAtlas::pixelHeight(float scale) const {
    return kBakePx * 7.f * scale / _capHeight;
}

const FontAtlas::Glyph& FontAtlas::glyph(char c) const {
    const char k = (c < kFirst || c > kLast) ? '?' : c;
    return _glyphs[static_cast<std::size_t>(k - kFirst)];
}

FontAtlas::AtlasRect FontAtlas::atlasRect(char c) const {
    return glyph(c).rect;
}

glm::vec2 FontAtlas::measure(std::string_view text, float pixelHeight) const {
    const float k = pixelHeight / kBakePx;
    float width = 0.f;
    for (char c : text)
        width += glyph(c).advance * k;
    return {width, pixelHeight};
}

std::vector<GlyphQuad> FontAtlas::layout(std::string_view text, glm::vec2 topLeft, float pixelHeight) const {
    const float k = pixelHeight / kBakePx;
    const float inv = 1.f / static_cast<float>(kSize);
    const float baseline = topLeft.y + _ascent * k;
    float pen = topLeft.x;
    std::vector<GlyphQuad> quads;
    quads.reserve(text.size());
    for (char c : text) {
        const Glyph& g = glyph(c);
        if (g.rect.x1 > g.rect.x0 && g.rect.y1 > g.rect.y0)  // a space has no bitmap
            quads.push_back({{pen + g.xoff * k, baseline + g.yoff * k},
                             {pen + g.xoff2 * k, baseline + g.yoff2 * k},
                             {static_cast<float>(g.rect.x0) * inv, static_cast<float>(g.rect.y0) * inv},
                             {static_cast<float>(g.rect.x1) * inv, static_cast<float>(g.rect.y1) * inv}});
        pen += g.advance * k;
    }
    return quads;
}

}  // namespace Render
}  // namespace Bowlingbox
```

- [ ] **Step 7: Run the tests to verify they pass**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build -R test_font_atlas --output-on-failure`
Expected: PASS, 8 tests.

- [ ] **Step 8: Licence notices**

In `licenses/README.md`, add two rows to the "In-tree (`3rdparty/`)" table, after the `stb_easy_font` row:

```markdown
| [stb_truetype](https://github.com/nothings/stb) | 1.26 | Render (HUD font atlas) | MIT OR Unlicense (public domain) | [stb.txt](stb.txt) |
| [Rubik](https://github.com/googlefonts/rubik) Medium | 2.x static | Render (HUD font, embedded) | SIL Open Font License 1.1 | [Rubik-OFL.txt](Rubik-OFL.txt) |
```

In `installer/THIRD_PARTY_NOTICES.txt`, make three changes:
- In "Components whose licences need no notice", after the `stb_easy_font` line, add: `  - stb_truetype 1.26      public domain (Unlicense option)`
- In "Components whose notices follow in full", add: `  - Rubik Medium font      SIL Open Font License 1.1`
- At the end of the file, append a section in the same style as the GLM one: a `====` rule, the title `Rubik font (3rdparty/fonts/Rubik-Medium.ttf, embedded in libBowlingboxRender)`, a `----` rule, then the full contents of `licenses/Rubik-OFL.txt`.

- [ ] **Step 9: Commit**

```bash
git add 3rdparty/stb/stb_truetype.h 3rdparty/fonts/Rubik-Medium.ttf 3rdparty/CMakeLists.txt \
        cmake/EmbedFile.cmake libraries/Render/src/EmbeddedFont.h libraries/Render/src/FontAtlas.cpp \
        libraries/Render/include/Bowlingbox/Render/FontAtlas.h libraries/Render/test/src/test_FontAtlas.cpp \
        libraries/Render/CMakeLists.txt licenses/Rubik-OFL.txt licenses/README.md installer/THIRD_PARTY_NOTICES.txt
git commit -m "feat(render): embedded Rubik font and a CPU font atlas"
```

---

### Task 5: New lit shader (GGX, hemisphere ambient, wood, ball swirl) on the basic path

This task changes what the game looks like without adding offscreen passes: all lighting, patterns and tone mapping run in the lit shader, straight into the window. Later tasks reuse this shader unchanged.

**Files:**
- Create: `libraries/Render/src/Shaders.h`, `libraries/Render/src/Shaders.cpp`
- Modify: `libraries/Render/src/Render.cpp` (shader strings move out; `SceneRenderer::Impl` and `draw` rewritten; ball 48 segments; GL error report)
- Modify: `libraries/Render/src/GlShader.h/.cpp` (`setInt`, cached uniform locations)
- Modify: `libraries/Render/src/ContextData.h` (`reportedGlError`)
- Modify: `libraries/Render/include/Bowlingbox/Render/Render.h` (`Camera::eye()`)
- Modify: `libraries/Render/CMakeLists.txt`

**Interfaces:**
- Consumes: `Material`, `Pattern`, `PointLight`, the `Theme` lighting fields (Task 2), `srgbToLinear` (Task 1).
- Produces:
  - `namespace shaders` with `kLitVs`, `litFs()`, `kFlatVs`, `kFlatFs`, `kSkyVs`, `skyFs()` and `kToneMapGlsl`.
  - `GlShader::setInt(const char*, int)`; every setter now uses cached uniform locations.
  - `Camera::eye()`.
  - In `SceneRenderer::Impl`: `struct PassParams`, `setFrameUniforms(GlShader&, const PassParams&)`, `drawOpaque(GlShader&, const PassParams&, const SceneDescription&, const std::vector<std::size_t>* only, bool withLane)`, `drawSky(bool toneMap)`, `drawGuide(GlShader&, const SceneDescription&)` and `mainPass(const PassParams&, const SceneDescription&)`.
  - Lit-shader uniforms used by later tasks: `uShadows`, `uShadowMap` (unit 1), `uLightVP`, `uReflect`, `uReflection` (unit 2), `uViewportSize`, `uClipPlane` and `uToneMap`.

- [ ] **Step 1: Add `setInt`, a uniform-location cache, `Camera::eye()` and the GL error flag**

The passes added in Tasks 7–10 set about ten uniforms per draw, ~150 draws, up to three passes a frame. Today every setter calls `glGetUniformLocation`, so cache the locations.

`GlShader.h`: add `#include <string>` and `#include <unordered_map>`. Add `void setInt(const char* name, int v) const;` after `setBool`. In the private section, add:

```cpp
    int location(const char* name) const;  // cached glGetUniformLocation; -1 if unused
    mutable std::unordered_map<std::string, int> _locations;
```

`GlShader.cpp`: add the two functions below. Then, in every existing setter, replace `glGetUniformLocation(_program, name)` with `location(name)`.

```cpp
int GlShader::location(const char* name) const {
    if (const auto it = _locations.find(name); it != _locations.end())
        return it->second;
    const int loc = glGetUniformLocation(_program, name);
    _locations.emplace(name, loc);
    return loc;
}

void GlShader::setInt(const char* name, int v) const {
    glUniform1i(location(name), v);
}
```

`Render.h`: in `class Camera`'s public section, after `projection()`, add `glm::vec3 eye() const { return _eye; }`.

`ContextData.h`: add `bool reportedGlError = false;  // debug builds report the first GL error once` after `int height = 1;`.

- [ ] **Step 2: Create `Shaders.h`**

```cpp
#ifndef BOWLINGBOX_RENDER_SHADERS_H
#define BOWLINGBOX_RENDER_SHADERS_H
#include <string>

namespace Bowlingbox::Render::shaders {

/// GLSL helpers shared by the lit, sky and composite shaders: acesFit, linearToSrgb, toDisplay.
extern const char* const kToneMapGlsl;

/// 3D meshes: world, object and light-space positions; gl_ClipDistance[0] from uClipPlane.
extern const char* const kLitVs;
/// GGX + hemisphere ambient + point lights, wood/ball patterns, shadows, lane reflection, fog,
/// and (uToneMap) tone mapping to sRGB for the basic path.
std::string litFs();

extern const char* const kFlatVs;  // 2D overlay rectangles
extern const char* const kFlatFs;

extern const char* const kSkyVs;  // attribute-less full-screen triangle, vT = 0 bottom .. 1 top
std::string skyFs();

}  // namespace Bowlingbox::Render::shaders
#endif  // BOWLINGBOX_RENDER_SHADERS_H
```

- [ ] **Step 3: Create `Shaders.cpp`**

Move `kFlatVs`, `kFlatFs` and `kSkyVs` out of `Render.cpp` unchanged, as `extern`-declared `const char* const` definitions. Add the rest:

```cpp
#include "Shaders.h"

namespace Bowlingbox::Render::shaders {

const char* const kToneMapGlsl = R"(
vec3 acesFit(vec3 x) {  // Narkowicz 2015
    return clamp(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
vec3 linearToSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}
vec3 toDisplay(vec3 hdr, float exposure) { return linearToSrgb(acesFit(hdr * exposure)); }
)";

const char* const kLitVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj; uniform mat3 uNormalMat;
uniform mat4 uLightVP;
uniform vec4 uClipPlane;  // world space; (0, 0, 0, 1) keeps everything
out vec3 vNormal; out vec3 vWorldPos; out vec3 vLocalPos; out vec3 vViewPos; out vec4 vLightPos;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vLocalPos = aPos;
    vNormal = normalize(uNormalMat * aNormal);
    vec4 viewPos = uView * world;
    vViewPos = viewPos.xyz;
    vLightPos = uLightVP * world;
    gl_ClipDistance[0] = dot(world, uClipPlane);
    gl_Position = uProj * viewPos;
})";

namespace {
const char* const kLitBody = R"(
in vec3 vNormal; in vec3 vWorldPos; in vec3 vLocalPos; in vec3 vViewPos; in vec4 vLightPos;

uniform vec3 uColor;  // linear
uniform vec3 uBandColor; uniform vec2 uBandRange;  // pin neck stripe, object-space y
uniform bool uEmissive; uniform float uEmissiveStrength;
uniform float uRoughness; uniform float uSpecular;
uniform int uPattern;  // 0 none, 1 wood, 2 ball swirl
uniform float uAlpha;

uniform vec3 uCameraPos;
uniform vec3 uLightDir; uniform vec3 uKeyColor;  // keyColor * keyIntensity
uniform vec3 uSkyAmbient; uniform vec3 uGroundAmbient;
uniform int uPointCount;
uniform vec3 uPointPos[4]; uniform vec3 uPointColor[4]; uniform float uPointRange[4];

uniform bool uFog; uniform vec3 uFogColor; uniform float uFogStart; uniform float uFogEnd;

uniform bool uShadows; uniform sampler2DShadow uShadowMap;  // texture unit 1
uniform bool uReflect; uniform sampler2D uReflection;       // texture unit 2
uniform vec2 uViewportSize;
uniform float uLaneHalfWidth; uniform float uDeckZ;
uniform float uBoard;  // board width: 2 * laneHalfWidth / 39

uniform bool uToneMap; uniform float uExposure;

out vec4 FragColor;

const float PI = 3.14159265;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float noise3(vec3 p) {
    vec3 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);
    float a = mix(hash3(i), hash3(i + vec3(1, 0, 0)), u.x);
    float b = mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), u.x);
    float c = mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), u.x);
    float d = mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), u.x);
    return mix(mix(a, b, u.y), mix(c, d, u.y), u.z);
}
float fbm3(vec3 p) {
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 4; ++i) { sum += amp * noise3(p); p *= 2.03; amp *= 0.5; }
    return sum;
}

// Dark-red inlay on the main lane: 7 arrows about 4.6 m down and 7 dots on the approach,
// on boards 5, 10, ... 35 of 39. Board 20 is the middle board, so the pattern is symmetric
// and it doesn't matter which edge counts as board 1.
float marking(vec3 p) {
    if (abs(p.x) > uLaneHalfWidth) return 0.0;
    float fromEdge = (p.x + uLaneHalfWidth) / uBoard;
    float board = floor(fromEdge) + 1.0;                        // 1 .. 39
    if (board < 5.0 || board > 35.0 || mod(board, 5.0) != 0.0) return 0.0;
    float u = fract(fromEdge) - 0.5;                            // -0.5 .. 0.5 across the board
    float tip = 4.3 + 0.6 * (1.0 - abs(board - 20.0) / 15.0);   // the middle arrow is furthest
    float along = (tip - p.z) / 0.15;                           // 0 at the tip, 1 at the base
    float arrow = (along >= 0.0 && along <= 1.0 && abs(u) <= 0.5 * along) ? 1.0 : 0.0;
    float spot = length(vec2(u * uBoard, p.z + 0.15)) < 0.008 ? 1.0 : 0.0;
    return max(arrow, spot);
}

// Boards are counted from the main lane's edge, so its 39 boards line up with both edges.
// Neighbour lanes reuse the same grid; it's off their edges by under a millimetre.
vec3 wood(vec3 base, vec3 p) {
    float fromEdge = (p.x + uLaneHalfWidth) / uBoard;
    float board = floor(fromEdge);
    float tint = 1.0 + 0.16 * (hash(vec2(board, 7.0)) - 0.5);
    float phase = hash(vec2(board, 13.0)) * 50.0;
    float fine = clamp(1.0 - fwidth(p.x) * 750.0, 0.0, 1.0);  // fade grain finer than a pixel
    float grain = 0.6 * noise(vec2(p.x * 400.0, p.z * 3.0 + phase))
                + 0.4 * fine * noise(vec2(p.x * 1500.0, p.z * 9.0 + phase));
    float streak = smoothstep(0.75, 0.95, noise(vec2(board * 3.1, p.z * 0.4 + phase)));
    vec3 c = base * tint * (0.92 + 0.12 * grain) * (1.0 - 0.18 * streak);
    float seam = smoothstep(0.92, 1.0, abs(fract(fromEdge) - 0.5) * 2.0);
    c *= 1.0 - 0.35 * seam * clamp(1.0 - fwidth(p.x) / uBoard, 0.0, 1.0);
    if (p.z < 0.0) c *= vec3(1.04, 1.02, 0.97);           // approach
    else if (p.z > uDeckZ) c *= vec3(0.96, 0.94, 0.92);   // pin deck
    return mix(c, vec3(0.30, 0.02, 0.02), marking(p));
}

// Two-tone marbling in object space, so it rolls with the ball, plus three finger holes.
vec3 swirl(vec3 base, vec3 p) {
    vec3 d = normalize(p);
    float warp = fbm3(d * 2.5 + vec3(3.1));
    float m = fbm3(d * 3.0 + 2.0 * vec3(warp));
    vec3 c = mix(base, base * 0.35, smoothstep(0.45, 0.55, m));
    vec3 holes[3] = vec3[3](normalize(vec3(0.0, -0.15, 1.0)), normalize(vec3(-0.06, 0.12, 1.0)),
                            normalize(vec3(0.06, 0.12, 1.0)));
    for (int i = 0; i < 3; ++i) {
        float a = acos(clamp(dot(d, holes[i]), -1.0, 1.0));  // radians from the hole centre
        float r = i == 0 ? 0.11 : 0.09;
        c *= mix(0.08, 1.0, smoothstep(r - 0.02, r + 0.01, a));
    }
    return c;
}

float distributionGgx(float nDotH, float a) {
    float a2 = a * a;
    float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float visibilitySmith(float nDotV, float nDotL, float a) {  // includes 1 / (4 nDotV nDotL)
    float k = a * 0.5;
    return 0.25 / ((nDotV * (1.0 - k) + k) * (nDotL * (1.0 - k) + k));
}
vec3 brdf(vec3 n, vec3 v, vec3 l, vec3 albedo, vec3 radiance) {
    float nDotL = max(dot(n, l), 0.0);
    if (nDotL <= 0.0) return vec3(0.0);
    vec3 h = normalize(v + l);
    float nDotV = max(dot(n, v), 1e-4), nDotH = max(dot(n, h), 0.0), vDotH = max(dot(v, h), 0.0);
    float a = max(uRoughness * uRoughness, 0.002);
    vec3 f = vec3(uSpecular) + (1.0 - vec3(uSpecular)) * pow(1.0 - vDotH, 5.0);
    vec3 spec = distributionGgx(nDotH, a) * visibilitySmith(nDotV, nDotL, a) * f;
    vec3 diff = (1.0 - f) * albedo / PI;
    return (diff + spec) * radiance * nDotL;
}

float keyLightVisible(vec3 n) {
    if (!uShadows) return 1.0;
    vec3 p = vLightPos.xyz / vLightPos.w * 0.5 + 0.5;
    if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0)))) return 1.0;  // outside: lit
    float bias = max(0.0025 * (1.0 - max(dot(n, -normalize(uLightDir)), 0.0)), 0.0005);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            lit += texture(uShadowMap, vec3(p.xy + vec2(x, y) * texel, p.z - bias));
    return lit / 9.0;
}

void main() {
    vec3 base = (vLocalPos.y >= uBandRange.x && vLocalPos.y <= uBandRange.y) ? uBandColor : uColor;
    if (uPattern == 1) base = wood(base, vWorldPos);
    else if (uPattern == 2) base = swirl(base, vLocalPos);

    vec3 rgb;
    if (uEmissive) {
        rgb = base * uEmissiveStrength;
    } else {
        vec3 n = normalize(vNormal);
        vec3 v = normalize(uCameraPos - vWorldPos);
        rgb = mix(uGroundAmbient, uSkyAmbient, n.y * 0.5 + 0.5) * base;
        rgb += brdf(n, v, -normalize(uLightDir), base, uKeyColor) * keyLightVisible(n);
        for (int i = 0; i < uPointCount; ++i) {
            vec3 toLight = uPointPos[i] - vWorldPos;
            float d = length(toLight);
            float window = clamp(1.0 - pow(d / uPointRange[i], 4.0), 0.0, 1.0);
            rgb += brdf(n, v, toLight / d, base, uPointColor[i] * window * window / (d * d + 1.0));
        }
        if (uReflect) {
            vec2 uv = gl_FragCoord.xy / uViewportSize;
            float o = uRoughness * 0.004;
            vec4 r = 0.25 * (texture(uReflection, uv + vec2(o, 0.0)) + texture(uReflection, uv - vec2(o, 0.0))
                           + texture(uReflection, uv + vec2(0.0, o)) + texture(uReflection, uv - vec2(0.0, o)));
            float fresnel = uSpecular + (1.0 - uSpecular) * pow(1.0 - max(dot(n, v), 0.0), 5.0);
            float k = clamp(fresnel * (1.0 - uRoughness) * 0.6, 0.0, 1.0);
            // r is premultiplied (cleared to transparent black): a plain mix would darken soft edges.
            rgb = rgb * (1.0 - k * r.a) + k * r.rgb;
        }
    }
    if (uFog) {
        float f = clamp((length(vViewPos) - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        rgb = mix(rgb, uFogColor, uEmissive ? 0.5 * f : f);
    }
    FragColor = vec4(uToneMap ? toDisplay(rgb, uExposure) : rgb, uAlpha);
}
)";

const char* const kSkyBody = R"(
in float vT;
uniform vec3 uTop; uniform vec3 uBottom;  // linear
uniform bool uToneMap; uniform float uExposure;
out vec4 FragColor;
void main() {
    vec3 c = mix(uBottom, uTop, clamp(vT, 0.0, 1.0));
    FragColor = vec4(uToneMap ? toDisplay(c, uExposure) : c, 1.0);
}
)";
}  // namespace

std::string litFs() {
    return std::string("#version 330 core\n") + kToneMapGlsl + kLitBody;
}
std::string skyFs() {
    return std::string("#version 330 core\n") + kToneMapGlsl + kSkyBody;
}

}  // namespace Bowlingbox::Render::shaders
```

Add `src/Shaders.h` to `TARGET_HEADERS` and `src/Shaders.cpp` to `TARGET_SOURCES`.

- [ ] **Step 4: Build the new shaders in `Context::create()` and report GL errors**

In `Render.cpp`, delete the anonymous-namespace shader strings (`kLitVs`, `kLitFs`, `kFlatVs`, `kFlatFs`, `kSkyVs`, `kSkyFs`), and add `#include "Shaders.h"`, `#include <Bowlingbox/Render/Lighting.h>` and `#include <cstdio>`. In `Context::create()`, build the shaders like this:

```cpp
    auto lit = GlShader::build(shaders::kLitVs, shaders::litFs().c_str());
    if (lit.isError())
        return Bowlingbox::Core::Error<>("lit shader: " + lit.error());
    auto flat = GlShader::build(shaders::kFlatVs, shaders::kFlatFs);
    if (flat.isError())
        return Bowlingbox::Core::Error<>("flat shader: " + flat.error());
    auto sky = GlShader::build(shaders::kSkyVs, shaders::skyFs().c_str());
    if (sky.isError())
        return Bowlingbox::Core::Error<>("sky shader: " + sky.error());
```

After the three `ctx->_d->... = ...()` assignments, add:

```cpp
    // Samplers of different types must not share a texture unit.
    ctx->_d->lit->use();
    ctx->_d->lit->setInt("uShadowMap", 1);
    ctx->_d->lit->setInt("uReflection", 2);
    glUseProgram(0);
```

Replace `Context::endFrame` with:

```cpp
void Context::endFrame() {
    glBindVertexArray(0);
    glUseProgram(0);
#ifndef NDEBUG
    if (const GLenum err = glGetError(); err != GL_NO_ERROR && !_d->reportedGlError) {
        std::fprintf(stderr, "render: GL error 0x%04x\n", err);
        _d->reportedGlError = true;
    }
#endif
}
```

- [ ] **Step 5: Rewrite `SceneRenderer`**

In the file's top anonymous namespace, keep `kNoBand` and `struct Vertex2D` (the overlay uses it), and add `kGuide` after `kNoBand`:

```cpp
const Material kGuide {{0.f, 0.898f, 1.f}, true, 1.f, 0.f, 1.f, Pattern::None};
```

Leave the `Context` and `Camera` sections alone. Replace only the `// ---- SceneRenderer` section, from its comment line through the end of `SceneRenderer::draw`, with:

```cpp
// ---------------------------------------------------------------- SceneRenderer
namespace {
/// Per-pass settings for the lit shader.
struct PassParams {
    glm::mat4 view {1.f}, proj {1.f};
    glm::vec3 eye {0.f};
    bool toneMap = false;  // basic path: tone map + sRGB in the lit shader
    bool shadows = false;
    glm::mat4 lightViewProj {1.f};
    glm::vec4 clipPlane {0.f, 0.f, 0.f, 1.f};  // keeps everything
    glm::vec2 viewportSize {1.f};
    bool reflection = false;  // the main lane samples the reflection texture
};
}  // namespace

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
          ball(sphereData(geo.ballRadius, 48)),
          pin(shapeMesh(Shape::Pin, geo)),
          guide(boxData(0.01f, 0.003f, 0.5f)),
          unitBox(shapeMesh(Shape::Box, geo)),
          unitCylinder(shapeMesh(Shape::Cylinder, geo)),
          unitCone(shapeMesh(Shape::Cone, geo)) {
        glGenVertexArrays(1, &skyVao);
        setTheme(id);
    }
    ~Impl() { glDeleteVertexArrays(1, &skyVao); }

    void setTheme(ThemeId id) {  // CPU only
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
    static glm::mat4 at(float x, float y, float z) {
        return glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
    }
    glm::vec2 neck() const {
        return {0.55f * g.pinHeight - g.pinComHeight, 0.65f * g.pinHeight - g.pinComHeight};
    }

    PassParams cameraPass(const Camera& camera, glm::ivec2 size) const {
        PassParams pp;
        pp.view = camera.view();
        pp.proj = camera.projection();
        pp.eye = camera.eye();
        pp.viewportSize = glm::vec2(size);
        return pp;
    }

    void setFrameUniforms(GlShader& s, const PassParams& pp) const {
        const Theme& t = *current;
        s.use();
        s.setMat4("uView", pp.view);
        s.setMat4("uProj", pp.proj);
        s.setVec3("uCameraPos", pp.eye);
        s.setVec3("uLightDir", glm::normalize(t.lightDir));
        s.setVec3("uKeyColor", t.keyColor * t.keyIntensity);
        s.setVec3("uSkyAmbient", t.skyAmbient);
        s.setVec3("uGroundAmbient", t.groundAmbient);
        static constexpr std::array<const char*, kMaxPointLights> kPos {"uPointPos[0]", "uPointPos[1]",
                                                                         "uPointPos[2]", "uPointPos[3]"};
        static constexpr std::array<const char*, kMaxPointLights> kColor {"uPointColor[0]", "uPointColor[1]",
                                                                           "uPointColor[2]", "uPointColor[3]"};
        static constexpr std::array<const char*, kMaxPointLights> kRange {"uPointRange[0]", "uPointRange[1]",
                                                                           "uPointRange[2]", "uPointRange[3]"};
        s.setInt("uPointCount", t.pointLightCount);
        for (std::size_t i = 0; i < static_cast<std::size_t>(t.pointLightCount); ++i) {
            const PointLight& l = t.pointLights[i];
            s.setVec3(kPos[i], l.position);
            s.setVec3(kColor[i], l.color * l.intensity);
            s.setFloat(kRange[i], l.range);
        }
        s.setBool("uFog", true);
        s.setVec3("uFogColor", srgbToLinear(t.fogColor));
        s.setFloat("uFogStart", t.fogStart);
        s.setFloat("uFogEnd", t.fogEnd);
        s.setVec3("uBandColor", srgbToLinear(t.pinBand.color));
        s.setBool("uToneMap", pp.toneMap);
        s.setFloat("uExposure", t.exposure);
        s.setBool("uShadows", pp.shadows);
        s.setMat4("uLightVP", pp.lightViewProj);
        s.setVec4("uClipPlane", pp.clipPlane);
        s.setVec2("uViewportSize", pp.viewportSize);
        s.setBool("uReflect", false);
        s.setFloat("uLaneHalfWidth", g.laneHalfWidth);
        s.setFloat("uBoard", 2.f * g.laneHalfWidth / 39.f);  // a real lane has 39 boards
        s.setFloat("uDeckZ", g.headPinZ - 0.3f);
        s.setFloat("uAlpha", 1.f);
    }

    void drawMesh(GlShader& s, const GlMesh& mesh, const glm::mat4& model, const Material& m,
                  glm::vec2 band = kNoBand) const {
        s.setMat4("uModel", model);
        s.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        s.setVec3("uColor", srgbToLinear(m.color));
        s.setVec2("uBandRange", band);
        s.setBool("uEmissive", m.emissive);
        s.setFloat("uEmissiveStrength", m.emissiveStrength);
        s.setFloat("uRoughness", m.roughness);
        s.setFloat("uSpecular", m.specular);
        s.setInt("uPattern", static_cast<int>(m.pattern));
        mesh.draw();
    }

    /// Props (all, or only the listed indices), then lane + gutters (if withLane), gutter walls,
    /// standing pins and the ball. Works with any program built on kLitVs.
    void drawOpaque(GlShader& s, const PassParams& pp, const SceneDescription& scene,
                    const std::vector<std::size_t>* only, bool withLane) const {
        const Theme& t = *current;
        const auto drawProp = [&](const Prop& prop) {
            drawMesh(s, meshFor(prop.shape), prop.transform.matrix(), prop.material, prop.pinBand ? neck() : kNoBand);
        };
        if (only)
            for (std::size_t i : *only)
                drawProp(props[i]);
        else
            for (const Prop& prop : props)
                drawProp(prop);

        const float outerX = g.laneHalfWidth + g.gutterWidth;
        if (withLane) {
            s.setBool("uReflect", pp.reflection);
            drawMesh(s, lane, at(0.f, -0.05f, (g.laneLength - g.approach) / 2), t.lane);
            s.setBool("uReflect", false);
            for (float sx : {-1.f, 1.f})
                drawMesh(s, gutter, at(sx * (g.laneHalfWidth + g.gutterWidth / 2), -g.gutterDepth - 0.05f, g.laneLength / 2),
                         t.gutter);
        }
        for (float sx : {-1.f, 1.f})
            drawMesh(s, wall, at(sx * (outerX + 0.02f), 0.05f, g.laneLength / 2), t.gutter);
        for (std::size_t i = 0; i < scene.pins.size(); ++i)
            if (scene.pinVisible[i])
                drawMesh(s, pin, scene.pins[i].matrix(), t.pinBody, neck());
        drawMesh(s, ball, scene.ball.matrix(), t.ball);
    }

    void drawSky(bool toneMap) {
        const Theme& t = *current;
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        GlShader& sky = *ctx.data().sky;
        sky.use();
        sky.setVec3("uTop", srgbToLinear(t.backgroundTop));
        sky.setVec3("uBottom", srgbToLinear(t.backgroundBottom));
        sky.setBool("uToneMap", toneMap);
        sky.setFloat("uExposure", t.exposure);
        glBindVertexArray(skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void drawGuide(GlShader& s, const SceneDescription& scene) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        s.setBool("uFog", false);  // the guide keeps its colour
        s.setFloat("uAlpha", 0.40f);
        const float length = g.laneLength + 1.f;  // from the ball start (z = -1) to the deck end
        const float a = glm::radians(scene.aimAngleDeg);
        const glm::vec3 dir(std::sin(a), 0.f, std::cos(a));
        const glm::vec3 start(scene.aimOffset, 0.004f, -1.f);
        const glm::mat4 model = glm::translate(glm::mat4(1.f), start + dir * (length / 2)) *
                                glm::rotate(glm::mat4(1.f), a, glm::vec3(0.f, 1.f, 0.f)) *
                                glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, length));
        drawMesh(s, guide, model, kGuide);
        s.setFloat("uAlpha", 1.f);
        s.setBool("uFog", true);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    /// Sky, everything opaque, then the aim guide, into whatever framebuffer is bound.
    void mainPass(const PassParams& pp, const SceneDescription& scene) {
        glDisable(GL_BLEND);
        drawSky(pp.toneMap);
        GlShader& s = *ctx.data().lit;
        setFrameUniforms(s, pp);
        drawOpaque(s, pp, scene, nullptr, true);
        if (scene.showAimGuide)
            drawGuide(s, scene);
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
    if (viewportH < 1)
        return;  // minimised, or the HUD bands take the whole window
    glViewport(0, viewportY, p.ctx.width(), viewportH);
    PassParams pp = p.cameraPass(camera, {p.ctx.width(), viewportH});
    pp.toneMap = true;
    p.mainPass(pp, scene);
}
```

The lit shader reads `gl_FragCoord` only for `uReflect`, which stays off in this task, so `uViewportSize` doesn't matter on the basic path.

- [ ] **Step 6: Build and run the unit tests**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all.

- [ ] **Step 7: Smoke run and look**

Run the smoke loop from *Global Constraints* with `TASK=task5`.

Expected:
- No stderr output, so no `render: GL error` and no shader compile error.
- In the screenshots, every theme shows wood boards with grain and seams, and seven arrows about 4.6 m out. The arrows form a symmetric V centred on the lane, with the middle arrow furthest and the outer two 4 boards in from each edge. There's a glossy marbled ball with finger holes, glossy pins with a specular highlight, and steel-looking gutters.

If a theme is much too dark or bright, tune only that theme's `keyIntensity`, `skyAmbient`/`groundAmbient` and point-light intensities in `Environment.cpp`, within the ranges the tests enforce. Rerun the tests after any tuning.

- [ ] **Step 8: Commit**

```bash
git add libraries/Render/src/Shaders.h libraries/Render/src/Shaders.cpp libraries/Render/src/Render.cpp \
        libraries/Render/src/GlShader.h libraries/Render/src/GlShader.cpp libraries/Render/src/ContextData.h \
        libraries/Render/include/Bowlingbox/Render/Render.h libraries/Render/CMakeLists.txt \
        libraries/Render/src/Environment.cpp
git commit -m "feat(render): GGX lighting, hemisphere ambient, wood lane and swirled ball"
```

---

### Task 6: HUD text with the TrueType font

**Files:**
- Create: `libraries/Render/src/GlTexture.h`, `libraries/Render/src/GlTexture.cpp`
- Modify: `libraries/Render/src/ContextData.h`, `libraries/Render/src/Shaders.h/.cpp`, `libraries/Render/src/Render.cpp` (`Context::create`, `Overlay`)
- Modify: `libraries/Render/CMakeLists.txt`

**Interfaces:**
- Consumes: `FontAtlas` (Task 4), `shaders::` (Task 5).
- Produces:
  - `class GlTexture { void create(); unsigned id() const; }` (RAII, movable).
  - `ContextData::font` (`std::optional<FontAtlas>`), `ContextData::fontTexture` and `ContextData::text`.
  - `shaders::kTextVs` and `shaders::kTextFs`.

- [ ] **Step 1: Add `GlTexture`**

`libraries/Render/src/GlTexture.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_GLTEXTURE_H
#define BOWLINGBOX_RENDER_GLTEXTURE_H

namespace Bowlingbox {
namespace Render {

/// Owns one GL texture name; deletes it on destruction (needs a current context).
class GlTexture {
public:
    GlTexture() = default;
    ~GlTexture();
    GlTexture(GlTexture&& other) noexcept;
    GlTexture& operator=(GlTexture&& other) noexcept;
    GlTexture(const GlTexture&) = delete;
    GlTexture& operator=(const GlTexture&) = delete;

    void create();  // generates a name if there is none
    void reset();   // deletes the name
    unsigned id() const { return _id; }

private:
    unsigned _id = 0;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLTEXTURE_H
```

`libraries/Render/src/GlTexture.cpp`:

```cpp
#include "GlTexture.h"
#include <glad/gl.h>
#include <utility>

namespace Bowlingbox {
namespace Render {

GlTexture::~GlTexture() {
    reset();
}
GlTexture::GlTexture(GlTexture&& other) noexcept : _id(std::exchange(other._id, 0u)) {}
GlTexture& GlTexture::operator=(GlTexture&& other) noexcept {
    if (this != &other) {
        reset();
        _id = std::exchange(other._id, 0u);
    }
    return *this;
}
void GlTexture::create() {
    if (_id == 0)
        glGenTextures(1, &_id);
}
void GlTexture::reset() {
    if (_id != 0)
        glDeleteTextures(1, &_id);
    _id = 0;
}

}  // namespace Render
}  // namespace Bowlingbox
```

Add both files to the CMake lists.

- [ ] **Step 2: Text shaders**

`Shaders.h`, add:

```cpp
extern const char* const kTextVs;  // 2D glyph quads: position + atlas uv
extern const char* const kTextFs;  // colour * atlas coverage
```

`Shaders.cpp`, add:

```cpp
const char* const kTextVs = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMVP;
out vec2 vUv;
void main() { vUv = aUv; gl_Position = uMVP * vec4(aPos, 0.0, 1.0); })";

const char* const kTextFs = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uAtlas;
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = vec4(uColor.rgb, uColor.a * texture(uAtlas, vUv).r); })";
```

- [ ] **Step 3: Context data and upload**

`ContextData.h`: add `#include "GlTexture.h"`, `#include <Bowlingbox/Render/FontAtlas.h>` and `#include <optional>`, plus these members:

```cpp
    std::unique_ptr<GlShader> text;  // 2D glyph quads from the font atlas
    std::optional<FontAtlas> font;   // empty: the overlay falls back to stb_easy_font
    GlTexture fontTexture;           // R8 atlas with mip levels 0..FontAtlas::kMaxMipLevel
```

In `Context::create()`, after the samplers block from Task 5, add:

```cpp
    auto text = GlShader::build(shaders::kTextVs, shaders::kTextFs);
    auto font = FontAtlas::bakeEmbedded();
    if (text.isError()) {
        std::fprintf(stderr, "render: text shader failed (%s); using the built-in font\n", text.error().c_str());
    } else if (font.isError()) {
        std::fprintf(stderr, "render: HUD font unavailable (%s); using the built-in font\n", font.error().c_str());
    } else {
        ContextData& d = *ctx->_d;
        d.text = text();
        d.font = font();
        d.fontTexture.create();
        glBindTexture(GL_TEXTURE_2D, d.fontTexture.id());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FontAtlas::kSize, FontAtlas::kSize, 0, GL_RED, GL_UNSIGNED_BYTE,
                     d.font->pixels().data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, FontAtlas::kMaxMipLevel);  // see kPadding
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
```

- [ ] **Step 4: Overlay draws glyph quads**

In `struct Overlay::Impl`, add the members and the constructor/destructor lines:

```cpp
    struct TextVertex {
        float x, y, u, v;
    };
    unsigned textVao = 0, textVbo = 0;
    std::vector<TextVertex> textVertices;
```

In the constructor, after the existing VAO setup:

```cpp
        glGenVertexArrays(1, &textVao);
        glGenBuffers(1, &textVbo);
        glBindVertexArray(textVao);
        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                              reinterpret_cast<void*>(static_cast<std::uintptr_t>(2 * sizeof(float))));
        glBindVertexArray(0);
```

In the destructor: `glDeleteBuffers(1, &textVbo); glDeleteVertexArrays(1, &textVao);`. Add `#include <cstdint>` at the top of `Render.cpp`.

Add the member function:

```cpp
    void drawGlyphs(const std::vector<GlyphQuad>& quads, glm::vec2 offset, glm::vec4 color) {
        textVertices.clear();
        for (const GlyphQuad& q : quads) {
            const float x0 = q.p0.x + offset.x, y0 = q.p0.y + offset.y;
            const float x1 = q.p1.x + offset.x, y1 = q.p1.y + offset.y;
            textVertices.insert(textVertices.end(), {{x0, y0, q.uv0.x, q.uv0.y}, {x1, y0, q.uv1.x, q.uv0.y},
                                                     {x1, y1, q.uv1.x, q.uv1.y}, {x0, y0, q.uv0.x, q.uv0.y},
                                                     {x1, y1, q.uv1.x, q.uv1.y}, {x0, y1, q.uv0.x, q.uv1.y}});
        }
        GlShader& s = *ctx.data().text;
        s.use();
        s.setMat4("uMVP", proj);
        s.setVec4("uColor", color);
        s.setInt("uAtlas", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx.data().fontTexture.id());
        glBindVertexArray(textVao);
        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(textVertices.size() * sizeof(TextVertex)),
                     textVertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textVertices.size()));
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
```

Replace `Overlay::drawText` and `Overlay::measureText` with:

```cpp
void Overlay::drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb) {
    if (text.empty())
        return;
    if (const auto& font = _p->ctx.data().font) {
        const auto quads = font->layout(text, px, font->pixelHeight(scale));
        _p->drawGlyphs(quads, {1.f, 1.f}, {0.f, 0.f, 0.f, 0.5f});  // drop shadow
        _p->drawGlyphs(quads, {0.f, 0.f}, glm::vec4(rgb, 1.f));
        return;
    }
    std::string s(text);
    const int quads = stb_easy_font_print(0.f, 0.f, s.data(), nullptr, _p->textBuffer.data(),
                                          static_cast<int>(_p->textBuffer.size()));
    const glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(px, 0.f)) *
                            glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, 1.f));
    _p->drawQuads(_p->textBuffer.data(), quads, model, glm::vec4(rgb, 1.f));
}

glm::vec2 Overlay::measureText(std::string_view text, float scale) const {
    if (const auto& font = _p->ctx.data().font)
        return font->measure(text, font->pixelHeight(scale));
    std::string s(text);
    return {static_cast<float>(stb_easy_font_width(s.data())) * scale,
            static_cast<float>(stb_easy_font_height(s.data())) * scale};
}
```

- [ ] **Step 5: Build, test and smoke**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all. The `bowlinggl` HUD tests use a fake canvas and are unaffected.

Run the smoke loop with `TASK=task6`.
Expected:
- No stderr output.
- The scoreboard digits, hint and result text are smooth Rubik with a soft shadow.
- The text sits centred in its cells as before, and nothing spills out of a scoreboard cell.

- [ ] **Step 6: Commit**

```bash
git add libraries/Render/src/GlTexture.h libraries/Render/src/GlTexture.cpp libraries/Render/src/ContextData.h \
        libraries/Render/src/Shaders.h libraries/Render/src/Shaders.cpp libraries/Render/src/Render.cpp \
        libraries/Render/CMakeLists.txt
git commit -m "feat(render): HUD text from the Rubik atlas, with stb_easy_font as fallback"
```

---

### Task 7: HDR + MSAA offscreen target, tone mapping, and the basic-path switch

**Files:**
- Create: `libraries/Render/include/Bowlingbox/Render/RenderOptions.h`, `libraries/Render/src/RenderOptions.cpp`
- Create: `libraries/Render/src/GlFramebuffer.h`, `libraries/Render/src/GlFramebuffer.cpp`
- Create: `libraries/Render/src/PostChain.h`, `libraries/Render/src/PostChain.cpp`
- Create: `libraries/Render/test/src/test_RenderOptions.cpp`
- Modify: `libraries/Render/src/ContextData.h`, `Shaders.h/.cpp`, `Render.cpp`, `libraries/Render/CMakeLists.txt`

**Interfaces:**
- Consumes: the shaders and `SceneRenderer::Impl` from Task 5.
- Produces:
  - `struct FrameSizes { glm::ivec2 scene, reflection; std::array<glm::ivec2, 4> bloom; }` and `FrameSizes frameSizes(int width, int bandHeight)`.
  - `struct RenderOptions { bool forceBasic, stats; }` and `RenderOptions renderOptions(const char* render, const char* stats)`.
  - `class GlFramebuffer` (`Spec`, `create`, `bind`, `blitColorTo`, `colorTexture`, `depthTexture`, `width`, `height`).
  - `class PostChain { bool ensureSize(const FrameSizes&, int samples); void beginScene(); void present(int viewportY, float exposure, float bloomStrength); }`.
  - In `ContextData`: `options`, `fullPipeline`, `samples`, `composite`, `resolve`.
  - `shaders::kPostVs`, `shaders::kResolveFs` and `shaders::compositeFs()`.

- [ ] **Step 1: Write the failing tests**

Create `libraries/Render/test/src/test_RenderOptions.cpp`:

```cpp
#include <Bowlingbox/Render/RenderOptions.h>
#include <gtest/gtest.h>

using namespace Bowlingbox::Render;

TEST(test_render_options, render_options_read_the_environment_strictly) {
    EXPECT_FALSE(renderOptions(nullptr, nullptr).forceBasic);
    EXPECT_FALSE(renderOptions(nullptr, nullptr).stats);
    EXPECT_TRUE(renderOptions("basic", nullptr).forceBasic);
    EXPECT_TRUE(renderOptions("Basic", nullptr).forceBasic);
    EXPECT_TRUE(renderOptions("BASIC", nullptr).forceBasic);
    EXPECT_FALSE(renderOptions("", nullptr).forceBasic);
    EXPECT_FALSE(renderOptions("full", nullptr).forceBasic);
    EXPECT_FALSE(renderOptions("basics", nullptr).forceBasic);
    EXPECT_TRUE(renderOptions(nullptr, "1").stats);
    EXPECT_FALSE(renderOptions(nullptr, "0").stats);
    EXPECT_FALSE(renderOptions(nullptr, "").stats);
    EXPECT_FALSE(renderOptions(nullptr, "yes").stats);
}

TEST(test_render_options, frame_sizes_never_reach_zero) {
    for (auto [w, h] : {std::pair {0, 0}, std::pair {1, 1}, std::pair {3, 0}, std::pair {-5, 7}}) {
        const FrameSizes s = frameSizes(w, h);
        EXPECT_GE(s.scene.x, 1);
        EXPECT_GE(s.scene.y, 1);
        EXPECT_GE(s.reflection.x, 1);
        EXPECT_GE(s.reflection.y, 1);
        for (const glm::ivec2& b : s.bloom) {
            EXPECT_GE(b.x, 1);
            EXPECT_GE(b.y, 1);
        }
    }
}

TEST(test_render_options, frame_sizes_halve_down_the_chain) {
    const FrameSizes s = frameSizes(1920, 777);
    EXPECT_EQ(s.scene, glm::ivec2(1920, 777));
    EXPECT_EQ(s.reflection, glm::ivec2(960, 388));
    EXPECT_EQ(s.bloom[0], glm::ivec2(960, 388));
    EXPECT_EQ(s.bloom[1], glm::ivec2(480, 194));
    EXPECT_EQ(s.bloom[2], glm::ivec2(240, 97));
    EXPECT_EQ(s.bloom[3], glm::ivec2(120, 48));
}
```

Add it to `TST_SOURCES`.

- [ ] **Step 2: Run the build to verify it fails**

Run: `cmake --build build -j"$(nproc)" 2>&1 | grep -m3 error`
Expected: FAIL with `Bowlingbox/Render/RenderOptions.h: No such file or directory`.

- [ ] **Step 3: Implement `RenderOptions`**

`libraries/Render/include/Bowlingbox/Render/RenderOptions.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_RENDER_OPTIONS_H
#define BOWLINGBOX_RENDER_RENDER_OPTIONS_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <glm/glm.hpp>
#include <array>

namespace Bowlingbox {
namespace Render {

/// Offscreen target sizes for a 3D band of width x bandHeight. Every size is at least 1x1.
struct FrameSizes {
    glm::ivec2 scene {1};                // HDR main pass
    glm::ivec2 reflection {1};           // half of scene
    std::array<glm::ivec2, 4> bloom {};  // half, quarter, eighth, sixteenth of scene
};
BOWLINGBOX_RENDER_API FrameSizes frameSizes(int width, int bandHeight);

struct RenderOptions {
    bool forceBasic = false;  // BOWLINGBOX_RENDER=basic (any case)
    bool stats = false;       // BOWLINGBOX_RENDER_STATS=1
};
/// Reads the two environment values (either may be null). Anything else is ignored.
BOWLINGBOX_RENDER_API RenderOptions renderOptions(const char* render, const char* stats);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_RENDER_OPTIONS_H
```

`libraries/Render/src/RenderOptions.cpp`:

```cpp
#include <Bowlingbox/Render/RenderOptions.h>
#include <algorithm>
#include <cctype>
#include <string_view>

namespace Bowlingbox {
namespace Render {

FrameSizes frameSizes(int width, int bandHeight) {
    const auto half = [](glm::ivec2 s) { return glm::max(s / 2, glm::ivec2(1)); };
    FrameSizes out;
    out.scene = glm::max(glm::ivec2(width, bandHeight), glm::ivec2(1));
    out.reflection = half(out.scene);
    out.bloom[0] = half(out.scene);
    for (std::size_t i = 1; i < out.bloom.size(); ++i)
        out.bloom[i] = half(out.bloom[i - 1]);
    return out;
}

RenderOptions renderOptions(const char* render, const char* stats) {
    RenderOptions out;
    if (render) {
        const std::string_view v(render), basic("basic");
        out.forceBasic = v.size() == basic.size() &&
                         std::equal(v.begin(), v.end(), basic.begin(), [](char a, char b) {
                             return std::tolower(static_cast<unsigned char>(a)) == b;
                         });
    }
    out.stats = stats && std::string_view(stats) == "1";
    return out;
}

}  // namespace Render
}  // namespace Bowlingbox
```

Add both files to the CMake lists.

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build -R test_render_options --output-on-failure`
Expected: PASS, 3 tests.

- [ ] **Step 4: `GlFramebuffer`**

`libraries/Render/src/GlFramebuffer.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_GLFRAMEBUFFER_H
#define BOWLINGBOX_RENDER_GLFRAMEBUFFER_H

namespace Bowlingbox {
namespace Render {

/// RAII framebuffer with an optional colour and depth attachment.
class GlFramebuffer {
public:
    struct Spec {
        int width = 1, height = 1;
        int samples = 0;            // > 0: multisample textures (colour and depth), resolved by PostChain's shader
        unsigned colorFormat = 0;   // e.g. GL_RGBA16F; 0 = no colour attachment
        bool depth = false;         // depth renderbuffer
        bool depthTexture = false;  // depth as a comparable texture (shadow maps); replaces `depth`
    };

    GlFramebuffer() = default;
    ~GlFramebuffer();
    GlFramebuffer(const GlFramebuffer&) = delete;
    GlFramebuffer& operator=(const GlFramebuffer&) = delete;

    /// (Re)creates the attachments. False (and nothing allocated) if the framebuffer is incomplete.
    bool create(const Spec& spec);
    void bind() const;  // binds and sets the viewport to the whole target
    void blitColorTo(const GlFramebuffer& dst) const;  // single-sample targets only
    /// GL_TEXTURE_2D, or GL_TEXTURE_2D_MULTISAMPLE when samples > 0.
    unsigned colorTexture() const { return _colorTex; }
    unsigned depthTexture() const { return _depthTex; }
    int width() const { return _spec.width; }
    int height() const { return _spec.height; }

private:
    void release();
    unsigned _fbo = 0, _colorTex = 0, _depthRb = 0, _depthTex = 0;
    Spec _spec;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLFRAMEBUFFER_H
```

`libraries/Render/src/GlFramebuffer.cpp`:

```cpp
#include "GlFramebuffer.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {
namespace {
void clampedLinear() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
}  // namespace

GlFramebuffer::~GlFramebuffer() {
    release();
}

void GlFramebuffer::release() {
    if (_depthTex) glDeleteTextures(1, &_depthTex);
    if (_colorTex) glDeleteTextures(1, &_colorTex);
    if (_depthRb) glDeleteRenderbuffers(1, &_depthRb);
    if (_fbo) glDeleteFramebuffers(1, &_fbo);
    _fbo = _colorTex = _depthRb = _depthTex = 0;
}

bool GlFramebuffer::create(const Spec& spec) {
    release();
    _spec = spec;
    const GLsizei w = spec.width, h = spec.height;
    const bool ms = spec.samples > 0;
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    if (spec.colorFormat != 0) {
        if (ms) {
            // A texture, not a renderbuffer, so PostChain's resolve shader can read every sample.
            glGenTextures(1, &_colorTex);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _colorTex);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, spec.samples, spec.colorFormat, w, h, GL_TRUE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, _colorTex, 0);
        } else {
            glGenTextures(1, &_colorTex);
            glBindTexture(GL_TEXTURE_2D, _colorTex);
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(spec.colorFormat), w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
            clampedLinear();
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);
        }
    } else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    if (spec.depthTexture) {
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D, _depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        clampedLinear();  // linear + compare = 2x2 hardware PCF
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
    } else if (spec.depth && ms) {
        // Also a texture: a renderbuffer may round the sample count differently, which leaves
        // the framebuffer incomplete.
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _depthTex);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, spec.samples, GL_DEPTH_COMPONENT24, w, h, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, _depthTex, 0);
    } else if (spec.depth) {
        glGenRenderbuffers(1, &_depthRb);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRb);
    }
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    if (!complete)
        release();
    return complete;
}

void GlFramebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, _spec.width, _spec.height);
}

void GlFramebuffer::blitColorTo(const GlFramebuffer& dst) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst._fbo);
    glBlitFramebuffer(0, 0, _spec.width, _spec.height, 0, 0, dst._spec.width, dst._spec.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace Render
}  // namespace Bowlingbox
```

- [ ] **Step 5: Post shaders**

`Shaders.h`:

```cpp
extern const char* const kPostVs;     // attribute-less full-screen triangle with vUv
extern const char* const kResolveFs;  // MSAA -> one sample, weighted so bright samples don't swamp edges
/// Scene (+ bloom) * exposure -> ACES -> sRGB.
std::string compositeFs();
```

`Shaders.cpp`. Put `kCompositeBody` inside the anonymous namespace next to `kSkyBody`:

```cpp
const char* const kPostVs = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    vUv = (p + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";

// A plain average lets one sample of a neon strip (5.0) or the ball's highlight (~40) dominate an
// edge pixel, so edges look jagged again after tone mapping. Averaging c / (1 + max(c)) and
// inverting that curve afterwards keeps the edge gradient.
const char* const kResolveFs = R"(#version 330 core
uniform sampler2DMS uSrc; uniform int uSamples;
out vec4 FragColor;
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec3 sum = vec3(0.0);
    for (int i = 0; i < uSamples; ++i) {
        vec3 c = texelFetch(uSrc, p, i).rgb;
        sum += c / (1.0 + max(max(c.r, c.g), c.b));
    }
    vec3 t = sum / float(uSamples);
    FragColor = vec4(t / max(1.0 - max(max(t.r, t.g), t.b), 1e-4), 1.0);
})";

// (in the anonymous namespace)
const char* const kCompositeBody = R"(
in vec2 vUv;
uniform sampler2D uScene; uniform sampler2D uBloom;  // units 0 and 1
uniform bool uHasBloom; uniform float uBloomStrength; uniform float uExposure;
out vec4 FragColor;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    if (uHasBloom) c += texture(uBloom, vUv).rgb * uBloomStrength;
    FragColor = vec4(toDisplay(c, uExposure), 1.0);
}
)";

std::string compositeFs() {
    return std::string("#version 330 core\n") + kToneMapGlsl + kCompositeBody;
}
```

- [ ] **Step 6: `PostChain`**

`libraries/Render/src/PostChain.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_POSTCHAIN_H
#define BOWLINGBOX_RENDER_POSTCHAIN_H
#include "GlFramebuffer.h"
#include <Bowlingbox/Render/RenderOptions.h>

namespace Bowlingbox {
namespace Render {
struct ContextData;

/// The HDR main-pass target (RGBA16F, MSAA), its resolve, and the final composite into the window.
class PostChain {
public:
    explicit PostChain(ContextData& d);
    ~PostChain();
    PostChain(const PostChain&) = delete;
    PostChain& operator=(const PostChain&) = delete;

    /// Reallocates when the sizes changed. False if a framebuffer is incomplete.
    bool ensureSize(const FrameSizes& sizes, int samples);
    /// Binds the MSAA HDR target and clears colour and depth.
    void beginScene();
    /// Resolves, then tone maps into the default framebuffer at (0, viewportY), scene-sized.
    void present(int viewportY, float exposure, float bloomStrength);

private:
    void resolve();             // _msaa -> _resolved
    void drawTriangle() const;  // the full-screen triangle of kPostVs

    ContextData& _d;
    glm::ivec2 _size {0};
    int _samples = 0;
    GlFramebuffer _msaa, _resolved;
    unsigned _vao = 0;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_POSTCHAIN_H
```

`libraries/Render/src/PostChain.cpp`:

```cpp
#include "PostChain.h"
#include "ContextData.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {

PostChain::PostChain(ContextData& d) : _d(d) {
    glGenVertexArrays(1, &_vao);
}
PostChain::~PostChain() {
    glDeleteVertexArrays(1, &_vao);
}

bool PostChain::ensureSize(const FrameSizes& sizes, int samples) {
    if (sizes.scene == _size && samples == _samples)
        return true;
    _size = sizes.scene;
    _samples = samples;
    const bool ok = _msaa.create({_size.x, _size.y, samples, GL_RGBA16F, true, false}) &&
                    _resolved.create({_size.x, _size.y, 0, GL_RGBA16F, false, false});
    if (!ok)
        _size = glm::ivec2(0);  // retry next time
    return ok;
}

void PostChain::beginScene() {
    _msaa.bind();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void PostChain::drawTriangle() const {
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PostChain::resolve() {
    if (_samples == 0) {  // no MSAA on this GPU: both targets are plain textures
        _msaa.blitColorTo(_resolved);
        return;
    }
    _resolved.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *_d.resolve;
    s.use();
    s.setInt("uSrc", 0);
    s.setInt("uSamples", _samples);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _msaa.colorTexture());
    drawTriangle();
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
}

void PostChain::present(int viewportY, float exposure, float bloomStrength) {
    resolve();
    // Task 8 inserts the bloom chain here.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, viewportY, _size.x, _size.y);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *_d.composite;
    s.use();
    s.setInt("uScene", 0);
    s.setInt("uBloom", 1);
    s.setBool("uHasBloom", false);
    s.setFloat("uBloomStrength", bloomStrength);
    s.setFloat("uExposure", exposure);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _resolved.colorTexture());
    drawTriangle();
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace Render
}  // namespace Bowlingbox
```

Add `GlFramebuffer.h/.cpp` and `PostChain.h/.cpp` to the CMake lists.

- [ ] **Step 7: Decide the pipeline in `Context::create()` and use it in `draw`**

`ContextData.h`: add `#include <Bowlingbox/Render/RenderOptions.h>` and:

```cpp
    std::unique_ptr<GlShader> composite;  // HDR -> tone map -> sRGB (full pipeline only)
    std::unique_ptr<GlShader> resolve;    // weighted MSAA resolve (full pipeline only)
    RenderOptions options;
    bool fullPipeline = false;  // shadows, reflection, HDR + bloom; false = basic path
    int samples = 4;            // main-pass MSAA, clamped to what multisample textures support
```

In `Render.cpp`, add `#include "GlFramebuffer.h"`, `#include "PostChain.h"` and `#include <cstdlib>`. Then add this to the anonymous namespace:

```cpp
/// Builds the full pipeline's shaders and checks the GPU can render to RGBA16F with MSAA.
/// On failure prints why and returns false: the basic path runs instead.
bool enableFullPipeline(ContextData& d) {
    // The main pass uses multisample *textures*, whose limits are separate from GL_MAX_SAMPLES.
    GLint colorSamples = 0, depthSamples = 0;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &colorSamples);
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &depthSamples);
    d.samples = std::clamp(static_cast<int>(std::min(colorSamples, depthSamples)), 0, 4);
    auto composite = GlShader::build(shaders::kPostVs, shaders::compositeFs().c_str());
    if (composite.isError()) {
        std::fprintf(stderr, "render: composite shader failed (%s); using the basic path\n", composite.error().c_str());
        return false;
    }
    d.composite = composite();
    auto resolve = GlShader::build(shaders::kPostVs, shaders::kResolveFs);
    if (resolve.isError()) {
        std::fprintf(stderr, "render: resolve shader failed (%s); using the basic path\n", resolve.error().c_str());
        return false;
    }
    d.resolve = resolve();
    GlFramebuffer probe;
    if (!probe.create({4, 4, d.samples, GL_RGBA16F, true, false})) {
        std::fprintf(stderr, "render: RGBA16F x%d target incomplete; using the basic path\n", d.samples);
        return false;
    }
    return true;
}
```

In `Context::create()`, before `return Result<...>(std::move(ctx));`:

```cpp
    ContextData& d = *ctx->_d;
    d.options = renderOptions(std::getenv("BOWLINGBOX_RENDER"), std::getenv("BOWLINGBOX_RENDER_STATS"));
    if (d.options.forceBasic)
        std::fprintf(stderr, "render: basic path (BOWLINGBOX_RENDER=basic)\n");
    else
        d.fullPipeline = enableFullPipeline(d);
```

The message for `BOWLINGBOX_RENDER=basic` is intentional, so in the smoke check, its stderr line is expected.

In `SceneRenderer::Impl`, add the member `std::unique_ptr<PostChain> post;`. At the end of the constructor body, add:

```cpp
        if (ctx.data().fullPipeline)
            post = std::make_unique<PostChain>(ctx.data());
```

Replace `SceneRenderer::draw` with:

```cpp
void SceneRenderer::draw(const Camera& camera, const SceneDescription& scene, int viewportY,
                         int viewportH) {
    Impl& p = *_p;
    if (viewportH < 1)
        return;  // minimised, or the HUD bands take the whole window
    ContextData& d = p.ctx.data();
    const FrameSizes sizes = frameSizes(p.ctx.width(), viewportH);
    if (d.fullPipeline && !p.post->ensureSize(sizes, d.samples)) {
        std::fprintf(stderr, "render: offscreen targets incomplete at %dx%d; using the basic path\n",
                     sizes.scene.x, sizes.scene.y);
        d.fullPipeline = false;
    }
    PassParams pp = p.cameraPass(camera, sizes.scene);
    if (!d.fullPipeline) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, viewportY, p.ctx.width(), viewportH);
        pp.toneMap = true;
        p.mainPass(pp, scene);
        return;
    }
    p.post->beginScene();
    p.mainPass(pp, scene);
    p.post->present(viewportY, p.current->exposure, p.current->bloomStrength);
}
```

- [ ] **Step 8: Build, test, smoke (both paths, and resize)**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all.

Run the smoke loop with `TASK=task7`.
Expected:
- No stderr output.
- Edges of the pins, the lane and the gutter walls are clearly smoother than in `task6-*.png` (MSAA). So are the edges of the Neon Hall strips and of the ball's highlight (weighted resolve).

Then run the basic path:

```bash
BOWLINGBOX_RENDER=basic build/bin/BowlingGL --theme=neon 2> build/shots/basic.err & pid=$!
sleep 4; import -window root build/shots/task7-basic.png; kill $pid; wait $pid 2>/dev/null
cat build/shots/basic.err
```

Expected:
- stderr is exactly `render: basic path (BOWLINGBOX_RENDER=basic)`.
- The picture matches `task6-neon.png`.

Resize check (Review Focus 1): start `build/bin/BowlingGL`, minimise and restore the window, and drag it as short as the window manager allows, then back. Expected: no stderr output, and the scene redraws correctly after each step.

- [ ] **Step 9: Commit**

```bash
git add libraries/Render/include/Bowlingbox/Render/RenderOptions.h libraries/Render/src/RenderOptions.cpp \
        libraries/Render/src/GlFramebuffer.h libraries/Render/src/GlFramebuffer.cpp \
        libraries/Render/src/PostChain.h libraries/Render/src/PostChain.cpp \
        libraries/Render/src/ContextData.h libraries/Render/src/Shaders.h libraries/Render/src/Shaders.cpp \
        libraries/Render/src/Render.cpp libraries/Render/test/src/test_RenderOptions.cpp libraries/Render/CMakeLists.txt
git commit -m "feat(render): HDR MSAA main pass with ACES tone mapping and a basic-path fallback"
```

---

### Task 8: Bloom

**Files:**
- Modify: `libraries/Render/src/PostChain.h/.cpp`, `libraries/Render/src/Shaders.h/.cpp`, `libraries/Render/src/ContextData.h`, `libraries/Render/src/Render.cpp` (`enableFullPipeline`)

**Interfaces:**
- Consumes: `PostChain` and `FrameSizes::bloom` (Task 7).
- Produces: `ContextData::bright`, `bloomDown` and `bloomUp`; `shaders::kBrightFs`, `kBloomDownFs` and `kBloomUpFs`. `PostChain::present` now uses `bloomStrength`.

- [ ] **Step 1: Bloom shaders**

`Shaders.h`:

```cpp
extern const char* const kBrightFs;     // keeps what exceeds uThreshold, clamped to kMaxBright
extern const char* const kBloomDownFs;  // 13-tap downsample
extern const char* const kBloomUpFs;    // 3x3 tent upsample (drawn additively)
```

`Shaders.cpp`:

```cpp
const char* const kBrightFs = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uSrc; uniform float uThreshold;
out vec4 FragColor;
// The ball's highlight reaches ~40 on a few pixels; unclamped, those pixels make the glow
// flicker as the ball rolls. Neon (5) and anything a viewer reads as a light stay below this.
const float kMaxBright = 16.0;
void main() {
    vec3 c = min(texture(uSrc, vUv).rgb, vec3(kMaxBright));
    float peak = max(max(c.r, c.g), c.b);
    FragColor = vec4(c * max(peak - uThreshold, 0.0) / max(peak, 1e-4), 1.0);
})";

const char* const kBloomDownFs = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uSrc; uniform vec2 uTexel;  // source texel size
out vec4 FragColor;
vec3 at(float x, float y) { return texture(uSrc, vUv + uTexel * vec2(x, y)).rgb; }
void main() {
    vec3 r = at(0.0, 0.0) * 0.125
           + (at(-2.0, 2.0) + at(2.0, 2.0) + at(-2.0, -2.0) + at(2.0, -2.0)) * 0.03125
           + (at(0.0, 2.0) + at(-2.0, 0.0) + at(2.0, 0.0) + at(0.0, -2.0)) * 0.0625
           + (at(-1.0, 1.0) + at(1.0, 1.0) + at(-1.0, -1.0) + at(1.0, -1.0)) * 0.125;
    FragColor = vec4(r, 1.0);
})";

const char* const kBloomUpFs = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uSrc; uniform vec2 uTexel;  // source texel size
out vec4 FragColor;
vec3 at(float x, float y) { return texture(uSrc, vUv + uTexel * vec2(x, y)).rgb; }
void main() {
    vec3 r = 4.0 * at(0.0, 0.0)
           + 2.0 * (at(0.0, 1.0) + at(-1.0, 0.0) + at(1.0, 0.0) + at(0.0, -1.0))
           + (at(-1.0, 1.0) + at(1.0, 1.0) + at(-1.0, -1.0) + at(1.0, -1.0));
    FragColor = vec4(r / 16.0, 1.0);
})";
```

- [ ] **Step 2: Build them with the full pipeline**

`ContextData.h`:

```cpp
    std::unique_ptr<GlShader> bright, bloomDown, bloomUp;  // bloom chain (full pipeline only)
```

In `enableFullPipeline`, after `d.composite = composite();`:

```cpp
    struct Stage {
        std::unique_ptr<GlShader>& slot;
        const char* fs;
        const char* name;
    };
    for (Stage st : {Stage {d.bright, shaders::kBrightFs, "bright"}, Stage {d.bloomDown, shaders::kBloomDownFs, "bloom down"},
                     Stage {d.bloomUp, shaders::kBloomUpFs, "bloom up"}}) {
        auto built = GlShader::build(shaders::kPostVs, st.fs);
        if (built.isError()) {
            std::fprintf(stderr, "render: %s shader failed (%s); using the basic path\n", st.name, built.error().c_str());
            return false;
        }
        st.slot = built();
    }
```

- [ ] **Step 3: Allocate and run the chain**

`PostChain.h`: add `#include <array>` and the member `std::array<GlFramebuffer, 4> _bloom;`. (`drawTriangle` exists since Task 7.)

In `PostChain.cpp`'s `ensureSize`, extend `ok`:

```cpp
    bool ok = _msaa.create({_size.x, _size.y, samples, GL_RGBA16F, true, false}) &&
              _resolved.create({_size.x, _size.y, 0, GL_RGBA16F, false, false});
    for (std::size_t i = 0; ok && i < _bloom.size(); ++i)
        ok = _bloom[i].create({sizes.bloom[i].x, sizes.bloom[i].y, 0, GL_RGBA16F, false, false});
```

In `present`, replace the comment `// Task 8 inserts the bloom chain here.` with:

```cpp
    const bool bloom = bloomStrength > 0.f;
    if (bloom) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0);
        GlShader& bright = *_d.bright;
        bright.use();
        bright.setInt("uSrc", 0);
        bright.setFloat("uThreshold", 1.f);
        _bloom[0].bind();
        glBindTexture(GL_TEXTURE_2D, _resolved.colorTexture());
        drawTriangle();
        GlShader& down = *_d.bloomDown;
        down.use();
        down.setInt("uSrc", 0);
        for (std::size_t i = 1; i < _bloom.size(); ++i) {
            const GlFramebuffer& src = _bloom[i - 1];
            down.setVec2("uTexel", {1.f / static_cast<float>(src.width()), 1.f / static_cast<float>(src.height())});
            _bloom[i].bind();
            glBindTexture(GL_TEXTURE_2D, src.colorTexture());
            drawTriangle();
        }
        GlShader& up = *_d.bloomUp;
        up.use();
        up.setInt("uSrc", 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (std::size_t i = _bloom.size() - 1; i > 0; --i) {
            const GlFramebuffer& src = _bloom[i];
            up.setVec2("uTexel", {1.f / static_cast<float>(src.width()), 1.f / static_cast<float>(src.height())});
            _bloom[i - 1].bind();
            glBindTexture(GL_TEXTURE_2D, src.colorTexture());
            drawTriangle();
        }
        glDisable(GL_BLEND);
    }
```

In the composite part of `present`, change `s.setBool("uHasBloom", false);` to `s.setBool("uHasBloom", bloom);`. After binding unit 0, add:

```cpp
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _bloom[0].colorTexture());
    glActiveTexture(GL_TEXTURE0);
```

After the composite's `drawTriangle();`, unbind unit 1 the same way.

- [ ] **Step 4: Build, test, smoke**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all.

Run the smoke loop with `TASK=task8`.
Expected:
- No stderr output.
- Neon Hall: the neon strips and wall screens have a soft glow halo.
- Classic: only a faint glow around the ceiling panels (strength 1.5).
- Outdoor: practically unchanged from `task7-outdoor.png`.
- Throw a ball in Neon Hall and watch it roll: its highlight may glow a little, but it doesn't sparkle or flicker from frame to frame. If it does, lower `kMaxBright`.

- [ ] **Step 5: Commit**

```bash
git add libraries/Render/src/PostChain.h libraries/Render/src/PostChain.cpp libraries/Render/src/Shaders.h \
        libraries/Render/src/Shaders.cpp libraries/Render/src/ContextData.h libraries/Render/src/Render.cpp
git commit -m "feat(render): bloom with a four-level downsample/upsample chain"
```

---

### Task 9: Shadow map

**Files:**
- Create: `libraries/Render/src/ShadowMap.h`, `libraries/Render/src/ShadowMap.cpp`
- Modify: `libraries/Render/include/Bowlingbox/Render/Lighting.h`, `libraries/Render/src/Lighting.cpp` (`propsInBox`)
- Modify: `libraries/Render/src/Shaders.h/.cpp` (`kDepthFs`), `ContextData.h` (`depth`), `Render.cpp`, `libraries/Render/CMakeLists.txt`
- Test: `libraries/Render/test/src/test_Lighting.cpp`

**Interfaces:**
- Consumes: `fitLightFrustum`, `shadowBox` and `kShadowTopY` (Task 1); `GlFramebuffer` (Task 7); `drawOpaque` and `PassParams` (Task 5).
- Produces:
  - `std::vector<std::size_t> propsInBox(const std::vector<Prop>&, const SceneGeometry&, const Aabb&)`.
  - `class ShadowMap { static constexpr int kWidth = 1024, kHeight = 4096; bool create(); void begin() const; void end() const; unsigned depthTexture() const; }`.
  - `ContextData::depth`.

- [ ] **Step 1: Write the failing test**

Append to `libraries/Render/test/src/test_Lighting.cpp`:

```cpp
TEST(test_lighting, shadow_casters_skip_the_ceiling_and_keep_the_ball_returns) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        const auto props = buildProps(t, kG);
        const auto casters = propsInBox(props, kG, shadowBox(kG));
        ASSERT_FALSE(casters.empty()) << t.name;
        for (std::size_t i : casters)
            EXPECT_LT(bounds(props[i], kG).min.y, kShadowTopY) << t.name << " prop " << i;
        const bool hasReturn = std::any_of(casters.begin(), casters.end(), [&](std::size_t i) {
            const Aabb b = bounds(props[i], kG);
            return std::abs(b.max.y - kReturnHeight) < 1e-4f &&
                   std::abs((b.min.x + b.max.x) / 2 - lanePitch(kG) / 2) < 1e-3f;
        });
        EXPECT_TRUE(hasReturn) << t.name << ": the ball return beside the lane casts";
        if (t.indoor) {
            const bool ceilingCasts = std::any_of(casters.begin(), casters.end(), [&](std::size_t i) {
                return bounds(props[i], kG).min.y >= kHallHeight - 1e-3f;
            });
            EXPECT_FALSE(ceilingCasts) << t.name;
        }
    }
}
```

- [ ] **Step 2: Run the build to verify it fails**

Run: `cmake --build build -j"$(nproc)" 2>&1 | grep -m3 error`
Expected: FAIL with `'propsInBox' was not declared in this scope`.

- [ ] **Step 3: Implement `propsInBox`**

`Lighting.h`: add `#include <cstddef>` and `#include <vector>`, then:

```cpp
/// Indices of the props whose world box overlaps `box` (used to pick shadow and reflection casters).
BOWLINGBOX_RENDER_API std::vector<std::size_t> propsInBox(const std::vector<Prop>& props, const SceneGeometry& g,
                                                         const Aabb& box);
```

`Lighting.cpp`:

```cpp
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
```

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build -R test_lighting --output-on-failure`
Expected: PASS, 8 tests.

- [ ] **Step 4: `ShadowMap` and the depth shader**

`libraries/Render/src/ShadowMap.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_SHADOWMAP_H
#define BOWLINGBOX_RENDER_SHADOWMAP_H
#include "GlFramebuffer.h"

namespace Bowlingbox {
namespace Render {

/// The key light's depth map, sampled through sampler2DShadow.
class ShadowMap {
public:
    // Same memory as 2048², shaped like the lane: fitLightFrustum puts the lane along y.
    static constexpr int kWidth = 1024, kHeight = 4096;
    bool create();        // false if the depth framebuffer is incomplete
    void begin() const;   // binds, clears depth, turns on depth clamp and slope-scaled polygon offset
    void end() const;     // turns them off, binds the default framebuffer
    unsigned depthTexture() const { return _fbo.depthTexture(); }

private:
    GlFramebuffer _fbo;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_SHADOWMAP_H
```

`libraries/Render/src/ShadowMap.cpp`:

```cpp
#include "ShadowMap.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {

bool ShadowMap::create() {
    return _fbo.create({kWidth, kHeight, 0, 0, false, true});
}

void ShadowMap::begin() const {
    _fbo.bind();
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_CLAMP);  // casters between the light and the box still cast
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
}

void ShadowMap::end() const {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_DEPTH_CLAMP);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace Render
}  // namespace Bowlingbox
```

`Shaders.h`: add `extern const char* const kDepthFs;  // depth-only pass on kLitVs`. `Shaders.cpp`:

```cpp
const char* const kDepthFs = R"(#version 330 core
void main() {})";
```

`ContextData.h`: add `std::unique_ptr<GlShader> depth;  // shadow pass (full pipeline only)`.

In `enableFullPipeline`, add `Stage {d.depth, shaders::kDepthFs, "depth"}` to the list. That stage builds from `shaders::kLitVs`, not `kPostVs`, so give `Stage` a `const char* vs` field. Pass `shaders::kPostVs` for the three bloom stages and `shaders::kLitVs` for depth, and call `GlShader::build(st.vs, st.fs)`.

Add `ShadowMap.h/.cpp` to the CMake lists.

- [ ] **Step 5: Shadow pass in `SceneRenderer`**

In `Render.cpp`, add `#include "ShadowMap.h"`. In `SceneRenderer::Impl`, add:

```cpp
    std::unique_ptr<ShadowMap> shadow;
    std::vector<std::size_t> shadowCasters;  // indices into props, rebuilt by setTheme
    LightFrustum light;
```

At the end of `setTheme`, still CPU only:

```cpp
        shadowCasters = propsInBox(props, g, shadowBox(g));
        light = fitLightFrustum(current->lightDir, shadowBox(g));
```

In the constructor, after creating `post`:

```cpp
        if (ctx.data().fullPipeline) {
            shadow = std::make_unique<ShadowMap>();
            if (!shadow->create()) {
                std::fprintf(stderr, "render: shadow map incomplete; using the basic path\n");
                ctx.data().fullPipeline = false;
            }
        }
```

Add the member function:

```cpp
    void shadowPass(const SceneDescription& scene) {
        GlShader& s = *ctx.data().depth;
        PassParams pp;
        pp.view = light.view;
        pp.proj = light.projection;
        s.use();
        s.setMat4("uView", pp.view);
        s.setMat4("uProj", pp.proj);
        s.setVec4("uClipPlane", pp.clipPlane);
        shadow->begin();
        drawOpaque(s, pp, scene, &shadowCasters, true);
        shadow->end();
    }
```

In `SceneRenderer::draw`'s full-pipeline branch, replace `p.post->beginScene();` with:

```cpp
    p.shadowPass(scene);
    pp.shadows = true;
    pp.lightViewProj = p.light.viewProjection();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, p.shadow->depthTexture());
    glActiveTexture(GL_TEXTURE0);
    p.post->beginScene();
```

After `p.post->present(...)`, add:

```cpp
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
```

- [ ] **Step 6: Build, test, smoke**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all.

Run the smoke loop with `TASK=task9`.
Expected:
- No stderr output.
- Pins cast soft shadows on the deck, the ball has a shadow under it, and the ball returns shade the gutters.
- The ball's shadow touches the ball where it meets the lane. Its outline is sharp near the foul line and still sharp at the pins: the map has the same resolution along the whole lane.
- The lane isn't covered in a ceiling shadow.
- There's no shadow acne (striped self-shadowing) on the lane or the pins.

If acne shows, raise `glPolygonOffset` to `(3.f, 6.f)`. If shadows detach from the pin bases (peter-panning), lower the shader's `0.0025` slope bias to `0.0015`.

- [ ] **Step 7: Commit**

```bash
git add libraries/Render/src/ShadowMap.h libraries/Render/src/ShadowMap.cpp \
        libraries/Render/include/Bowlingbox/Render/Lighting.h libraries/Render/src/Lighting.cpp \
        libraries/Render/src/Shaders.h libraries/Render/src/Shaders.cpp libraries/Render/src/ContextData.h \
        libraries/Render/src/Render.cpp libraries/Render/test/src/test_Lighting.cpp libraries/Render/CMakeLists.txt
git commit -m "feat(render): key-light shadow map with 3x3 PCF"
```

---

### Task 10: Lane reflection

**Files:**
- Modify: `libraries/Render/include/Bowlingbox/Render/Lighting.h`, `libraries/Render/src/Lighting.cpp` (`reflectionBox`)
- Modify: `libraries/Render/src/Render.cpp`
- Test: `libraries/Render/test/src/test_Lighting.cpp`

**Interfaces:**
- Consumes: `propsInBox` (Task 9), `GlFramebuffer` and `FrameSizes::reflection` (Task 7), the `uReflect`/`uReflection`/`uViewportSize`/`uClipPlane` uniforms (Task 5).
- Produces: `Aabb reflectionBox(const SceneGeometry&)`.

- [ ] **Step 1: Write the failing test**

Append to `test_Lighting.cpp`:

```cpp
TEST(test_lighting, reflection_casters_skip_the_floor_and_the_neighbour_lanes) {
    for (ThemeId id : kThemes) {
        const Theme& t = theme(id);
        const auto props = buildProps(t, kG);
        const auto casters = propsInBox(props, kG, reflectionBox(kG));
        for (std::size_t i : casters) {
            EXPECT_GT(bounds(props[i], kG).max.y, 0.f) << t.name << ": nothing below the lane";
            EXPECT_NE(props[i].material.pattern, Pattern::Wood) << t.name << ": no neighbour lane";
        }
        const bool hasReturn = std::any_of(casters.begin(), casters.end(), [&](std::size_t i) {
            return std::abs(bounds(props[i], kG).max.y - kReturnHeight) < 1e-4f;
        });
        EXPECT_TRUE(hasReturn) << t.name;
    }
}
```

- [ ] **Step 2: Run the build to verify it fails**

Run: `cmake --build build -j"$(nproc)" 2>&1 | grep -m3 error`
Expected: FAIL with `'reflectionBox' was not declared in this scope`.

- [ ] **Step 3: Implement `reflectionBox`**

`Lighting.h`:

```cpp
/// What the lane mirrors: above the lane surface, the lane and gutters plus 0.3 m each side
/// (the ball returns), from the approach to the end of the pit.
BOWLINGBOX_RENDER_API Aabb reflectionBox(const SceneGeometry& g);
```

`Lighting.cpp`:

```cpp
Aabb reflectionBox(const SceneGeometry& g) {
    const float x = g.laneHalfWidth + g.gutterWidth + 0.3f;
    return {{-x, 0.f, -g.approach}, {x, kShadowTopY, g.laneLength + kPitLength}};
}
```

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build -R test_lighting --output-on-failure`
Expected: PASS, 9 tests.

- [ ] **Step 4: Reflection pass**

In `SceneRenderer::Impl`, add:

```cpp
    GlFramebuffer reflection;
    glm::ivec2 reflectionSize {0};
    std::vector<std::size_t> reflectionCasters;  // rebuilt by setTheme
```

At the end of `setTheme`: `reflectionCasters = propsInBox(props, g, reflectionBox(g));`.

Add the member functions:

```cpp
    bool ensureReflection(glm::ivec2 size) {
        if (size == reflectionSize)
            return true;
        reflectionSize = size;
        if (reflection.create({size.x, size.y, 0, GL_RGBA16F, true, false}))
            return true;
        reflectionSize = glm::ivec2(0);
        return false;
    }

    /// Ball, pins and nearby props mirrored in the lane plane (y = 0). Alpha marks covered pixels.
    void reflectionPass(const PassParams& camera, const SceneDescription& scene) {
        PassParams pp = camera;
        pp.view = camera.view * glm::scale(glm::mat4(1.f), glm::vec3(1.f, -1.f, 1.f));
        pp.eye = {camera.eye.x, -camera.eye.y, camera.eye.z};
        pp.clipPlane = {0.f, 1.f, 0.f, 0.f};  // keep y >= 0
        pp.toneMap = false;
        pp.shadows = false;
        pp.reflection = false;
        reflection.bind();
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glEnable(GL_CLIP_DISTANCE0);
        GlShader& s = *ctx.data().lit;
        setFrameUniforms(s, pp);
        drawOpaque(s, pp, scene, &reflectionCasters, false);
        glDisable(GL_CLIP_DISTANCE0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
```

In `SceneRenderer::draw`, extend the size check:

```cpp
    if (d.fullPipeline && (!p.post->ensureSize(sizes, d.samples) || !p.ensureReflection(sizes.reflection))) {
```

In the full-pipeline branch, after the shadow block and before `p.post->beginScene();`:

```cpp
    p.reflectionPass(pp, scene);
    pp.reflection = true;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, p.reflection.colorTexture());
    glActiveTexture(GL_TEXTURE0);
```

Note that `pp.shadows` is already `true` when this copy is made. `reflectionPass` turns it off in its own copy.

After `present`, unbind unit 2 the same way as unit 1.

- [ ] **Step 5: Build, test, smoke**

Run: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`
Expected: PASS for all.

Run the smoke loop with `TASK=task10`. Also take one shot after rolling the ball halfway: start the game, throw with the default aim, and run `import -window root` after about 1.5 s.

Expected:
- No stderr output.
- The pins show a faint, slightly blurred mirror image on the lane in front of them, and it's strongest at grazing angles.
- The rolling ball is mirrored under itself.
- The lane where nothing is reflected looks exactly as in `task9-*.png`, with no darkening.
- Nothing from below the lane (gutter undersides) appears in the reflection.

- [ ] **Step 6: Commit**

```bash
git add libraries/Render/include/Bowlingbox/Render/Lighting.h libraries/Render/src/Lighting.cpp \
        libraries/Render/src/Render.cpp libraries/Render/test/src/test_Lighting.cpp
git commit -m "feat(render): planar reflection on the oiled lane"
```

---

### Task 11: GPU timing, docs, and final verification

**Files:**
- Create: `libraries/Render/src/GpuTimer.h`, `libraries/Render/src/GpuTimer.cpp`
- Modify: `libraries/Render/src/Render.cpp`, `libraries/Render/CMakeLists.txt`
- Modify: `docs/MANUAL.md`, `docs/ARCHITECTURE.md`

**Interfaces:**
- Consumes: `RenderOptions::stats` (Task 7), the passes in `SceneRenderer::draw` (Tasks 7–10).
- Produces: `class GpuTimer { enum Pass { Shadow, Reflection, Main, Post, kPassCount }; void begin(Pass); void end(); void endFrame(); }`.

- [ ] **Step 1: `GpuTimer`**

`libraries/Render/src/GpuTimer.h`:

```cpp
#ifndef BOWLINGBOX_RENDER_GPUTIMER_H
#define BOWLINGBOX_RENDER_GPUTIMER_H
#include <array>
#include <chrono>
#include <cstddef>

namespace Bowlingbox {
namespace Render {

/// GL_TIME_ELAPSED per pass, BOWLINGBOX_RENDER_STATS=1. The queries rotate through kFrames sets.
/// A result is read only when GL_QUERY_RESULT_AVAILABLE says it's ready, so the CPU never waits
/// on the GPU (drivers often queue 2-3 frames). The averages go to stderr about once a second.
class GpuTimer {
public:
    enum Pass { Shadow, Reflection, Main, Post, kPassCount };
    static constexpr std::size_t kFrames = 3;
    GpuTimer();
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    void begin(Pass pass);
    void end();
    void endFrame();

private:
    void collect(std::size_t set);  // adds every finished query of `set` to the sums

    std::array<std::array<unsigned, kPassCount>, kFrames> _queries {};
    std::array<std::array<bool, kPassCount>, kFrames> _issued {};  // begun, result not read yet
    std::array<double, kPassCount> _sumMs {};
    std::array<int, kPassCount> _samples {};  // results summed per pass since the last print
    std::size_t _write = 0;
    bool _open = false;  // begin() started a query that end() must close
    int _frames = 0;
    std::chrono::steady_clock::time_point _last = std::chrono::steady_clock::now();
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GPUTIMER_H
```

`libraries/Render/src/GpuTimer.cpp`:

```cpp
#include "GpuTimer.h"
#include <glad/gl.h>
#include <cstdio>

namespace Bowlingbox {
namespace Render {

GpuTimer::GpuTimer() {
    for (auto& set : _queries)
        glGenQueries(kPassCount, set.data());
}
GpuTimer::~GpuTimer() {
    for (auto& set : _queries)
        glDeleteQueries(kPassCount, set.data());
}

void GpuTimer::begin(Pass pass) {
    const auto p = static_cast<std::size_t>(pass);
    if (_issued[_write][p])  // the GPU is kFrames behind: drop this measurement rather than wait
        return;
    glBeginQuery(GL_TIME_ELAPSED, _queries[_write][p]);
    _issued[_write][p] = true;
    _open = true;
}
void GpuTimer::end() {
    if (_open)
        glEndQuery(GL_TIME_ELAPSED);
    _open = false;
}

void GpuTimer::collect(std::size_t set) {
    for (std::size_t p = 0; p < kPassCount; ++p) {
        if (!_issued[set][p])
            continue;
        GLint ready = GL_FALSE;
        glGetQueryObjectiv(_queries[set][p], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (!ready)
            continue;
        GLuint64 ns = 0;
        glGetQueryObjectui64v(_queries[set][p], GL_QUERY_RESULT, &ns);
        _sumMs[p] += static_cast<double>(ns) / 1e6;
        ++_samples[p];
        _issued[set][p] = false;
    }
}

void GpuTimer::endFrame() {
    for (std::size_t set = 0; set < kFrames; ++set)
        if (set != _write)
            collect(set);
    _write = (_write + 1) % kFrames;
    ++_frames;
    const auto now = std::chrono::steady_clock::now();
    if (now - _last >= std::chrono::seconds(1)) {
        const auto avg = [this](Pass p) { return _samples[p] > 0 ? _sumMs[p] / _samples[p] : 0.0; };
        std::fprintf(stderr, "render: shadow %.2f ms, reflection %.2f ms, main %.2f ms, post %.2f ms (GPU, %d frames)\n",
                     avg(Shadow), avg(Reflection), avg(Main), avg(Post), _frames);
        _sumMs = {};
        _samples = {};
        _frames = 0;
        _last = now;
    }
}

}  // namespace Render
}  // namespace Bowlingbox
```

Add both files to the CMake lists.

- [ ] **Step 2: Time the passes**

In `SceneRenderer::Impl`, add `std::unique_ptr<GpuTimer> timer;` and `#include "GpuTimer.h"`. At the end of the constructor:

```cpp
        if (ctx.data().options.stats)
            timer = std::make_unique<GpuTimer>();
```

Add a small helper to `Impl`:

```cpp
    template <typename F>
    void timed(GpuTimer::Pass pass, F&& f) {
        if (timer) timer->begin(pass);
        f();
        if (timer) timer->end();
    }
```

In `SceneRenderer::draw`, wrap each call:
- `p.timed(GpuTimer::Shadow, [&] { p.shadowPass(scene); });`
- `p.timed(GpuTimer::Reflection, [&] { p.reflectionPass(pp, scene); });`
- `p.timed(GpuTimer::Main, [&] { p.post->beginScene(); p.mainPass(pp, scene); });` (on the basic path, just `p.mainPass`)
- `p.timed(GpuTimer::Post, [&] { p.post->present(...); });`

Call `if (p.timer) p.timer->endFrame();` as the last statement of *both* branches.

Build: `cmake --build build -j"$(nproc)"`. Then run:

```bash
BOWLINGBOX_RENDER_STATS=1 timeout 6 build/bin/BowlingGL --theme=neon
```

Expected: one `render: shadow … ms, reflection … ms, main … ms, post … ms (GPU, N frames)` line per second, with N around 60.

- [ ] **Step 3: Docs**

In `docs/MANUAL.md`, add a section after the one that lists BowlingGL's command-line options:

```markdown
### Graphics

BowlingGL renders with soft shadows, a reflecting lane, glow (bloom) and a filmic tone curve.
If your graphics card can't do this, it falls back to a simpler look by itself, and prints the reason
to the terminal. Two environment variables help when something looks wrong:

| Variable | Effect |
|---|---|
| `BOWLINGBOX_RENDER=basic` | Use the simpler look: same lighting and materials, no shadows, reflection or glow. |
| `BOWLINGBOX_RENDER_STATS=1` | Print the GPU time of each render pass once a second. |
```

In `docs/ARCHITECTURE.md`, in the Render library section, add:

```markdown
**Frame pipeline (full path).** `SceneRenderer::draw` runs, in order: a 1024×4096 shadow map from the key
light (fitted to `shadowBox`), a half-resolution mirror of the scene in the lane plane, the HDR main
pass (RGBA16F, 4× MSAA) and `PostChain::present` (weighted MSAA resolve, 4-level bloom, ACES tone map, sRGB) into
the 3D band. The *basic path* draws the main pass straight into the window, with tone mapping done in
the lit shader. It runs when `Context::create` finds no usable RGBA16F/MSAA target, a full-pipeline
shader fails, or `BOWLINGBOX_RENDER=basic` is set.

**Testable without GL:** `Lighting.h` (sRGB, `fitLightFrustum`, `shadowBox`, `reflectionBox`,
`propsInBox`), `RenderOptions.h` (`frameSizes`, `renderOptions`) and `FontAtlas.h` (the embedded Rubik
font, baked with stb_truetype).
```

- [ ] **Step 4: Final verification**

1. Unit tests: `cmake --build build -j"$(nproc)" && ctest --test-dir build --label-regex unit --output-on-failure`. Expected: PASS for all.
2. Smoke loop with `TASK=final`. Expected: no stderr output. Compare each `final-<theme>.png` with the pre-work look by building `main` in `build/worktrees/before` and running the same loop there with `TASK=before`. These pairs go in the PR description.
3. Basic path for every theme: `BOWLINGBOX_RENDER=basic` with each `--theme`. Expected: only the `render: basic path` line on stderr, and the scene is lit and toned like `final-*`, minus the shadows, reflection and glow.
4. Frame time: on an integrated GPU at 1920×1080 (maximise the window), run `BOWLINGBOX_RENDER_STATS=1 build/bin/BowlingGL` with a throw in progress. Expected: the sum of the four passes stays under 10 ms. Record the numbers and the GPU name (`glxinfo -B | grep renderer`) for the PR.
5. Windows: the CI workflow builds the static Windows package on the PR. Its run and the installer stage check must pass.

- [ ] **Step 5: Commit**

```bash
git add libraries/Render/src/GpuTimer.h libraries/Render/src/GpuTimer.cpp libraries/Render/src/Render.cpp \
        libraries/Render/CMakeLists.txt docs/MANUAL.md docs/ARCHITECTURE.md
git commit -m "feat(render): per-pass GPU timing; document the graphics pipeline"
```
