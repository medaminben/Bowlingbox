#include "SoundDirector.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

using namespace Bowlingbox;
using Alley::Body;
using Alley::Impact;
using Alley::Phase;
using Audio::LoopSlot;
using Audio::SoundId;
using Audio::Voice;

namespace {
constexpr float kDt = 1.f / 60.f;

class RecordingOutput final : public Audio::IAudioOutput {
public:
    std::vector<std::pair<SoundId, Voice>> played;
    std::array<std::optional<SoundId>, Audio::kLoopSlotCount> loops {};
    std::array<Voice, Audio::kLoopSlotCount> loopVoices {};

    void play(SoundId id, const Voice& v) override { played.emplace_back(id, v); }
    void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& v) override {
        loops[static_cast<std::size_t>(slot)] = id;
        loopVoices[static_cast<std::size_t>(slot)] = v;
    }
    void setMuted(bool m) override { _muted = m; }
    bool muted() const override { return _muted; }
    void setVolume(float v) override { _volume = v; }
    float volume() const override { return _volume; }

    int count(SoundId id) const {
        return static_cast<int>(std::count_if(played.begin(), played.end(), [id](const auto& p) { return p.first == id; }));
    }
    std::optional<SoundId> loop(LoopSlot s) const { return loops[static_cast<std::size_t>(s)]; }
    const Voice& loopVoice(LoopSlot s) const { return loopVoices[static_cast<std::size_t>(s)]; }

private:
    bool _muted = false;
    float _volume = 1.f;
};

Alley::Snapshot at(Phase p, float x = 0.f, float z = 0.f, float y = 0.109f) {
    Alley::Snapshot s;
    s.phase = p;
    s.ball.position = {x, y, z};
    return s;
}

struct Fixture {
    RecordingOutput out;
    SoundDirector director {out};
    Score::ScoreCalculator sheet;
    void update(const Alley::Snapshot& s, std::vector<Impact> impacts = {}, float dt = kDt) {
        director.update(s, sheet, impacts, dt);
    }
};
}  // namespace

TEST(test_sound_director, first_update_only_records_the_snapshot) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.f, 5.f), {{Body::Ball, Body::Pin, 3.f, {}}});
    EXPECT_TRUE(f.out.played.empty());
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
}

TEST(test_sound_director, release_plays_once_when_charging_turns_into_rolling) {
    Fixture f;
    f.update(at(Phase::Charging));
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    f.update(at(Phase::Rolling, 0.f, 0.2f));
    EXPECT_EQ(f.out.count(SoundId::Release), 1);
}

// Charge and release inside one timer tick: the director never sees Charging.
TEST(test_sound_director, aiming_straight_into_rolling_releases_and_rearms_the_gutter_drop) {
    Fixture f;
    f.update(at(Phase::Aiming));
    f.update(at(Phase::Rolling, 0.6f, 0.1f));
    f.update(at(Phase::Rolling, 0.6f, 0.2f));
    EXPECT_EQ(f.out.count(SoundId::Release), 1);
    EXPECT_EQ(f.out.count(SoundId::GutterDrop), 1);
    f.update(at(Phase::FrameDone, 0.6f, 18.f));
    f.update(at(Phase::Aiming));
    f.update(at(Phase::Rolling, 0.6f, 0.1f));
    EXPECT_EQ(f.out.count(SoundId::Release), 2);
    EXPECT_EQ(f.out.count(SoundId::GutterDrop), 2);
}

TEST(test_sound_director, charge_loop_pitch_follows_power_and_stops_after_release) {
    Fixture f;
    f.update(at(Phase::Aiming));
    auto s = at(Phase::Charging);
    s.power = 0.5f;
    f.update(s);
    EXPECT_EQ(f.out.loop(LoopSlot::Charge), SoundId::ChargeTone);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Charge).pitch, 1.1f, 1e-5f);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Charge).volume, SoundDirector::kChargeVolume, 1e-5f);
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    EXPECT_FALSE(f.out.loop(LoopSlot::Charge));
}

