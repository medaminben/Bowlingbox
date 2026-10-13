#include "bowlingScreen.h"
#include "screenTools.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif
#include <cstdlib>
#include <unistd.h>
////////////////////////////////////////////////////////////////
// Screen class implementation
////////////////////////////////////////////////////////////////
Console::Screen::Screen(std::shared_ptr<Bowlingbox> g) {
    _game = g;
    SetUp();
}
void Console::Screen::SetUp() {
    // frames = std::vector<Frame>(10);
    for (auto i = 0; i < 9; i++) {
        _frames.push_back(Frame());
    }
    _frames.push_back(Frame(Frame::Type::Last));
}

void Console::Screen::start() {
    setStatus(Status::FristStart);
    while (!isExit()) {
        plotIntr();
        plotGrid();
        plotMenu();
        evaluate();
    }
}

void Console::Screen::updateGrid() {
    auto rolls = _game->rolls();
    auto scores = _game->scores();

    int first, second, score, last = (int)Frame::Bonus::None;
    for (auto frame_index = 0; frame_index < 10; frame_index += 1) {
        first = rolls[2 * frame_index];
        second = rolls[2 * frame_index + 1];
        score = scores[frame_index];
        if (frame_index == 9)
            last = rolls[2 * frame_index + 2];

        _frames[frame_index].setFrame(score, first, second, last);
    }
}

void Console::Screen::plotIntr() {
    if (clearScreen()) {
        std::string hash_stone = "#", wave_stone = "~", spacer = " ", new_line = "\n";

        int width = getConsoleWidth();

        std::string title =
            ScreenTools::formatTitle(width, "Execution of BowlingGame from Gamebox Library");

        std::string introduction =
            "This console game is an interface to the bowling game implemented under the Gamebox "
            "library.";

        std::string about = ScreenTools::giveAbout("github.com/medaminben");

        // create a bars
        std::string hash_bar = ScreenTools::generateBar(width, hash_stone);
        std::string wave_bar = ScreenTools::generateBar(width, wave_stone);
        // bundle all together in a banner
        std::string banner =
            hash_bar + title + hash_bar + introduction + new_line + about + wave_bar + new_line;
        // print the banner
        std::cout << banner;
    }
}

void Console::Screen::plotGrid() {
    // refresh data
    updateGrid();
    auto spacer = " ";
    // first we need to know the width of the screen
    int width = getConsoleWidth();
    // then we need to know the number of frames to be plotted
    int frameSize = _frames.size();
    int frameHeight = _frames[0].getFrame().size();
    // fix the index of the last frame just for readability proposal
    int lastIndex = frameSize - 1;
    // then we need to know the width of each frame
    int last_width = _frames[lastIndex].getFrame()[0].length();
    int unit_width = _frames[0].getFrame()[0].length() + 1;  // plus 1 for the spacer;

    // then we need to know the number of regular units can be plotted
    //  in the screen at the same line other than the last frame.
    int regular_width = width - last_width + 1;
    // here we add 1 for the last frame
    int units = ((regular_width + 2) / (unit_width + 1)) + 1;

    // find the best unit width
    if (units < frameSize) {  // else all the frames in one row
        if (units >= 5)       // 5 frames can be plotted in one row
            units = 5;  // 5 frames is a good choice in that way we 2 similar rows in the screen
        else            // if the screen is smaller than 5 frames 2 frames can be plotted in one row
            if (units >= 2)
                units = 2;  // 2 frames is a good choice in that way we 5 similar rows in the screen
            else
                units = 1;  // the minimum number of frames can be plotted in one row
                            // else an exception should be thrown, something wrong
    }

    // findout if an extra row is needed
    int rest = (frameSize % units == 0) ? 0 : 1;
    // a stack is a alligned group of frames in the screen
    int rows = (frameSize / units) + rest;
    // plot the frames
    int frameIndex = 0;  // frame index
    /// load the frames to the rows
    for (auto i = 0; i < rows; i++) {
        // check the remaining to avoid the overflow
        auto remain = frameSize - (i * units);
        int stop = (remain < units) ? remain : units;
        if (stop <= 0)
            return;
        // the row is constituted by the frames external vector size.
        //(frames are vector of vector of strings)
        auto lines = std::vector<std::string>(frameHeight);
        // initialize the lines
        for (auto& line : lines)
            line = std::string("");
        // load the frames line by line,
        for (auto j = 0; j < stop; j++) {
            for (auto k = 0; k < frameHeight; k++)
                lines[k] += spacer + _frames[frameIndex].getFrame()[k];
            // increase the index to move to the next frame
            frameIndex++;
        }
        // print the row in the screen line by line
        for (auto line : lines) {
            std::cout << line << "\n";
        }
    }
}

