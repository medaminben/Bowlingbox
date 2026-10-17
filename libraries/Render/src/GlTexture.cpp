#include "GlTexture.h"
#include <glad/gl.h>
#include <utility>

namespace Bowlingbox {
namespace Render {

GlTexture::~GlTexture() {
    reset();
}
GlTexture::GlTexture(GlTexture&& other) noexcept : _id(std::exchange(other._id, 0u)) {}
GlTexture& GlTexture::operator=(GlTexture&& other) noexcept {
    if (this != &other) {
        reset();
        _id = std::exchange(other._id, 0u);
    }
    return *this;
}
void GlTexture::create() {
    if (_id == 0)
        glGenTextures(1, &_id);
}
void GlTexture::reset() {
    if (_id != 0)
        glDeleteTextures(1, &_id);
    _id = 0;
}

}  // namespace Render
}  // namespace Bowlingbox
