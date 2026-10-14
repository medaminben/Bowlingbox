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

    void update(const Bowlingbox::Alley::Snapshot& s, float dt);
    void applyTo(Bowlingbox::Render::Camera& camera) const;
    float z() const { return _z; }

private:
    float _z = kHomeZ;
};
#endif  // BOWLINGGL_FOLLOW_CAMERA_H
