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
