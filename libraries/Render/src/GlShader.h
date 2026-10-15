#ifndef BOWLINGBOX_RENDER_GLSHADER_H
#define BOWLINGBOX_RENDER_GLSHADER_H
#include <Bowlingbox/Core/Core.h>
#include <glm/glm.hpp>
#include <memory>

namespace Bowlingbox {
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
    void setFloat(const char* name, float v) const;
    void setBool(const char* name, bool v) const;

private:
    explicit GlShader(unsigned program) : _program(program) {}
    unsigned _program;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLSHADER_H
