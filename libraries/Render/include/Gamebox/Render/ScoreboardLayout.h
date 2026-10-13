#ifndef GAMEBOX_RENDER_SCOREBOARD_LAYOUT_H
#define GAMEBOX_RENDER_SCOREBOARD_LAYOUT_H
#include <Gamebox/Render/Gamebox_Render_export.h>
#include <string>

namespace Gamebox {
namespace Render {

struct Rect {
    float x, y, w, h;
};

/// Pixel geometry of the 10-frame grid. Frames 0..8 are 2 cells wide, frame 9
/// is 3 cells wide; the top row holds the rolls, the bottom row the score.
struct GAMEBOX_RENDER_API ScoreboardLayout {
    float originX;
    float originY;
    float cell;

    Rect frameRect(int frame) const;      // 0..9
    Rect rollCellRect(int roll) const;    // 0..20, Gamebox 21-slot encoding
    Rect scoreRect(int frame) const;      // 0..9
    float totalWidth() const { return cell * 21.f; }
};

/// Gamebox value -> text; see Gamebox::Score::notation.
GAMEBOX_RENDER_API std::string glyphFor(int value, bool isScore = false);

}  // namespace Render
}  // namespace Gamebox
#endif  // GAMEBOX_RENDER_SCOREBOARD_LAYOUT_H
