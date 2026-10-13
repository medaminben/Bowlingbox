#ifndef SANDBOX_RENDER_GLSHADER_H
#define SANDBOX_RENDER_GLSHADER_H
#include <Sandbox/Core/Core.h>
#include <glm/glm.hpp>
#include <memory>

namespace Sandbox {
namespace Render {

class GlShader {
public:
    static Result<std::unique_ptr<GlShader>> build(const char* vertexSrc, const char* fragmentSrc);
    ~GlShader();
    GlShader(const GlShader&) = delete;
    GlShader& operator=(const GlShader&) = delete;

    void use() const;
    void setMat4(const char* name, const glm::mat4& m) const;
    void setMat3(const char* name, const glm::mat3& m) const;
    void setVec2(const char* name, const glm::vec2& v) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setVec4(const char* name, const glm::vec4& v) const;

private:
    explicit GlShader(unsigned program) : _program(program) {}
    unsigned _program;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_GLSHADER_H
