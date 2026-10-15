#ifndef BOWLINGBOX_RENDER_PROP_BUILDERS_H
#define BOWLINGBOX_RENDER_PROP_BUILDERS_H
#include <Bowlingbox/Render/Environment.h>

namespace Bowlingbox {
namespace Render {
namespace detail {

/// A shape centred at `centre`, `size` metres across (unit shapes are 1 m).
inline Prop shaped(Shape shape, glm::vec3 centre, glm::vec3 size, const Material& m) {
    Prop p;
    p.shape = shape;
    p.transform.position = centre;
    p.transform.scale = size;
    p.material = m;
    return p;
}
inline Prop box(glm::vec3 centre, glm::vec3 size, const Material& m) {
    return shaped(Shape::Box, centre, size, m);
}
/// An axis-aligned box from its low corner to its high corner.
inline Prop slab(glm::vec3 lo, glm::vec3 hi, const Material& m) {
    return box((lo + hi) * 0.5f, hi - lo, m);
}
/// A pin whose origin (pinComHeight above its base) is at `origin`.
inline Prop pin(glm::vec3 origin, glm::vec3 scale, const Material& m) {
    return shaped(Shape::Pin, origin, scale, m);
}

}  // namespace detail
}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_PROP_BUILDERS_H
