#ifndef SCORE_GRID_H
#define SCORE_GRID_H
#include <Gamebox/Score/IBowlingGame.h>
#include <Gamebox/Score/Notation.h>
#include <string>
#include <vector>

namespace Console {

/// ASCII art for one frame of the score sheet.
class Frame {
public:
    enum class Type { Regular = 0, Last = 1 };
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
                  const int& last_roll = Gamebox::Score::toInt(Gamebox::Score::Mark::None));

protected:
    Type _type = Type::Regular;

    std::string _firstRoll {"   "};
    std::string _secondRoll {"   "};
    std::string _framescore {"   "};
    std::string _lastRoll {"   "};
    std::vector<std::string> _frame {};
};

/// The ten frames of a game, laid out to fit a terminal width.
class ScoreGrid {
public:
    ScoreGrid();
    void reset();
    void update(const Gamebox::Score::IScoreSheet& sheet);
    /// Rows of frames separated by newlines; wraps at 5, 2 or 1 frames per row.
    std::string render(int width) const;

private:
    std::vector<Frame> _frames {};
};
}  // namespace Console
#endif  // SCORE_GRID_H
