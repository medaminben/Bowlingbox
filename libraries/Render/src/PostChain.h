#ifndef BOWLINGBOX_RENDER_POSTCHAIN_H
#define BOWLINGBOX_RENDER_POSTCHAIN_H
#include "GlFramebuffer.h"
#include <Bowlingbox/Render/RenderOptions.h>

namespace Bowlingbox {
namespace Render {
struct ContextData;

/// The HDR main-pass target (RGBA16F, MSAA), its resolve, and the final composite into the window.
class PostChain {
public:
    explicit PostChain(ContextData& d);
    ~PostChain();
    PostChain(const PostChain&) = delete;
    PostChain& operator=(const PostChain&) = delete;

    /// Reallocates when the sizes changed. False if a framebuffer is incomplete.
    bool ensureSize(const FrameSizes& sizes, int samples);
    /// Binds the MSAA HDR target and clears colour and depth.
    void beginScene();
    /// Resolves, then tone maps into the default framebuffer at (0, viewportY), scene-sized.
    void present(int viewportY, float exposure, float bloomStrength);

private:
    void resolve();             // _msaa -> _resolved
    void drawTriangle() const;  // the full-screen triangle of kPostVs

    ContextData& _d;
    glm::ivec2 _size {0};
    int _samples = 0;
    GlFramebuffer _msaa, _resolved;
    unsigned _vao = 0;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_POSTCHAIN_H
