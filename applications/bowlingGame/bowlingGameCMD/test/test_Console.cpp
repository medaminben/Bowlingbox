#include "banner.h"
#include "bowlingScreen.h"
#include "terminal.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

using Bowlingbox::Score::Mark;
using Bowlingbox::Score::toInt;

namespace {
/// Feeds fixed lines to the game, then reports end of input.
class ScriptedTerminal final : public Console::ITerminal {
public:
    explicit ScriptedTerminal(std::vector<std::string> lines) : _lines(std::move(lines)) {}
    int width() const override { return 80; }
    bool clear() override { return false; }
    void write(const std::string& text) override { output += text; }
    std::optional<std::string> readLine() override {
        if (_next == _lines.size())
            return std::nullopt;
        return _lines[_next++];
    }

    std::string output;

private:
    std::vector<std::string> _lines;
    std::size_t _next = 0;
};
}  // namespace

TEST(test_console, end_of_input_quits) {
    ScriptedTerminal terminal({"p", "3"});
    Bowlingbox::Score::ScoreCalculator game;
    Console::Screen screen(game, terminal);
    screen.start();  // must return instead of looping forever
    EXPECT_EQ(game.rolls()[0], 3);
}

TEST(test_console, huge_number_is_rejected_not_fatal) {
    ScriptedTerminal terminal({"p", "99999999999", "q"});
    Bowlingbox::Score::ScoreCalculator game;
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_NE(terminal.output.find("Invalid choice!"), std::string::npos);
    EXPECT_EQ(game.rolls()[0], toInt(Mark::None));
}

TEST(test_console, more_pins_than_standing_are_rejected) {
    ScriptedTerminal terminal({"p", "7", "4", "q"});
    Bowlingbox::Score::ScoreCalculator game;
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_EQ(game.rolls()[0], 7);
    EXPECT_EQ(game.rolls()[1], toInt(Mark::None));
    EXPECT_NE(terminal.output.find("Invalid choice!"), std::string::npos);
}

TEST(test_console, banner_matches_todays_text) {
    // Captured from plotIntr() on main (9bbbf6b) at 80 columns: 14 spaces on each side of the title.
    const std::string title = "Execution of BowlingGame from Bowlingbox library";
    const std::string expected = std::string(79, '#') + "\n" +
                                 std::string(14, ' ') + title + std::string(14, ' ') + "\n" +
                                 std::string(79, '#') + "\n" +
                                 "This console game is an interface to the bowling game implemented in the "
                                 "Bowlingbox library.\n"
                                 "=About=\n"
                                 " Made by: github.com/medaminben\n" +
                                 std::string(79, '~') + "\n\n";
    EXPECT_EQ(Console::banner(80), expected);
}

TEST(test_console, banner_fits_the_terminal) {
    const std::string title = "Execution of BowlingGame from Bowlingbox library";
    // Today's title padding: none up to 52 columns, then one space each side at 53 and 54.
    EXPECT_NE(Console::banner(52).find("\n" + title + "\n"), std::string::npos);
    EXPECT_NE(Console::banner(54).find("\n " + title + " \n"), std::string::npos);
    EXPECT_NO_THROW(Console::banner(0));
    EXPECT_NO_THROW(Console::banner(-5));
    EXPECT_EQ(Console::banner(10).substr(0, 10), std::string(9, '#') + "\n");
}

TEST(test_console, a_roll_shows_the_score_then_a_blank_line) {
    ScriptedTerminal terminal({"p", "7"});
    Bowlingbox::Score::ScoreCalculator game;
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_NE(terminal.output.find("You have scored: 7\n\nEnter a number between 0 and 3\n"), std::string::npos);
    EXPECT_EQ(terminal.output.find("Your choice: 7"), std::string::npos);
}

TEST(test_console, a_number_after_game_over_is_invalid) {
    std::vector<std::string> lines {"p"};
    lines.insert(lines.end(), 12, "10");
    lines.push_back("5");
    ScriptedTerminal terminal(lines);
    Bowlingbox::Score::ScoreCalculator game;
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_NE(terminal.output.find("Game over! You scored: 300\n\nChoose an option:\n"), std::string::npos);
    EXPECT_NE(terminal.output.find("Your choice: 5\nInvalid choice!\nChoose an option:\n"), std::string::npos);
    EXPECT_EQ(terminal.output.find("starting"), std::string::npos);
}
