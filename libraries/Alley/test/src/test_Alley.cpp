#include <Gamebox/Alley/Alley.h>
#include <Gamebox/Alley/PhysicsWorld.h>
#include <gtest/gtest.h>
#include <cmath>
#include <vector>

using namespace Gamebox::Alley;

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kDt = 1.f / 60.f;

/// Time at which the 1 Hz charge curve power(t) = (1 - cos 2πt)/2 reaches `power`.
float chargeTimeFor(float power) {
    return std::acos(1.f - 2.f * power) / (2.f * kPi);
}

int standingCount(const Snapshot& s) {
    int n = 0;
    for (bool b : s.pinStanding)
        n += b ? 1 : 0;
    return n;
}

/// Full throw: aim, charge to `power`, release, step until the ball is scored
/// and the lane is back in Aiming (or GameOver).
void throwBall(Lane& lane, float offset, float angleDeg, float power) {
    const auto s = lane.snapshot();
    lane.moveAim(offset - s.aimOffset);
    lane.rotateAim(angleDeg - s.aimAngle);
    lane.startCharge();
    lane.step(chargeTimeFor(power));
    lane.release();
    for (int i = 0; i < 60 * 60; ++i) {
        const auto phase = lane.snapshot().phase;
        if (phase == Phase::Aiming || phase == Phase::GameOver)
            return;
        lane.step(kDt);
    }
}

/// Lateral offset that produces a strike with power 0.7 and angle 0 in this
/// deterministic world. Tuned once in Task 4; the README uses the same value.
constexpr float kPocketOffset = 0.078f;
constexpr float kStrikePower = 0.8f;
}  // namespace

class test_alley : public ::testing::Test {
public:
    Lane lane;
};

TEST_F(test_alley, initial_state_is_aiming_with_full_rack) {
    const auto s = lane.snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_EQ(standingCount(s), 10);
    EXPECT_EQ(s.lastPinsDown, -1);
    EXPECT_FLOAT_EQ(s.aimOffset, 0.f);
    EXPECT_EQ(lane.scoreSheet().score(), 0);
    EXPECT_FALSE(lane.scoreSheet().isOver());
}

TEST_F(test_alley, release_in_aiming_is_an_error) {
    auto r = lane.release();
    EXPECT_TRUE(r.isError());
    EXPECT_STREQ(r.error().c_str(), "not charging");
}

TEST_F(test_alley, start_charge_twice_is_an_error) {
    EXPECT_TRUE(lane.startCharge().isValue());
    auto r = lane.startCharge();
    EXPECT_TRUE(r.isError());
    EXPECT_STREQ(r.error().c_str(), "not aiming");
}

TEST_F(test_alley, move_aim_clamps_to_range) {
    EXPECT_TRUE(lane.moveAim(5.f).isValue());
    EXPECT_FLOAT_EQ(lane.snapshot().aimOffset, kMaxAimOffset);
    EXPECT_FLOAT_EQ(lane.snapshot().ball.position[0], kMaxAimOffset);
    EXPECT_TRUE(lane.rotateAim(-90.f).isValue());
    EXPECT_FLOAT_EQ(lane.snapshot().aimAngle, -kMaxAimAngle);
}

TEST_F(test_alley, aim_while_rolling_is_an_error) {
    lane.startCharge();
    lane.step(0.25f);
    lane.release();
    EXPECT_EQ(lane.snapshot().phase, Phase::Rolling);
    auto r = lane.moveAim(0.1f);
    EXPECT_TRUE(r.isError());
    EXPECT_STREQ(r.error().c_str(), "ball in motion");
}

TEST_F(test_alley, charging_power_follows_one_hertz_cosine) {
    lane.startCharge();
    lane.step(0.25f);
    EXPECT_NEAR(lane.snapshot().power, 0.5f, 1e-4f);
    lane.step(0.25f);
    EXPECT_NEAR(lane.snapshot().power, 1.0f, 1e-4f);
    lane.step(0.5f);
    EXPECT_NEAR(lane.snapshot().power, 0.0f, 1e-4f);
}

