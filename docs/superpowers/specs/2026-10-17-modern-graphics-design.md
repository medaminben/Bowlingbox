# Modern Graphics — Design

Date: 2026-10-17. Branch: `feature/modern-graphics`, based on `main` (633456a).

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

1. **Shadow pass.** Depth of the ball, pins and props into a 2048² `DEPTH_COMPONENT24` map, seen from the
   theme's key light. The light's orthographic view is fitted to the lane-and-deck box (x ±outerX,
   z −approach … laneLength + kPitLength, y 0 … pinHeight), not the whole hall, and snapped to shadow-map
   texels so shadows don't shimmer when the camera moves.
2. **Reflection pass.** Ball, pins and props within 1 m of the lane edges, mirrored in y = 0, into a
   half-resolution `RGBA16F` texture. `gl_ClipDistance` drops geometry below the lane. No shadows and no
   reflection inside this pass.
3. **Main pass.** Into an `RGBA16F` framebuffer with 4× MSAA and a 24-bit depth buffer: the sky gradient,
   then every lit mesh. The lane samples the reflection texture, and every lit surface samples the shadow
   map. The aim guide draws last, blended, as today.
4. **Resolve + post.** Resolve the MSAA into a single-sample HDR texture. Bloom: a bright-pass at half
   resolution, then a 4-level downsample/upsample chain (13-tap down, tent-filter up). Composite:
   scene + bloom × `theme.bloomStrength`, × `theme.exposure`, ACES-fit tone mapping, linear → sRGB. The
   result goes into the default framebuffer, inside the 3D band viewport only.
5. **Overlay.** The HUD, drawn as today, with the new font.

The window's default framebuffer needs no multisampling. All MSAA happens in the main-pass FBO.

**Budget on an integrated GPU (rough):** shadow pass ~1 ms, reflection ~1.5 ms, main pass ~3 ms, post
~1 ms. That's about 6.5 ms of the 16.6 ms frame. The target is under 10 ms of GPU time.

## Code structure

New units in `libraries/Render/src/` (all private to the library):

| Unit | Purpose | GL? |
|---|---|---|
| `Shaders.h/.cpp` | All GLSL sources as embedded strings, moved out of `Render.cpp` | no |
| `GlFramebuffer.h/.cpp` | RAII FBO with colour/depth attachments, resize, completeness check, MSAA resolve | yes |
| `ShadowMap.h/.cpp` | Depth target + `fitLightFrustum(lightDir, box, mapSize)`, a pure function | split |
| `PostChain.h/.cpp` | HDR target, bloom mip chain, composite/tone-map pass | yes |
| `FontAtlas.h/.cpp` | Bakes the embedded TTF with `stb_truetype`, glyph layout and measuring | no |
| `ColorSpace.h` | `srgbToLinear` / `linearToSrgb` (`constexpr`-friendly) | no |

`ContextData` gains the new shader programs (lit, depth-only, bloom down/up, composite, textured 2D), a
`bool fullPipeline`, and the font atlas texture. `Context::create()` decides `fullPipeline` (see *Error
handling*).

The public API keeps its signatures: `Context`, `Camera`, `SceneRenderer`, `Overlay`, `ICanvas2D`.
`Context::resize` also resizes the FBOs, lazily on the next frame, so it stays callable without a current
context.

New third-party files:
- `3rdparty/stb/stb_truetype.h` (public domain / MIT, same as `stb_easy_font`).
- `3rdparty/fonts/Rubik-Medium.ttf` + `3rdparty/licenses/Rubik-OFL.txt`. A CMake step turns the TTF into a
  generated `.cpp` byte array linked into Render.
- `installer/THIRD_PARTY_NOTICES.txt` and `3rdparty/licenses/README.md` list Rubik under the OFL.

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
| Neon accents (`accentA/B`, `screen` in Neon Hall) | — | — | — | `emissiveStrength` 4–6 |
| Floor, wall, ceiling, trim | 0.80 | 0.04 | None | |

Neighbour lanes use the lane material, which replaces today's `Prop::stripes` flag. `stripes` becomes
`pattern == Pattern::Wood`, and the `neighbour_lanes_have_board_stripes` test is updated to match.

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

- Boards are 25 mm wide (as today). Each board gets a hashed tint (±8 %) and a grain phase offset.
- Grain runs along z: stretched 2-octave value noise for fine lines plus rare darker streaks.
- Seams: thin dark lines at board edges, faded by `fwidth` like today's stripes so they don't shimmer.
- Zones: the approach (z < 0) and the pin deck (z > headPinZ − 0.3) get a slightly different tint.
- Markings: 7 targeting arrows at z ≈ 4.6 m on boards 5, 10, 15, 20, 25, 30, 35, counted from the right,
  plus approach dots at z ≈ −0.15 m. Both are dark-red inlays.
- Reflection: `mix(lit, reflection, fresnel × (1 − roughness) × 0.6)`, with the reflection sampled using
  a small roughness-scaled offset blur (4 taps).

### Ball (fragment shader, `Pattern::BallSwirl`)

- 3D fractal noise in the ball's object space, so the pattern rolls with the ball and makes its spin
  visible. The vertex shader passes object-space position.
- Two tones: `color` and `color × 0.35`, marbled through a warped noise threshold.
- Three finger holes: dark discs at fixed object-space directions, with a soft rim.

### Geometry

- `pinData`: the profile goes from 12 to ~28 points, sampled from a Catmull-Rom spline through the
  current 12 control points, with the default segments raised from 24 to 40. The signature, height and
  max radius stay the same.
- Ball: `sphereData(radius, 48)` instead of 32.

## HUD font

- `FontAtlas` bakes ASCII 32–126 from Rubik Medium at 48 px into one `R8` texture (512×512) with
  `stbtt_PackFontRange`, at `Context::create()`.
- `Overlay::drawText` draws textured quads through a new 2D shader. The glyph alpha goes through
  `smoothstep` over ±0.5 px for crisp scaling. A 1 px drop shadow (black, 50 %) goes under every string.
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
  `GL_TIME_ELAPSED` queries.

## Testing

Unit tests (GL-free; go in `libraries/Render/test/src/`):

- `fitLightFrustum`: the lane-and-deck box lies inside the light volume for all three themes' light
  directions; moving the camera never moves the fitted volume; snapping keeps the origin on whole texels.
- `FontAtlas` (loads the embedded TTF on the CPU only): measured width grows strictly with each added
  character; glyph rectangles don't overlap in the atlas; an unknown character measures the same as `?`;
  the scale → pixel-size mapping is increasing.
- Themes: roughness and specular in [0, 1] for every material; `pointLightCount` ≤ 4; `exposure` > 0;
  `bloomStrength` in [0, 1]; `emissiveStrength` > 1 only in Neon Hall. This replaces
  `fog_and_ambient_are_in_range` with a fog-and-lighting version.
- `pinData`: the angle between neighbouring profile normals stays under 25°; height and max radius stay
  the same (the existing bounds test keeps passing).
- `ColorSpace`: sRGB → linear → sRGB round-trips within 1e-5 on [0, 1].
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
