#ifndef BOWLINGBOX_RENDER_CONTEXT_DATA_H
#define BOWLINGBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include <memory>

namespace Bowlingbox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, Lambert + colour band
    std::unique_ptr<GlShader> flat;  // 2D overlay
    int width = 1;
    int height = 1;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_CONTEXT_DATA_H
