#ifndef BOWLINGBOX_RENDER_SHADOWMAP_H
#define BOWLINGBOX_RENDER_SHADOWMAP_H
#include "GlFramebuffer.h"

namespace Bowlingbox {
namespace Render {

/// The key light's depth map, sampled through sampler2DShadow.
class ShadowMap {
public:
    // Same memory as 2048^2, shaped like the lane: fitLightFrustum puts the lane along y.
    static constexpr int kWidth = 1024, kHeight = 4096;
    bool create();       // false if the depth framebuffer is incomplete
    void begin() const;  // binds, clears depth, turns on depth clamp and slope-scaled polygon offset
    void end() const;    // turns them off, binds the default framebuffer
    unsigned depthTexture() const { return _fbo.depthTexture(); }

private:
    GlFramebuffer _fbo;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_SHADOWMAP_H
