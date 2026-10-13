#ifndef SANDBOX_RENDER_CONTEXT_DATA_H
#define SANDBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include <memory>

namespace Sandbox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, Lambert + colour band
    std::unique_ptr<GlShader> flat;  // 2D overlay
    int width = 1;
    int height = 1;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_CONTEXT_DATA_H
