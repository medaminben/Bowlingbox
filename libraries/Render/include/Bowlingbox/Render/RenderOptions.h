#ifndef BOWLINGBOX_RENDER_RENDER_OPTIONS_H
#define BOWLINGBOX_RENDER_RENDER_OPTIONS_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <glm/glm.hpp>
#include <array>

namespace Bowlingbox {
namespace Render {

/// Offscreen target sizes for a 3D band of width x bandHeight. Every size is at least 1x1.
struct FrameSizes {
    glm::ivec2 scene {1};                // HDR main pass
    glm::ivec2 reflection {1};           // half of scene
    std::array<glm::ivec2, 4> bloom {};  // half, quarter, eighth, sixteenth of scene
};
BOWLINGBOX_RENDER_API FrameSizes frameSizes(int width, int bandHeight);

struct RenderOptions {
    bool forceBasic = false;  // BOWLINGBOX_RENDER=basic (any case)
    bool stats = false;       // BOWLINGBOX_RENDER_STATS=1
};
/// Reads the two environment values (either may be null). Anything else is ignored.
BOWLINGBOX_RENDER_API RenderOptions renderOptions(const char* render, const char* stats);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_RENDER_OPTIONS_H
