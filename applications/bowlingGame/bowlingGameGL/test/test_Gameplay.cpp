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

TEST(test_lane_commands, sound_commands_leave_the_lane_alone) {
    Alley::Lane lane;
    const Alley::Snapshot before = lane.snapshot();
    for (auto type : {Command::Type::ToggleMute, Command::Type::VolumeUp, Command::Type::VolumeDown,
                      Command::Type::ToggleAmbience})
        EXPECT_TRUE(LaneCommands::execute({type}, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, before.phase);
}

TEST(test_command_target, every_command_has_one_owner) {
    using T = Command::Type;
    EXPECT_EQ(targetOf(T::None), CommandTarget::None);
    for (auto t : {T::MoveAim, T::RotateAim, T::StartCharge, T::Release, T::NewGame})
        EXPECT_EQ(targetOf(t), CommandTarget::Lane);
    EXPECT_EQ(targetOf(T::CycleTheme), CommandTarget::View);
    for (auto t : {T::ToggleMute, T::VolumeUp, T::VolumeDown, T::ToggleAmbience})
        EXPECT_EQ(targetOf(t), CommandTarget::Sound);
    EXPECT_EQ(targetOf(T::Quit), CommandTarget::App);
}

TEST(test_sound_settings, after_applies_each_sound_command) {
    SoundSettings s;
    EXPECT_TRUE(s.after(Command::Type::ToggleMute).muted);
    EXPECT_EQ(s.after(Command::Type::VolumeUp).volume, 90);
    EXPECT_EQ(s.after(Command::Type::VolumeDown).volume, 70);
    EXPECT_FALSE(s.after(Command::Type::ToggleAmbience).ambience);
}

TEST(test_sound_settings, after_ignores_other_commands) {
    const SoundSettings s;
    for (auto t : {Command::Type::None, Command::Type::NewGame, Command::Type::CycleTheme, Command::Type::Quit})
        EXPECT_EQ(s.after(t), s);
}

TEST(test_sound_settings, volume_steps_by_ten_clamps_and_unmutes) {
    SoundSettings s;
    EXPECT_EQ(s.volume, SoundSettings::kDefaultVolume);
    s.muted = true;
    s.stepVolume(1);
    EXPECT_EQ(s.volume, 90);
    EXPECT_FALSE(s.muted);
    s.stepVolume(5);
    EXPECT_EQ(s.volume, 100);
    s.stepVolume(-20);
    EXPECT_EQ(s.volume, 0);
}

TEST(test_sound_settings, gain_is_the_squared_volume) {
    SoundSettings s;
    s.volume = 50;
    EXPECT_FLOAT_EQ(s.gain(), 0.25f);
    s.volume = 100;
    EXPECT_FLOAT_EQ(s.gain(), 1.f);
    s.volume = 0;
    EXPECT_FLOAT_EQ(s.gain(), 0.f);
}

TEST(test_hud_text, sound_status_shows_volume_mute_and_ambience) {
    SoundSettings s;
    s.volume = 70;
    EXPECT_EQ(HudText::soundStatus(s), "volume 70%");
    s.muted = true;
    EXPECT_EQ(HudText::soundStatus(s), "sound muted");
    s.ambience = false;
    EXPECT_EQ(HudText::soundStatus(s), "sound muted, no ambience");
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

    EXPECT_EQ(sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::result(s, sheet), "STRIKE!");

    EXPECT_EQ(sheet.roll(3), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 3;
    EXPECT_EQ(HudText::result(s, sheet), "3 pins");

    EXPECT_EQ(sheet.roll(7), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 7;
    EXPECT_EQ(HudText::result(s, sheet), "SPARE!");

    EXPECT_EQ(sheet.roll(0), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 0;
    EXPECT_EQ(HudText::result(s, sheet), "Gutter");

    EXPECT_EQ(HudText::result(snapshotIn(Phase::Aiming), sheet), "");
    EXPECT_EQ(HudText::result(snapshotIn(Phase::GameOver), sheet), "Game over - score 30");
}

TEST(test_hud_text, ten_pins_after_a_gutter_ball_is_a_spare) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);
    EXPECT_EQ(sheet.roll(0), Bowlingbox::Score::RollStatus::Accepted);
    EXPECT_EQ(sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::result(s, sheet), "SPARE!");
}

TEST(test_hud_text, throw_result_ten_after_a_gutter_ball_is_a_spare) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);
    EXPECT_EQ(sheet.roll(0), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 0;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Gutter);
    EXPECT_EQ(sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Spare);
}

TEST(test_hud_text, throw_result_names_strikes_and_counts_and_nothing_outside_frame_done) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);
    EXPECT_EQ(sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Strike);
    EXPECT_EQ(sheet.roll(4), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 4;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Pins);
    EXPECT_EQ(HudText::throwResult(snapshotIn(Phase::Rolling), sheet), HudText::ThrowResult::None);
}

TEST(test_hud_text, status_line_shows_phase_score_and_standing_pins) {
    Score::ScoreCalculator sheet;
    EXPECT_EQ(sheet.roll(4), Bowlingbox::Score::RollStatus::Accepted);
    auto s = snapshotIn(Phase::Settling);
    s.pinStanding.fill(true);
    s.pinStanding[0] = false;
    EXPECT_EQ(HudText::status(s, sheet), "Settling   |   score 4   |   pins standing 9");
}

// ------------------------------------------------------------ Hud
TEST(test_hud, draws_scoreboard_hint_and_result_without_gl) {
    Score::ScoreCalculator sheet;
    EXPECT_EQ(sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
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
    for (const char* key : {"Left / Right", "A / D", "Space", "N", "Esc", "Ctrl+Q", "T: change the scenery",
                            "M: mute sound", "+ / -: volume", "B: background ambience"})
        EXPECT_NE(help.find(key), std::string::npos) << key;
}

TEST(test_hud_text, hint_offers_the_theme_key_only_while_standing_still) {
    EXPECT_NE(HudText::hint(Phase::Aiming).find("T theme"), std::string::npos);
    EXPECT_NE(HudText::hint(Phase::GameOver).find("T: theme"), std::string::npos);
    EXPECT_EQ(HudText::hint(Phase::Charging), "release to throw");
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
