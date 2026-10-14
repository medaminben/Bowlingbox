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
    int middlePx() const { return height - topPx - bottomPx; }  // the 3D scene
};

/// The bands for a width x height viewport: 18 % on top, 10 % at the bottom.
HudLayout hudLayout(int width, int height);

/// Draws the scoreboard, power bar, hint and last result.
void drawHud(Bowlingbox::Render::ICanvas2D& canvas, const Bowlingbox::Alley::Snapshot& s,
             const Bowlingbox::Score::IScoreSheet& sheet, const HudLayout& layout);
#endif  // BOWLINGGL_HUD_H
