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
- Render owns the theme data, the presets, the theme names and a pure, GL-free prop layout function.
  BowlingGL only picks the theme.
- The decor is visual only. Physics, rules and the Alley library do not change.

**Expected visible changes in Neon Hall:**
- The ambient weight goes from 0.35 to 0.40.
- The lane gets board stripes.
- Fog is added.
- The black void becomes the hall.

## Coordinates

As today: x across the lane, y up, z down the lane.

- **Lane:** the surface is y = 0, from z = -approach (-2) to z = laneLength (18.3).
- **Gutters:** they reach x = ±outerX, with outerX = laneHalfWidth + gutterWidth (0.765).
- **Pit:** it runs from z = laneLength to laneLength + 2.4 (`kPitLength` in `BulletWorld.cpp`), and its
  floor is at about y = -0.25.
- **Follow camera:** it stays at x = 0, y = 1.8, z in [-3.5, 12.8].

Three volumes must stay free of props (enforced by tests):

| Volume | x | y | z |
|---|---|---|---|
| Play corridor (lane, gutters, approach, pins, ball, flying pins) | [-0.82, 0.82] | [-0.14, 0.8] | [-4.0, 19.0] |
| Sight corridor (camera to pins) | [-0.82, 0.82] | [-0.14, 2.2] | [-4.0, laneLength] |
| Camera box | [-0.6, 0.6] | [1.4, 2.4] | [-4.0, 13.5] |

The play corridor's x range covers the ball at full aim offset (kMaxAimOffset + ballRadius = 0.809).

## Render API

### `Bowlingbox/Render/Environment.h` (new)

`Transform` and `SceneGeometry` move here from `Render.h`, and `Render.h` includes `Environment.h`.
The include goes one way only.

`SceneGeometry` gets `pinSpacing` and `headPinZ`, which `SceneAdapter::geometry` copies from
`Alley::kLane`.

```cpp
namespace Bowlingbox::Render {

struct Transform { /* unchanged */ };
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
    std::string_view name;          // "neon", "classic", "outdoor" (command line); the only name table
    std::string_view label;         // "Neon Hall", "Classic Alley", "Outdoor" (menu)
    glm::vec3 backgroundTop, backgroundBottom;
    glm::vec3 fogColor;
    float fogStart, fogEnd;         // metres from the camera, fogStart < fogEnd
    glm::vec3 lightDir;             // direction the light travels (points away from the light)
    float ambient;                  // 0..1
    Material lane, gutter, ball, pinBody, pinBand;
    bool indoor;                    // walls + ceiling; otherwise open ground
    int neighbourLanesPerSide;      // 0..3
    Material floor, wall, ceiling, trim, accentA, accentB, screen;
    PropSet props;
};

enum class Shape { Box, Cylinder, Cone, Pin };

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
BOWLINGBOX_RENDER_API float lanePitch(const SceneGeometry& g);  // 2·outerX + kReturnGap
BOWLINGBOX_RENDER_API std::vector<Prop> buildProps(const Theme& theme, const SceneGeometry& g);
/// The mesh drawn for a shape, and its local box (min/max over the mesh vertices).
BOWLINGBOX_RENDER_API MeshData shapeMesh(Shape shape, const SceneGeometry& g);
BOWLINGBOX_RENDER_API Aabb localBounds(Shape shape, const SceneGeometry& g);
/// World box: localBounds' eight corners through transform.matrix().
BOWLINGBOX_RENDER_API Aabb bounds(const Prop& prop, const SceneGeometry& g);
}
```

**Shapes:** the mesh and the bounds come from the same place.

| Shape | `shapeMesh` | `localBounds` |
|---|---|---|
| Box | `boxData(0.5, 0.5, 0.5)` | (-0.5, -0.5, -0.5) … (0.5, 0.5, 0.5) |
| Cylinder | `cylinderData(0.5, 1.0, 24)` | (-0.5, -0.5, -0.5) … (0.5, 0.5, 0.5) |
| Cone | `latheData({{0.5, -0.5}, {0.0, 0.5}}, 24)` | (-0.5, -0.5, -0.5) … (0.5, 0.5, 0.5) |
| Pin | `pinData(pinHeight, pinMaxRadius, pinComHeight)` | (-pinMaxRadius, -pinComHeight, -pinMaxRadius) … (pinMaxRadius, pinHeight - pinComHeight, pinMaxRadius) |

So a Box prop's `transform.scale` is its full size in metres.

## Layout (`buildProps`)

All positions are derived from `SceneGeometry` and the constants below. There is no randomness.

