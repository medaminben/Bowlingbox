#ifndef CONSOLE_SESSION_H
#define CONSOLE_SESSION_H
#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>
#include <string>
#include <string_view>

namespace Console {

/// Game flow of the console front-end, without any I/O: one line of input in,
/// one message out.
class Session {
public:
    enum class Mode { Menu, Playing, GameOver };

    explicit Session(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game);

    /// Applies one line of input: "q" quits, "p" / "r" start a new game and a
    /// number rolls while playing. Surrounding blanks and '\r' are ignored.
    void handle(std::string_view input);

    Mode mode() const { return _mode; }
    bool quit() const { return _quit; }
    /// Result of the last input, e.g. "You have scored: 7\n" or "Your choice: x\nInvalid choice!".
    const std::string& message() const { return _message; }
    /// The menu line for the current mode.
    std::string prompt() const;
    const Bowlingbox::Score::IScoreSheet& sheet() const { return *_game; }

private:
    std::shared_ptr<Bowlingbox::Score::IBowlingGame> _game;
    Mode _mode = Mode::Menu;
    bool _quit = false;
    std::string _message = "Welcome to the Bowling Game!";
};
}  // namespace Console
#endif  // CONSOLE_SESSION_H
