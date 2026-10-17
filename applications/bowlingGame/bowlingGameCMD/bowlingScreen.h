#ifndef BOWLING_SCREEN_H
#define BOWLING_SCREEN_H
#include <Bowlingbox/Score/IBowlingGame.h>

#include "session.h"
#include "terminal.h"

namespace Console {

/// Console front-end: draws the banner, score sheet and menu, and feeds input
/// lines to a Session. Scoring and terminal I/O are injected.
class Screen {
public:
    /// Draws and runs a game owned by the caller; `game` and `terminal` must outlive the Screen.
    Screen(Bowlingbox::Score::IBowlingGame& game, ITerminal& terminal);
    /// Runs until "q" or the end of input.
    void start();

private:
    void draw();

    Session _session;
    ITerminal& _terminal;
};
}  // namespace Console

#endif  // BOWLING_SCREEN_H