TEST_F(test_alley, new_game_resets_everything) {
    lane.moveAim(0.3f);
    lane.startCharge();
    EXPECT_TRUE(lane.newGame().isValue());
    const auto s = lane.snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_FLOAT_EQ(s.aimOffset, 0.f);
    EXPECT_EQ(standingCount(s), 10);
    EXPECT_EQ(lane.scoreSheet().score(), 0);
}

TEST_F(test_alley, gutter_ball_scores_zero_and_keeps_the_rack) {
    throwBall(lane, kMaxAimOffset, 0.f, 0.6f);
    const auto s = lane.snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_EQ(s.lastPinsDown, 0);
    EXPECT_EQ(standingCount(s), 10);
    EXPECT_EQ(lane.scoreSheet().rolls()[0], 0);  // Miss
    EXPECT_EQ(lane.scoreSheet().pinsLeft(), 10);
}

TEST_F(test_alley, pocket_throw_is_a_strike) {
    throwBall(lane, kPocketOffset, 0.f, kStrikePower);
    const auto s = lane.snapshot();
    EXPECT_EQ(s.lastPinsDown, 10);
    EXPECT_EQ(lane.scoreSheet().rolls()[0], -2);  // Strike
    EXPECT_EQ(standingCount(s), 10);  // fresh rack for the next frame
    // Gamebox reports provisional running frame score once the strike is recorded.
    EXPECT_EQ(lane.scoreSheet().scores()[0], 10);
}

TEST_F(test_alley, same_inputs_give_identical_results) {
    Lane other;
    throwBall(lane, 0.2f, 1.5f, 0.5f);
    throwBall(other, 0.2f, 1.5f, 0.5f);
    const auto a = lane.snapshot();
    const auto b = other.snapshot();
    EXPECT_EQ(a.lastPinsDown, b.lastPinsDown);
    EXPECT_EQ(a.pinStanding, b.pinStanding);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(a.pins[i].position, b.pins[i].position);
}

TEST_F(test_alley, second_ball_keeps_standing_pins) {
    throwBall(lane, 0.42f, 0.f, 0.4f);  // edge hit: partial knockdown
    const auto s = lane.snapshot();
    const int down = s.lastPinsDown;
    ASSERT_GE(down, 1);
    ASSERT_LT(down, 10);
    EXPECT_EQ(standingCount(s), 10 - down);
    EXPECT_EQ(lane.scoreSheet().pinsLeft(), 10 - down);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(s.pinInPlay[i], s.pinStanding[i]);  // fallen pins were removed
}

TEST_F(test_alley, fallen_pins_stay_visible_until_aiming) {
    lane.moveAim(kPocketOffset);
    lane.startCharge();
    lane.step(chargeTimeFor(kStrikePower));
    lane.release();
    for (int i = 0; i < 60 * 20 && lane.snapshot().phase != Phase::FrameDone; ++i)
        lane.step(kDt);
    const auto s = lane.snapshot();
    ASSERT_EQ(s.phase, Phase::FrameDone);
    for (bool inPlay : s.pinInPlay)
        EXPECT_TRUE(inPlay);
}

TEST_F(test_alley, twenty_gutter_balls_end_the_game_with_zero) {
    for (int i = 0; i < 20; ++i)
        throwBall(lane, kMaxAimOffset, 0.f, 0.5f);
    EXPECT_EQ(lane.snapshot().phase, Phase::GameOver);
    EXPECT_TRUE(lane.scoreSheet().isOver());
    EXPECT_EQ(lane.scoreSheet().score(), 0);
    EXPECT_TRUE(lane.startCharge().isError());
}

TEST_F(test_alley, twelve_strikes_score_three_hundred) {
    for (int i = 0; i < 12; ++i)
        throwBall(lane, kPocketOffset, 0.f, kStrikePower);
    EXPECT_EQ(lane.snapshot().phase, Phase::GameOver);
    EXPECT_EQ(lane.scoreSheet().score(), 300);
}

// ------------------------------------------------------------ injected collaborators
namespace {
/// Deterministic stand-in for Bullet: the ball drops into the pit as soon as it
/// is launched and knocks down the first `knockDown` standing pins.
class FakeWorld final : public IPhysicsWorld {
public:
    int knockDown = 0;
    int rebuilds = 0;

