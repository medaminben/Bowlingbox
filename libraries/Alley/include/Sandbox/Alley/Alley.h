#ifndef SANDBOX_ALLEY_H
#define SANDBOX_ALLEY_H
#include <Sandbox/Alley/Sandbox_Alley_export.h>
#include <Sandbox/Core/Core.h>
#include <array>
#include <memory>
#include <vector>

namespace Sandbox {
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

class LaneImpl;

class SANDBOX_ALLEY_API Lane {
public:
    Lane();
    ~Lane();
    Lane(const Lane&) = delete;
    Lane& operator=(const Lane&) = delete;

    // Input. Valid only in Aiming / Charging (newGame: any phase); otherwise Error.
    Result<> moveAim(float deltaOffset);
    Result<> rotateAim(float deltaAngleDeg);
    Result<> startCharge();  // Aiming  -> Charging
    Result<> release();      // Charging -> Rolling
    Result<> newGame();      // any      -> Aiming

    // Time.
    void step(float dtSeconds) noexcept;

    // Read.
    Snapshot snapshot() const;
    std::vector<int> rolls() const;        // 21-slot Gamebox encoding
    std::vector<int> frameScores() const;  // 10 running scores, -3 when not yet known
    int totalScore() const;
    bool isGameOver() const;
    int pinsLeft() const;

private:
    std::unique_ptr<LaneImpl> _impl;
};

}  // namespace Alley
}  // namespace Sandbox
#endif  // SANDBOX_ALLEY_H