void Console::Screen::plotMenu() {
    std::string spacer = " ";
    std::string message = "";
    // add static choices
    std::vector<std::string> choices = {"q - quit"};
    ScreenTools::print(_lastMessage);

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
    ScreenTools::print(message + " to continue...");
}

void Console::Screen::evaluate() {
    std::string choice {};
    std::getline(std::cin, choice);

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
        if (ScreenTools::isNumber(choice)) {
            int pins = std::stoi(choice);

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
    // frames = std::vector<Frame>(10);
    _frames.clear();
    for (auto i = 0; i < 9; i++) {
        _frames.push_back(Frame());
    }
    _frames.push_back(Frame(Frame::Type::Last));
    // for (auto& frame : _frames)
    //     frame.resetFrame();
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
int Console::Screen::getConsoleWidth() {
#ifdef WIN32
    // Get the width of the console screen
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    return size.ws_col;
#endif  // WIN32
}
bool Console::Screen::clearScreen() {
#ifdef _WIN32
    return system("cls") != -1;  // For Windows
#else
    return system("clear") != -1;  // For Linux/Mac
#endif  // WIN32
}
// ### Status getters
bool Console::Screen::isEndGame() {
    return Status::End == _status;
}

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

bool Console::Screen::isFirstStart() {
    return Status::FristStart == _status;
}

bool Console::Screen::isTypo() {
    return Status::Typo == _status;
}

bool Console::Screen::isGame() {
    return isRunning() || isStarted() || isTypo();
}

////////////////////////////////////////////////////////////////
// Frame struct implementation
////////////////////////////////////////////////////////////////
Console::Frame::Frame(Type t) : _type(t) {
    _frame = std::vector<std::string>(5, "   ");
    resetFrame();
}

void Console::Frame::setRoll(const Order& position, const int& value) {
    std::string value_str = ScreenTools::valueToConsole(value);
    switch (position) {
        case Order::First:
            _firstRoll = value_str;
            break;
        case Order::Second:
            _secondRoll = value_str;
            break;
        case Order::Last:
            _lastRoll = value_str;
            break;
    }
    updateRolls();
}

void Console::Frame::setScore(const int& score) {
    const bool isScore = true;
    _framescore = ScreenTools::valueToConsole(score, isScore);
    updateScore();
}

std::vector<std::string> Console::Frame::getFrame() const {
    return _frame;
}

void Console::Frame::updateRolls() {
    std::string leftEdge = "| ", rightEdge = " |", middleEdge = " | ";
    // reset the frame line with first roll first
    _frame[1] = leftEdge + _firstRoll;
    // prepare the right side of the frame by the second roll
    auto rightSide = middleEdge + _secondRoll;
    // evaluate if it is the last frame then support a 3rd roll
    if (_type == Type::Last)
        rightSide += middleEdge + _lastRoll;
    // append the right side and the edge to the frame line
    _frame[1] += rightSide + rightEdge;
}

void Console::Frame::updateScore() {
    std::string leftedge = "|    ", rightedge = "    |";

    if (_type == Type::Last) {
        // inlarge the edges for the last frame
        leftedge += "   ";
        rightedge = "   " + rightedge;
    }

    _frame[3] = leftedge + _framescore + rightedge;
}

/**
 * @brief updates the frame attributes and rebuild the frame structure
 *
 * @param frameScore
 * @param firstRoll
 * @param secondRoll
 * @param lastRoll
 */
void Console::Frame::setFrame(const int& frameScore, const int& firstRoll, const int& secondRoll,
                              const int& lastRoll) {
    setRoll(Order::First, firstRoll);
    setRoll(Order::Second, secondRoll);
    setRoll(Order::Last, lastRoll);
    setScore(frameScore);
}

/**
 * @brief the frame builder; it builds the basic structure of a frame
 */
void Console::Frame::resetFrame() {
    _firstRoll = "   ";
    _secondRoll = "   ";
    _lastRoll = "   ";
    _framescore = "   ";
    switch (_type) {
        case Type::Regular: {
            // regular frame
            _frame[0] = ".-----------.";
            _frame[1] = "|     |     |";
            _frame[2] = "|     '-----|";
            _frame[3] = "|           |";
            _frame[4] = "'-----------'";
        } break;
        case Type::Last: {
            // last frame
            _frame[0] = ".-----------------.";
            _frame[1] = "|     |     |     |";
            _frame[2] = "|     '-----'-----|";
            _frame[3] = "|                 |";
            _frame[4] = "'-----------------'";
        } break;
    }
}
