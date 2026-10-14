#ifndef BOWLINGGL_INPUT_MAPPER_H
#define BOWLINGGL_INPUT_MAPPER_H
#include "Command.h"
#include <Bowlingbox/Alley/Types.h>

/// Pure mapping from wx key codes / mouse buttons + current phase to commands.
namespace InputMapper {
constexpr float kKeyOffsetStep = 0.02f;  // metres per key event
constexpr float kKeyAngleStep = 0.25f;   // degrees per key event

Command keyDown(int keyCode, bool ctrl, Bowlingbox::Alley::Phase phase);
Command keyUp(int keyCode, Bowlingbox::Alley::Phase phase);
Command mouseDown(bool leftButton, Bowlingbox::Alley::Phase phase);
Command mouseUp(bool leftButton, Bowlingbox::Alley::Phase phase);
float mouseDxToOffset(int dx, int canvasWidthPx);  // 1.2 m across the whole canvas
float mouseDxToAngle(int dx);                       // 0.05° per pixel
}  // namespace InputMapper
#endif  // BOWLINGGL_INPUT_MAPPER_H
