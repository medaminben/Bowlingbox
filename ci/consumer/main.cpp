#include <Bowlingbox/Score/Score.h>

int main() {
    Bowlingbox::Score::ScoreCalculator game;
    for (int i = 0; i < 12; ++i)
        if (game.roll(10) != Bowlingbox::Score::RollStatus::Accepted)
            return 1;
    return game.score() == 300 ? 0 : 1;
}
