#include "Gamebox_impl.h"
#include <Sandbox/Gamebox/Gamebox.h>

/**
 * @brief Constructs a new Bowlingbox instance.
 *
 * This constructor initializes a unique pointer to a BowlingSet object,
 * which is used to store the game states.
 */
Sandbox::Gamebox::Bowlingbox::Bowlingbox() {
    // Initialize a unique pointer to a BowlingSet object
    _gameSet = std::make_unique<BowlingSet>();
}

Sandbox::Gamebox::Bowlingbox::~Bowlingbox() {}

/**
 * @brief Rolls a number of pins in the bowling game.
 *
 * This function takes an integer representing the number of pins rolled and
 * passes it to the BowlingSet object to update the game state.
 *
 * @param pins The number of pins rolled in the current turn.
 */
void Sandbox::Gamebox::Bowlingbox::roll(const int& pins) {
    // Pass the number of pins rolled to the BowlingSet object to update the game state
    _gameSet->roll(pins);
}

/**
 * @brief Retrieves the current score of the bowling game.
 *
 * This function calls the `score()` method of the `BowlingSet` object,
 * which returns the current score of the game.
 *
 * @return The current score of the bowling game.
 */
int Sandbox::Gamebox::Bowlingbox::score() {
    // Call the `score()` method of the `BowlingSet` object and return the result
    return _gameSet->score();
}

std::vector<int> Sandbox::Gamebox::Bowlingbox::rolls() {
    return _gameSet->rolls();
}

std::vector<int> Sandbox::Gamebox::Bowlingbox::scores() {
    return _gameSet->scores();
}

int Sandbox::Gamebox::Bowlingbox::pinsLeft() {
    return _gameSet->pinsLeft();
}
int Sandbox::Gamebox::Bowlingbox::getFrameScore(const int& frameIndex) {
    return _gameSet->frameScore(frameIndex);
}
/**
 * @brief Resets the bowling game to its initial state.
 *
 * This function calls the `reset()` method of the `BowlingSet` object,
 * which resets the game state to its initial state.
 */
void Sandbox::Gamebox::Bowlingbox::reset() {
    // Call the `reset()` method of the `BowlingSet` object to reset the game state
    _gameSet->reset();
}

bool Sandbox::Gamebox::Bowlingbox::isOver() {
    return _gameSet->isOver();
}
