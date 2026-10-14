#include "scoreGrid.h"
#include <Bowlingbox/Score/Notation.h>
#include <algorithm>
#include <cstddef>

namespace Console {
namespace {
/// Score sheet notation centred in three columns; "   " when blank.
std::string cell(int value, bool isScore = false) {
    const std::string text = Bowlingbox::Score::notation(value, isScore);
    switch (text.size()) {
        case 0:
            return "   ";
        case 1:
            return " " + text + " ";
        case 2:
            return " " + text;
        default:
            return text;
    }
}

/// Frames per row, at the same widths as before the refactor. Those came from
/// ((width - 16) / 15) + 1 rounded down to 10, 5, 2 or 1. A row of 2 is 34
/// columns wide, so at 31-33 columns the terminal wraps it; that is kept on purpose.
int framesPerRow(int width) {
    if (width >= 151)
        return 10;
    if (width >= 76)
        return 5;
    if (width >= 31)
        return 2;
    return 1;
}
}  // namespace

std::array<std::string, 5> drawFrame(const Bowlingbox::Score::FrameView& frame) {
    const std::string first = cell(frame.rolls[0]);
    const std::string second = cell(frame.rolls[1]);
    const std::string score = cell(frame.score, true);
    if (frame.isLast()) {
        const std::string third = cell(frame.rolls[2]);
        return {".-----------------.",
                "| " + first + " | " + second + " | " + third + " |",
                "|     '-----'-----|",
                "|       " + score + "       |",
                "'-----------------'"};
    }
    return {".-----------.",
            "| " + first + " | " + second + " |",
            "|     '-----|",
            "|    " + score + "    |",
            "'-----------'"};
}

std::string renderScoreSheet(const Bowlingbox::Score::IScoreSheet& sheet, int width) {
    using Bowlingbox::Score::kFrames;
    const auto frames = Bowlingbox::Score::frames(sheet);
    const int perRow = framesPerRow(width);
    std::string out;
    for (int rowStart = 0; rowStart < kFrames; rowStart += perRow) {
        std::array<std::string, 5> lines;
        const int rowEnd = std::min(rowStart + perRow, kFrames);
        for (int f = rowStart; f < rowEnd; ++f) {
            const auto box = drawFrame(frames[static_cast<std::size_t>(f)]);
            for (std::size_t k = 0; k < lines.size(); ++k)
                lines[k] += " " + box[k];
        }
        for (const auto& line : lines)
            out += line + "\n";
    }
    return out;
}

}  // namespace Console
