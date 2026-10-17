#ifndef BOWLINGBOX_RENDER_GLFRAMEBUFFER_H
#define BOWLINGBOX_RENDER_GLFRAMEBUFFER_H

namespace Bowlingbox {
namespace Render {

/// RAII framebuffer with an optional colour and depth attachment.
class GlFramebuffer {
public:
    struct Spec {
        int width = 1, height = 1;
        int samples = 0;            // > 0: multisample textures (colour and depth), resolved by PostChain's shader
        unsigned colorFormat = 0;   // e.g. GL_RGBA16F; 0 = no colour attachment
        bool depth = false;         // depth renderbuffer
        bool depthTexture = false;  // depth as a comparable texture (shadow maps); replaces `depth`
    };

    GlFramebuffer() = default;
    ~GlFramebuffer();
    GlFramebuffer(const GlFramebuffer&) = delete;
    GlFramebuffer& operator=(const GlFramebuffer&) = delete;

    /// (Re)creates the attachments. False (and nothing allocated) if the framebuffer is incomplete.
    bool create(const Spec& spec);
    void bind() const;  // binds and sets the viewport to the whole target
    void blitColorTo(const GlFramebuffer& dst) const;  // single-sample targets only
    /// GL_TEXTURE_2D, or GL_TEXTURE_2D_MULTISAMPLE when samples > 0.
    unsigned colorTexture() const { return _colorTex; }
    unsigned depthTexture() const { return _depthTex; }
    int width() const { return _spec.width; }
    int height() const { return _spec.height; }

private:
    void release();
    unsigned _fbo = 0, _colorTex = 0, _depthRb = 0, _depthTex = 0;
    Spec _spec;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLFRAMEBUFFER_H
