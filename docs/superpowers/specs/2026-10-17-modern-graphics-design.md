# Modern Graphics — Design

Date: 2026-10-17. Branch: `feature/modern-graphics`, based on `main` (633456a).
Revised 2026-10-17 after the plan review: the decisions taken while planning are recorded here, so this
spec and [the plan](../plans/2026-10-17-modern-graphics.md) agree.

## Goal

BowlingGL looks like an early-80s console game today. This work makes it look modern and more realistic,
with glossy ball and pins, shadows, an oiled lane that reflects, glowing neon and a proper font, while
keeping 60 fps on integrated GPUs.

This supersedes the "no bloom, no reflections" decision in
[2026-10-15-environment-themes-design.md](2026-10-15-environment-themes-design.md). The themes, the prop
layout and their tests stay as they are.

## Decisions

- **Hardware floor:** OpenGL 3.3 core on integrated GPUs (Intel/AMD laptop graphics), 60 fps. There is no
  quality setting.
- **Renderer:** forward rendering with post-processing (shadow pass, reflection pass, HDR main pass,
  bloom + tone mapping). We rejected a deferred renderer (it fights MSAA and the transparent aim guide, and
  is too heavy for integrated GPUs) and adopting bgfx/Filament (a large dependency for a small scene).
- **Assets:** all surface detail is procedural, in shaders. The only new asset is one OFL font, **Rubik
  Medium**, embedded in the binary. There are no image textures, no asset folder and no installer changes
  beyond the licence notice.
- **Fallback:** if the GPU can't run the full pipeline, a *basic path* runs instead (the new lighting and
  gamma, without shadows, reflection or bloom). The game never refuses to start over a missing effect.
- **Scope boundary:** the Render library only, plus at most the GL attribute line in `BowlingFrame.cpp`.
  Gameplay, physics, camera, the Alley library and the HUD layout code don't change.

## Current state (what makes it look retro)

| Symptom | Cause |
|---|---|
| Matte, plastic look | Lambert diffuse only, one light, no specular (`kLitFs` in `Render.cpp`) |
| Objects float | No shadows |
| Flat, washed-out colour | No gamma correction, no tone mapping |
| Jagged edges | No MSAA (`BowlingFrame.cpp:51`) |
| 8-bit HUD | `stb_easy_font` bitmap font |
| Lane is a striped box | ±6 % alternating stripes, no grain, no gloss, no reflection |
| Lumpy pins | 12-point pin profile |

## Frame pipeline

`SceneRenderer::draw` runs these passes, in order:

1. **Shadow pass.** Depth of the ball, pins and the props inside the shadow box into a 1024×4096
   `DEPTH_COMPONENT24` map, seen from the theme's key light. The shadow box is the lane, gutters, approach,
   deck and pit (x ±(outerX + 0.1), z −approach … laneLength + kPitLength, y kPitTopY − 0.1 … 2.5 m), not
   the whole hall. 2.5 m lets the masking unit cast, and keeps the ceiling and ceiling lights out. The
   light's orthographic view uses the lane direction (+z) as its up vector, so the lane runs along the
   map's long axis (about 5–6 mm per texel along the lane), and it is fitted tightly around the box. The fit
   depends only on the light and the box, never on the camera, so shadows can't shimmer when the camera
   moves, and no texel snapping is needed.
2. **Reflection pass.** Ball, pins and props in the reflection box (the lane and gutters plus 0.3 m each
   side, so the ball returns; y 0 … 2.5 m), mirrored in y = 0, into a half-resolution `RGBA16F` texture.
   `gl_ClipDistance` drops geometry below the lane. No shadows and no reflection inside this pass. The
   target is cleared to transparent black, so its alpha marks where something was reflected.
3. **Main pass.** Into an `RGBA16F` framebuffer with 4× MSAA (multisample textures) and a 24-bit depth
   buffer: the sky gradient, then every lit mesh. Only the main lane samples the reflection texture;
   every lit surface samples the shadow map. The aim guide draws last, blended, as today.
4. **Resolve + post.** Resolve the MSAA into a single-sample HDR texture with a shader that averages
   `c / (1 + max(c))` over the samples and inverts that curve afterwards. A plain average lets one very
   bright sample (a neon strip, a specular highlight) dominate the pixel, so edges look jagged again after
   tone mapping. Bloom: a bright pass at half resolution that clamps each pixel to 16, so single-pixel
   specular highlights on the rolling ball don't make the glow flicker; then a 4-level
   downsample/upsample chain (13-tap down, tent-filter up). Composite: scene + bloom ×
   `theme.bloomStrength`, × `theme.exposure`, ACES-fit tone mapping, linear → sRGB. The result goes into
   the default framebuffer, inside the 3D band viewport only.
