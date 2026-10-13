#include <Sandbox/Render/ScoreboardLayout.h>
#include <algorithm>

namespace Sandbox {
namespace Render {

Rect ScoreboardLayout::frameRect(int frame) const {
    const float w = (frame == 9 ? 3.f : 2.f) * cell;
    return {originX + 2.f * cell * static_cast<float>(frame), originY, w, 2.f * cell};
}

Rect ScoreboardLayout::rollCellRect(int roll) const {
    const int frame = std::min(roll / 2, 9);
    const int within = roll - 2 * frame;  // 0, 1 (or 2 in the last frame)
    const Rect f = frameRect(frame);
    return {f.x + cell * static_cast<float>(within), f.y, cell, cell};
}

Rect ScoreboardLayout::scoreRect(int frame) const {
    const Rect f = frameRect(frame);
    return {f.x, f.y + cell, f.w, cell};
}

std::string glyphFor(int value, bool isScore) {
    switch (value) {
        case -4:
        case -3:
            return "";
        case -2:
            return "X";
        case -1:
            return "/";
        case 0:
            return isScore ? "0" : "-";
        default:
            return std::to_string(value);
    }
}

}  // namespace Render
}  // namespace Sandbox
