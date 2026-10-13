#ifndef BOWLINGGL_INPUT_MAPPER_H
#define BOWLINGGL_INPUT_MAPPER_H
#include "Command.h"
#include <Gamebox/Alley/Types.h>

/// Pure mapping from wx key codes / mouse buttons + current phase to commands.
namespace InputMapper {
constexpr float kKeyOffsetStep = 0.02f;  // metres per key event
constexpr float kKeyAngleStep = 0.25f;   // degrees per key event

Command keyDown(int keyCode, bool ctrl, Gamebox::Alley::Phase phase);
Command keyUp(int keyCode, Gamebox::Alley::Phase phase);
Command mouseDown(bool leftButton, Gamebox::Alley::Phase phase);
Command mouseUp(bool leftButton, Gamebox::Alley::Phase phase);
float mouseDxToOffset(int dx, int canvasWidthPx);  // 1.2 m across the whole canvas
float mouseDxToAngle(int dx);                       // 0.05° per pixel
}  // namespace InputMapper
#endif  // BOWLINGGL_INPUT_MAPPER_H
