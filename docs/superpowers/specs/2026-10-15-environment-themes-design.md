# Environment Themes — Design

Date: 2026-10-15. Branch: `feature/environment`, based on `refactor/drop-qt` (PR #4).

## Goal

BowlingGL draws a surrounding environment instead of a black void, in the spirit of the splash screens
(`docs/Bowlingbox_ Neon Strike.png`). The environment is chosen from built-in presets and can be switched
while playing.

## Decisions

- Three presets: **Neon Hall** (default, like the splash), **Classic Alley** (warm indoor), **Outdoor**
  (terrace under the sky).
- Presets are C++ data. No theme files.
- Look: flat-coloured shapes, glowing (emissive) materials, distance fog, a background gradient and a
  procedural board pattern on the lane. No image textures, no bloom, no reflections.
- Render owns the theme data, the presets and a pure, GL-free prop layout function. BowlingGL only picks
  the theme.
- The decor is visual only. Physics, rules and the Alley library do not change.

## Coordinates

As today: x across the lane, y up, z down the lane. The lane surface is y = 0, from z = -approach (-2)
to z = laneLength (18.3). The gutters reach x = ±outerX with outerX = laneHalfWidth + gutterWidth (0.765).
The follow camera stays at x = 0, y = 1.8, z in [-3.5, 12.8].

Two volumes must stay free of props (enforced by tests):

| Volume | x | y | z |
|---|---|---|---|
| Play corridor (lane, gutters, approach, pins, flight of the ball) | [-0.82, 0.82] | [-0.14, 2.2] | [-4.0, 19.0] |
| Camera box | [-0.6, 0.6] | [1.4, 2.4] | [-4.0, 13.5] |

The play corridor reaches y = 2.2, so it also covers the line of sight from the camera to the pins.

## Render API — `Bowlingbox/Render/Environment.h` (new)

```cpp
namespace Bowlingbox::Render {

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
    std::string_view name;          // "neon", "classic", "outdoor" (command line and menu)
    std::string_view label;         // "Neon Hall", "Classic Alley", "Outdoor"
    glm::vec3 backgroundTop, backgroundBottom;
    glm::vec3 fogColor;
    float fogStart, fogEnd;         // metres from the camera, fogStart < fogEnd
    glm::vec3 lightDir;
    float ambient;                  // 0..1, share of the colour that is lit regardless of the light
    Material lane, gutter, ball, pinBody, pinBand;
    bool indoor;                    // walls + ceiling; otherwise open ground
    int neighbourLanesPerSide;      // 0..3
    Material floor, wall, ceiling, trim, accentA, accentB, screen;
    PropSet props;
};

enum class Shape { Box, Cylinder, Cone, Pin };

/// One decor object. Box/Cylinder/Cone are unit shapes (1 m wide, 1 m tall, centred at the
/// origin) scaled by `transform`; Pin is the real pin mesh, base at y = -pinComHeight.
struct Prop {
    Shape shape;
    Transform transform;
    Material material;
    bool pinBand = false;  // draw the theme's pinBand on the neck (Pin only)
};

struct Aabb { glm::vec3 min, max; };

BOWLINGBOX_RENDER_API const Theme& theme(ThemeId id);
BOWLINGBOX_RENDER_API std::optional<ThemeId> themeFromName(std::string_view name);
BOWLINGBOX_RENDER_API ThemeId nextTheme(ThemeId id);  // Neon -> Classic -> Outdoor -> Neon
BOWLINGBOX_RENDER_API std::vector<Prop> buildProps(const Theme& theme, const SceneGeometry& g);
BOWLINGBOX_RENDER_API Aabb bounds(const Prop& prop, const SceneGeometry& g);  // world-space box
}
```

`Transform` moves from `Render.h` to `Environment.h`, and `Render.h` includes it. `SceneGeometry`
gets `pinSpacing` and `headPinZ`, which `SceneAdapter::geometry` copies from `Alley::kLane`.

## Layout (`buildProps`)

All positions are computed from `SceneGeometry`. No randomness: tree positions come from a fixed list.

- **Neighbour lanes:** lane centres are at x = ±1.83·k for k = 1…n.
  - Each has a lane box, two gutters and a rack of 10 static pins in the same triangle as
    `BulletWorld::pinPositions()`, built from `pinSpacing` and `headPinZ`.
  - A low ball-return box (0.14 m wide, 0.3 m tall, so it spans x = 0.845–0.985 next to the main lane) sits halfway between neighbouring lane centres.
- **Indoor:**
  - Hall half-width W = 1.83·(n + 0.5) + 0.6 (n = neighbourLanesPerSide).
  - Height 4.5 m; z runs from -8 to 22.
  - Floor box with its top at y = -0.15.
  - Side walls, back wall at z = 22, front wall at z = -8, ceiling.
  - A masking unit hides the pit: a box over all lanes at z = 19.6–19.9, y = 0.9–2.4.
- **Pillars:** trim-coloured boxes (0.4 × 4.5 × 0.4), placed against the side walls every 5 m.
- **Neon strips:** emissive boxes 0.06 m thick, running along both side walls and the masking unit.
  accentA (cyan) runs at y = 1.0, accentB (purple) at y = 3.4.
- **Ceiling lights:** emissive panels (1.2 × 0.05 × 0.3), 0.2 m below the ceiling, in two rows every
  4 m.
- **Wall screens:** on each side wall at z = 6 and z = 12, y centre 2.3. Each is a dark frame box, a
  `screen` emissive panel (2.4 × 1.4), and a flattened emissive white `Pin` icon with the band.
- **Score monitor:** a box hanging above the masking unit (3 × 0.9 × 0.1, y centre 2.9) with an
  emissive `screen` face.
- **Outdoor:**
  - A grass ground box with its top at y = -0.15, x ±40, z from -20 to 80.
- **Fences:** wooden posts every 2 m, with two rails at 0.4 m and 0.8 m, at x = ±(outer neighbour lane
  edge + 1.0).
- **Trees:** a trunk cylinder (accentB) plus a cone canopy (accentA).
  - Positions come from a fixed list of about 16 (x, z, scale) entries.
  - Every tree has |x| ≥ fence x + 2 and z in [-10, 60].

Presets:

| | Neon Hall | Classic Alley | Outdoor |
|---|---|---|---|
| background top / bottom | (0.01,0.02,0.08) / (0.04,0.05,0.18) | (0.93,0.88,0.78) / (0.80,0.72,0.60) | (0.35,0.60,0.95) / (0.80,0.90,1.00) |
| fog colour, start–end | (0.03,0.04,0.14), 12–35 | (0.85,0.80,0.70), 20–45 | (0.80,0.88,0.98), 25–60 |
| light dir, ambient | (-0.3,-1,0.4), 0.40 | (-0.2,-1,0.3), 0.55 | (-0.4,-1,0.3), 0.45 |
| lane / gutter | (0.831,0.639,0.451) / (0.008,0.055,0.22) | (0.86,0.70,0.50) / (0.25,0.25,0.28) | (0.831,0.639,0.451) / (0.30,0.30,0.32) |
| ball, pin body, pin band | today's colours (blue ball, white pin, red band) | same | same |
| indoor, neighbour lanes per side | yes, 2 | yes, 2 | no, 1 |
| floor / wall / ceiling | (0.02,0.03,0.10) / (0.03,0.05,0.20) / (0.02,0.03,0.12) | (0.45,0.30,0.18) / (0.88,0.82,0.70) / (0.95,0.95,0.92) | grass (0.25,0.55,0.20) / – / – |
| trim | (0.05,0.08,0.30) | (0.50,0.32,0.18) | fence wood (0.55,0.40,0.25) |
| accentA / accentB | cyan (0,0.9,1) emissive / purple (0.62,0.2,1) emissive | warm white (1,0.96,0.85) emissive / – | foliage (0.15,0.45,0.15) / trunk (0.40,0.26,0.13) |
| screen | blue (0.1,0.3,0.9) emissive | dark blue (0.05,0.1,0.3) emissive | – |
| props | pillars, neonStrips, ceilingLights, wallScreens | ceilingLights (accentA), scoreMonitor | fences, trees |

## Rendering

- **Lit shader:**
  - New uniforms: `uEmissive`, `uAmbient`, `uFogColor`, `uFogStart`, `uFogEnd`, `uStripes`.
  - The vertex shader passes the view-space distance and the local x.
  - Lit colour = base · (ambient + (1 − ambient) · diffuse). Emissive colour = base.
  - Fog factor f = clamp((d − start) / (end − start), 0, 1). Lit surfaces get the full factor, emissive
    surfaces f/2. The colour is then `mix(colour, fogColor, f)`.
  - Lane stripes (lane only): boards 0.025 m wide alternate brightness by ±6 % across local x.
- **Background:** a new `sky` shader draws one full-viewport triangle with depth testing and depth
  writes off, from `backgroundBottom` at the bottom of the 3D viewport to `backgroundTop` at the top.
  `ContextData` owns it next to `lit` and `flat`.
- **`SceneRenderer`:**
  - The constructor takes a `ThemeId` (default `NeonHall`).
  - `setTheme(ThemeId)` stores the theme and rebuilds the prop list.
  - It keeps one mesh per shape: a 1 m box, a radius-0.5 height-1 cylinder, a cone built with
    `latheData`, and the pin.
  - Draw order:
    1. background
    2. props
    3. lane
    4. gutters and low side walls
    5. pins
    6. ball
    7. aim guide

  The lane, gutter, ball and pin colours come from the theme.
- **Far plane:** the camera far plane goes from 100 m to 150 m, so the Outdoor ground at z = 80 is
  not cut off.

## BowlingGL

- **Command:** `Command::Type::CycleTheme`.
  - `InputMapper::keyDown` maps `T` and `t` to it in every phase.
  - `LaneCommands::execute` treats it like `Quit`: it succeeds and does nothing.
  - `BowlingCanvas::apply` handles it with `setTheme(nextTheme(current))`.
- **Canvas:**
  - `setTheme(ThemeId)` and `theme()`.
  - An `onThemeChanged` callback, which `BowlingFrame` uses to check the matching menu item.
  - The theme reaches the renderer once the GL context exists. If `setTheme` is called earlier, the
    choice is stored and applied when the renderer is created.
- **Menu:** **View → Theme** with three radio items: Neon Hall, Classic Alley, Outdoor.
- **Command line:**
  - `BowlingApp` overrides `OnInitCmdLine` and `OnCmdLineParsed` to accept `--theme=<name>`.
  - A pure helper in `bowlinggl_core`, `ThemeOption::parse(std::string_view)`, returns
    `{ThemeId id, bool known}`. An unknown name gives `{NeonHall, false}`.
  - When `known` is false, the app writes `unknown theme '<x>'; use neon, classic or outdoor` to
    stderr and starts with Neon Hall.
- **Help:** `HudText::controlsHelp()` gets the line `T: change the scenery`. The README controls table
  gets a row.

## Tests

`test_bowlingbox_render` (no GL):

- `theme(id).id == id` for every id.
- `themeFromName(theme(id).name) == id`, and `themeFromName("x")` is empty.
- `nextTheme` cycles through all three themes and returns to the start.
- `fogStart < fogEnd`, and `ambient` is in [0, 1].
- For each theme, no prop's `bounds` intersects the play corridor or the camera box.
- Indoor themes: some prop reaches x ≤ −W and some reaches x ≥ W (side walls), some has min.y ≥ 4.4 (ceiling)
  and some has min.z ≥ 21.9 (back wall). Outdoor: no prop has max.y > 6.
- Pin props: count == 10 · 2 · neighbourLanesPerSide, all with `pinBand`. Each pin stands on y = 0, and
  lies within its neighbour lane's x range and between headPinZ − 0.1 and laneLength.
- `bounds` of a rotated box encloses all eight transformed corners.

`test_bowlinggl`:

- `T` maps to `CycleTheme` in every phase, and `t` does too.
- `LaneCommands::execute(CycleTheme)` succeeds and leaves the snapshot unchanged.
- `ThemeOption::parse` accepts `neon`, `classic` and `outdoor`, and returns `{NeonHall, false}` for
  `""` and `"space"`.
- `controlsHelp()` mentions `T`.

**By hand** (added to the README checklist):
- Start with each `--theme`.
- Press `T` through all three themes and check that the menu follows.
- Check that a throw is always visible.
- Check that the frame rate stays smooth.
- Take one screenshot per theme for the README.

## Docs

- `applications/bowlingGame/README.md`: a themes section, the `T` key, `--theme`, the checklist entries
  and screenshots.
- `docs/ARCHITECTURE.md`: the Environment module and the new shader inputs.
- `docs/MANUAL.md`: `--theme` and `T`.

## Out of scope

Image textures, bloom, reflections, theme files, remembering the theme between runs, animated decor,
and any change to Alley, Score or physics.
