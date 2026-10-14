#ifndef BOWLINGBOX_RENDER_CANVAS2D_H
#define BOWLINGBOX_RENDER_CANVAS2D_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Render/ScoreboardLayout.h>
#include <glm/glm.hpp>
#include <string_view>

namespace Bowlingbox {
namespace Render {

/// 2D drawing primitives in pixels, origin top-left. Widgets such as the
/// scoreboard or the HUD draw through this so they do not depend on OpenGL.
class BOWLINGBOX_RENDER_API ICanvas2D {
public:
    virtual ~ICanvas2D() = default;
    virtual void drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb) = 0;
    virtual glm::vec2 measureText(std::string_view text, float scale) const = 0;
    virtual void drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba) = 0;

    /// Outline drawn with four drawRect calls.
    void strokeRect(const Rect& r, float thickness, glm::vec4 rgba);
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_CANVAS2D_H
