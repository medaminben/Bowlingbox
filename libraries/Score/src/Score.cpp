#include "Score_impl.h"
#include <Gamebox/Score/Score.h>

/**
 * @brief Constructs a new Bowlingbox instance.
 *
 * This constructor initializes a unique pointer to a BowlingSet object,
 * which is used to store the game states.
 */
Gamebox::Score::Bowlingbox::Bowlingbox() : _gameSet(std::make_unique<BowlingSet>()) {}

Gamebox::Score::Bowlingbox::~Bowlingbox() = default;

/**
 * @brief Rolls a number of pins in the bowling game.
 *
 * @param pins The number of pins rolled in the current turn.
 * @return false when the game is already over.
 */
bool Gamebox::Score::Bowlingbox::roll(int pins) {
    return _gameSet->roll(pins);
}

/**
 * @brief Resets the bowling game to its initial state.
 */
void Gamebox::Score::Bowlingbox::reset() {
    _gameSet->reset();
}

/**
 * @brief Retrieves the current score of the bowling game.
 */
int Gamebox::Score::Bowlingbox::score() const {
    return _gameSet->score();
}

std::vector<int> Gamebox::Score::Bowlingbox::rolls() const {
    return _gameSet->rolls();
}

std::vector<int> Gamebox::Score::Bowlingbox::scores() const {
    return _gameSet->scores();
}

int Gamebox::Score::Bowlingbox::pinsLeft() const {
    return _gameSet->pinsLeft();
}

int Gamebox::Score::Bowlingbox::frameScore(int frameIndex) const {
    return _gameSet->frameScore(frameIndex);
}

bool Gamebox::Score::Bowlingbox::isOver() const {
    return _gameSet->isOver();
}
