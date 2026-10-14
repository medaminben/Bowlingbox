#include "Score_impl.h"
#include <Bowlingbox/Score/Score.h>

/**
 * @brief Constructs a new ScoreCalculator instance.
 *
 * This constructor initializes a unique pointer to a BowlingSet object,
 * which is used to store the game states.
 */
Bowlingbox::Score::ScoreCalculator::ScoreCalculator() : _gameSet(std::make_unique<BowlingSet>()) {}

Bowlingbox::Score::ScoreCalculator::~ScoreCalculator() = default;

/**
 * @brief Rolls a number of pins in the bowling game.
 *
 * @param pins The number of pins rolled in the current turn.
 * @return false when the game is already over.
 */
bool Bowlingbox::Score::ScoreCalculator::roll(int pins) {
    return _gameSet->roll(pins);
}

/**
 * @brief Resets the bowling game to its initial state.
 */
void Bowlingbox::Score::ScoreCalculator::reset() {
    _gameSet->reset();
}

/**
 * @brief Retrieves the current score of the bowling game.
 */
int Bowlingbox::Score::ScoreCalculator::score() const {
    return _gameSet->score();
}

std::vector<int> Bowlingbox::Score::ScoreCalculator::rolls() const {
    return _gameSet->rolls();
}

std::vector<int> Bowlingbox::Score::ScoreCalculator::scores() const {
    return _gameSet->scores();
}

int Bowlingbox::Score::ScoreCalculator::pinsLeft() const {
    return _gameSet->pinsLeft();
}

int Bowlingbox::Score::ScoreCalculator::frameScore(int frameIndex) const {
    return _gameSet->frameScore(frameIndex);
}

bool Bowlingbox::Score::ScoreCalculator::isOver() const {
    return _gameSet->isOver();
}
