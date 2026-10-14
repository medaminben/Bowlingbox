#include <Bowlingbox/Render/Canvas2D.h>

namespace Bowlingbox {
namespace Render {

void ICanvas2D::strokeRect(const Rect& r, float t, glm::vec4 rgba) {
    drawRect({r.x, r.y}, {r.w, t}, rgba);
    drawRect({r.x, r.y + r.h - t}, {r.w, t}, rgba);
    drawRect({r.x, r.y}, {t, r.h}, rgba);
    drawRect({r.x + r.w - t, r.y}, {t, r.h}, rgba);
}

}  // namespace Render
}  // namespace Bowlingbox
