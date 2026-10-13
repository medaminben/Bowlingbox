#ifndef SANDBOX_RENDER_GEOMETRY_H
#define SANDBOX_RENDER_GEOMETRY_H
#include <Sandbox/Render/Sandbox_Render_export.h>
#include <cstddef>
#include <utility>
#include <vector>

namespace Sandbox {
namespace Render {

/// Interleaved triangle mesh: position xyz + normal xyz per vertex.
struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned> indices;
    std::size_t vertexCount() const { return vertices.size() / 6; }
};

SANDBOX_RENDER_API MeshData boxData(float hx, float hy, float hz);
SANDBOX_RENDER_API MeshData sphereData(float radius, int segments);
/// Cylinder along y, centred at the origin.
SANDBOX_RENDER_API MeshData cylinderData(float radius, float height, int segments);
/// Surface of revolution around y. profile = (radius, y) pairs from bottom to top.
SANDBOX_RENDER_API MeshData latheData(const std::vector<std::pair<float, float>>& profile,
                                      int segments);
/// Bowling pin with its origin comHeight above the base (so it matches the
/// physics body). Spans y in [-comHeight, height - comHeight].
SANDBOX_RENDER_API MeshData pinData(float height, float maxRadius, float comHeight,
                                    int segments = 24);

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_GEOMETRY_H
