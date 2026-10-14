#ifndef SCORE_GRID_H
#define SCORE_GRID_H
#include <Bowlingbox/Score/Frames.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <array>
#include <string>

namespace Console {

/// Five text lines for one frame box: 13 columns, 19 for the tenth frame.
std::array<std::string, 5> drawFrame(const Bowlingbox::Score::FrameView& frame);

/// The ten frame boxes, each preceded by a space, wrapped to fit `width`
/// columns: 10, 5, 2 or 1 frames per row.
std::string renderScoreSheet(const Bowlingbox::Score::IScoreSheet& sheet, int width);

}  // namespace Console
#endif  // SCORE_GRID_H
