#ifndef BOWLINGGL_COMMAND_H
#define BOWLINGGL_COMMAND_H

/// A player intent, independent of the input device that produced it.
struct Command {
    enum class Type { None, MoveAim, RotateAim, StartCharge, Release, NewGame, Quit };
    Type type = Type::None;
    float amount = 0.f;  // MoveAim: metres, RotateAim: degrees; unused otherwise

    bool operator==(const Command&) const = default;
};

#endif  // BOWLINGGL_COMMAND_H
