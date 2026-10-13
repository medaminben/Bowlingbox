#ifndef GAMEBOX_RENDER_SCOREBOARD_PAINTER_H
#define GAMEBOX_RENDER_SCOREBOARD_PAINTER_H
#include <Gamebox/Render/Gamebox_Render_export.h>
#include <Gamebox/Render/Canvas2D.h>
#include <Gamebox/Render/ScoreboardLayout.h>
#include <vector>

namespace Gamebox {
namespace Render {

/// Draws the 10-frame grid, the 21 roll cells, the frame scores and the total.
/// `rolls` / `scores` use the Gamebox::Score encoding.
GAMEBOX_RENDER_API void paintScoreboard(ICanvas2D& canvas, const std::vector<int>& rolls,
                                        const std::vector<int>& scores, int total,
                                        const ScoreboardLayout& layout);

}  // namespace Render
}  // namespace Gamebox
#endif  // GAMEBOX_RENDER_SCOREBOARD_PAINTER_H
