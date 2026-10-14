#ifndef BOWLINGGL_INPUT_MAPPER_H
#define BOWLINGGL_INPUT_MAPPER_H
#include "Command.h"
#include <Bowlingbox/Alley/Types.h>

/// Pure mapping from wx key codes / mouse events + current phase to commands.
namespace InputMapper {
constexpr float kKeyOffsetStep = 0.02f;   // metres per key event
constexpr float kKeyAngleStep = 0.25f;    // degrees per key event
constexpr float kMouseOffsetSpan = 1.2f;  // metres across the whole canvas
constexpr float kMouseAngleStep = 0.05f;  // degrees per pixel

Command keyDown(int keyCode, bool ctrl, Bowlingbox::Alley::Phase phase);
Command keyUp(int keyCode, Bowlingbox::Alley::Phase phase);
Command mouseDown(bool leftButton, Bowlingbox::Alley::Phase phase);
Command mouseUp(bool leftButton, Bowlingbox::Alley::Phase phase);
/// The pointer moved dx pixels. While aiming, a right-drag turns the aim and a
/// move with no button shifts the ball; a left-drag (charging) does nothing.
Command mouseMove(int dx, bool leftDown, bool rightDown, int canvasWidthPx, Bowlingbox::Alley::Phase phase);
}  // namespace InputMapper
#endif  // BOWLINGGL_INPUT_MAPPER_H
