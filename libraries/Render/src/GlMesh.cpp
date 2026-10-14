#include "GlMesh.h"
#include <glad/gl.h>
#include <cstdint>

namespace Bowlingbox {
namespace Render {

GlMesh::GlMesh(const MeshData& data) : _count(static_cast<int>(data.indices.size())) {
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(float)),
                 data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned)),
                 data.indices.data(), GL_STATIC_DRAW);
    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(3 * sizeof(float))));
    glBindVertexArray(0);
}

GlMesh::~GlMesh() {
    glDeleteBuffers(1, &_ebo);
    glDeleteBuffers(1, &_vbo);
    glDeleteVertexArrays(1, &_vao);
}

void GlMesh::draw() const {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace Render
}  // namespace Bowlingbox
