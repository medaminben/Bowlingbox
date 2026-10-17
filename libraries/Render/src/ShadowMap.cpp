#include "ShadowMap.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {

bool ShadowMap::create() {
    return _fbo.create({kWidth, kHeight, 0, 0, false, true});
}

void ShadowMap::begin() const {
    _fbo.bind();
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_CLAMP);  // casters between the light and the box still cast
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
}

void ShadowMap::end() const {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_DEPTH_CLAMP);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace Render
}  // namespace Bowlingbox
