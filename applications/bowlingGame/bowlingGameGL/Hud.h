#ifndef BOWLINGGL_HUD_H
#define BOWLINGGL_HUD_H
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Render/Canvas2D.h>
#include <Bowlingbox/Score/IBowlingGame.h>

/// Screen bands in pixels: scoreboard on top, power bar / hint / result at the bottom.
struct HudLayout {
    int width;
    int height;
    int topPx;
    int bottomPx;
};

/// Draws the scoreboard, power bar, hint and last result.
void drawHud(Bowlingbox::Render::ICanvas2D& canvas, const Bowlingbox::Alley::Snapshot& s,
             const Bowlingbox::Score::IScoreSheet& sheet, const HudLayout& layout);
#endif  // BOWLINGGL_HUD_H
