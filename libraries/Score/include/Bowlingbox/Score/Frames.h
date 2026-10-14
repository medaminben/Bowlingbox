#ifndef BOWLINGBOX_SCORE_FRAMES_H
#define BOWLINGBOX_SCORE_FRAMES_H
#include <Bowlingbox/Score/Bowlingbox_Score_export.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <Bowlingbox/Score/Notation.h>
#include <array>
#include <vector>

namespace Bowlingbox {
namespace Score {

/// One frame of the score sheet, cut out of the 21 roll slots.
struct FrameView {
    int index = 0;  // 0..9
    /// Slot values (pins or Mark) of slots 2 * index + 0, 1 and, for the tenth frame, 2.
    std::array<int, 3> rolls {toInt(Mark::None), toInt(Mark::None), toInt(Mark::None)};
    int rollCount = 2;               // 3 for the tenth frame
    int score = toInt(Mark::None);   // running total, or a Mark while not final
    bool isLast() const noexcept { return index == kFrames - 1; }
};

/// Splits the roll slots and frame totals into ten frames. Missing entries read as Mark::None.
BOWLINGBOX_SCORE_API std::array<FrameView, kFrames> frames(const std::vector<int>& rolls,
                                                           const std::vector<int>& scores);
BOWLINGBOX_SCORE_API std::array<FrameView, kFrames> frames(const IScoreSheet& sheet);

}  // namespace Score
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_SCORE_FRAMES_H
