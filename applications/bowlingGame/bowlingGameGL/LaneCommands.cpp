#include "LaneCommands.h"

namespace LaneCommands {

Result<> execute(Command command, Bowlingbox::Alley::Lane& lane) {
    switch (command.type) {
        case Command::Type::MoveAim:     return lane.moveAim(command.amount);
        case Command::Type::RotateAim:   return lane.rotateAim(command.amount);
        case Command::Type::StartCharge: return lane.startCharge();
        case Command::Type::Release:     return lane.release();
        case Command::Type::NewGame:     return lane.newGame();
        case Command::Type::Quit:
        case Command::Type::None:
            break;
    }
    return Result<>(std::monostate {});
}

}  // namespace LaneCommands
