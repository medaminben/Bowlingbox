#ifndef BOWLINGGL_COMMAND_H
#define BOWLINGGL_COMMAND_H

/// Player intents, independent of the input device that produced them.
enum class Command { None, MoveLeft, MoveRight, AngleLeft, AngleRight, StartCharge, Release, NewGame, Quit };

#endif  // BOWLINGGL_COMMAND_H
