#ifndef GAMEBOX_RENDER_CONTEXT_DATA_H
#define GAMEBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include <memory>

namespace Gamebox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, Lambert + colour band
    std::unique_ptr<GlShader> flat;  // 2D overlay
    int width = 1;
    int height = 1;
};

}  // namespace Render
}  // namespace Gamebox
#endif  // GAMEBOX_RENDER_CONTEXT_DATA_H
