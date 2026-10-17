#include "PostChain.h"
#include "ContextData.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {

PostChain::PostChain(ContextData& d) : _d(d) {
    glGenVertexArrays(1, &_vao);
}
PostChain::~PostChain() {
    glDeleteVertexArrays(1, &_vao);
}

bool PostChain::ensureSize(const FrameSizes& sizes, int samples) {
    if (sizes.scene == _size && samples == _samples)
        return true;
    _size = sizes.scene;
    _samples = samples;
    return _msaa.create({_size.x, _size.y, samples, GL_RGBA16F, true, false}) &&
           _resolved.create({_size.x, _size.y, 0, GL_RGBA16F, false, false});
}

void PostChain::beginScene() {
    _msaa.bind();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void PostChain::drawTriangle() const {
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PostChain::resolve() {
    if (_samples == 0) {  // no MSAA on this GPU: both targets are plain textures
        _msaa.blitColorTo(_resolved);
        return;
    }
    _resolved.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *_d.resolve;
    s.use();
    s.setInt("uSrc", 0);
    s.setInt("uSamples", _samples);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _msaa.colorTexture());
    drawTriangle();
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
}

void PostChain::present(int viewportY, float exposure, float bloomStrength) {
    resolve();
    // Task 8 inserts the bloom chain here.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, viewportY, _size.x, _size.y);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *_d.composite;
    s.use();
    s.setInt("uScene", 0);
    s.setInt("uBloom", 1);
    s.setBool("uHasBloom", false);
    s.setFloat("uBloomStrength", bloomStrength);
    s.setFloat("uExposure", exposure);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _resolved.colorTexture());
    drawTriangle();
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace Render
}  // namespace Bowlingbox
