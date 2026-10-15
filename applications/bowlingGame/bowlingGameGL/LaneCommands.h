#ifndef BOWLINGGL_LANE_COMMANDS_H
#define BOWLINGGL_LANE_COMMANDS_H
#include "Command.h"
#include <Bowlingbox/Alley/Alley.h>

/// Applies a game command to the lane, using its amount for MoveAim / RotateAim.
/// CycleTheme, Quit and None are not lane commands and succeed without effect;
/// the caller handles CycleTheme and Quit.
namespace LaneCommands {
Result<> execute(Command command, Bowlingbox::Alley::Lane& lane);
}  // namespace LaneCommands
#endif  // BOWLINGGL_LANE_COMMANDS_H
