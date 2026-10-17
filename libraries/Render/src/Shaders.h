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

extern const char* const kTextVs;  // 2D glyph quads: position + atlas uv
extern const char* const kTextFs;  // colour * atlas coverage

extern const char* const kSkyVs;  // attribute-less full-screen triangle, vT = 0 bottom .. 1 top
std::string skyFs();

extern const char* const kPostVs;     // attribute-less full-screen triangle with vUv
extern const char* const kResolveFs;  // MSAA -> one sample, weighted so bright samples don't swamp edges
/// Scene (+ bloom) * exposure -> ACES -> sRGB.
std::string compositeFs();

}  // namespace Bowlingbox::Render::shaders
#endif  // BOWLINGBOX_RENDER_SHADERS_H
