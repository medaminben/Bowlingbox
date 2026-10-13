#include "Gamebox_impl.h"
#include <vector>
/**
 * @brief Rolls a number of pins in the bowling game.
 * This function takes an integer representing the number of pins rolled and
 * updates the game state accordingly. If the game is over, a message is
 * printed and the function returns.
 * @param pins The number of pins rolled in the current turn.
 * @throws None.
 */
bool Sandbox::Gamebox::BowlingSet::roll(const int& pins) {
    // Check if the game is over
    if (isOver())
        return false;

    auto is_valide_roll = managePins(pins);
    // entry guard, not valide pins number return
    if (!is_valide_roll)
        return false;
    // Update the pins array with the number of pins rolled
    // and iterate the cursor
    _rolls[_rollIndex++] = pins;
    return true;
}

/**
 * @brief Calculates the total score of the bowling game.
 *
 * This function calculates the total score of the bowling game
 * by calling the `frameScore` function for the 9th frame.
 *
 * @return The total score of the bowling game.
 *
 * @see BowlingSet::frameScore()
 */
int Sandbox::Gamebox::BowlingSet::score() {
    return frameScore(9);
}

/**
 * @brief Resets the bowling game by setting all pins to 0 and resetting the roll index.
 */
void Sandbox::Gamebox::BowlingSet::reset() {
    for (int& pins : _rolls) {
        pins = (int)Status::None;
    }
    _rollIndex = 0;
    _pinsLeft = 10;
}
/**
 * @brief manages pins is an input guard for the roll function
 *
 * @param pins number of pins down
 * @return true  doable
 * @return false input error
 */
bool Sandbox::Gamebox::BowlingSet::managePins(const int& pins) {
    // Strike or Spare
    if ((_pinsLeft == pins)
        // or new frame
        || (manageCursor() % 2 == 1))
        _pinsLeft = 10;
    else
        _pinsLeft -= pins;
    return true;
}
/**
 * @brief manages a cursor
 * This transforms the roll index into a frame cursor
 * It's needed to remap roll index with game frames
 * @return int the real cursor position in the frame grid
 */
int Sandbox::Gamebox::BowlingSet::manageCursor() {
    int cur = 0;
    for (auto i = 0; i < _rollIndex; i++) {
        bool isRegular = cur < 18;
        bool isEven = (cur % 2) == 0;
        if (isEven)
            cur += (isRegular && isStrike(i)) ? 2 : 1;
        else
            cur++;
    }
    return cur;
}
/**
 * @brief validates the roll
 * an adapter function that translate roll status to zero
 * it s a helper for the manipulation of out_rolls
 * @param value the roll as integer
 * @return int The real value inthe cell
 */
int Sandbox::Gamebox::BowlingSet::validateRoll(const int& value) {
    if (((int)Status::None == value) || ((int)Status::Empty == value))
        return 0;
    return value;
}
/**
 * @brief serves the rolls as an array of 21 integers
 * the integers can be roll pins between 1 and 9 or a game status
 * like None,Strike, Spare, Miss or Empty (see Status enum)
 * @return int* a pointer to an array of 21 integers
 */
std::vector<int> Sandbox::Gamebox::BowlingSet::rolls() {
    static int out_rolls[21];
    int cur = 0, r = 0;
    for (; cur < 21; cur++, r++) {
        if (r <= _rollIndex) {
            // it's just a zero
            if (isMiss(r)) {
                out_rolls[cur] = (int)Status::Miss;
                continue;
            }

            auto position = cur % 2;
            switch (position) {
                case 0:  // first shot in a frame
                    if (isStrike(r)) {
                        out_rolls[cur] = (int)Status::Strike;
                        if (cur < 18) {  // regular frame  so jump to next frame
                            // by incrementing the cursor
                            cur++;
                            // and closing the frame by an empty cell
                            out_rolls[cur] = (int)Status::Empty;
                        }

                    } else
                        out_rolls[cur] = _rolls[r];

                    break;

                case 1:  // if 1 second shot
                    if (cur < 19) {
                        if (isSpare(r))
                            out_rolls[cur] = (int)Status::Spare;
                        else
                            out_rolls[cur] = _rolls[r];
                    } else {
                        if (isSpare(r))
                            out_rolls[cur] = (int)Status::Spare;
                        else if (isStrike(r))
                            out_rolls[cur] = (int)Status::Strike;
                        else
                            out_rolls[cur] = _rolls[r];
                    }
                    break;
            }
        } else
            out_rolls[cur] = (int)Status::None;
    }
    std::vector<int> rolls = {};
    copy(&out_rolls[0], &out_rolls[21], std::back_inserter(rolls));
    return rolls;
}

