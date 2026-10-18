#include <Bowlingbox/Render/FontAtlas.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace Bowlingbox::Render;

namespace {
const FontAtlas& font() {
    static const FontAtlas atlas = [] {
        auto baked = FontAtlas::bakeEmbedded();
        if (baked.isError())
            throw std::runtime_error(baked.error());
        return std::move(baked).value();
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
        const float h = font().pixelHeight(scale);
        const auto q = font().layout("H", {0.f, 0.f}, h);
        ASSERT_EQ(q.size(), 1u);
        const float cap = q[0].p1.y - q[0].p0.y;
        EXPECT_NEAR(cap, 7.f * scale, 1.5f) << "scale " << scale;
        EXPECT_GE(font().measure("H", h).y, cap) << "line height at scale " << scale;
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
