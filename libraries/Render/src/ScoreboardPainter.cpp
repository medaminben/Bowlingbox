#include <Gamebox/Render/ScoreboardPainter.h>
#include <algorithm>
#include <string>

namespace Gamebox {
namespace Render {

void paintScoreboard(ICanvas2D& canvas, const std::vector<int>& rolls, const std::vector<int>& scores,
                     int total, const ScoreboardLayout& layout) {
    const glm::vec4 line {0.f, 0.898f, 1.f, 1.f};
    const glm::vec4 panel {0.008f, 0.055f, 0.22f, 0.78f};
    const glm::vec3 ink {1.f, 1.f, 1.f};
    const float textScale = layout.cell / 20.f;
    const float t = std::max(1.f, layout.cell / 30.f);

    const Rect board {layout.originX, layout.originY, layout.totalWidth() + layout.cell * 5.f,
                      layout.cell * 2.f};
    canvas.drawRect({board.x, board.y}, {board.w, board.h}, panel);

    auto centred = [&](const Rect& r, const std::string& s) {
        if (s.empty())
            return;
        const glm::vec2 size = canvas.measureText(s, textScale);
        canvas.drawText(s, {r.x + (r.w - size.x) / 2, r.y + (r.h - size.y) / 2}, textScale, ink);
    };

    for (int f = 0; f < 10; ++f) {
        const Rect fr = layout.frameRect(f);
        canvas.strokeRect(fr, t, line);
        const int cells = f == 9 ? 3 : 2;
        for (int c = 0; c < cells; ++c) {
            const int roll = 2 * f + c;
            const Rect rc = layout.rollCellRect(roll);
            if (c > 0)
                canvas.drawRect({rc.x, rc.y}, {t, rc.h}, line);
            if (roll < static_cast<int>(rolls.size()))
                centred(rc, glyphFor(rolls[static_cast<std::size_t>(roll)]));
        }
        const Rect sr = layout.scoreRect(f);
        canvas.drawRect({sr.x, sr.y}, {sr.w, t}, line);
        if (f < static_cast<int>(scores.size()))
            centred(sr, glyphFor(scores[static_cast<std::size_t>(f)], true));
    }
    const Rect totalRect {layout.originX + layout.totalWidth() + layout.cell * 0.5f, layout.originY,
                          layout.cell * 4.f, layout.cell * 2.f};
    centred(totalRect, "Total " + std::to_string(total));
}

}  // namespace Render
}  // namespace Gamebox
