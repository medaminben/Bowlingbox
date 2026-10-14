#include <Bowlingbox/Score/Frames.h>
#include <cstddef>

namespace Bowlingbox {
namespace Score {

namespace {
int valueAt(const std::vector<int>& values, int i) {
    return i < static_cast<int>(values.size()) ? values[static_cast<std::size_t>(i)] : toInt(Mark::None);
}
}  // namespace

std::array<FrameView, kFrames> frames(const std::vector<int>& rolls, const std::vector<int>& scores) {
    std::array<FrameView, kFrames> out {};
    for (int f = 0; f < kFrames; ++f) {
        FrameView& view = out[static_cast<std::size_t>(f)];
        view.index = f;
        view.rollCount = f == kFrames - 1 ? 3 : 2;
        for (int r = 0; r < view.rollCount; ++r)
            view.rolls[static_cast<std::size_t>(r)] = valueAt(rolls, 2 * f + r);
        view.score = valueAt(scores, f);
    }
    return out;
}

std::array<FrameView, kFrames> frames(const IScoreSheet& sheet) {
    return frames(sheet.rolls(), sheet.scores());
}

}  // namespace Score
}  // namespace Bowlingbox
