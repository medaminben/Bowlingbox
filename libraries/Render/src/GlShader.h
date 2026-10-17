#ifndef BOWLINGBOX_RENDER_GLSHADER_H
#define BOWLINGBOX_RENDER_GLSHADER_H
#include <Bowlingbox/Core/Core.h>
#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

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
    void setInt(const char* name, int v) const;

private:
    explicit GlShader(unsigned program) : _program(program) {}
    int location(const char* name) const;  // cached glGetUniformLocation; -1 if unused

    struct NameHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
    };
    unsigned _program;
    mutable std::unordered_map<std::string, int, NameHash, std::equal_to<>> _locations;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GLSHADER_H
