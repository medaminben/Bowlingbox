#include "InputMapper.h"
#include <wx/defs.h>

using Gamebox::Alley::Phase;

namespace {
bool canAim(Phase p) {
    return p == Phase::Aiming || p == Phase::Charging;
}
}  // namespace

namespace InputMapper {

Command keyDown(int keyCode, bool ctrl, Phase phase) {
    if (keyCode == WXK_ESCAPE || (ctrl && (keyCode == 'Q' || keyCode == 'q')))
        return Command::Quit;
    if (keyCode == 'N' || keyCode == 'n')
        return Command::NewGame;
    switch (keyCode) {
        case WXK_LEFT:
            return canAim(phase) ? Command::MoveLeft : Command::None;
        case WXK_RIGHT:
            return canAim(phase) ? Command::MoveRight : Command::None;
        case 'A':
        case 'a':
            return canAim(phase) ? Command::AngleLeft : Command::None;
        case 'D':
        case 'd':
            return canAim(phase) ? Command::AngleRight : Command::None;
        case WXK_SPACE:
            return phase == Phase::Aiming ? Command::StartCharge : Command::None;
        default:
            return Command::None;
    }
}

Command keyUp(int keyCode, Phase phase) {
    if (keyCode == WXK_SPACE && phase == Phase::Charging)
        return Command::Release;
    return Command::None;
}

Command mouseDown(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Aiming) ? Command::StartCharge : Command::None;
}

Command mouseUp(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Charging) ? Command::Release : Command::None;
}

float mouseDxToOffset(int dx, int canvasWidthPx) {
    return 1.2f * static_cast<float>(dx) / static_cast<float>(canvasWidthPx > 0 ? canvasWidthPx : 1);
}

float mouseDxToAngle(int dx) {
    return 0.05f * static_cast<float>(dx);
}

}  // namespace InputMapper
