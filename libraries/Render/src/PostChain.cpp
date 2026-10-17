#include "PostChain.h"
#include "ContextData.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {

namespace {
void bindUnit(unsigned unit, unsigned target, unsigned tex) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(target, tex);
    glActiveTexture(GL_TEXTURE0);
}
}  // namespace

PostChain::PostChain(ContextData& d) : _d(d) {
    glGenVertexArrays(1, &_vao);
}
PostChain::~PostChain() {
    glDeleteVertexArrays(1, &_vao);
}

bool PostChain::ensureSize(const FrameSizes& sizes, int samples) {
    if (sizes.scene == _size && samples == _samples)
        return true;
    // _size/_samples are updated before the targets exist: after a failure a second call at the same
    // size returns true over released targets. That is safe because draw() turns fullPipeline off
    // permanently on the first failure.
    _size = sizes.scene;
    _samples = samples;
    bool ok = _msaa.create({_size.x, _size.y, samples, GL_RGBA16F, true, false}) &&
              _resolved.create({_size.x, _size.y, 0, GL_RGBA16F, false, false});
    for (std::size_t i = 0; ok && i < _bloom.size(); ++i)
        ok = _bloom[i].create({sizes.bloom[i].x, sizes.bloom[i].y, 0, GL_RGBA16F, false, false});
    return ok;
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
    const bool bloom = bloomStrength > 0.f;
    if (bloom) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0);
        GlShader& bright = *_d.bright;
        bright.use();
        bright.setInt("uSrc", 0);
        bright.setFloat("uThreshold", 1.f);
        _bloom[0].bind();
        glBindTexture(GL_TEXTURE_2D, _resolved.colorTexture());
        drawTriangle();
        GlShader& down = *_d.bloomDown;
        down.use();
        down.setInt("uSrc", 0);
        for (std::size_t i = 1; i < _bloom.size(); ++i) {
            const GlFramebuffer& src = _bloom[i - 1];
            down.setVec2("uTexel", {1.f / static_cast<float>(src.width()), 1.f / static_cast<float>(src.height())});
            _bloom[i].bind();
            glBindTexture(GL_TEXTURE_2D, src.colorTexture());
            drawTriangle();
        }
        GlShader& up = *_d.bloomUp;
        up.use();
        up.setInt("uSrc", 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (std::size_t i = _bloom.size() - 1; i > 0; --i) {
            const GlFramebuffer& src = _bloom[i];
            up.setVec2("uTexel", {1.f / static_cast<float>(src.width()), 1.f / static_cast<float>(src.height())});
            _bloom[i - 1].bind();
            glBindTexture(GL_TEXTURE_2D, src.colorTexture());
            drawTriangle();
        }
        glDisable(GL_BLEND);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, viewportY, _size.x, _size.y);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *_d.composite;
    s.use();
    s.setInt("uScene", 0);
    s.setInt("uBloom", 1);
    s.setBool("uHasBloom", bloom);
    s.setFloat("uBloomStrength", bloomStrength);
    s.setFloat("uExposure", exposure);
    bindUnit(0, GL_TEXTURE_2D, _resolved.colorTexture());
    bindUnit(1, GL_TEXTURE_2D, _bloom[0].colorTexture());
    drawTriangle();
    bindUnit(1, GL_TEXTURE_2D, 0);
    bindUnit(0, GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
}

}  // namespace Render
}  // namespace Bowlingbox
