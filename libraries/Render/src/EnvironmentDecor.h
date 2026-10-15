#ifndef BOWLINGBOX_RENDER_ENVIRONMENT_DECOR_H
#define BOWLINGBOX_RENDER_ENVIRONMENT_DECOR_H
#include <Bowlingbox/Render/Environment.h>
#include <vector>

namespace Bowlingbox {
namespace Render {
/// Appends the theme's optional props (pillars, neon strips, ceiling lights, wall screens,
/// score monitor, fences, trees) as selected by theme.props.
void addDecor(std::vector<Prop>& out, const Theme& theme, const SceneGeometry& g);
}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_ENVIRONMENT_DECOR_H
