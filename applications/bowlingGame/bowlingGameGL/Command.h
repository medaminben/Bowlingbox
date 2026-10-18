#ifndef BOWLINGGL_COMMAND_H
#define BOWLINGGL_COMMAND_H

/// A player intent, independent of the input device that produced it.
struct Command {
    enum class Type { None, MoveAim, RotateAim, StartCharge, Release, NewGame, CycleTheme, ToggleMute, VolumeUp, VolumeDown,
                      ToggleAmbience, Quit };
    Type type = Type::None;
    float amount = 0.f;  // MoveAim: metres, RotateAim: degrees; unused otherwise

    bool operator==(const Command&) const = default;
};

/// Which part of BowlingGL handles a command.
enum class CommandTarget { None, Lane, View, Sound, App };

/// No default case: a new Command::Type does not compile (-Wswitch, -Werror) until it has a target.
constexpr CommandTarget targetOf(Command::Type type) {
    switch (type) {
        case Command::Type::None:           return CommandTarget::None;
        case Command::Type::MoveAim:
        case Command::Type::RotateAim:
        case Command::Type::StartCharge:
        case Command::Type::Release:
        case Command::Type::NewGame:        return CommandTarget::Lane;
        case Command::Type::CycleTheme:     return CommandTarget::View;
        case Command::Type::ToggleMute:
        case Command::Type::VolumeUp:
        case Command::Type::VolumeDown:
        case Command::Type::ToggleAmbience: return CommandTarget::Sound;
        case Command::Type::Quit:           return CommandTarget::App;
    }
    return CommandTarget::None;
}

#endif  // BOWLINGGL_COMMAND_H
