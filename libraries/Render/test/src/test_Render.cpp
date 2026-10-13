#include <Gamebox/Render/Geometry.h>
#include <Gamebox/Render/ScoreboardLayout.h>
#include <Gamebox/Render/ScoreboardPainter.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace Gamebox::Render;

namespace {
void expectWellFormed(const MeshData& m) {
    ASSERT_EQ(m.vertices.size() % 6, 0u);
    ASSERT_EQ(m.indices.size() % 3, 0u);
    const unsigned n = static_cast<unsigned>(m.vertexCount());
    for (unsigned idx : m.indices)
        EXPECT_LT(idx, n);
    for (std::size_t v = 0; v < m.vertexCount(); ++v) {
        const float nx = m.vertices[v * 6 + 3], ny = m.vertices[v * 6 + 4], nz = m.vertices[v * 6 + 5];
        EXPECT_NEAR(std::sqrt(nx * nx + ny * ny + nz * nz), 1.f, 1e-4f);
    }
}
float minY(const MeshData& m) {
    float y = 1e9f;
    for (std::size_t v = 0; v < m.vertexCount(); ++v)
        y = std::min(y, m.vertices[v * 6 + 1]);
    return y;
}
float maxY(const MeshData& m) {
    float y = -1e9f;
    for (std::size_t v = 0; v < m.vertexCount(); ++v)
        y = std::max(y, m.vertices[v * 6 + 1]);
    return y;
}
}  // namespace

TEST(test_render_geometry, box_has_24_vertices_and_12_triangles) {
    const auto m = boxData(1.f, 2.f, 3.f);
    EXPECT_EQ(m.vertexCount(), 24u);
    EXPECT_EQ(m.indices.size(), 36u);
    expectWellFormed(m);
    EXPECT_FLOAT_EQ(minY(m), -2.f);
    EXPECT_FLOAT_EQ(maxY(m), 2.f);
}

TEST(test_render_geometry, sphere_is_well_formed_and_bounded) {
    const auto m = sphereData(0.5f, 16);
    expectWellFormed(m);
    EXPECT_NEAR(minY(m), -0.5f, 1e-5f);
    EXPECT_NEAR(maxY(m), 0.5f, 1e-5f);
}

TEST(test_render_geometry, cylinder_is_centred_on_origin) {
    const auto m = cylinderData(0.3f, 1.0f, 12);
    expectWellFormed(m);
    EXPECT_NEAR(minY(m), -0.5f, 1e-5f);
    EXPECT_NEAR(maxY(m), 0.5f, 1e-5f);
}

TEST(test_render_geometry, pin_spans_from_base_to_top_relative_to_com) {
    const auto m = pinData(0.38f, 0.06f, 0.12f);
    expectWellFormed(m);
    EXPECT_NEAR(minY(m), -0.12f, 1e-5f);
    EXPECT_NEAR(maxY(m), 0.26f, 1e-5f);
}

TEST(test_render_scoreboard, glyphs_follow_gamebox_encoding) {
    EXPECT_EQ(glyphFor(-4), "");
    EXPECT_EQ(glyphFor(-3), "");
    EXPECT_EQ(glyphFor(-2), "X");
    EXPECT_EQ(glyphFor(-1), "/");
    EXPECT_EQ(glyphFor(0), "-");
    EXPECT_EQ(glyphFor(0, true), "0");
    EXPECT_EQ(glyphFor(7), "7");
    EXPECT_EQ(glyphFor(300, true), "300");
}

TEST(test_render_scoreboard, frames_are_two_cells_wide_except_the_last) {
    const ScoreboardLayout l {10.f, 20.f, 30.f};
    const Rect f0 = l.frameRect(0);
    EXPECT_FLOAT_EQ(f0.x, 10.f);
    EXPECT_FLOAT_EQ(f0.y, 20.f);
    EXPECT_FLOAT_EQ(f0.w, 60.f);
    EXPECT_FLOAT_EQ(f0.h, 60.f);
    const Rect f9 = l.frameRect(9);
    EXPECT_FLOAT_EQ(f9.x, 10.f + 9 * 60.f);
    EXPECT_FLOAT_EQ(f9.w, 90.f);
    EXPECT_FLOAT_EQ(l.totalWidth(), 21 * 30.f);
}

TEST(test_render_scoreboard, roll_cells_map_the_21_slot_encoding) {
    const ScoreboardLayout l {0.f, 0.f, 10.f};
    const Rect r0 = l.rollCellRect(0);
    EXPECT_FLOAT_EQ(r0.x, 0.f);
    EXPECT_FLOAT_EQ(r0.w, 10.f);
    const Rect r3 = l.rollCellRect(3);
    EXPECT_FLOAT_EQ(r3.x, 30.f);
    const Rect r20 = l.rollCellRect(20);
    EXPECT_FLOAT_EQ(r20.x, 180.f + 20.f);
    const Rect s4 = l.scoreRect(4);
    EXPECT_FLOAT_EQ(s4.x, 80.f);
    EXPECT_FLOAT_EQ(s4.y, 10.f);
    EXPECT_FLOAT_EQ(s4.w, 20.f);
}

namespace {
/// ICanvas2D that records instead of drawing: no GL context needed.
class RecordingCanvas final : public ICanvas2D {
public:
    std::vector<std::string> texts;
    int rects = 0;
    void drawText(std::string_view text, glm::vec2, float, glm::vec3) override { texts.emplace_back(text); }
    glm::vec2 measureText(std::string_view text, float scale) const override {
        return {static_cast<float>(text.size()) * 6.f * scale, 7.f * scale};
    }
    void drawRect(glm::vec2, glm::vec2, glm::vec4) override { ++rects; }
};
}  // namespace

TEST(test_render_scoreboard, painter_writes_marks_scores_and_total) {
    std::vector<int> rolls(21, -3);
    rolls[0] = -2;  // strike
    rolls[1] = -4;  // empty cell after it
    rolls[2] = 3;
    rolls[3] = -1;  // spare
    std::vector<int> scores(10, -3);
    scores[0] = 20;
    RecordingCanvas canvas;
    paintScoreboard(canvas, rolls, scores, 20, ScoreboardLayout {0.f, 0.f, 20.f});
    const std::vector<std::string> expected {"X", "20", "3", "/", "Total 20"};  // frame by frame
    EXPECT_EQ(canvas.texts, expected);
    // panel + 10 frame outlines (4 each) + 11 cell dividers + 10 score separators
    EXPECT_EQ(canvas.rects, 1 + 40 + 11 + 10);
}
