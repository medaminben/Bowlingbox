#ifndef BOWLINGGL_HUD_H
#define BOWLINGGL_HUD_H
#include <Gamebox/Alley/Types.h>
#include <Gamebox/Render/Canvas2D.h>
#include <Gamebox/Score/IBowlingGame.h>

/// Screen bands in pixels: scoreboard on top, power bar / hint / result at the bottom.
struct HudLayout {
    int width;
    int height;
    int topPx;
    int bottomPx;
};

/// Draws the scoreboard, power bar, hint and last result.
void drawHud(Gamebox::Render::ICanvas2D& canvas, const Gamebox::Alley::Snapshot& s,
             const Gamebox::Score::IScoreSheet& sheet, const HudLayout& layout);
#endif  // BOWLINGGL_HUD_H
