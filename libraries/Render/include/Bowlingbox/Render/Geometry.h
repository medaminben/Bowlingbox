#ifndef BOWLINGBOX_RENDER_GEOMETRY_H
#define BOWLINGBOX_RENDER_GEOMETRY_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <cstddef>
#include <utility>
#include <vector>

namespace Bowlingbox {
namespace Render {

/// Interleaved triangle mesh: position xyz + normal xyz per vertex.
struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned> indices;
    std::size_t vertexCount() const { return vertices.size() / 6; }
};

BOWLINGBOX_RENDER_API MeshData boxData(float hx, float hy, float hz);
BOWLINGBOX_RENDER_API MeshData sphereData(float radius, int segments);
/// Cylinder along y, centred at the origin.
BOWLINGBOX_RENDER_API MeshData cylinderData(float radius, float height, int segments);
/// Surface of revolution around y. profile = (radius, y) pairs from bottom to top.
BOWLINGBOX_RENDER_API MeshData latheData(const std::vector<std::pair<float, float>>& profile,
                                      int segments);
/// Bowling pin with its origin comHeight above the base (so it matches the
/// physics body). Spans y in [-comHeight, height - comHeight].
BOWLINGBOX_RENDER_API MeshData pinData(float height, float maxRadius, float comHeight,
                                    int segments = 24);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GEOMETRY_H
