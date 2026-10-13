#include "LaneCommands.h"
#include "InputMapper.h"

namespace LaneCommands {

Result<> execute(Command command, Gamebox::Alley::Lane& lane) {
    switch (command) {
        case Command::MoveLeft:    return lane.moveAim(-InputMapper::kKeyOffsetStep);
        case Command::MoveRight:   return lane.moveAim(InputMapper::kKeyOffsetStep);
        case Command::AngleLeft:   return lane.rotateAim(-InputMapper::kKeyAngleStep);
        case Command::AngleRight:  return lane.rotateAim(InputMapper::kKeyAngleStep);
        case Command::StartCharge: return lane.startCharge();
        case Command::Release:     return lane.release();
        case Command::NewGame:     return lane.newGame();
        case Command::Quit:
        case Command::None:
            break;
    }
    return Result<>(std::monostate {});
}

}  // namespace LaneCommands
