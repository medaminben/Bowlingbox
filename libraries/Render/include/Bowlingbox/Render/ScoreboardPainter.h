#ifndef BOWLINGBOX_RENDER_SCOREBOARD_PAINTER_H
#define BOWLINGBOX_RENDER_SCOREBOARD_PAINTER_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Render/Canvas2D.h>
#include <Bowlingbox/Render/ScoreboardLayout.h>
#include <vector>

namespace Bowlingbox {
namespace Render {

/// Draws the 10-frame grid, the 21 roll cells, the frame scores and the total.
/// `rolls` / `scores` use the Bowlingbox::Score encoding.
BOWLINGBOX_RENDER_API void paintScoreboard(ICanvas2D& canvas, const std::vector<int>& rolls,
                                        const std::vector<int>& scores, int total,
                                        const ScoreboardLayout& layout);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_SCOREBOARD_PAINTER_H
