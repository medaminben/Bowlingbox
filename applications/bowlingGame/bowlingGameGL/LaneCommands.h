#ifndef BOWLINGGL_LANE_COMMANDS_H
#define BOWLINGGL_LANE_COMMANDS_H
#include "Command.h"
#include <Bowlingbox/Alley/Alley.h>

/// Applies a CommandTarget::Lane command to the lane, using its amount for MoveAim / RotateAim.
/// Any other command succeeds without effect; BowlingCanvas::apply never passes one.
namespace LaneCommands {
Bowlingbox::Result<> execute(Command command, Bowlingbox::Alley::Lane& lane);
}  // namespace LaneCommands
#endif  // BOWLINGGL_LANE_COMMANDS_H
