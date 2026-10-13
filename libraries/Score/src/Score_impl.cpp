#include "Score_impl.h"
#include <algorithm>
#include <vector>
/**
 * @brief Rolls a number of pins in the bowling game.
 * This function takes an integer representing the number of pins rolled and
 * updates the game state accordingly. If the game is over, a message is
 * printed and the function returns.
 * @param pins The number of pins rolled in the current turn.
 * @throws None.
 */
bool Gamebox::Score::BowlingSet::roll(int pins) {
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
int Gamebox::Score::BowlingSet::score() const {
    return frameScore(9);
}

/**
 * @brief Resets the bowling game by setting all pins to 0 and resetting the roll index.
 */
void Gamebox::Score::BowlingSet::reset() {
    std::fill(std::begin(_rolls), std::end(_rolls), toInt(Mark::None));
    _rollIndex = 0;
    _pinsLeft = kPins;
}
/**
 * @brief manages pins is an input guard for the roll function
 *
 * @param pins number of pins down
 * @return true  doable
 * @return false input error
 */
bool Gamebox::Score::BowlingSet::managePins(int pins) {
    // Strike or Spare
    if ((_pinsLeft == pins)
        // or new frame
        || (manageCursor() % 2 == 1))
        _pinsLeft = kPins;
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
int Gamebox::Score::BowlingSet::manageCursor() const {
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
int Gamebox::Score::BowlingSet::validateRoll(int value) {
    if ((toInt(Mark::None) == value) || (toInt(Mark::Empty) == value))
        return 0;
    return value;
}
/**
 * @brief serves the rolls as an array of 21 integers
 * the integers can be roll pins between 1 and 9 or a game status
 * like None,Strike, Spare, Miss or Empty (see Mark enum)
 * @return the 21 roll slots
 */
std::vector<int> Gamebox::Score::BowlingSet::rolls() const {
    std::vector<int> out_rolls(kRollSlots, toInt(Mark::None));
    int cur = 0, r = 0;
    for (; cur < kRollSlots; cur++, r++) {
        if (r <= _rollIndex) {
            // it's just a zero
            if (isMiss(r)) {
                out_rolls[cur] = toInt(Mark::Miss);
                continue;
            }

            auto position = cur % 2;
            switch (position) {
                case 0:  // first shot in a frame
                    if (isStrike(r)) {
                        out_rolls[cur] = toInt(Mark::Strike);
                        if (cur < 18) {  // regular frame  so jump to next frame
                            // by incrementing the cursor
                            cur++;
                            // and closing the frame by an empty cell
                            out_rolls[cur] = toInt(Mark::Empty);
                        }

                    } else
                        out_rolls[cur] = _rolls[r];

                    break;

                case 1:  // if 1 second shot
                    if (cur < 19) {
                        if (isSpare(r))
                            out_rolls[cur] = toInt(Mark::Spare);
                        else
                            out_rolls[cur] = _rolls[r];
                    } else {
                        if (isSpare(r))
                            out_rolls[cur] = toInt(Mark::Spare);
                        else if (isStrike(r))
                            out_rolls[cur] = toInt(Mark::Strike);
                        else
                            out_rolls[cur] = _rolls[r];
                    }
                    break;
            }
        } else
            out_rolls[cur] = toInt(Mark::None);
    }
    return out_rolls;
}

/**
 * @brief serves the  frame scores as an array of 10 integers
 *
 * @return the 10 frame scores
 */
std::vector<int> Gamebox::Score::BowlingSet::scores() const {
    std::vector<int> scores(kFrames, toInt(Mark::None));
    auto cur = manageCursor();
    for (int i = 0; i < kFrames; i++) {
        if (2 * i < cur)
            scores[i] = frameScore(i);
        else
            scores[i] = toInt(Mark::None);
    }
    return scores;
}

/**
 * @brief backtracker of the pins befor rolling
 *
 * @return int the number of pin left
 */
int Gamebox::Score::BowlingSet::pinsLeft() const {
    return _pinsLeft;
}

/**
 * @brief Game status tracker
 *
 * @return true  is game over
 * @return false  the game is not over yet
 */
bool Gamebox::Score::BowlingSet::isOver() const {
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
bool Gamebox::Score::BowlingSet::isStrike(int index) const {
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
bool Gamebox::Score::BowlingSet::isSpare(int index) const {
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
 * @brief checks if a value is a miss
 *
 * @param index
 * @return true
 * @return false
 */
bool Gamebox::Score::BowlingSet::isMiss(int index) const {
    return toInt(Mark::Miss) == _rolls[index];
}
/**
 * @brief compute the score of a certain frame defined by its index
 *
 * @param frameIndex the index of the needed fframe
 * @return int the score
 */
int Gamebox::Score::BowlingSet::frameScore(int frameIndex) const {
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