#include "scoreGrid.h"
#include "screenTools.h"

using Gamebox::Score::kFrames;

////////////////////////////////////////////////////////////////
// ScoreGrid implementation
////////////////////////////////////////////////////////////////
Console::ScoreGrid::ScoreGrid() {
    reset();
}

void Console::ScoreGrid::reset() {
    _frames.clear();
    for (auto i = 0; i < kFrames - 1; i++)
        _frames.push_back(Frame());
    _frames.push_back(Frame(Frame::Type::Last));
}

void Console::ScoreGrid::update(const Gamebox::Score::IScoreSheet& sheet) {
    auto rolls = sheet.rolls();
    auto scores = sheet.scores();

    int first, second, score, last = Gamebox::Score::toInt(Gamebox::Score::Mark::None);
    for (auto frame_index = 0; frame_index < kFrames; frame_index += 1) {
        first = rolls[2 * frame_index];
        second = rolls[2 * frame_index + 1];
        score = scores[frame_index];
        if (frame_index == kFrames - 1)
            last = rolls[2 * frame_index + 2];

        _frames[frame_index].setFrame(score, first, second, last);
    }
}

std::string Console::ScoreGrid::render(int width) const {
    std::string out;
    auto spacer = " ";
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
            return out;
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
        for (const auto& line : lines)
            out += line + "\n";
    }
    return out;
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
