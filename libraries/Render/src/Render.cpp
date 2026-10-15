#include <Bowlingbox/Render/Render.h>
#include "ContextData.h"
#include "GlMesh.h"
#include <glad/gl.h>
#include <stb_easy_font.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace Bowlingbox {
namespace Render {
namespace {

const char* kLitVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj; uniform mat3 uNormalMat;
out vec3 vNormal; out float vLocalY;
void main() {
    vNormal = normalize(uNormalMat * aNormal);
    vLocalY = aPos.y;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
})";

const char* kLitFs = R"(#version 330 core
in vec3 vNormal; in float vLocalY;
uniform vec4 uColor; uniform vec4 uBandColor; uniform vec2 uBandRange; uniform vec3 uLightDir;
out vec4 FragColor;
void main() {
    vec4 base = (vLocalY >= uBandRange.x && vLocalY <= uBandRange.y) ? uBandColor : uColor;
    float diff = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
    FragColor = vec4(base.rgb * (0.35 + 0.65 * diff), base.a);
})";

const char* kFlatVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos.xy, 0.0, 1.0); })";

const char* kFlatFs = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; })";

const glm::vec2 kNoBand {1.f, -1.f};  // min > max: matches no y
const glm::vec3 kLightDir {-0.3f, -1.f, 0.4f};

struct Vertex2D {
    float x, y, z;
    unsigned char rgba[4];
};
}  // namespace

// ---------------------------------------------------------------- Context
Context::Context() : _d(std::make_unique<ContextData>()) {}
Context::~Context() = default;

Result<std::unique_ptr<Context>> Context::create() {
    const int version = gladLoaderLoadGL();
    if (version == 0)
        return Bowlingbox::Core::Error<>("could not load OpenGL function pointers (is a GL context current?)");
    const int major = GLAD_VERSION_MAJOR(version), minor = GLAD_VERSION_MINOR(version);
    if (major * 10 + minor < 33)
        return Bowlingbox::Core::Error<>("OpenGL 3.3 core required, got " + std::to_string(major) + "." +
                           std::to_string(minor));
    auto lit = GlShader::build(kLitVs, kLitFs);
    if (lit.isError())
        return Bowlingbox::Core::Error<>("lit shader: " + lit.error());
    auto flat = GlShader::build(kFlatVs, kFlatFs);
    if (flat.isError())
        return Bowlingbox::Core::Error<>("flat shader: " + flat.error());
    std::unique_ptr<Context> ctx(new Context());
    ctx->_d->lit = lit();
    ctx->_d->flat = flat();
    return Result<std::unique_ptr<Context>>(std::move(ctx));
}

void Context::resize(int width, int height) {
    _d->width = width > 0 ? width : 1;
    _d->height = height > 0 ? height : 1;
}
int Context::width() const { return _d->width; }
int Context::height() const { return _d->height; }
ContextData& Context::data() { return *_d; }