5. **Overlay.** The HUD, drawn as today, with the new font.

The window's default framebuffer needs no multisampling. All MSAA happens in the main-pass FBO.

**Budget on an integrated GPU (rough):** shadow pass ~1 ms, reflection ~1.5 ms, main pass ~3 ms, post
~1 ms. That's about 6.5 ms of the 16.6 ms frame. The target is under 10 ms of GPU time.

## Code structure

Render is built with hidden symbol visibility, so the GL-free pieces that the unit tests call are public,
exported headers in `include/Bowlingbox/Render/`:

| Header | Contents |
|---|---|
| `Lighting.h` | `srgbToLinear` / `linearToSrgb`; `LightFrustum`, `fitLightFrustum(lightDir, box)`; `shadowBox`, `reflectionBox`, `kShadowTopY`; `propsInBox` (picks shadow and reflection casters) |
| `RenderOptions.h` | `frameSizes(width, bandHeight)` (every offscreen size, never below 1×1) and `renderOptions(render, stats)` (reads the two environment variables) |
| `FontAtlas.h` | Bakes a TTF with `stb_truetype`; glyph layout and measuring |

New private units in `libraries/Render/src/`:

| Unit | Purpose |
|---|---|
| `Shaders.h/.cpp` | All GLSL sources as embedded strings, moved out of `Render.cpp` |
| `GlFramebuffer.h/.cpp` | RAII FBO with colour/depth attachments (single-sample, multisample or depth-compare texture), completeness check |
| `GlTexture.h/.cpp` | RAII texture name (the font atlas) |
| `ShadowMap.h/.cpp` | The 1024×4096 depth target and the state for drawing into it |
| `PostChain.h/.cpp` | HDR MSAA target, weighted resolve, bloom chain, composite/tone-map pass |
| `GpuTimer.h/.cpp` | `GL_TIME_ELAPSED` queries per pass for `BOWLINGBOX_RENDER_STATS` |
| `EmbeddedFont.h` | Declares the generated font byte array |

`GlShader` caches uniform locations, because the extra passes set roughly three times as many uniforms
per frame.

`ContextData` gains the new shader programs (depth-only, resolve, bright pass, bloom down/up, composite,
textured 2D), the render options, a `bool fullPipeline`, the MSAA sample count, and the font atlas with
its texture. `Context::create()` decides `fullPipeline` (see *Error handling*).

The public API keeps its signatures: `Context`, `Camera`, `SceneRenderer`, `Overlay`, `ICanvas2D`.
`Context::resize` also resizes the FBOs, lazily on the next frame, so it stays callable without a current
context.

New third-party files:
- `3rdparty/stb/stb_truetype.h` (public domain / MIT, same as `stb_easy_font`).
- `3rdparty/fonts/Rubik-Medium.ttf` + `licenses/Rubik-OFL.txt` (next to the existing licence files). A
  CMake step (`cmake/EmbedFile.cmake`, at configure time) turns the TTF into a generated `.cpp` byte array
  linked into Render.
- `installer/THIRD_PARTY_NOTICES.txt` and `licenses/README.md` list stb_truetype and Rubik (OFL).

`stb_easy_font` stays as the fallback font.

## Materials and lighting

### Material

```cpp
enum class Pattern { None, Wood, BallSwirl };

struct Material {
    glm::vec3 color {1.f};          // sRGB, as authored; converted to linear on upload
    bool emissive = false;
    float roughness = 0.6f;         // 0 = sharp highlight, 1 = chalky
    float specular = 0.04f;         // reflectance at normal incidence
    float emissiveStrength = 1.f;   // > 1 pushes glow over the bloom threshold
    Pattern pattern = Pattern::None;
};
```

`lit(r,g,b)` and `glow(r,g,b)` keep their signatures and fill the defaults. Tuned materials:

