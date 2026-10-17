#ifndef BOWLINGBOX_RENDER_GLTEXTURE_H
#define BOWLINGBOX_RENDER_GLTEXTURE_H

namespace Bowlingbox {
namespace Render {

/// Owns one GL texture name; deletes it on destruction (needs a current context).
class GlTexture {
public:
    GlTexture() = default;
    ~GlTexture();
    GlTexture(GlTexture&& other) noexcept;
    GlTexture& operator=(GlTexture&& other) noexcept;
    GlTexture(const GlTexture&) = delete;
    GlTexture& operator=(const GlTexture&) = delete;

    void create();  // generates a name if there is none
    void reset();   // deletes the name
    unsigned id() const { return _id; }

private:
    unsigned _id = 0;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLTEXTURE_H