void Context::beginFrame(const glm::vec3& c) {
    glViewport(0, 0, _d->width, _d->height);
    glClearColor(c.r, c.g, c.b, 1.f);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void Context::endFrame() {
    glBindVertexArray(0);
    glUseProgram(0);
}

// ---------------------------------------------------------------- Camera
void Camera::setViewport(int width, int height) {
    _aspect = static_cast<float>(width > 0 ? width : 1) / static_cast<float>(height > 0 ? height : 1);
}
void Camera::lookAt(glm::vec3 eye, glm::vec3 target) {
    _eye = eye;
    _target = target;
}
glm::mat4 Camera::view() const {
    return glm::lookAt(_eye, _target, glm::vec3(0.f, 1.f, 0.f));
}
glm::mat4 Camera::projection() const {
    return glm::perspective(glm::radians(45.f), _aspect, 0.1f, 100.f);
}

// ---------------------------------------------------------------- SceneRenderer
struct SceneRenderer::Impl {
    Context& ctx;
    SceneGeometry g;
    GlMesh lane, gutter, wall, ball, pin, guide;

    explicit Impl(Context& c, const SceneGeometry& geo)
        : ctx(c), g(geo),
          lane(boxData(geo.laneHalfWidth, 0.05f, (geo.laneLength + geo.approach) / 2)),
          gutter(boxData(geo.gutterWidth / 2, 0.05f, geo.laneLength / 2)),
          wall(boxData(0.02f, 0.15f, geo.laneLength / 2)),
          ball(sphereData(geo.ballRadius, 32)),
          pin(pinData(geo.pinHeight, geo.pinMaxRadius, geo.pinComHeight)),
          guide(boxData(0.01f, 0.003f, 0.5f)) {}

    void drawMesh(const GlMesh& mesh, const glm::mat4& model, glm::vec4 color,
                  glm::vec2 band = kNoBand, glm::vec4 bandColor = glm::vec4(1.f)) {
        GlShader& s = *ctx.data().lit;
        s.setMat4("uModel", model);
        s.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        s.setVec4("uColor", color);
        s.setVec2("uBandRange", band);
        s.setVec4("uBandColor", bandColor);
        mesh.draw();
    }
    static glm::mat4 at(float x, float y, float z) {
        return glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
    }
};

SceneRenderer::SceneRenderer(Context& ctx, const SceneGeometry& geometry)
    : _p(std::make_unique<Impl>(ctx, geometry)) {}
SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::draw(const Camera& camera, const SceneDescription& scene, int viewportY,
                         int viewportH) {
    Impl& p = *_p;
    const SceneGeometry& g = p.g;
    glViewport(0, viewportY, p.ctx.width(), viewportH);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *p.ctx.data().lit;
    s.use();
    s.setMat4("uView", camera.view());
    s.setMat4("uProj", camera.projection());
    s.setVec3("uLightDir", kLightDir);

    const float outerX = g.laneHalfWidth + g.gutterWidth;
    // Theme tokens: spec §4.4 / plan “Visual theme constants”.
    p.drawMesh(p.lane, Impl::at(0.f, -0.05f, (g.laneLength - g.approach) / 2), {0.831f, 0.639f, 0.451f, 1.f});
    for (float sx : {-1.f, 1.f}) {
        p.drawMesh(p.gutter, Impl::at(sx * (g.laneHalfWidth + g.gutterWidth / 2), -g.gutterDepth - 0.05f, g.laneLength / 2),
                   {0.008f, 0.055f, 0.22f, 1.f});
        p.drawMesh(p.wall, Impl::at(sx * (outerX + 0.02f), 0.05f, g.laneLength / 2), {0.008f, 0.055f, 0.22f, 1.f});
    }
    const glm::vec2 neck {0.55f * g.pinHeight - g.pinComHeight, 0.65f * g.pinHeight - g.pinComHeight};
    for (int i = 0; i < 10; ++i)
        if (scene.pinVisible[i])
            p.drawMesh(p.pin, scene.pins[i].matrix(), {0.941f, 0.941f, 0.941f, 1.f}, neck, {0.902f, 0.f, 0.f, 1.f});
    p.drawMesh(p.ball, scene.ball.matrix(), {0.098f, 0.365f, 0.925f, 1.f});

    if (scene.showAimGuide) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        const float length = g.laneLength + 1.f;  // from the ball start (z = -1) to the deck end
        const float a = glm::radians(scene.aimAngleDeg);
        const glm::vec3 dir(std::sin(a), 0.f, std::cos(a));
        const glm::vec3 start(scene.aimOffset, 0.004f, -1.f);
        glm::mat4 model = glm::translate(glm::mat4(1.f), start + dir * (length / 2)) *
                          glm::rotate(glm::mat4(1.f), a, glm::vec3(0.f, 1.f, 0.f)) *
                          glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, length));
        p.drawMesh(p.guide, model, {0.f, 0.898f, 1.f, 0.40f});
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

// ---------------------------------------------------------------- Overlay
struct Overlay::Impl {
    Context& ctx;
    unsigned vao = 0, vbo = 0, ebo = 0;
    glm::mat4 proj {1.f};
    std::vector<char> textBuffer = std::vector<char>(200000);
    std::vector<unsigned> indices;

    explicit Impl(Context& c) : ctx(c) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), nullptr);
        glBindVertexArray(0);
    }
    ~Impl() {
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    /// Upload `quads` quads of Vertex2D and draw them as triangles.
    void drawQuads(const void* vertices, int quads, const glm::mat4& model, glm::vec4 color) {
        indices.clear();
        for (int q = 0; q < quads; ++q) {
            const unsigned b = static_cast<unsigned>(q) * 4;
            indices.insert(indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
        }
        GlShader& s = *ctx.data().flat;
        s.use();
        s.setMat4("uMVP", proj * model);
        s.setVec4("uColor", color);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(quads * 4 * sizeof(Vertex2D)), vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)), indices.data(), GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};

Overlay::Overlay(Context& ctx) : _p(std::make_unique<Impl>(ctx)) {}
Overlay::~Overlay() = default;

void Overlay::begin(int width, int height) {
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    _p->proj = glm::ortho(0.f, static_cast<float>(width), static_cast<float>(height), 0.f, -1.f, 1.f);
}

void Overlay::drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb) {
    if (text.empty())
        return;
    std::string s(text);
    const int quads = stb_easy_font_print(0.f, 0.f, s.data(), nullptr, _p->textBuffer.data(),
                                          static_cast<int>(_p->textBuffer.size()));
    const glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(px, 0.f)) *
                            glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, 1.f));
    _p->drawQuads(_p->textBuffer.data(), quads, model, glm::vec4(rgb, 1.f));
}

glm::vec2 Overlay::measureText(std::string_view text, float scale) const {
    std::string s(text);
    return {static_cast<float>(stb_easy_font_width(s.data())) * scale,
            static_cast<float>(stb_easy_font_height(s.data())) * scale};
}

void Overlay::drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba) {
    const Vertex2D v[4] = {{px.x, px.y, 0.f, {}}, {px.x + size.x, px.y, 0.f, {}},
                           {px.x + size.x, px.y + size.y, 0.f, {}}, {px.x, px.y + size.y, 0.f, {}}};
    _p->drawQuads(v, 1, glm::mat4(1.f), rgba);
}

}  // namespace Render
}  // namespace Bowlingbox
