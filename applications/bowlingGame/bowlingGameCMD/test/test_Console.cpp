#include "bowlingScreen.h"
#include "terminal.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <memory>
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
    auto game = std::make_shared<Bowlingbox::Score::ScoreCalculator>();
    Console::Screen screen(game, terminal);
    screen.start();  // must return instead of looping forever
    EXPECT_EQ(game->rolls()[0], 3);
}

TEST(test_console, huge_number_is_rejected_not_fatal) {
    ScriptedTerminal terminal({"p", "99999999999", "q"});
    auto game = std::make_shared<Bowlingbox::Score::ScoreCalculator>();
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_NE(terminal.output.find("Invalid choice!"), std::string::npos);
    EXPECT_EQ(game->rolls()[0], toInt(Mark::None));
}

TEST(test_console, more_pins_than_standing_are_rejected) {
    ScriptedTerminal terminal({"p", "7", "4", "q"});
    auto game = std::make_shared<Bowlingbox::Score::ScoreCalculator>();
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(game->rolls()[1], toInt(Mark::None));
    EXPECT_NE(terminal.output.find("Invalid choice!"), std::string::npos);
}
