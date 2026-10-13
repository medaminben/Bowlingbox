#include <Gamebox/Score/Score.h>
#include "bowlingScreen.h"
#include "terminal.h"

#include <memory>

int main(void) {
    // Setup: the concrete collaborators are chosen here and nowhere else.
    Console::SystemTerminal terminal;
    Console::Screen screen(std::make_shared<Gamebox::Score::Bowlingbox>(), terminal);
    // loop
    screen.start();
    return 0;
}
