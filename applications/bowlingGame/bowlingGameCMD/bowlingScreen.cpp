#include "bowlingScreen.h"
#include "screenTools.h"
#include <charconv>
////////////////////////////////////////////////////////////////
// Screen class implementation
////////////////////////////////////////////////////////////////
Console::Screen::Screen(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game, ITerminal& terminal)
    : _game(std::move(game)), _terminal(terminal) {}

void Console::Screen::start() {
    setStatus(Status::FristStart);
    while (!isExit()) {
        plotIntr();
        plotGrid();
        plotMenu();
        evaluate();
    }
}

void Console::Screen::println(const std::string& text) {
    _terminal.write(text + "\n");
}

void Console::Screen::plotIntr() {
    if (_terminal.clear()) {
        std::string hash_stone = "#", wave_stone = "~", spacer = " ", new_line = "\n";

        int width = _terminal.width();

        std::string title =
            ScreenTools::formatTitle(width, "Execution of BowlingGame from Bowlingbox Library");

        std::string introduction =
            "This console game is an interface to the bowling game implemented under the Bowlingbox "
            "library.";

        std::string about = ScreenTools::giveAbout("github.com/medaminben");

        // create a bars
        std::string hash_bar = ScreenTools::generateBar(width, hash_stone);
        std::string wave_bar = ScreenTools::generateBar(width, wave_stone);
        // bundle all together in a banner
        std::string banner =
            hash_bar + title + hash_bar + introduction + new_line + about + wave_bar + new_line;
        // print the banner
        _terminal.write(banner);
    }
}

void Console::Screen::plotGrid() {
    _grid.update(*_game);
    _terminal.write(_grid.render(_terminal.width()));
}

void Console::Screen::plotMenu() {
    std::string spacer = " ";
    std::string message = "";
    // add static choices
    std::vector<std::string> choices = {"q - quit"};
    println(_lastMessage);

    if (isGame()) {
        message += "Enter a number between 0 and " + std::to_string(_game->pinsLeft()) + "\n";
        message += "or the letter: ";

        choices.push_back("r - restart");
    } else {
        message += "Select a choice:\n";

        choices.push_back("p - play");

        if (!isTryStart())
            setStatus(Status::TryStart);
    }

    // append the choices
    for (auto choice : choices) {
        message += spacer + choice;
    }
    println(message + " to continue...");
}

void Console::Screen::evaluate() {
    const auto line = _terminal.readLine();
    if (!line) {  // end of input: nothing more will come
        setStatus(Status::Quit);
        return;
    }
    const std::string& choice = *line;

    _lastMessage = "Your choice: " + choice + "\n";

    auto maxPins = _game->pinsLeft();

    if (choice == "q") {
        setStatus(Status::Quit);
        return;
    } else if (choice == "p" || choice == "r") {
        resetGame();
        setStatus(Status::Started);
        return;
    }

    if (isGame()) {  // the game is already started or running
        int pins = -1;
        const char* const end = choice.data() + choice.size();
        const auto [parsedEnd, error] = std::from_chars(choice.data(), end, pins);
        if (error == std::errc() && parsedEnd == end) {  // a whole number that fits an int
            if (pins >= 0 && pins <= maxPins) {  // in interval

                _game->roll(pins);

                if (_game->isOver())  // check the game if it s over set an end
                    setStatus(Status::End);
                else  // else keep running
                    setStatus(Status::Running);
            } else
                setStatus(Status::Typo);
        } else
            setStatus(Status::Typo);
    } else
        setStatus(Status::TryStart);
}

void Console::Screen::resetGame() {
    _grid.reset();
    _game->reset();
}

// ### Status setter
void Console::Screen::setStatus(Status current_status) {
    _status = current_status;
    switch (_status) {
        case Status::End:
            _lastMessage = "Game Over!\t You have scored: " + std::to_string(_game->score()) + "\n";
            break;
        case Status::Running:
            _lastMessage = "You have scored: " + std::to_string(_game->score()) + "\n";
            break;
        case Status::FristStart:
            _lastMessage = "Welcome to the Bowling Game!";
            break;
        case Status::Started:
            _lastMessage = "New game started! Good Luck!";
            break;
        case Status::TryStart:
            _lastMessage = "starting ";
            break;
        case Status::Typo:
        default:
            _lastMessage += "Invalid choice!";
            break;
    }
}
// ### Status getters
bool Console::Screen::isExit() {
    return Status::Quit == _status;
}

bool Console::Screen::isRunning() {
    return Status::Running == _status;
}

bool Console::Screen::isTryStart() {
    return Status::TryStart == _status;
}

bool Console::Screen::isStarted() {
    return Status::Started == _status;
}

bool Console::Screen::isTypo() {
    return Status::Typo == _status;
}

bool Console::Screen::isGame() {
    return isRunning() || isStarted() || isTypo();
}
