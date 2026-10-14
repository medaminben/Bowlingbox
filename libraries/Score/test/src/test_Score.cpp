// Include headers of other components from this project path
// relative to the "include" directory of the component, just like external projects
// would
#include <Gamebox/Score/Score.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <iostream>
#include <initializer_list>
#include <string>
using namespace Gamebox::Score;

class test_score : public ::testing::Test {
public:
    std::unique_ptr<Bowlingbox> bowlingGame;
    void SetUp() { bowlingGame = std::make_unique<Bowlingbox>(); }
    void TearDown() {}
    void roll_n_times(int n, int pins) {
        for (auto i = 0; i < n; i++) {
            bowlingGame->roll(pins);
        }
    }
};

// Test case names must start with the name of the executable they are
TEST_F(test_score, roll_all_zeros) {
    roll_n_times(20, 0);
    auto result = bowlingGame->score();
    EXPECT_EQ(result, 0);
}
TEST_F(test_score, roll_all_ones) {
    roll_n_times(20, 1);
    auto result = bowlingGame->score();
    EXPECT_EQ(result, 20);
}

TEST_F(test_score, roll_a_spare) {
    bowlingGame->roll(5);
    bowlingGame->roll(5);
    bowlingGame->roll(6);
    roll_n_times(17, 0);
    EXPECT_EQ(22, bowlingGame->score());
}

TEST_F(test_score, roll_a_perfect_game) {
    roll_n_times(12, 10);
    EXPECT_EQ(300, bowlingGame->score());
}

TEST_F(test_score, play_a_normal_game) {
    bowlingGame->roll(5);   // 0 | score 5
    bowlingGame->roll(0);   // 1 | score 5
    bowlingGame->roll(10);  // 2 and 3 | score 30
    bowlingGame->roll(10);  // 4 and 5 | score 50
    bowlingGame->roll(5);   // 6 | score 55
    bowlingGame->roll(5);   // 7 | score 64
    bowlingGame->roll(4);   // 8 and 9 | score 68
    roll_n_times(11, 0);
    // result should be 68
    EXPECT_EQ(68, bowlingGame->score());
}

TEST_F(test_score, isOver_returns_true) {
    roll_n_times(20, 0);
    EXPECT_TRUE(bowlingGame->isOver());
}

TEST_F(test_score, isOver_returns_false_spare) {
    roll_n_times(18, 0);
    bowlingGame->roll(1);
    bowlingGame->roll(9);
    EXPECT_FALSE(bowlingGame->isOver());
}

TEST_F(test_score, isOver_returns_true_nothing) {
    roll_n_times(18, 0);
    bowlingGame->roll(1);
    bowlingGame->roll(2);
    EXPECT_TRUE(bowlingGame->isOver());
}

TEST_F(test_score, isOver_returns_false_strike) {
    roll_n_times(18, 0);
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    EXPECT_FALSE(bowlingGame->isOver());
}

TEST_F(test_score, isOver_returns_true_strike) {
    roll_n_times(18, 0);
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    EXPECT_TRUE(bowlingGame->isOver());
}

TEST_F(test_score, frame_score) {
    roll_n_times(4, 10);
    EXPECT_EQ(60, bowlingGame->frameScore(1));
    EXPECT_EQ(90, bowlingGame->frameScore(9));
}

TEST_F(test_score, frame_score_perfect_game) {
    roll_n_times(12, 10);
    EXPECT_EQ(300, bowlingGame->frameScore(9));
}

TEST_F(test_score, frame_score_reset_game) {
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    bowlingGame->reset();
    EXPECT_EQ(0, bowlingGame->frameScore(9));
}

TEST_F(test_score, roll_almost_perfect_game) {
    roll_n_times(10, 10);
    EXPECT_EQ(270, bowlingGame->score());
}

TEST_F(test_score, roll_returns_true_while_playing) {
    EXPECT_TRUE(bowlingGame->roll(3));
    EXPECT_TRUE(bowlingGame->roll(4));
}

TEST_F(test_score, roll_returns_false_when_game_is_over) {
    roll_n_times(20, 0);
    EXPECT_FALSE(bowlingGame->roll(1));
}

TEST_F(test_score, new_game_starts_blank_without_reset) {
    for (int value : bowlingGame->rolls())
        EXPECT_EQ(value, toInt(Mark::None));
    for (int value : bowlingGame->scores())
        EXPECT_EQ(value, toInt(Mark::None));
    EXPECT_EQ(bowlingGame->pinsLeft(), kPins);
    EXPECT_FALSE(bowlingGame->isOver());
}

TEST_F(test_score, usable_through_the_interfaces) {
    IBowlingGame& game = *bowlingGame;
    game.roll(10);
    const IScoreSheet& sheet = game;
    EXPECT_EQ(sheet.rolls()[0], toInt(Mark::Strike));
    EXPECT_EQ(sheet.rolls()[1], toInt(Mark::Empty));
    EXPECT_EQ(static_cast<int>(sheet.rolls().size()), kRollSlots);
    EXPECT_EQ(static_cast<int>(sheet.scores().size()), kFrames);
}