**Constants:**
- `kReturnGap = 0.30`
- `kHallHeight = 4.5`
- `kHallFrontZ = -8`
- `kHallBackZ = 22`
- `kWallThickness = 0.2`
- `pitch = lanePitch(g)`, which is 1.83 for today's lane
- `n = neighbourLanesPerSide`

**Neighbour lanes** (centres at x = ±pitch·k for k = 1…n):
- Each one is a copy of the main lane:
  - A lane box spanning z = -approach … laneLength, including its approach.
  - Two gutters.
  - 10 static pins in `BulletWorld::pinPositions()`'s triangle (from `pinSpacing` and `headPinZ`). Their
    origins sit at y = pinComHeight, so the bases rest on y = 0. All of them have `pinBand`.
- Neighbour lanes get no low side walls.
- **Ball returns:** one box (0.14 × 0.3 × (laneLength + approach)) in the middle of every gap between
  adjacent lanes, base on y = 0, gutter material.
  - Next to the main lane it spans x = outerX + 0.08 … outerX + 0.22 (0.845 … 0.985), which is clear of
    the corridors.

**Indoor:**
- Hall half-width W = pitch·(n + 0.5) + 0.6.
- **Inner faces:**
  - side walls at x = ±W
  - ceiling at y = 4.5
  - back wall at z = 22
  - front wall at z = -8

  Each of these is 0.2 m thick, on the outside of its face.
- **Floor:** boxes with their top at y = -0.15, covering the hall except the main pit.
- **Main pit:** a dark box (gutter material) with its top at y = -0.25, covering
  x = ±(outerX + 0.12) and z = laneLength … laneLength + 2.4.
- **Masking unit:** hides the pinsetter above the deck. It is a box over all lanes (x = ±W), spanning
  z = laneLength + 0.05 … laneLength + 0.35 and y = 0.9 … 2.4, in the trim material. It sits beyond the
  sight corridor's z and above the play corridor's y.

**Optional props:**
- **Pillars:** trim boxes (0.4 × kHallHeight × 0.4) against both side walls, at z = -6, -1, 4, 9, 14, 19.
- **Neon strips:** emissive boxes 0.06 m thick.
  - Each side wall has two: accentA (cyan) at y = 1.0 and accentB (purple) at y = 3.4, both spanning
    z = -8 … 22.
  - The masking unit has one: accentA along its lower front edge, at y = 0.9, centred on the front face
    (z = laneLength + 0.05, so it spans z = 18.32 … 18.38 and stays outside the sight corridor).
- **Ceiling lights:** accentA emissive panels (1.2 × 0.05 × 0.3), 0.2 m below the ceiling, at
  x = ±pitch/2 and z = -6, -2, …, 18 (every 4 m).
- **Wall screens:** one on each side wall at z = 6 and at z = 12, y centre 2.3. Sizes below are (along
  the wall, height, thickness). On a side wall, "along" is z and "thickness" is x. Every part is set
  against the wall's inner face, layered towards the hall. Each screen has three parts:
  - a trim frame box (2.6 × 1.6 × 0.08)
  - a `screen` emissive panel (2.4 × 1.4 × 0.02) in front of the frame
  - a Pin icon in front of the panel: scale (0.2, 3, 3) after a 90° turn about y (so it is flat
    against the wall), origin y = 2.3 − 0.15, white emissive, with `pinBand`

  That is 4 icons in total.
- **Score monitor:** a trim box (3 × 0.9 × 0.1) centred at x = 0, y = 2.9, z = laneLength + 0.2. On
  its front face sits a `screen` emissive panel (2.8 × 0.7 × 0.02).

**Outdoor:**
- **Ground:** grass boxes with their top at y = -0.15, x = ±40, z = -20 … 80, leaving out the main pit.
- **Main pit:** the same pit box as indoors.
- **Fences:** along x = ±F, with F = pitch·n + outerX + 1.0 (3.595 for n = 1).
  - Posts are trim boxes (0.1 × 1.0 × 0.1, base on y = -0.15) every 2 m from z = -6 to z = 30 (19 per
    side).
  - Two rails are trim boxes (0.05 × 0.08 × 36), centred at y = 0.4 and y = 0.8.
- **Trees:** a trunk Cylinder (accentB) of radius 0.15·s and height 1.5·s, base on y = -0.15, with a
  Cone canopy (accentA) of radius 1.2·s and height 2.5·s on top.
  - The tallest tree reaches y = -0.15 + 4·1.6 = 6.25.
  - The 16 trees sit at (±x, z, s):