/**
 * @brief serves the  frame scores as an array of 10 integers
 *
 * @return int*  a pointer to an array of 10 integers
 */
std::vector<int> Sandbox::Gamebox::BowlingSet::scores() {
    static unsigned scores[10];
    auto cur = manageCursor();
    for (int i = 0; i < 10; i++) {
        if (2 * i < cur)
            scores[i] = frameScore(i);
        else
            scores[i] = (int)Status::None;
    }
    std::vector<int> scores_out = {};
    copy(&scores[0], &scores[10], std::back_inserter(scores_out));
    return scores_out;
}

/**
 * @brief backtracker of the pins befor rolling
 *
 * @return int the number of pin left
 */
int Sandbox::Gamebox::BowlingSet::pinsLeft() {
    return _pinsLeft;
}

/**
 * @brief Game status tracker
 *
 * @return true  is game over
 * @return false  the game is not over yet
 */
bool Sandbox::Gamebox::BowlingSet::isOver() {
    auto cur = manageCursor();
    if (cur < 20)
        return false;
    else if ((cur == 20))
        if (isSpare(_rollIndex - 1) || isStrike(_rollIndex - 1))
            return false;
    return true;
}

/**
 * @brief Checks if the frame at the given index is a strike.
 *
 * This function checks if the frame at the given index is a strike by verifying
 * if the pin count in the frame is 10. If the index is less than 19, it checks
 * if the pin count in the frame is 10 and if the index is even. If the index
 * is 19 or greater, it simply checks if the pin count is 10.
 *
 * @param index The index of the frame to check.
 *
 * @return True if the frame is a strike, false otherwise.
 */
bool Sandbox::Gamebox::BowlingSet::isStrike(const int& index) {
    return _rolls[index] == 10;
}

/**
 * @brief Checks if the frame at the given index is a spare.
 *
 * This function checks if the frame at the given index is a spare by verifying
 * if the sum of pins in the previous and current frame is 10 and if the pin
 * count in the previous frame is not 10. If these conditions are met, the
 * function returns true indicating that the frame is a spare. Otherwise, it
 * returns false.
 *
 * @param index The index of the frame to check.
 * @return bool True if the frame is a spare, false otherwise.
 */
bool Sandbox::Gamebox::BowlingSet::isSpare(const int& index) {
    if (isMiss(index))
        return false;
    // Check if the index is valid
    if (index >= 1)  // && (index % 2 == 1))
        // Check if the sum of pins in the current index and before is 10
        // but the current is not miss
        return _rolls[index - 1] + _rolls[index] == 10;
    // If the index is not valid return false too
    return false;
}
/**
 * @brief check if a value is an empty cell
 *
 * @param value
 * @return true if the value is empty
 * @return false else
 */
bool Sandbox::Gamebox::BowlingSet::isEmpty(const int& index) {
    return (int)Status::Empty == _rolls[index];
}
/**
 * @brief checks if a value is a miss
 *
 * @param index
 * @return true
 * @return false
 */
bool Sandbox::Gamebox::BowlingSet::isMiss(const int& index) {
    return (int)Status::Miss == _rolls[index];
}
/**
 * @brief check if a value is not set
 *
 * @param value
 * @return true
 * @return false
 */
bool Sandbox::Gamebox::BowlingSet::isNone(const int& index) {
    return (int)Status::None == _rolls[index];
}
/**
 * @brief compute the score of a certain frame defined by its index
 *
 * @param frameIndex the index of the needed fframe
 * @return int the score
 */
int Sandbox::Gamebox::BowlingSet::frameScore(const int& frameIndex) {
    int cur = 0, score = 0;

    for (auto f = 0; f <= frameIndex; f++) {
        auto first = validateRoll(_rolls[cur]);

        if (isStrike(cur)) {
            auto bonus1 = validateRoll(_rolls[cur + 1]);
            auto bonus2 = validateRoll(_rolls[cur + 2]);
            score += 10 + bonus1 + bonus2;
            cur++;
        } else {
            auto second = validateRoll(_rolls[cur + 1]);
            if (isSpare(cur + 1))
                score += 10 + validateRoll(_rolls[cur + 2]);
            else
                score += first + second;
            cur += 2;
        }
    }
    return score;
}