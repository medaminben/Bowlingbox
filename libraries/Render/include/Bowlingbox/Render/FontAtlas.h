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

    /// Expects trusted, embedded font data: it rejects data too short or without a usable
    /// TrueType header, or glyphs that do not fit the atlas, but does not fully validate a TTF.
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