| x | z | s |
|---|---|---|
| 6.5 | -4 | 1.0 |
| 8.0 | 4 | 1.3 |
| 6.0 | 12 | 0.9 |
| 9.5 | 18 | 1.4 |
| 7.0 | 26 | 1.1 |
| 11.0 | 34 | 1.5 |
| 8.5 | 44 | 1.2 |
| 12.0 | 55 | 1.6 |

## Presets

| | Neon Hall | Classic Alley | Outdoor |
|---|---|---|---|
| background top / bottom | (0.01,0.02,0.08) / (0.04,0.05,0.18) | (0.93,0.88,0.78) / (0.80,0.72,0.60) | (0.35,0.60,0.95) / (0.80,0.90,1.00) |
| fog colour, start–end | (0.03,0.04,0.14), 12–35 | (0.85,0.80,0.70), 20–45 | (0.80,0.88,0.98), 25–60 |
| light dir, ambient | (-0.3,-1,0.4), 0.40 | (-0.2,-1,0.3), 0.55 | (-0.4,-1,0.3), 0.45 |
| lane / gutter | (0.831,0.639,0.451) / (0.008,0.055,0.22) | (0.86,0.70,0.50) / (0.25,0.25,0.28) | (0.831,0.639,0.451) / (0.30,0.30,0.32) |
| ball / pin body / pin band | (0.098,0.365,0.925) / (0.941,0.941,0.941) / (0.902,0,0) | same | same |
| indoor, neighbour lanes per side | yes, 2 | yes, 2 | no, 1 |
| floor / wall / ceiling | (0.02,0.03,0.10) / (0.03,0.05,0.20) / (0.02,0.03,0.12) | (0.45,0.30,0.18) / (0.88,0.82,0.70) / (0.95,0.95,0.92) | grass (0.25,0.55,0.20) / unused (0.5,0.5,0.5) / unused (0.5,0.5,0.5) |
| trim | (0.05,0.08,0.30) | (0.50,0.32,0.18) | fence wood (0.55,0.40,0.25) |
| accentA / accentB | cyan (0,0.9,1) emissive / purple (0.62,0.2,1) emissive | warm white (1,0.96,0.85) emissive / unused (0.3,0.3,0.3) | foliage (0.15,0.45,0.15) / trunk (0.40,0.26,0.13) |
| screen | blue (0.1,0.3,0.9) emissive | dark blue (0.05,0.1,0.3) emissive | unused (0.1,0.1,0.1) |
| props | pillars, neonStrips, ceilingLights, wallScreens | ceilingLights, scoreMonitor | fences, trees |

## Rendering

**Lit shader.** It keeps all of today's inputs and outputs, and adds fog, emissive and stripes:

```glsl
// vertex: as today, plus
out float vViewDist; out float vLocalX;
vViewDist = length((uView * uModel * vec4(aPos, 1.0)).xyz);
vLocalX = aPos.x;

// fragment
uniform vec4 uColor; uniform vec4 uBandColor; uniform vec2 uBandRange; uniform vec3 uLightDir;
uniform float uAmbient; uniform bool uEmissive; uniform bool uFog; uniform bool uStripes;
uniform vec3 uFogColor; uniform float uFogStart; uniform float uFogEnd;
void main() {
    vec4 base = (vLocalY >= uBandRange.x && vLocalY <= uBandRange.y) ? uBandColor : uColor;
    if (uStripes)  // boards 0.025 m wide, alternating ±6 %
        base.rgb *= mod(floor(vLocalX / 0.025), 2.0) < 1.0 ? 1.06 : 0.94;
    vec3 rgb;
    if (uEmissive) {
        rgb = base.rgb;
    } else {
        float diffuse = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);  // today's convention
        rgb = base.rgb * (uAmbient + (1.0 - uAmbient) * diffuse);
    }
    if (uFog) {
        float f = clamp((vViewDist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        rgb = mix(rgb, uFogColor, uEmissive ? 0.5 * f : f);
    }
    FragColor = vec4(rgb, base.a);  // alpha passes through: the aim guide stays at 0.40
}
```

- **Pins:** the band (`uBandRange`, `uBandColor`) works as today. A `Prop` with `pinBand` uses the
  theme's `pinBand` colour and today's neck range.
- **Fog:** on for everything except the aim guide, which keeps its 0.40 alpha and stays unfogged.
- **Stripes:** on only for the main and neighbour lane boxes.
- **Background:** a new `sky` shader draws one full-viewport triangle with depth testing and depth
  writes off, from `backgroundBottom` at the bottom of the 3D viewport to `backgroundTop` at the top.
  `ContextData` owns it next to `lit` and `flat`.
