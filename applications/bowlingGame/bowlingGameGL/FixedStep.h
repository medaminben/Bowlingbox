#ifndef BOWLINGGL_FIXED_STEP_H
#define BOWLINGGL_FIXED_STEP_H

/// Turns irregular timer ticks into whole physics steps of kDt, so the game
/// runs at the same speed whatever the timer jitter.
class FixedStep {
public:
    static constexpr float kDt = 1.f / 60.f;
    static constexpr int kMaxSteps = 5;  // after a stall, drop the backlog instead of fast-forwarding

    /// Adds the wall time since the last tick; returns how many kDt steps to run now.
    int advance(float elapsedSeconds);

private:
    float _pending = 0.f;
};
#endif  // BOWLINGGL_FIXED_STEP_H
