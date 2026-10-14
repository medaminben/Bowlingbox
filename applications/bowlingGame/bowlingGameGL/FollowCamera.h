#ifndef BOWLINGGL_FOLLOW_CAMERA_H
#define BOWLINGGL_FOLLOW_CAMERA_H
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Render/Render.h>

/// Camera rig: waits behind the foul line, follows the ball down the lane
/// while it is rolling and eases back afterwards.
class FollowCamera {
public:
    static constexpr float kHomeZ = -3.5f;
    static constexpr float kHeight = 1.8f;
    static constexpr float kLookAhead = 9.f;
    static constexpr float kBehindBall = 3.5f;  // metres between the camera and a rolling ball
    static constexpr float kDeckMargin = 5.5f;  // stop this far before the end of the deck
    static constexpr float kEaseRate = 6.f;     // 1/s: how fast the camera catches up

    void update(const Bowlingbox::Alley::Snapshot& s, float dt);
    void applyTo(Bowlingbox::Render::Camera& camera) const;
    float z() const { return _z; }

private:
    float _z = kHomeZ;
};
#endif  // BOWLINGGL_FOLLOW_CAMERA_H
