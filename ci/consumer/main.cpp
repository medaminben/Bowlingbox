#include <Gamebox/Score/Score.h>

int main() {
    Gamebox::Score::Bowlingbox game;
    for (int i = 0; i < 12; ++i)
        game.roll(10);
    return game.score() == 300 ? 0 : 1;
}