TEST(test_sound_director, roll_loop_volume_pitch_and_pan_follow_the_ball) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.3825f, 0.f));
    f.update(at(Phase::Rolling, 0.3825f, 0.1f));  // 0.1 m in 1/60 s = 6 m/s
    ASSERT_EQ(f.out.loop(LoopSlot::Roll), SoundId::Roll);
    const Voice& v = f.out.loopVoice(LoopSlot::Roll);
    EXPECT_NEAR(v.volume, SoundDirector::kRollVolume * 0.625f, 1e-4f);  // 0.25 + 0.75 * 0.5
    EXPECT_NEAR(v.pitch, 1.0f, 1e-4f);     // 0.8 + 0.4 * 0.5
    EXPECT_NEAR(v.pan, 0.5f, 1e-4f);       // 0.3825 / 0.765
}

TEST(test_sound_director, gutter_lowers_the_roll_pitch_and_drops_once) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.6f, 0.f));
    f.update(at(Phase::Rolling, 0.6f, 0.1f));
    f.update(at(Phase::Rolling, 0.6f, 0.2f));
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Roll).pitch, 0.85f, 1e-4f);
    EXPECT_EQ(f.out.count(SoundId::GutterDrop), 1);
}

TEST(test_sound_director, roll_stops_in_the_pit_when_slow_and_outside_rolling) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.f, 0.f));
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    ASSERT_TRUE(f.out.loop(LoopSlot::Roll));
    f.update(at(Phase::Settling, 0.f, 18.5f, -0.5f));  // in the pit
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
    f.update(at(Phase::Settling, 0.f, 18.f));
    f.update(at(Phase::Settling, 0.f, 18.f));  // on the lane but not moving
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
    f.update(at(Phase::Aiming, 0.f, 5.f));  // teleport while aiming: never a roll
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
}

TEST(test_sound_director, impacts_map_to_hit_and_clatter_with_scaled_volume_and_pan) {
    Fixture f;
    f.update(at(Phase::Rolling));
    f.update(at(Phase::Rolling), {{Body::Pin, Body::Ball, 2.f, {0.3825f, 0.2f, 17.4f}},  // order does not matter
                                  {Body::Pin, Body::Lane, 0.75f, {-0.3825f, 0.f, 17.7f}},
                                  {Body::Pin, Body::Pin, 0.2f, {}},     // below threshold
                                  {Body::Ball, Body::Lane, 5.f, {}},    // no sound
                                  {Body::Pin, Body::Other, 5.f, {}}});  // unknown body
    ASSERT_EQ(f.out.played.size(), 2u);
    EXPECT_EQ(f.out.played[0].first, SoundId::PinHit);
    EXPECT_NEAR(f.out.played[0].second.volume, 0.5f, 1e-5f);
    EXPECT_NEAR(f.out.played[0].second.pan, 0.5f, 1e-4f);
    EXPECT_EQ(f.out.played[1].first, SoundId::PinClatter);
    EXPECT_NEAR(f.out.played[1].second.volume, 0.5f, 1e-5f);
    EXPECT_NEAR(f.out.played[1].second.pan, -0.5f, 1e-4f);
}

TEST(test_sound_director, at_most_four_impact_sounds_per_update_strongest_first) {
    Fixture f;
    f.update(at(Phase::Rolling));
    std::vector<Impact> burst;
    for (float impulse : {0.4f, 0.9f, 0.5f, 1.2f, 0.6f, 0.7f})
        burst.push_back({Body::Pin, Body::Pin, impulse, {}});
    f.update(at(Phase::Rolling), burst);
    ASSERT_EQ(f.out.played.size(), 4u);
    EXPECT_NEAR(f.out.played[0].second.volume, 0.8f, 1e-5f);  // 1.2 / 1.5 capped at 0.8
    EXPECT_NEAR(f.out.played[3].second.volume, 0.6f / 1.5f, 1e-5f);
    EXPECT_NEAR(f.out.played[0].second.pitch, 1.0f, 1e-5f);
    EXPECT_NEAR(f.out.played[1].second.pitch, 0.96f, 1e-5f);
}

