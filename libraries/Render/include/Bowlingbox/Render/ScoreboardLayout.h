#ifndef BOWLINGBOX_RENDER_SCOREBOARD_LAYOUT_H
#define BOWLINGBOX_RENDER_SCOREBOARD_LAYOUT_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <string>

namespace Bowlingbox {
namespace Render {

struct Rect {
    float x, y, w, h;
};

/// Pixel geometry of the 10-frame grid. Frames 0..8 are 2 cells wide, frame 9
/// is 3 cells wide; the top row holds the rolls, the bottom row the score.
struct BOWLINGBOX_RENDER_API ScoreboardLayout {
    float originX;
    float originY;
    float cell;

    Rect frameRect(int frame) const;      // 0..9
    Rect rollCellRect(int roll) const;    // 0..20, Bowlingbox 21-slot encoding
    Rect scoreRect(int frame) const;      // 0..9
    float totalWidth() const { return cell * 21.f; }
};

/// Bowlingbox value -> text; see Bowlingbox::Score::notation.
BOWLINGBOX_RENDER_API std::string glyphFor(int value, bool isScore = false);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_SCOREBOARD_LAYOUT_H
