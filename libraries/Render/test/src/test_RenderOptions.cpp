#include <Bowlingbox/Render/RenderOptions.h>
#include <gtest/gtest.h>

using namespace Bowlingbox::Render;

TEST(test_render_options, render_options_read_the_environment_strictly) {
    EXPECT_FALSE(renderOptions(nullptr, nullptr).forceBasic);
    EXPECT_FALSE(renderOptions(nullptr, nullptr).stats);
    EXPECT_TRUE(renderOptions("basic", nullptr).forceBasic);
    EXPECT_TRUE(renderOptions("Basic", nullptr).forceBasic);
    EXPECT_TRUE(renderOptions("BASIC", nullptr).forceBasic);
    EXPECT_FALSE(renderOptions("", nullptr).forceBasic);
    EXPECT_FALSE(renderOptions("full", nullptr).forceBasic);
    EXPECT_FALSE(renderOptions("basics", nullptr).forceBasic);
    EXPECT_TRUE(renderOptions(nullptr, "1").stats);
    EXPECT_FALSE(renderOptions(nullptr, "0").stats);
    EXPECT_FALSE(renderOptions(nullptr, "").stats);
    EXPECT_FALSE(renderOptions(nullptr, "yes").stats);
}

TEST(test_render_options, frame_sizes_never_reach_zero) {
    for (auto [w, h] : {std::pair {0, 0}, std::pair {1, 1}, std::pair {3, 0}, std::pair {-5, 7}}) {
        const FrameSizes s = frameSizes(w, h);
        EXPECT_GE(s.scene.x, 1);
        EXPECT_GE(s.scene.y, 1);
        EXPECT_GE(s.reflection.x, 1);
        EXPECT_GE(s.reflection.y, 1);
        for (const glm::ivec2& b : s.bloom) {
            EXPECT_GE(b.x, 1);
            EXPECT_GE(b.y, 1);
        }
    }
}

TEST(test_render_options, frame_sizes_halve_down_the_chain) {
    const FrameSizes s = frameSizes(1920, 777);
    EXPECT_EQ(s.scene, glm::ivec2(1920, 777));
    EXPECT_EQ(s.reflection, glm::ivec2(960, 388));
    EXPECT_EQ(s.bloom[0], glm::ivec2(960, 388));
    EXPECT_EQ(s.bloom[1], glm::ivec2(480, 194));
    EXPECT_EQ(s.bloom[2], glm::ivec2(240, 97));
    EXPECT_EQ(s.bloom[3], glm::ivec2(120, 48));
}
