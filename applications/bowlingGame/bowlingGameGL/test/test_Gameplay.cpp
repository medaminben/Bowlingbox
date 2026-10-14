#include "FollowCamera.h"
#include "Hud.h"
#include "HudText.h"
#include "LaneCommands.h"
#include <Gamebox/Score/Score.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace Gamebox;
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
    EXPECT_TRUE(LaneCommands::execute(Command::MoveRight, lane).isValue());
    EXPECT_GT(lane.snapshot().aimOffset, 0.f);
    EXPECT_TRUE(LaneCommands::execute(Command::StartCharge, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, Phase::Charging);
    EXPECT_TRUE(LaneCommands::execute(Command::NewGame, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, Phase::Aiming);
}

TEST(test_lane_commands, reports_rejections_and_ignores_non_lane_commands) {
    Alley::Lane lane;
    EXPECT_TRUE(LaneCommands::execute(Command::Release, lane).isError());
    EXPECT_TRUE(LaneCommands::execute(Command::Quit, lane).isValue());
    EXPECT_TRUE(LaneCommands::execute(Command::None, lane).isValue());
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
    EXPECT_NEAR(cam.z(), 8.f - 3.5f, 1e-3f);
    s.ball.position = {0.f, 0.1f, 100.f};
    for (int i = 0; i < 600; ++i)
        cam.update(s, 1.f / 60.f);
    EXPECT_NEAR(cam.z(), Alley::kLane.length - 5.5f, 1e-3f);
}

// ------------------------------------------------------------ HudText
TEST(test_hud_text, result_names_strikes_spares_and_gutters) {
    Score::Bowlingbox sheet;
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
    Score::Bowlingbox sheet;
    auto s = snapshotIn(Phase::FrameDone);
    sheet.roll(0);
    sheet.roll(10);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::result(s, sheet), "SPARE!");
}

TEST(test_hud_text, status_line_shows_phase_score_and_standing_pins) {
    Score::Bowlingbox sheet;
    sheet.roll(4);
    auto s = snapshotIn(Phase::Settling);
    s.pinStanding.fill(true);
    s.pinStanding[0] = false;
    EXPECT_EQ(HudText::status(s, sheet), "Settling   |   score 4   |   pins standing 9");
}

// ------------------------------------------------------------ Hud
TEST(test_hud, draws_scoreboard_hint_and_result_without_gl) {
    Score::Bowlingbox sheet;
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
