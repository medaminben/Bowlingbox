#ifndef BOWLING_SCREEN_H
#define BOWLING_SCREEN_H
#include <iostream>
#include <string>
#include <vector>

#include <memory>

#include <Sandbox/Gamebox/Gamebox.h>

using namespace Sandbox::Gamebox;

namespace Console {
class Frame;

class Screen {
public:
    Screen(std::shared_ptr<Bowlingbox>);
    void SetUp();
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
    bool isFirstStart();
    bool isTryStart();
    bool isStarted();
    bool isRunning();
    bool isEndGame();
    bool isExit();
    bool isTypo();
    bool isGame();

    void updateGrid();
    void resetGame();

    void plotIntr();
    void plotGrid();
    void plotMenu();
    void evaluate();
    void setStatus(Status);

    std::shared_ptr<Bowlingbox> _game {nullptr};
    std::vector<Frame> _frames {};
    Status _status {Status::FristStart};
    std::string _lastMessage {};

    int getConsoleWidth();
    bool clearScreen();
};

class Frame {
public:
    enum class Type { Regular = 0, Last = 1 };
    enum class Bonus { None = -3, Strike = -2, Spare = -1 };
    enum class Order { First = 1, Second = 2, Last };
    Frame(Type type = Type::Regular);

public:
    void setRoll(const Order&, const int&);
    void setScore(const int&);

public:
    void updateRolls();
    void updateScore();
    void resetFrame();
    std::vector<std::string> getFrame() const;
    void setFrame(const int& frame_score, const int& first_roll, const int& second_roll,
                  const int& last_roll = (int)Bonus::None);

protected:
    Type _type = Type::Regular;

    std::string _firstRoll {"   "};
    std::string _secondRoll {"   "};
    std::string _framescore {"   "};
    std::string _lastRoll {"   "};
    std::vector<std::string> _frame {};
};
}  // namespace Console

#endif  // BOWLING_SCREEN_H