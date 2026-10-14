#ifndef BOWLINGBOX_RENDER_GLMESH_H
#define BOWLINGBOX_RENDER_GLMESH_H
#include <Bowlingbox/Render/Geometry.h>

namespace Bowlingbox {
namespace Render {

/// VAO/VBO/EBO for a MeshData: location 0 = position, location 1 = normal.
class GlMesh {
public:
    explicit GlMesh(const MeshData& data);
    ~GlMesh();
    GlMesh(const GlMesh&) = delete;
    GlMesh& operator=(const GlMesh&) = delete;
    void draw() const;

private:
    unsigned _vao = 0, _vbo = 0, _ebo = 0;
    int _count = 0;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLMESH_H
