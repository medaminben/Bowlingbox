#include "scoreGrid.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

using namespace Bowlingbox::Score;

namespace {
/// X | 7/ | 9- | X | -8 | 8/ | -6 | X | X | X81 = 167
void play(IBowlingGame& game) {
    for (int pins : {10, 7, 3, 9, 0, 10, 0, 8, 8, 2, 0, 6, 10, 10, 10, 8, 1})
        EXPECT_EQ(game.roll(pins), Bowlingbox::Score::RollStatus::Accepted);
}

std::size_t lineCount(const std::string& text) {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}
}  // namespace

TEST(test_score_grid, eighty_columns_give_two_rows_of_five) {
    ScoreCalculator game;
    play(game);
    const std::string expected =
        " .-----------. .-----------. .-----------. .-----------. .-----------.\n"
        " |  X  |     | |  7  |  /  | |  9  |  -  | |  X  |     | |  -  |  8  |\n"
        " |     '-----| |     '-----| |     '-----| |     '-----| |     '-----|\n"
        " |     20    | |     39    | |     48    | |     66    | |     74    |\n"
        " '-----------' '-----------' '-----------' '-----------' '-----------'\n"
        " .-----------. .-----------. .-----------. .-----------. .-----------------.\n"
        " |  8  |  /  | |  -  |  6  | |  X  |     | |  X  |     | |  X  |  8  |  1  |\n"
        " |     '-----| |     '-----| |     '-----| |     '-----| |     '-----'-----|\n"
        " |     84    | |     90    | |    120    | |    148    | |       167       |\n"
        " '-----------' '-----------' '-----------' '-----------' '-----------------'\n";
    EXPECT_EQ(Console::renderScoreSheet(game, 80), expected);
}

TEST(test_score_grid, wide_terminal_gets_one_row) {
    ScoreCalculator game;
    play(game);
    const std::string expected =
        " .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------------.\n"
        " |  X  |     | |  7  |  /  | |  9  |  -  | |  X  |     | |  -  |  8  | |  8  |  /  | |  -  |  6  | |  X  |     | |  X  |     | |  X  |  8  |  1  |\n"
        " |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----'-----|\n"
        " |     20    | |     39    | |     48    | |     66    | |     74    | |     84    | |     90    | |    120    | |    148    | |       167       |\n"
        " '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------------'\n";
    EXPECT_EQ(Console::renderScoreSheet(game, 200), expected);
}

TEST(test_score_grid, narrow_terminals_wrap_to_two_or_one_frame) {
    ScoreCalculator game;
    play(game);
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, 40)), 25u);  // 5 rows of 2
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, 20)), 50u);  // 10 rows of 1
}

TEST(test_score_grid, rows_wrap_at_todays_widths) {
    ScoreCalculator game;
    play(game);
    // 5 lines per row: 10 rows of 1, 5 rows of 2, 2 rows of 5, 1 row of 10.
    const std::pair<int, std::size_t> cases[] = {{30, 50u},  {31, 25u},  {33, 25u}, {75, 25u},
                                                 {76, 10u},  {146, 10u}, {150, 10u}, {151, 5u}};
    for (const auto& [width, lines] : cases)
        EXPECT_EQ(lineCount(Console::renderScoreSheet(game, width)), lines) << "width " << width;
}

TEST(test_score_grid, unknown_or_tiny_width_still_renders) {
    ScoreCalculator game;
    play(game);
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, 0)), 50u);
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, -5)), 50u);
}

TEST(test_score_grid, empty_sheet_draws_blank_boxes) {
    ScoreCalculator game;
    const auto box = Console::drawFrame(frames(game)[0]);
    EXPECT_EQ(box[0], ".-----------.");
    EXPECT_EQ(box[1], "|     |     |");
    EXPECT_EQ(box[2], "|     '-----|");
    EXPECT_EQ(box[3], "|           |");
    EXPECT_EQ(box[4], "'-----------'");
}

TEST(test_score_grid, tenth_frame_box_is_wider_and_shows_three_rolls) {
    ScoreCalculator game;
    play(game);
    const auto box = Console::drawFrame(frames(game)[9]);
    EXPECT_EQ(box[0], ".-----------------.");
    EXPECT_EQ(box[1], "|  X  |  8  |  1  |");
    EXPECT_EQ(box[2], "|     '-----'-----|");
    EXPECT_EQ(box[3], "|       167       |");
    EXPECT_EQ(box[4], "'-----------------'");
}
