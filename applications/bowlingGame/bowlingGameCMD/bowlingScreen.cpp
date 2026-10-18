#include "bowlingScreen.h"
#include "banner.h"
#include "scoreGrid.h"

namespace Console {

Screen::Screen(Bowlingbox::Score::IBowlingGame& game, ITerminal& terminal)
    : _session(game), _terminal(terminal) {}

void Screen::start() {
    while (!_session.quit()) {
        draw();
        const auto line = _terminal.readLine();
        if (!line)  // end of input: nothing more will come
            return;
        _session.handle(*line);
    }
}

void Screen::draw() {
    const int width = _terminal.width();
    if (_terminal.clear())
        _terminal.write(banner(width));
    _terminal.write(renderScoreSheet(_session.sheet(), width));
    _terminal.write(_session.message() + "\n" + _session.prompt() + "\n");
}

}  // namespace Console
