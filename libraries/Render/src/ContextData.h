#ifndef BOWLINGBOX_RENDER_CONTEXT_DATA_H
#define BOWLINGBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include "GlTexture.h"
#include <Bowlingbox/Render/FontAtlas.h>
#include <Bowlingbox/Render/RenderOptions.h>
#include <memory>
#include <optional>

namespace Bowlingbox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, lit or emissive, colour band, fog, lane stripes
    std::unique_ptr<GlShader> flat;  // 2D overlay
    std::unique_ptr<GlShader> sky;   // background gradient behind the 3D scene
    std::unique_ptr<GlShader> text;  // 2D glyph quads from the font atlas
    std::unique_ptr<GlShader> composite;  // HDR -> tone map -> sRGB (full pipeline only)
    std::unique_ptr<GlShader> resolve;    // weighted MSAA resolve (full pipeline only)
    std::optional<FontAtlas> font;   // empty: the overlay falls back to stb_easy_font
    GlTexture fontTexture;           // R8 atlas with mip levels 0..FontAtlas::kMaxMipLevel
    RenderOptions options;
    bool fullPipeline = false;  // shadows, reflection, HDR + bloom; false = basic path
    int samples = 4;            // main-pass MSAA, clamped to what multisample textures support
    int width = 1;
    int height = 1;
    bool reportedGlError = false;  // debug builds report the first GL error once
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_CONTEXT_DATA_H
