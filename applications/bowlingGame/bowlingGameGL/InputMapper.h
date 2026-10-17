#ifndef BOWLINGGL_INPUT_MAPPER_H
#define BOWLINGGL_INPUT_MAPPER_H
#include "Command.h"
#include <Bowlingbox/Alley/Types.h>
#include <set>

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

/// Theme, mute and ambience switch state, so they fire once per key press.
bool isToggle(Command::Type type);

/// wx repeats key-down while a key is held; for toggles that would flash through themes.
class KeyLatch {
public:
    /// False for a repeated key-down of a held toggle key, true for everything else.
    bool accept(int keyCode, const Command& command);
    void release(int keyCode);
    void clear() { _held.clear(); }  // focus lost: the key-ups will not arrive

private:
    std::set<int> _held;  // upper-cased key codes
};
}  // namespace InputMapper
#endif  // BOWLINGGL_INPUT_MAPPER_H