TEST(test_sound_director, the_same_kind_waits_for_its_cooldown) {
    Fixture f;
    const std::vector<Impact> clatter {{Body::Pin, Body::Pin, 1.f, {}}};
    f.update(at(Phase::Rolling));
    f.update(at(Phase::Rolling), clatter);  // plays
    f.update(at(Phase::Rolling), clatter);  // 16 ms later: too soon
    f.update(at(Phase::Rolling), clatter);  // 33 ms after the first: plays
    EXPECT_EQ(f.out.count(SoundId::PinClatter), 2);
    f.update(at(Phase::Rolling), {{Body::Ball, Body::Pin, 1.f, {}}});  // the other kind is independent
    EXPECT_EQ(f.out.count(SoundId::PinHit), 1);
}

TEST(test_sound_director, strike_stinger_plays_once_on_entering_frame_done) {
    Fixture f;
    f.update(at(Phase::Settling));
    EXPECT_EQ(f.sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
    auto s = at(Phase::FrameDone);
    s.lastPinsDown = 10;
    f.update(s);
    f.update(s);
    EXPECT_EQ(f.out.count(SoundId::Strike), 1);
}

TEST(test_sound_director, ten_after_a_gutter_ball_plays_the_spare_stinger) {
    Fixture f;
    f.update(at(Phase::Settling));
    EXPECT_EQ(f.sheet.roll(0), Bowlingbox::Score::RollStatus::Accepted);
    auto s = at(Phase::FrameDone);
    s.lastPinsDown = 0;
    f.update(s);
    EXPECT_TRUE(f.out.played.empty());  // a gutter ball has no stinger
    f.update(at(Phase::Settling));
    EXPECT_EQ(f.sheet.roll(10), Bowlingbox::Score::RollStatus::Accepted);
    s.lastPinsDown = 10;
    f.update(s);
    EXPECT_EQ(f.out.count(SoundId::Spare), 1);
    EXPECT_EQ(f.out.count(SoundId::Strike), 0);
}

TEST(test_sound_director, a_plain_count_plays_no_stinger) {
    Fixture f;
    f.update(at(Phase::Settling));
    EXPECT_EQ(f.sheet.roll(7), Bowlingbox::Score::RollStatus::Accepted);
    auto s = at(Phase::FrameDone);
    s.lastPinsDown = 7;
    f.update(s);
    EXPECT_TRUE(f.out.played.empty());
}

TEST(test_sound_director, theme_sets_the_ambience_and_announces_only_when_asked) {
    Fixture f;
    f.director.setTheme(Render::ThemeId::ClassicAlley, false);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientClassic);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Ambient).volume, SoundDirector::kAmbientVolume, 1e-5f);
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 0);
    f.director.setTheme(Render::ThemeId::Outdoor, true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientOutdoor);
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 1);
    f.director.setTheme(Render::ThemeId::NeonHall, true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientNeon);
}

TEST(test_sound_director, new_game_chimes_and_stops_roll_and_charge_but_not_the_ambience) {
    Fixture f;
    f.director.setTheme(Render::ThemeId::NeonHall, false);
    f.update(at(Phase::Rolling, 0.f, 0.f));
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    ASSERT_TRUE(f.out.loop(LoopSlot::Roll));
    f.director.onNewGame();
    EXPECT_EQ(f.out.count(SoundId::NewGame), 1);
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
    EXPECT_FALSE(f.out.loop(LoopSlot::Charge));
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientNeon);
}

TEST(test_sound_director, ambience_toggle_stops_and_restores_the_theme_loop) {
    Fixture f;
    f.director.setTheme(Render::ThemeId::Outdoor, false);
    f.director.setAmbienceEnabled(false);
    EXPECT_FALSE(f.director.ambienceEnabled());
    EXPECT_FALSE(f.out.loop(LoopSlot::Ambient));
    f.director.setTheme(Render::ThemeId::ClassicAlley, true);  // switching theme keeps it off
    EXPECT_FALSE(f.out.loop(LoopSlot::Ambient));
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 1);
    f.director.setAmbienceEnabled(true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientClassic);
}

TEST(test_sound_director, ambience_stays_silent_until_a_theme_is_set) {
    Fixture f;
    f.director.setAmbienceEnabled(true);
    EXPECT_FALSE(f.out.loop(LoopSlot::Ambient));
}
