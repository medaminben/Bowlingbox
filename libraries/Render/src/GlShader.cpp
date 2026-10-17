#include "GlShader.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace Bowlingbox {
namespace Render {
namespace {
Result<unsigned> compile(GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        glDeleteShader(shader);
        return Bowlingbox::Core::Error<>(std::string(type == GL_VERTEX_SHADER ? "vertex: " : "fragment: ") + log);
    }
    return static_cast<unsigned>(shader);
}
}  // namespace

Result<std::unique_ptr<GlShader>> GlShader::build(const char* vertexSrc, const char* fragmentSrc) {
    auto vs = compile(GL_VERTEX_SHADER, vertexSrc);
    if (vs.isError())
        return Bowlingbox::Core::Error<>(vs.error());
    auto fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fs.isError()) {
        glDeleteShader(vs.value());
        return Bowlingbox::Core::Error<>(fs.error());
    }
    const GLuint vsId = vs.value(), fsId = fs.value();
    const GLuint program = glCreateProgram();
    glAttachShader(program, vsId);
    glAttachShader(program, fsId);
    glLinkProgram(program);
    glDeleteShader(vsId);
    glDeleteShader(fsId);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof log, nullptr, log);
        glDeleteProgram(program);
        return Bowlingbox::Core::Error<>(std::string("link: ") + log);
    }
    return std::unique_ptr<GlShader>(new GlShader(program));
}

GlShader::~GlShader() {
    glDeleteProgram(_program);
}

int GlShader::location(const char* name) const {
    const std::string_view key(name);
    if (const auto it = _locations.find(key); it != _locations.end())
        return it->second;
    const int loc = glGetUniformLocation(_program, name);
    _locations.emplace(std::string(key), loc);
    return loc;
}

void GlShader::use() const {
    glUseProgram(_program);
}
void GlShader::setMat4(const char* name, const glm::mat4& m) const {
    glUniformMatrix4fv(location(name), 1, GL_FALSE, glm::value_ptr(m));
}
void GlShader::setMat3(const char* name, const glm::mat3& m) const {
    glUniformMatrix3fv(location(name), 1, GL_FALSE, glm::value_ptr(m));
}
void GlShader::setVec2(const char* name, const glm::vec2& v) const {
    glUniform2fv(location(name), 1, glm::value_ptr(v));
}
void GlShader::setVec3(const char* name, const glm::vec3& v) const {
    glUniform3fv(location(name), 1, glm::value_ptr(v));
}
void GlShader::setVec4(const char* name, const glm::vec4& v) const {
    glUniform4fv(location(name), 1, glm::value_ptr(v));
}
void GlShader::setFloat(const char* name, float v) const {
    glUniform1f(location(name), v);
}
void GlShader::setInt(const char* name, int v) const {
    glUniform1i(location(name), v);
}
void GlShader::setBool(const char* name, bool v) const {
    glUniform1i(location(name), v ? 1 : 0);
}

}  // namespace Render
}  // namespace Bowlingbox
