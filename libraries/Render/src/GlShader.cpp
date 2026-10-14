#include "GlShader.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace Gamebox {
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
        return Gamebox::Core::Error<>(std::string(type == GL_VERTEX_SHADER ? "vertex: " : "fragment: ") + log);
    }
    return static_cast<unsigned>(shader);
}
}  // namespace

Result<std::unique_ptr<GlShader>> GlShader::build(const char* vertexSrc, const char* fragmentSrc) {
    auto vs = compile(GL_VERTEX_SHADER, vertexSrc);
    if (vs.isError())
        return Gamebox::Core::Error<>(vs.error());
    auto fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fs.isError()) {
        glDeleteShader(vs());
        return Gamebox::Core::Error<>(fs.error());
    }
    const GLuint vsId = vs(), fsId = fs();
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
        return Gamebox::Core::Error<>(std::string("link: ") + log);
    }
    return std::unique_ptr<GlShader>(new GlShader(program));
}

GlShader::~GlShader() {
    glDeleteProgram(_program);
}

void GlShader::use() const {
    glUseProgram(_program);
}
void GlShader::setMat4(const char* name, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(_program, name), 1, GL_FALSE, glm::value_ptr(m));
}
void GlShader::setMat3(const char* name, const glm::mat3& m) const {
    glUniformMatrix3fv(glGetUniformLocation(_program, name), 1, GL_FALSE, glm::value_ptr(m));
}
void GlShader::setVec2(const char* name, const glm::vec2& v) const {
    glUniform2fv(glGetUniformLocation(_program, name), 1, glm::value_ptr(v));
}
void GlShader::setVec3(const char* name, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(_program, name), 1, glm::value_ptr(v));
}
void GlShader::setVec4(const char* name, const glm::vec4& v) const {
    glUniform4fv(glGetUniformLocation(_program, name), 1, glm::value_ptr(v));
}

}  // namespace Render
}  // namespace Gamebox