    void step(float) override {}
    void placeBall(float) override { _launched = false; }
    void launchBall(float, float, float) override {
        _launched = true;
        int left = knockDown;
        for (int i = 0; i < 10 && left > 0; ++i)
            if (_inPlay[i] && _standing[i]) {
                _standing[i] = false;
                --left;
            }
    }
    void resetRack() override {
        _standing.fill(true);
        _inPlay.fill(true);
    }
    void rebuild() override {
        ++rebuilds;
        resetRack();
    }
    void removePin(int i) override { _inPlay[i] = false; }
    Pose ballPose() const override { return {}; }
    Pose pinPose(int) const override { return {}; }
    bool isPinStanding(int i) const override { return _inPlay[i] && _standing[i]; }
    bool isPinInPlay(int i) const override { return _inPlay[i]; }
    bool ballInPit() const override { return _launched; }
    float ballSpeed() const override { return 0.f; }
    bool allSlow() const override { return true; }

private:
    bool _launched = false;
    std::array<bool, 10> _standing {};
    std::array<bool, 10> _inPlay {};
};

/// Records every roll; reports "over" once `rollsUntilOver` rolls were made.
class FakeGame final : public Gamebox::Score::IBowlingGame {
public:
    std::vector<int> recorded;
    int rollsUntilOver = 100;
    int pinsLeftValue = 10;

    bool roll(int pins) override {
        if (isOver())
            return false;
        recorded.push_back(pins);
        return true;
    }
    void reset() override { recorded.clear(); }
    std::vector<int> rolls() const override { return recorded; }
    std::vector<int> scores() const override { return {}; }
    int score() const override { return 0; }
    int frameScore(int) const override { return 0; }
    int pinsLeft() const override { return pinsLeftValue; }
    bool isOver() const override { return static_cast<int>(recorded.size()) >= rollsUntilOver; }
};

void throwAndWait(Lane& lane, float seconds) {
    lane.startCharge();
    lane.step(0.25f);
    lane.release();
    for (float t = 0.f; t < seconds; t += kDt)
        lane.step(kDt);
}

struct InjectedLane {
    FakeWorld* world;
    FakeGame* game;
    std::unique_ptr<Lane> lane;
};
InjectedLane makeInjectedLane() {
    auto world = std::make_unique<FakeWorld>();
    auto game = std::make_unique<FakeGame>();
    InjectedLane l {world.get(), game.get(), nullptr};
    l.lane = std::make_unique<Lane>(std::move(world), std::move(game));
    return l;
}
}  // namespace

TEST(test_alley_injected, fallen_pins_are_credited_to_the_scorer) {
    auto l = makeInjectedLane();
    l.world->knockDown = 7;
    throwAndWait(*l.lane, 0.1f);
    EXPECT_EQ(l.lane->snapshot().phase, Phase::FrameDone);
    ASSERT_EQ(l.game->recorded.size(), 1u);
    EXPECT_EQ(l.game->recorded[0], 7);
    EXPECT_EQ(l.lane->snapshot().lastPinsDown, 7);
}

TEST(test_alley_injected, second_ball_removes_fallen_pins) {
    auto l = makeInjectedLane();
    l.world->knockDown = 7;
    l.game->pinsLeftValue = 3;
    throwAndWait(*l.lane, 2.f);
    const auto s = l.lane->snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_EQ(standingCount(s), 3);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(s.pinInPlay[i], s.pinStanding[i]);
}

TEST(test_alley_injected, full_rack_is_rebuilt_when_the_scorer_says_so) {
    auto l = makeInjectedLane();
    l.world->knockDown = 10;
    const int before = l.world->rebuilds;
    throwAndWait(*l.lane, 2.f);
    EXPECT_EQ(l.world->rebuilds, before + 1);
    EXPECT_EQ(standingCount(l.lane->snapshot()), 10);
}

TEST(test_alley_injected, game_over_is_decided_by_the_scorer) {
    auto l = makeInjectedLane();
    l.game->rollsUntilOver = 1;
    throwAndWait(*l.lane, 2.f);
    EXPECT_EQ(l.lane->snapshot().phase, Phase::GameOver);
    EXPECT_TRUE(l.lane->scoreSheet().isOver());
}
