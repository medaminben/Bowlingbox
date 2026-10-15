#include "FixedStep.h"
#include "FollowCamera.h"
#include "Hud.h"
#include "HudText.h"
#include "LaneCommands.h"
#include "SceneAdapter.h"
#include <Bowlingbox/Render/Environment.h>
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace Bowlingbox;
using Alley::Phase;

namespace {
Alley::Snapshot snapshotIn(Phase phase) {
    Alley::Snapshot s;
    s.phase = phase;
    return s;
}

class RecordingCanvas final : public Render::ICanvas2D {
public:
    std::vector<std::string> texts;
    void drawText(std::string_view text, glm::vec2, float, glm::vec3) override { texts.emplace_back(text); }
    glm::vec2 measureText(std::string_view text, float scale) const override {
        return {static_cast<float>(text.size()) * 6.f * scale, 7.f * scale};
    }
    void drawRect(glm::vec2, glm::vec2, glm::vec4) override {}
    bool drew(const std::string& t) const { return std::find(texts.begin(), texts.end(), t) != texts.end(); }
};
}  // namespace

// ------------------------------------------------------------ LaneCommands
TEST(test_lane_commands, routes_commands_to_the_lane) {
    Alley::Lane lane;
    EXPECT_TRUE(LaneCommands::execute({Command::Type::MoveAim, 0.1f}, lane).isValue());
    EXPECT_NEAR(lane.snapshot().aimOffset, 0.1f, 1e-6f);
    EXPECT_TRUE(LaneCommands::execute({Command::Type::RotateAim, 2.f}, lane).isValue());
    EXPECT_NEAR(lane.snapshot().aimAngle, 2.f, 1e-6f);
    EXPECT_TRUE(LaneCommands::execute({Command::Type::StartCharge}, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, Phase::Charging);
    EXPECT_TRUE(LaneCommands::execute({Command::Type::NewGame}, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, Phase::Aiming);
}

TEST(test_lane_commands, reports_rejections_and_ignores_non_lane_commands) {
    Alley::Lane lane;
    EXPECT_TRUE(LaneCommands::execute({Command::Type::Release}, lane).isError());
    EXPECT_TRUE(LaneCommands::execute({Command::Type::Quit}, lane).isValue());
    EXPECT_TRUE(LaneCommands::execute({Command::Type::None}, lane).isValue());
}

TEST(test_lane_commands, cycle_theme_leaves_the_lane_alone) {
    Alley::Lane lane;
    ASSERT_TRUE(LaneCommands::execute({Command::Type::MoveAim, 0.1f}, lane).isValue());
    const Alley::Snapshot before = lane.snapshot();
    EXPECT_TRUE(LaneCommands::execute({Command::Type::CycleTheme}, lane).isValue());
    const Alley::Snapshot after = lane.snapshot();
    EXPECT_EQ(after.phase, before.phase);
    EXPECT_FLOAT_EQ(after.aimOffset, before.aimOffset);
    EXPECT_FLOAT_EQ(after.aimAngle, before.aimAngle);
    EXPECT_FLOAT_EQ(after.power, before.power);
}

// ------------------------------------------------------------ FollowCamera
TEST(test_follow_camera, stays_home_while_aiming) {
    FollowCamera cam;
    auto s = snapshotIn(Phase::Aiming);
    s.ball.position = {0.f, 0.1f, 10.f};
    for (int i = 0; i < 60; ++i)
        cam.update(s, 1.f / 60.f);
    EXPECT_FLOAT_EQ(cam.z(), FollowCamera::kHomeZ);
}

TEST(test_follow_camera, follows_a_rolling_ball_but_not_past_the_deck) {
    FollowCamera cam;
    auto s = snapshotIn(Phase::Rolling);
    s.ball.position = {0.f, 0.1f, 8.f};
    for (int i = 0; i < 600; ++i)
        cam.update(s, 1.f / 60.f);
    EXPECT_NEAR(cam.z(), 8.f - FollowCamera::kBehindBall, 1e-3f);
    s.ball.position = {0.f, 0.1f, 100.f};
    for (int i = 0; i < 600; ++i)
        cam.update(s, 1.f / 60.f);
    EXPECT_NEAR(cam.z(), Alley::kLane.length - FollowCamera::kDeckMargin, 1e-3f);
}

// ------------------------------------------------------------ HudText
TEST(test_hud_text, result_names_strikes_spares_and_gutters) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);

    sheet.roll(10);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::result(s, sheet), "STRIKE!");

    sheet.roll(3);
    s.lastPinsDown = 3;
    EXPECT_EQ(HudText::result(s, sheet), "3 pins");

    sheet.roll(7);
    s.lastPinsDown = 7;
    EXPECT_EQ(HudText::result(s, sheet), "SPARE!");

    sheet.roll(0);
    s.lastPinsDown = 0;
    EXPECT_EQ(HudText::result(s, sheet), "Gutter");

    EXPECT_EQ(HudText::result(snapshotIn(Phase::Aiming), sheet), "");
    EXPECT_EQ(HudText::result(snapshotIn(Phase::GameOver), sheet), "Game over - score 30");
}

TEST(test_hud_text, ten_pins_after_a_gutter_ball_is_a_spare) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);
    sheet.roll(0);
    sheet.roll(10);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::result(s, sheet), "SPARE!");
}

TEST(test_hud_text, status_line_shows_phase_score_and_standing_pins) {
    Score::ScoreCalculator sheet;
    sheet.roll(4);
    auto s = snapshotIn(Phase::Settling);
    s.pinStanding.fill(true);
    s.pinStanding[0] = false;
    EXPECT_EQ(HudText::status(s, sheet), "Settling   |   score 4   |   pins standing 9");
}

