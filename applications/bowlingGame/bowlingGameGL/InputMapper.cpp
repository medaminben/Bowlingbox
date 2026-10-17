#include "InputMapper.h"
#include <wx/defs.h>

using Bowlingbox::Alley::Phase;
using Type = Command::Type;

namespace {
bool canAim(Phase p) {
    return p == Phase::Aiming || p == Phase::Charging;
}

int upperCased(int keyCode) {
    return (keyCode >= 'a' && keyCode <= 'z') ? keyCode - 'a' + 'A' : keyCode;
}
}  // namespace

namespace InputMapper {

Command keyDown(int keyCode, bool ctrl, Phase phase) {
    if (keyCode == WXK_ESCAPE || (ctrl && (keyCode == 'Q' || keyCode == 'q')))
        return {Type::Quit};
    if (keyCode == 'N' || keyCode == 'n')
        return {Type::NewGame};
    if (keyCode == 'T' || keyCode == 't')
        return {Type::CycleTheme};
    if (keyCode == 'M' || keyCode == 'm')
        return {Type::ToggleMute};
    if (keyCode == 'B' || keyCode == 'b')
        return {Type::ToggleAmbience};
    // '=' is the unshifted '+' on US layouts.
    if (keyCode == '+' || keyCode == '=' || keyCode == WXK_ADD || keyCode == WXK_NUMPAD_ADD)
        return {Type::VolumeUp};
    if (keyCode == '-' || keyCode == WXK_SUBTRACT || keyCode == WXK_NUMPAD_SUBTRACT)
        return {Type::VolumeDown};
    switch (keyCode) {
        case WXK_LEFT:
            return canAim(phase) ? Command {Type::MoveAim, -kKeyOffsetStep} : Command {};
        case WXK_RIGHT:
            return canAim(phase) ? Command {Type::MoveAim, kKeyOffsetStep} : Command {};
        case 'A':
        case 'a':
            return canAim(phase) ? Command {Type::RotateAim, -kKeyAngleStep} : Command {};
        case 'D':
        case 'd':
            return canAim(phase) ? Command {Type::RotateAim, kKeyAngleStep} : Command {};
        case WXK_SPACE:
            return phase == Phase::Aiming ? Command {Type::StartCharge} : Command {};
        default:
            return {};
    }
}

Command keyUp(int keyCode, Phase phase) {
    if (keyCode == WXK_SPACE && phase == Phase::Charging)
        return {Type::Release};
    return {};
}

Command mouseDown(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Aiming) ? Command {Type::StartCharge} : Command {};
}

Command mouseUp(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Charging) ? Command {Type::Release} : Command {};
}

Command mouseMove(int dx, bool leftDown, bool rightDown, int canvasWidthPx, Phase phase) {
    if (dx == 0 || leftDown || phase != Phase::Aiming)
        return {};
    if (rightDown)
        return {Type::RotateAim, kMouseAngleStep * static_cast<float>(dx)};
    if (canvasWidthPx <= 0)  // minimised: no meaningful scale
        return {};
    return {Type::MoveAim, kMouseOffsetSpan * static_cast<float>(dx) / static_cast<float>(canvasWidthPx)};
}

bool isToggle(Type type) {
    return type == Type::CycleTheme || type == Type::ToggleMute || type == Type::ToggleAmbience;
}

bool KeyLatch::accept(int keyCode, const Command& command) {
    if (!isToggle(command.type))
        return true;
    return _held.insert(upperCased(keyCode)).second;
}

void KeyLatch::release(int keyCode) {
    _held.erase(upperCased(keyCode));
}

}  // namespace InputMapper