| Material | roughness | specular | pattern | other |
|---|---|---|---|---|
| Lane | 0.25 | 0.04 | Wood | samples the reflection texture |
| Ball | 0.15 | 0.06 | BallSwirl | |
| Pin body, pin band | 0.30 | 0.05 | None | |
| Gutter | 0.40 | 0.50 | None | brushed steel |
| Neon accents (`accentA/B` in Neon Hall) | — | — | — | `emissiveStrength` 5 |
| Wall screens (`screen` in Neon Hall) | — | — | — | `emissiveStrength` 4 |
| Ceiling panels (`accentA` in Classic Alley) | — | — | — | `emissiveStrength` 1.5, a faint glow |
| Other emissives (icons, Classic's score monitor) | — | — | — | `emissiveStrength` 1, no glow |
| Floor, wall, ceiling, trim | 0.80 | 0.04 | None | |
| Aim guide | — | — | — | emissive cyan, alpha 0.40, so it reads the same under every theme |

Material colours are authored in sRGB and converted to linear per draw call (three `pow`s per draw,
about 150 draws: negligible).

The bloom bright pass keeps what exceeds 1.0, so only emissives above strength 1 (and strong highlights)
glow. Neon Hall glows hottest, Classic Alley's ceiling panels glow faintly, Outdoor has no emissive above
1.

Neighbour lanes use the lane material, which replaces today's `Prop::stripes` flag. `stripes` becomes
`pattern == Pattern::Wood`, and the `neighbour_lanes_have_board_stripes` test is updated to match.
Neighbour lanes don't reflect.

### Lighting model

- GGX specular with Schlick Fresnel and Smith visibility, plus Lambert diffuse. All in linear space.
- **Key light:** directional, casts shadows. `theme.lightDir` stays; adds `keyColor` and `keyIntensity`.
- **Point lights:** up to 4 per theme, no shadows, inverse-square falloff with a range cutoff. Classic
  Alley puts warm lights at its ceiling-light positions, Neon Hall cyan/violet ones along the strips,
  Outdoor has none.
- **Hemisphere ambient:** `skyAmbient` above, `groundAmbient` below, blended by the normal's y. This
  replaces `theme.ambient`.
- **Shadows:** 3×3 PCF with hardware comparison (`sampler2DShadow`), slope-scaled bias. Surfaces outside
  the fitted light box count as lit.
- **Fog:** unchanged formula, applied in linear space before tone mapping.

### Theme additions

`Theme` replaces `float ambient` with `glm::vec3 skyAmbient, groundAmbient` and adds:

```cpp
glm::vec3 keyColor; float keyIntensity;
std::array<PointLight, 4> pointLights; int pointLightCount;   // PointLight {position, color, intensity, range}
float exposure;       // > 0
float bloomStrength;  // 0 … 1; Neon ≈ 0.6, Classic ≈ 0.15, Outdoor ≈ 0.05
```

## Surface detail

### Lane wood (fragment shader, `Pattern::Wood`)

- The lane is 39 boards across, as a real lane, so a board is 2 × laneHalfWidth / 39 ≈ 26.9 mm (today's
  stripes are 25 mm, which gives 42). Boards start at the lane's edge, so board 20 is the middle board.
  Each board gets a hashed tint (±8 %) and a grain phase offset.
- Grain runs along z: stretched 2-octave value noise for fine lines plus rare darker streaks.
- Seams: thin dark lines at board edges, faded by `fwidth` like today's stripes so they don't shimmer.
- Zones: the approach (z < 0) and the pin deck (z > headPinZ − 0.3) get a slightly different tint.
- Markings, on the main lane only: 7 targeting arrows between z ≈ 4.3 m and 4.9 m (the middle one
  furthest) on boards 5, 10, 15, 20, 25, 30, 35, plus approach dots at z ≈ −0.15 m on the same boards.
  Both are dark-red inlays. With 39 boards the pattern is symmetric about board 20, so it doesn't matter
  which edge is counted from.
- Reflection, on the main lane only. The reflection texture holds premultiplied colour (it's cleared to
  transparent black), so with `k = fresnel × (1 − roughness) × 0.6` the lane becomes
  `lit × (1 − k × a) + k × reflection.rgb`. A plain `mix` would darken the soft edges of every reflected
  object. The texture is sampled with a small roughness-scaled offset blur (4 taps).

### Ball (fragment shader, `Pattern::BallSwirl`)

- 3D fractal noise in the ball's object space, so the pattern rolls with the ball and makes its spin
  visible. The vertex shader passes object-space position.
- Two tones: `color` and `color × 0.35`, marbled through a warped noise threshold.
- Three finger holes: dark discs at fixed object-space directions, with a soft rim.

### Geometry

- `pinData`: the profile goes from 12 to 45 points (4 samples per span over the 11 spans of a Catmull-Rom
  spline through the current 12 control points), with the default segments raised from 24 to 40. The
  signature, height and max radius stay the same.
- Ball: `sphereData(radius, 48)` instead of 32.

## HUD font

- `FontAtlas` bakes ASCII 32–126 from Rubik Medium at 48 px into one `R8` coverage texture (512×512) with
  `stbtt_PackFontRange`, at `Context::create()`. Glyphs are packed with 4 px padding.
- The texture has mipmaps with trilinear filtering, limited to levels 0–2. HUD text is smaller than the
  48 px bake, so it's minified. 4 px of padding still leaves a 1 px gap at level 2, so neighbouring glyphs
  never bleed into each other. A `smoothstep` edge only suits distance-field fonts, so it isn't used.
- `Overlay::drawText` draws textured quads through a new 2D shader. A 1 px drop shadow (black, 50 %) goes
  under every string.
- Scale mapping: one old `stb_easy_font` unit equals the old glyph height / 7. The overlay picks the pixel
  size so cap height = 7 × scale px. `Hud.cpp` and `ScoreboardPainter.cpp` keep their scale values, and
  layouts keep their proportions.
- `measureText` returns the real advance width and the line height at that size.
- Characters outside 32–126 render as `?`.

## Error handling

| Failure | Behaviour |
|---|---|
| A full-pipeline shader fails to compile/link | Basic path; shader log to stderr |
| `RGBA16F` not renderable, or an FBO incomplete at create or resize | Basic path; reason to stderr |
| Resize to 0×0 (minimised) | Clamp to 1×1 as today; skip reallocation |
| Font bake fails | Use `stb_easy_font` for the HUD; message to stderr |
| Basic shader fails | `Context::create()` returns an error, as today |

- **Basic path:** the new lit shader (GGX, hemisphere ambient, wood/ball patterns, fog) drawn straight
  into the default framebuffer, with tone mapping and sRGB done at the end of the lit shader. No shadows,
  no reflection, no bloom, no MSAA.
- `BOWLINGBOX_RENDER=basic` forces the basic path.
- `BOWLINGBOX_RENDER_STATS=1` prints the average frame time per pass once per second, using
  `GL_TIME_ELAPSED` queries. The queries rotate through three frames, and a result is read only once
  `GL_QUERY_RESULT_AVAILABLE` says it's ready, so measuring never stalls the frame it measures.
- When the 3D band is under 1 px tall (minimised, or the HUD takes the whole window), `SceneRenderer::draw`
  returns early.

## Testing

Unit tests (GL-free; go in `libraries/Render/test/src/`):

- `fitLightFrustum`: the shadow box lies inside the light volume, tightly, for all three themes' light
  directions; depth grows along the light; a straight-down light gives a finite matrix; moving along the
  lane (+z) never moves a point across the map's short axis. It has no camera input, so the camera can't
  move the volume.
- `shadowBox` / `reflectionBox` / `propsInBox`: for every theme the shadow casters include the ball
  returns and never the ceiling; the reflection casters include the ball returns and never anything below
  the lane or a neighbour lane.
- `FontAtlas` (loads the embedded TTF on the CPU only): measured width grows strictly with each added
  character; glyph rectangles don't overlap in the atlas; an unknown character measures the same as `?`;
  the scale → pixel-size mapping is increasing and gives capitals 7 × scale px tall.
- `frameSizes` / `renderOptions`: no size ever reaches 0; only `basic` (any case) and `1` switch anything.
- Themes: roughness and specular in [0, 1] for every material; `pointLightCount` ≤ 4; `exposure` > 0;
  `bloomStrength` in [0, 1]; Neon Hall's hottest emissive is hotter than Classic Alley's, which is above 1;
  Outdoor has no emissive above 1. This replaces `fog_and_ambient_are_in_range` with a fog-and-lighting
  version.
- `pinData`: the angle between neighbouring profile normals stays under 25° (the worst is about 19°);
  height and max radius stay the same (the existing bounds test keeps passing).
- sRGB: sRGB → linear → sRGB round-trips within 1e-5 on [0, 1].
- All existing `Render` and `bowlinggl` tests stay green.

Manual verification (GL-dependent):

- Before/after screenshots of all three themes from the start camera position and from the pin-deck
  camera position, attached to the PR.
- Frame time on an integrated GPU with `BOWLINGBOX_RENDER_STATS=1`: under 10 ms GPU time at 1920×1080.
- Every theme once with `BOWLINGBOX_RENDER=basic`.
- The Windows build (static wx, CI) still links, and the installer stage check still passes.

## Out of scope

SSAO, image-based lighting / HDRI, bundled textures, a graphics-quality menu, camera or physics changes,
HUD layout redesign, a new font for the console app.
