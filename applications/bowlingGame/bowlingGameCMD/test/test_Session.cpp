#include "session.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using Bowlingbox::Score::Mark;
using Bowlingbox::Score::ScoreCalculator;
using Bowlingbox::Score::toInt;
using Mode = Console::Session::Mode;

class test_session : public ::testing::Test {
public:
    std::shared_ptr<ScoreCalculator> game = std::make_shared<ScoreCalculator>();
    Console::Session session {game};
};

TEST_F(test_session, starts_in_the_menu) {
    EXPECT_EQ(session.mode(), Mode::Menu);
    EXPECT_EQ(session.message(), "Welcome to the Bowling Game!");
    EXPECT_EQ(session.prompt(), "Select a choice:\n q - quit p - play to continue...");
    EXPECT_FALSE(session.quit());
}

TEST_F(test_session, play_then_roll) {
    session.handle("p");
    EXPECT_EQ(session.mode(), Mode::Playing);
    EXPECT_EQ(session.message(), "New game started! Good Luck!");
    session.handle("7");
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(session.message(), "You have scored: 7\n");  // today's text: no echo, trailing blank line
    EXPECT_EQ(session.prompt(),
              "Enter a number between 0 and 3\nor the letter:  q - quit r - restart to continue...");
}

TEST_F(test_session, number_in_the_menu_is_rejected) {
    session.handle("5");
    EXPECT_EQ(session.mode(), Mode::Menu);
    EXPECT_EQ(session.message(), "Your choice: 5\nInvalid choice!");
    EXPECT_EQ(game->rolls()[0], toInt(Mark::None));
}

TEST_F(test_session, blanks_and_carriage_return_are_ignored) {
    session.handle(" p ");
    session.handle("7\r");
    session.handle("\t2 ");
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(game->rolls()[1], 2);
}

TEST_F(test_session, out_of_range_and_garbage_are_rejected) {
    session.handle("p");
    for (const char* bad : {"-1", "11", "abc", "7x", "", "99999999999"}) {
        session.handle(bad);
        EXPECT_EQ(session.message(), "Your choice: " + std::string(bad) + "\nInvalid choice!") << bad;
    }
    session.handle("7");
    session.handle("4");  // only 3 pins standing
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(game->rolls()[1], toInt(Mark::None));
    EXPECT_EQ(session.mode(), Mode::Playing);
}

TEST_F(test_session, perfect_game_ends_and_rejects_more_rolls) {
    session.handle("p");
    for (int i = 0; i < 12; ++i)
        session.handle("10");
    EXPECT_EQ(session.mode(), Mode::GameOver);
    EXPECT_EQ(session.message(), "Game Over!\t You have scored: 300\n");
    EXPECT_EQ(session.prompt(), "Select a choice:\n q - quit p - play to continue...");
    session.handle("5");
    EXPECT_EQ(session.message(), "Your choice: 5\nInvalid choice!");
    EXPECT_EQ(game->score(), 300);
}

TEST_F(test_session, restart_and_quit) {
    session.handle("p");
    session.handle("7");
    session.handle("r");
    EXPECT_EQ(game->rolls()[0], toInt(Mark::None));
    EXPECT_EQ(session.mode(), Mode::Playing);
    session.handle("q\r");
    EXPECT_TRUE(session.quit());
}