// ------------------------------------------------------------ Hud
TEST(test_hud, draws_scoreboard_hint_and_result_without_gl) {
    Score::ScoreCalculator sheet;
    sheet.roll(10);
    auto s = snapshotIn(Phase::FrameDone);
    s.lastPinsDown = 10;
    RecordingCanvas canvas;
    drawHud(canvas, s, sheet, {1280, 800, 144, 80});
    EXPECT_TRUE(canvas.drew("X"));
    EXPECT_TRUE(canvas.drew("Total 10"));
    EXPECT_TRUE(canvas.drew("STRIKE!"));

    RecordingCanvas aiming;
    drawHud(aiming, snapshotIn(Phase::Aiming), sheet, {1280, 800, 144, 80});
    EXPECT_TRUE(aiming.drew(HudText::hint(Phase::Aiming)));
}

// ------------------------------------------------------------ FixedStep
TEST(test_fixed_step, half_steps_accumulate) {
    FixedStep clock;
    EXPECT_EQ(clock.advance(FixedStep::kDt / 2), 0);
    EXPECT_EQ(clock.advance(FixedStep::kDt / 2), 1);
}

TEST(test_fixed_step, a_jittery_16ms_timer_still_runs_60_steps_per_second) {
    FixedStep clock;
    int steps = 0;
    for (int i = 0; i < 1000; ++i)
        steps += clock.advance(i % 2 == 0 ? 0.012f : 0.020f);  // 16 ms on average
    EXPECT_NEAR(steps, 960, 1);                                  // 16 s of game time
}

TEST(test_fixed_step, long_stall_runs_at_most_five_steps_and_drops_the_rest) {
    FixedStep clock;
    EXPECT_EQ(clock.advance(2.f), FixedStep::kMaxSteps);
    EXPECT_EQ(clock.advance(0.f), 0);
    EXPECT_EQ(clock.advance(-1.f), 0);  // a clock going backwards is ignored
}

// ------------------------------------------------------------ HUD layout and help
TEST(test_hud, layout_reserves_top_and_bottom_bands) {
    const HudLayout l = hudLayout(1000, 800);
    EXPECT_EQ(l.width, 1000);
    EXPECT_EQ(l.height, 800);
    EXPECT_EQ(l.topPx, 144);
    EXPECT_EQ(l.bottomPx, 80);
    EXPECT_EQ(l.middlePx(), 576);
}

TEST(test_hud_text, controls_help_names_every_binding) {
    const std::string help = HudText::controlsHelp();
    for (const char* key : {"Left / Right", "A / D", "Space", "N", "Esc", "Ctrl+Q", "T: change the scenery"})
        EXPECT_NE(help.find(key), std::string::npos) << key;
}

TEST(test_hud_text, unknown_theme_message_lists_every_name) {
    EXPECT_EQ(HudText::unknownThemeMessage("space"), "unknown theme 'space'; use neon, classic or outdoor");
    EXPECT_EQ(HudText::unknownThemeMessage(""), "unknown theme ''; use neon, classic or outdoor");
}

TEST(test_scene_adapter, geometry_copies_every_lane_dimension) {
    const auto g = SceneAdapter::geometry(Alley::kLane);
    const auto& l = Alley::kLane;
    EXPECT_FLOAT_EQ(g.laneHalfWidth, l.halfWidth);
    EXPECT_FLOAT_EQ(g.laneLength, l.length);
    EXPECT_FLOAT_EQ(g.approach, l.approach);
    EXPECT_FLOAT_EQ(g.gutterWidth, l.gutterWidth);
    EXPECT_FLOAT_EQ(g.gutterDepth, l.gutterDepth);
    EXPECT_FLOAT_EQ(g.ballRadius, l.ballRadius);
    EXPECT_FLOAT_EQ(g.pinHeight, l.pinHeight);
    EXPECT_FLOAT_EQ(g.pinMaxRadius, l.pinMaxRadius);
    EXPECT_FLOAT_EQ(g.pinComHeight, l.pinComHeight);
    EXPECT_FLOAT_EQ(g.pinSpacing, l.pinSpacing);
    EXPECT_FLOAT_EQ(g.headPinZ, l.headPinZ);
}

TEST(test_scene_adapter, decor_clears_the_play_area_of_the_real_lane) {
    const auto g = SceneAdapter::geometry(Alley::kLane);
    const Render::Aabb zones[] = {
        {{-0.82f, -0.14f, -4.f}, {0.82f, 0.8f, 19.f}},                         // play corridor
        {{-0.82f, -0.14f, -4.f}, {0.82f, 2.2f, Alley::kLane.length}},          // sight corridor
        {{-0.6f, 1.4f, -4.f}, {0.6f, 2.4f, 13.5f}},                            // camera box
    };
    for (const Render::ThemeId id : Render::kThemes) {
        const auto& th = Render::theme(id);
        const auto props = Render::buildProps(th, g);
        for (std::size_t i = 0; i < props.size(); ++i) {
            const Render::Aabb b = Render::bounds(props[i], g);
            for (const auto& z : zones) {
                const bool overlap = b.min.x < z.max.x && b.max.x > z.min.x && b.min.y < z.max.y &&
                                     b.max.y > z.min.y && b.min.z < z.max.z && b.max.z > z.min.z;
                EXPECT_FALSE(overlap) << "theme " << th.name << " prop " << i;
            }
        }
    }
}
