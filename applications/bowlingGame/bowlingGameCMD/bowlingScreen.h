#ifndef BOWLING_SCREEN_H
#define BOWLING_SCREEN_H
#include <memory>
#include <string>

#include <Bowlingbox/Score/IBowlingGame.h>

#include "scoreGrid.h"
#include "terminal.h"

namespace Console {

/// Game flow of the console front-end: banner, grid, menu, input.
/// Scoring and terminal I/O are injected.
class Screen {
public:
    Screen(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game, ITerminal& terminal);
    void start();

protected:
    enum class Status {
        FristStart = 0,
        TryStart = 1,
        Started = 2,
        Running = 3,
        End = 4,
        Quit = 5,
        Typo = 6
    };
    bool isTryStart();
    bool isStarted();
    bool isRunning();
    bool isExit();
    bool isTypo();
    bool isGame();

    void resetGame();

    void plotIntr();
    void plotGrid();
    void plotMenu();
    void evaluate();
    void setStatus(Status);
    void println(const std::string& text);

    std::shared_ptr<Bowlingbox::Score::IBowlingGame> _game {nullptr};
    ITerminal& _terminal;
    ScoreGrid _grid {};
    Status _status {Status::FristStart};
    std::string _lastMessage {};
};
}  // namespace Console

#endif  // BOWLING_SCREEN_H