TEST(test_score_notation, follows_score_sheet_conventions) {
    EXPECT_EQ(notation(toInt(Mark::Empty)), "");
    EXPECT_EQ(notation(toInt(Mark::None)), "");
    EXPECT_EQ(notation(toInt(Mark::Strike)), "X");
    EXPECT_EQ(notation(toInt(Mark::Spare)), "/");
    EXPECT_EQ(notation(toInt(Mark::Miss)), "-");
    EXPECT_EQ(notation(0, true), "0");
    EXPECT_EQ(notation(7), "7");
    EXPECT_EQ(notation(300, true), "300");
}

namespace {
void rollAll(Bowlingbox& game, std::initializer_list<int> balls) {
    for (int pins : balls)
        ASSERT_TRUE(game.roll(pins)) << "roll(" << pins << ") was refused";
}
void rollZeros(Bowlingbox& game, int n) {
    for (int i = 0; i < n; ++i)
        game.roll(0);
}
}  // namespace

TEST(test_score_tenth_frame, strike_then_open_balls_gets_both_bonus_balls) {
    Bowlingbox game;
    rollZeros(game, 18);
    rollAll(game, {10, 3});
    EXPECT_FALSE(game.isOver());
    EXPECT_EQ(game.pinsLeft(), 7);
    EXPECT_FALSE(game.roll(8));  // only 7 pins are standing
    rollAll(game, {4});
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.score(), 17);
}

TEST(test_score_tenth_frame, strike_then_spare) {
    Bowlingbox game;
    rollZeros(game, 18);
    rollAll(game, {10, 3, 7});
    const auto r = game.rolls();
    EXPECT_EQ(r[18], toInt(Mark::Strike));
    EXPECT_EQ(r[19], 3);
    EXPECT_EQ(r[20], toInt(Mark::Spare));
    EXPECT_EQ(game.score(), 20);
}

TEST(test_score_tenth_frame, two_strikes_then_open_ball) {
    Bowlingbox game;
    rollZeros(game, 18);
    rollAll(game, {10, 10, 3});
    const auto r = game.rolls();
    EXPECT_EQ(r[18], toInt(Mark::Strike));
    EXPECT_EQ(r[19], toInt(Mark::Strike));
    EXPECT_EQ(r[20], 3);
    EXPECT_EQ(game.score(), 23);
}

TEST(test_score_tenth_frame, spare_then_strike) {
    Bowlingbox game;
    rollZeros(game, 18);
    rollAll(game, {3, 7, 10});
    const auto r = game.rolls();
    EXPECT_EQ(r[18], 3);
    EXPECT_EQ(r[19], toInt(Mark::Spare));
    EXPECT_EQ(r[20], toInt(Mark::Strike));
    EXPECT_EQ(game.score(), 20);
}

TEST(test_score_tenth_frame, gutter_then_ten_is_a_spare) {
    Bowlingbox game;
    rollZeros(game, 18);
    rollAll(game, {0, 10, 5});
    const auto r = game.rolls();
    EXPECT_EQ(r[18], toInt(Mark::Miss));
    EXPECT_EQ(r[19], toInt(Mark::Spare));
    EXPECT_EQ(r[20], 5);
    EXPECT_EQ(game.score(), 15);
}

TEST(test_score_validation, refuses_impossible_pin_counts) {
    Bowlingbox game;
    EXPECT_FALSE(game.roll(11));
    EXPECT_FALSE(game.roll(-1));
    EXPECT_TRUE(game.roll(7));
    EXPECT_FALSE(game.roll(4));  // 7 + 4 > 10 in one frame
    EXPECT_TRUE(game.roll(3));
    EXPECT_EQ(game.score(), 10);
    EXPECT_EQ(game.rolls()[1], toInt(Mark::Spare));
}

TEST(test_score_validation, regular_frame_gutter_then_ten_is_a_spare) {
    Bowlingbox game;
    rollAll(game, {0, 10});
    EXPECT_EQ(game.rolls()[0], toInt(Mark::Miss));
    EXPECT_EQ(game.rolls()[1], toInt(Mark::Spare));
}

TEST(test_score_validation, no_pins_left_after_the_game) {
    Bowlingbox game;
    rollZeros(game, 20);
    EXPECT_TRUE(game.isOver());
    EXPECT_EQ(game.pinsLeft(), 0);
}

TEST(test_score_totals, frame_total_is_blank_until_its_bonus_is_known) {
    Bowlingbox game;
    rollAll(game, {10, 3});
    EXPECT_EQ(game.scores()[0], toInt(Mark::None));
    EXPECT_EQ(game.scores()[1], toInt(Mark::None));
    EXPECT_EQ(game.score(), 16);  // running total still counts what was thrown
    rollAll(game, {4});
    EXPECT_EQ(game.scores()[0], 17);
    EXPECT_EQ(game.scores()[1], 24);
    EXPECT_EQ(game.scores()[2], toInt(Mark::None));
}

TEST(test_score_totals, frame_score_clamps_the_frame_index) {
    Bowlingbox game;
    for (int i = 0; i < 20; ++i)
        game.roll(1);
    EXPECT_EQ(game.frameScore(10), 20);
    EXPECT_EQ(game.frameScore(-1), 2);
}
