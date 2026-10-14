#include "Hud.h"
#include "HudText.h"
#include <Bowlingbox/Render/ScoreboardPainter.h>
#include <algorithm>

using namespace Bowlingbox;

namespace {
const glm::vec4 kBarOutline {0.f, 0.898f, 1.f, 1.f};  // cyan, as the scoreboard lines
const glm::vec4 kPowerLow {1.f, 0.541f, 0.f, 1.f};    // orange
const glm::vec4 kPowerHigh {1.f, 0.780f, 0.f, 1.f};   // amber
const glm::vec3 kHintInk {0.85f, 0.85f, 0.85f};
const glm::vec3 kResultInk {1.f, 0.780f, 0.2f};
}  // namespace

HudLayout hudLayout(int width, int height) {
    return {width, height, static_cast<int>(static_cast<float>(height) * 0.18f),
            static_cast<int>(static_cast<float>(height) * 0.10f)};
}

void drawHud(Render::ICanvas2D& canvas, const Alley::Snapshot& s, const Score::IScoreSheet& sheet,
             const HudLayout& l) {
    const float w = static_cast<float>(l.width), h = static_cast<float>(l.height);
    const float topPx = static_cast<float>(l.topPx), bottomPx = static_cast<float>(l.bottomPx);
    const float margin = w * 0.02f;
    float cell = (w - 2 * margin) / 25.f;  // 21 cells + room for the total
    cell = std::min(cell, (topPx - 2 * margin) / 2.f);
    const Render::ScoreboardLayout layout {margin, margin, cell};
    Render::paintScoreboard(canvas, sheet.rolls(), sheet.scores(), sheet.score(), layout);

    // Power bar (left of the bottom band).
    const float barY = h - bottomPx * 0.7f, barH = bottomPx * 0.4f, barW = w * 0.3f;
    canvas.strokeRect({margin, barY, barW, barH}, 2.f, kBarOutline);
    const glm::vec4 powerFill = glm::mix(kPowerLow, kPowerHigh, s.power);
    canvas.drawRect({margin + 2.f, barY + 2.f}, {(barW - 4.f) * s.power, barH - 4.f}, powerFill);

    // Hint (centre) and result (right).
    const float textScale = std::max(1.f, bottomPx / 40.f);
    const std::string hint = HudText::hint(s.phase);
    const glm::vec2 hintSize = canvas.measureText(hint, textScale);
    canvas.drawText(hint, {(w - hintSize.x) / 2, barY + (barH - hintSize.y) / 2}, textScale, kHintInk);
    const std::string result = HudText::result(s, sheet);
    const glm::vec2 resSize = canvas.measureText(result, textScale * 1.5f);
    canvas.drawText(result, {w - margin - resSize.x, barY + (barH - resSize.y) / 2}, textScale * 1.5f,
                    kResultInk);
}
