#include "session.h"
#include <charconv>
#include <optional>

namespace Console {
namespace {
std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// A whole number that fits an int, or std::nullopt.
std::optional<int> parsePins(std::string_view text) {
    if (text.empty())
        return std::nullopt;
    int pins = 0;
    const char* const end = text.data() + text.size();
    const auto [parsedEnd, error] = std::from_chars(text.data(), end, pins);
    if (error != std::errc() || parsedEnd != end)
        return std::nullopt;
    return pins;
}
}  // namespace

Session::Session(Bowlingbox::Score::IBowlingGame& game) : _game(game) {}

void Session::handle(std::string_view input) {
    const std::string_view choice = trim(input);
    if (choice == "q") {
        _quit = true;
        return;
    }
    if (choice == "p" || choice == "r") {
        _game.reset();
        _mode = Mode::Playing;
        _message = "New game started! Good luck!";
        return;
    }

    const auto pins = parsePins(choice);
    // roll() refuses counts outside 0..pinsLeft(); no need to check them here.
    if (_mode != Mode::Playing || !pins || _game.roll(*pins) != Bowlingbox::Score::RollStatus::Accepted) {
        _message = "Your choice: " + std::string(choice) + "\nInvalid choice!";
        return;
    }
    // Same text as before the refactor: no echo, and a trailing newline that the
    // screen shows as a blank line before the prompt.
    const std::string total = std::to_string(_game.score());
    if (_game.isOver()) {
        _mode = Mode::GameOver;
        _message = "Game over! You scored: " + total + "\n";
    } else {
        _message = "You have scored: " + total + "\n";
    }
}

std::string Session::prompt() const {
    if (_mode == Mode::Playing)
        return "Enter a number between 0 and " + std::to_string(_game.pinsLeft()) +
               "\nor a letter: r - restart, q - quit";
    return "Choose an option:\n p - play, q - quit";
}

}  // namespace Console