- **`SceneRenderer`:**
  - The constructor takes a `ThemeId` (default `NeonHall`).
  - `setTheme(ThemeId)` stores the theme and rebuilds the prop list.
  - It builds one GL mesh per `Shape` from `shapeMesh`.
  - Draw order:
    1. background
    2. props
    3. main lane
    4. gutters and low side walls (the walls use the theme's **gutter** material, as today)
    5. pins
    6. ball
    7. aim guide
- **Far plane:** the camera far plane goes from 100 m to 150 m. The farthest Outdoor ground corner is
  about 93 m from the home camera.

## BowlingGL

- **Command:** `Command::Type::CycleTheme`.
  - `InputMapper::keyDown` maps `T` and `t` to it in every phase.
  - `LaneCommands::execute` treats it like `Quit`: it succeeds and does nothing.
  - `BowlingCanvas::apply` handles it with `setTheme(nextTheme(theme()))`.
- **Canvas:**
  - `setTheme(ThemeId)`, `theme()`, and an `onThemeChanged(ThemeId)` callback.
  - Before the GL context exists, `setTheme` only stores the id. `ensureRenderer` builds the
    `SceneRenderer` with the stored id.
- **Frame:**
  - `BowlingFrame(ThemeId initial)` builds a **View → Theme** menu with three radio items. Their labels
    come from `theme(id).label`.
  - It connects `canvas->onThemeChanged` so the menu checks the matching item, then calls
    `canvas->setTheme(initial)`.
  - Choosing a menu item calls `canvas->setTheme`.
- **App:**
  - `BowlingApp` overrides `OnInitCmdLine` to add the option `--theme=<name>`, and `OnCmdLineParsed`
    to read it.
  - It resolves the name with `themeFromName` and keeps the `ThemeId` in a member, default `NeonHall`.
    There is no second name table.
  - An unknown name writes `unknown theme '<x>'; use neon, classic or outdoor` to stderr and keeps
    Neon Hall. The list of names in the message is built from `kThemes`.
  - `OnInit` creates `BowlingFrame(_theme)`.
- **Help:** `HudText::controlsHelp()` gets the line `T: change the scenery`. The README controls table
  gets a row.

## Tests

`test_bowlingbox_render` (no GL):

- **Theme data:**
  - `theme(id).id == id` for every id.
  - `themeFromName(theme(id).name) == id`, and `themeFromName("x")` is empty.
  - `nextTheme` cycles through all three themes and returns to the start.
  - `fogStart < fogEnd`, and `ambient` is in [0, 1].
- **Shapes:**
  - For each `Shape`, `localBounds` equals the min/max of `shapeMesh`'s vertex positions.
  - `bounds` of a rotated, scaled Box encloses its eight transformed corners and is no larger.
  - `bounds` of a Pin at the origin with identity transform is
    (-pinMaxRadius, -pinComHeight, -pinMaxRadius) … (pinMaxRadius, pinHeight - pinComHeight, pinMaxRadius).
- **Clearance:**
  - For each theme, no prop's `bounds` intersects the play corridor, the sight corridor or the camera
    box.
  - The ball return next to the main lane is checked explicitly: its min.x is at least 0.82.
- **Indoor themes:**
  - Some prop reaches x ≤ -W and some reaches x ≥ W (side walls).
  - Some prop reaches y ≥ kHallHeight (ceiling).
  - Some prop reaches z ≥ kHallBackZ (back wall).
- **Outdoor:** no prop's max.y is above 6.5.
- **Rack pins:** Pin props whose origin y equals pinComHeight.
  - Their count is 10 · 2 · neighbourLanesPerSide.
  - All of them have `pinBand`.
  - Each lies within a neighbour lane's x range and in z [headPinZ - 0.1, laneLength].
- **Icon pins:** Pin props with origin y above 1. Their count is 4 when `wallScreens` is set, and 0
  otherwise.
- **Lane pitch:** `lanePitch(g)` is 2·(laneHalfWidth + gutterWidth) + 0.30.

`test_bowlinggl`:

- `T` maps to `CycleTheme` in every phase, and `t` does too.
- `LaneCommands::execute(CycleTheme)` succeeds and leaves the snapshot unchanged.
- `controlsHelp()` mentions `T`.

**Not unit-tested:** the canvas, menu and `--theme` wiring. It needs a window and a GL context, the same
as `Quit` today, so it is covered by the hand checklist instead.

**By hand** (added to the README checklist):
- Start with each `--theme`, and with an unknown name, which warns and starts Neon Hall.
- Press `T` through all three themes and check that the menu follows. Choose each theme from the menu.
- Check that a throw and the falling pins are always visible.
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
