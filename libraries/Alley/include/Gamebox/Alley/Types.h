#ifndef GAMEBOX_ALLEY_TYPES_H
#define GAMEBOX_ALLEY_TYPES_H
#include <array>

namespace Gamebox {
namespace Alley {

/// Throw state machine phases (see spec §3.2).
enum class Phase { Aiming, Charging, Rolling, Settling, FrameDone, GameOver };

/// Physical dimensions shared with the renderer (metres).
struct LaneDimensions {
    float halfWidth    = 0.525f;
    float length       = 18.3f;   // foul line (z = 0) to end of pin deck
    float approach     = 2.0f;    // walkable area before the foul line (negative z)
    float gutterWidth  = 0.24f;
    float gutterDepth  = 0.05f;
    float ballRadius   = 0.109f;
    float pinHeight    = 0.38f;
    float pinMaxRadius = 0.06f;
    float pinComHeight = 0.12f;   // rigid-body origin above the pin base
    float pinSpacing   = 0.3048f;
    float headPinZ     = 17.4f;
};
inline constexpr LaneDimensions kLane {};
inline constexpr float kMaxAimOffset = 0.7f;  // ball fully over the gutter (0.7 - r > halfWidth)
inline constexpr float kMaxAimAngle = 6.0f;   // degrees
inline constexpr float kBallStartZ = -1.0f;

struct Pose {
    std::array<float, 3> position {0.f, 0.f, 0.f};
    std::array<float, 4> rotation {0.f, 0.f, 0.f, 1.f};  // quaternion x, y, z, w
};

struct Snapshot {
    Phase phase = Phase::Aiming;
    Pose ball;
    std::array<Pose, 10> pins;
    std::array<bool, 10> pinStanding {};  // upright on the deck right now
    std::array<bool, 10> pinInPlay {};    // still part of the world (standing or lying)
    float aimOffset = 0.f;                // lateral ball position, m, [-kMaxAimOffset, +]
    float aimAngle = 0.f;                 // degrees, [-kMaxAimAngle, +]
    float power = 0.f;                    // [0, 1]
    int lastPinsDown = -1;                // pins credited for the last throw, -1 if none yet
};

}  // namespace Alley
}  // namespace Gamebox
#endif  // GAMEBOX_ALLEY_TYPES_H
