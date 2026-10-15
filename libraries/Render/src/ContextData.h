#ifndef BOWLINGBOX_RENDER_CONTEXT_DATA_H
#define BOWLINGBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include <memory>

namespace Bowlingbox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, lit or emissive, colour band, fog, lane stripes
    std::unique_ptr<GlShader> flat;  // 2D overlay
    std::unique_ptr<GlShader> sky;   // background gradient behind the 3D scene
    int width = 1;
    int height = 1;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_CONTEXT_DATA_H
