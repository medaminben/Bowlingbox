#include "GlFramebuffer.h"
#include <glad/gl.h>

namespace Bowlingbox {
namespace Render {
namespace {
void clampedLinear() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
}  // namespace

GlFramebuffer::~GlFramebuffer() {
    release();
}

void GlFramebuffer::release() {
    if (_depthTex) glDeleteTextures(1, &_depthTex);
    if (_colorTex) glDeleteTextures(1, &_colorTex);
    if (_depthRb) glDeleteRenderbuffers(1, &_depthRb);
    if (_fbo) glDeleteFramebuffers(1, &_fbo);
    _fbo = _colorTex = _depthRb = _depthTex = 0;
}

bool GlFramebuffer::create(const Spec& spec) {
    release();
    _spec = spec;
    const GLsizei w = spec.width, h = spec.height;
    const bool ms = spec.samples > 0;
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    if (spec.colorFormat != 0) {
        if (ms) {
            // A texture, not a renderbuffer, so PostChain's resolve shader can read every sample.
            glGenTextures(1, &_colorTex);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _colorTex);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, spec.samples, spec.colorFormat, w, h, GL_TRUE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, _colorTex, 0);
        } else {
            glGenTextures(1, &_colorTex);
            glBindTexture(GL_TEXTURE_2D, _colorTex);
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(spec.colorFormat), w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
            clampedLinear();
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);
        }
    } else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    if (spec.depthTexture) {
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D, _depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        clampedLinear();  // linear + compare = 2x2 hardware PCF
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
    } else if (spec.depth && ms) {
        // Also a texture: a renderbuffer may round the sample count differently, which leaves
        // the framebuffer incomplete.
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _depthTex);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, spec.samples, GL_DEPTH_COMPONENT24, w, h, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, _depthTex, 0);
    } else if (spec.depth) {
        glGenRenderbuffers(1, &_depthRb);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRb);
    }
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    if (!complete)
        release();
    return complete;
}

void GlFramebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, _spec.width, _spec.height);
}

void GlFramebuffer::blitColorTo(const GlFramebuffer& dst) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst._fbo);
    glBlitFramebuffer(0, 0, _spec.width, _spec.height, 0, 0, dst._spec.width, dst._spec.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace Render
}  // namespace Bowlingbox
