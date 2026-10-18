#include <Bowlingbox/Score/Score.h>
#include "bowlingScreen.h"
#include "terminal.h"

int main(void) {
    // Setup: the concrete collaborators are chosen here and nowhere else, and live as long as main.
    Bowlingbox::Score::ScoreCalculator game;
    Console::SystemTerminal terminal;
    Console::Screen screen(game, terminal);
    // loop
    screen.start();
    return 0;
}
