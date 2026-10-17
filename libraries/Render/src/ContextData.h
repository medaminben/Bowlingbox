#ifndef BOWLINGBOX_RENDER_CONTEXT_DATA_H
#define BOWLINGBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include "GlTexture.h"
#include <Bowlingbox/Render/FontAtlas.h>
#include <memory>
#include <optional>

namespace Bowlingbox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, lit or emissive, colour band, fog, lane stripes
    std::unique_ptr<GlShader> flat;  // 2D overlay
    std::unique_ptr<GlShader> sky;   // background gradient behind the 3D scene
    std::unique_ptr<GlShader> text;  // 2D glyph quads from the font atlas
    std::optional<FontAtlas> font;   // empty: the overlay falls back to stb_easy_font
    GlTexture fontTexture;           // R8 atlas with mip levels 0..FontAtlas::kMaxMipLevel
    int width = 1;
    int height = 1;
    bool reportedGlError = false;  // debug builds report the first GL error once
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_CONTEXT_DATA_H
