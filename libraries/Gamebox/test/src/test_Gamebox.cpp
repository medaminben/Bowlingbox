// Include headers of other components from this project path
// relative to the "include" directory of the component, just like external projects
// would
#include <Sandbox/Gamebox/Gamebox.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <iostream>
#include <string>
using namespace Sandbox::Gamebox;

class test_gamebox : public ::testing::Test {
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
TEST_F(test_gamebox, roll_all_zeros) {
    roll_n_times(20, 0);
    auto result = bowlingGame->score();
    EXPECT_EQ(result, 0);
}
TEST_F(test_gamebox, roll_all_ones) {
    roll_n_times(20, 1);
    auto result = bowlingGame->score();
    EXPECT_EQ(result, 20);
}

TEST_F(test_gamebox, roll_a_spare) {
    bowlingGame->roll(5);
    bowlingGame->roll(5);
    bowlingGame->roll(6);
    roll_n_times(17, 0);
    EXPECT_EQ(22, bowlingGame->score());
}

TEST_F(test_gamebox, roll_a_perfect_game) {
    roll_n_times(12, 10);
    EXPECT_EQ(300, bowlingGame->score());
}

TEST_F(test_gamebox, play_a_normal_game) {
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

TEST_F(test_gamebox, isOver_returns_true) {
    roll_n_times(20, 0);
    EXPECT_TRUE(bowlingGame->isOver());
}

TEST_F(test_gamebox, isOver_returns_false_spare) {
    roll_n_times(18, 0);
    bowlingGame->roll(1);
    bowlingGame->roll(9);
    EXPECT_FALSE(bowlingGame->isOver());
}

TEST_F(test_gamebox, isOver_returns_true_nothing) {
    roll_n_times(18, 0);
    bowlingGame->roll(1);
    bowlingGame->roll(2);
    EXPECT_TRUE(bowlingGame->isOver());
}

TEST_F(test_gamebox, isOver_returns_false_strike) {
    roll_n_times(18, 0);
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    EXPECT_FALSE(bowlingGame->isOver());
}

TEST_F(test_gamebox, isOver_returns_true_strike) {
    roll_n_times(18, 0);
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    EXPECT_TRUE(bowlingGame->isOver());
}

TEST_F(test_gamebox, frame_score) {
    roll_n_times(4, 10);
    EXPECT_EQ(60, bowlingGame->getFrameScore(1));
    EXPECT_EQ(90, bowlingGame->getFrameScore(9));
}

TEST_F(test_gamebox, frame_score_perfect_game) {
    roll_n_times(12, 10);
    EXPECT_EQ(300, bowlingGame->getFrameScore(9));
}

TEST_F(test_gamebox, frame_score_reset_game) {
    bowlingGame->roll(10);
    bowlingGame->roll(10);
    bowlingGame->reset();
    EXPECT_EQ(0, bowlingGame->getFrameScore(9));
}

TEST_F(test_gamebox, roll_almost_perfect_game) {
    roll_n_times(10, 10);
    EXPECT_EQ(270, bowlingGame->score());
}
