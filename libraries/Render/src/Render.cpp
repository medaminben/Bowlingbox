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
out vec3 vNormal; out float vLocalY; out float vLocalX; out vec3 vViewPos;
void main() {
    vNormal = normalize(uNormalMat * aNormal);
    vLocalY = aPos.y;
    vLocalX = aPos.x;
    vec4 viewPos = uView * uModel * vec4(aPos, 1.0);
    vViewPos = viewPos.xyz;
    gl_Position = uProj * viewPos;
})";

const char* kLitFs = R"(#version 330 core
in vec3 vNormal; in float vLocalY; in float vLocalX; in vec3 vViewPos;
uniform vec4 uColor; uniform vec4 uBandColor; uniform vec2 uBandRange; uniform vec3 uLightDir;
uniform float uAmbient; uniform bool uEmissive; uniform bool uFog; uniform bool uStripes;
uniform vec3 uFogColor; uniform float uFogStart; uniform float uFogEnd;
out vec4 FragColor;
void main() {
    vec4 base = (vLocalY >= uBandRange.x && vLocalY <= uBandRange.y) ? uBandColor : uColor;
    if (uStripes)  // boards 0.025 m wide, alternating +-6 %
        base.rgb *= mod(floor(vLocalX / 0.025), 2.0) < 1.0 ? 1.06 : 0.94;
    vec3 rgb;
    if (uEmissive) {
        rgb = base.rgb;
    } else {
        float diffuse = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
        rgb = base.rgb * (uAmbient + (1.0 - uAmbient) * diffuse);
    }
    if (uFog) {
        float dist = length(vViewPos);
        float f = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        rgb = mix(rgb, uFogColor, uEmissive ? 0.5 * f : f);
    }
    FragColor = vec4(rgb, base.a);
})";

const char* kFlatVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos.xy, 0.0, 1.0); })";

const char* kFlatFs = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; })";

// One triangle that covers the viewport; vT runs 0 (bottom) to 1 (top).
const char* kSkyVs = R"(#version 330 core
out float vT;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    vT = (p.y + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";

const char* kSkyFs = R"(#version 330 core
in float vT;
uniform vec3 uTop; uniform vec3 uBottom;
out vec4 FragColor;
void main() { FragColor = vec4(mix(uBottom, uTop, clamp(vT, 0.0, 1.0)), 1.0); })";

const glm::vec2 kNoBand {1.f, -1.f};  // min > max: matches no y

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
    auto sky = GlShader::build(kSkyVs, kSkyFs);
    if (sky.isError())
        return Bowlingbox::Core::Error<>("sky shader: " + sky.error());
    std::unique_ptr<Context> ctx(new Context());
    ctx->_d->lit = lit();
    ctx->_d->flat = flat();
    ctx->_d->sky = sky();
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
    return glm::perspective(glm::radians(45.f), _aspect, 0.1f, 150.f);
}

// ---------------------------------------------------------------- SceneRenderer
struct SceneRenderer::Impl {
    Context& ctx;
    SceneGeometry g;
    const Theme* current = nullptr;
    std::vector<Prop> props;
    GlMesh lane, gutter, wall, ball, pin, guide, unitBox, unitCylinder, unitCone;
    unsigned skyVao = 0;  // core profile needs a bound VAO even for attribute-less draws

    Impl(Context& c, const SceneGeometry& geo, ThemeId id)
        : ctx(c), g(geo),
          lane(boxData(geo.laneHalfWidth, 0.05f, (geo.laneLength + geo.approach) / 2)),
          gutter(boxData(geo.gutterWidth / 2, 0.05f, geo.laneLength / 2)),
          wall(boxData(0.02f, 0.15f, geo.laneLength / 2)),
          ball(sphereData(geo.ballRadius, 32)),
          pin(shapeMesh(Shape::Pin, geo)),
          guide(boxData(0.01f, 0.003f, 0.5f)),
          unitBox(shapeMesh(Shape::Box, geo)),
          unitCylinder(shapeMesh(Shape::Cylinder, geo)),
          unitCone(shapeMesh(Shape::Cone, geo)) {
        glGenVertexArrays(1, &skyVao);
        setTheme(id);
    }
    ~Impl() { glDeleteVertexArrays(1, &skyVao); }

    void setTheme(ThemeId id) {
        current = &theme(id);
        props = buildProps(*current, g);
    }

    const GlMesh& meshFor(Shape shape) const {
        switch (shape) {
            case Shape::Box:      return unitBox;
            case Shape::Cylinder: return unitCylinder;
            case Shape::Cone:     return unitCone;
            case Shape::Pin:      break;
        }
        return pin;
    }

    void drawMesh(const GlMesh& mesh, const glm::mat4& model, glm::vec4 color, bool emissive = false,
                  bool stripes = false, glm::vec2 band = kNoBand) {
        GlShader& s = *ctx.data().lit;
        s.setMat4("uModel", model);
        s.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        s.setVec4("uColor", color);
        s.setVec2("uBandRange", band);
        s.setBool("uEmissive", emissive);
        s.setBool("uStripes", stripes);
        mesh.draw();
    }
    void drawMaterial(const GlMesh& mesh, const glm::mat4& model, const Material& m, bool stripes = false,
                      glm::vec2 band = kNoBand) {
        drawMesh(mesh, model, glm::vec4(m.color, 1.f), m.emissive, stripes, band);
    }
    static glm::mat4 at(float x, float y, float z) {
        return glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
    }
};

SceneRenderer::SceneRenderer(Context& ctx, const SceneGeometry& geometry, ThemeId theme)
    : _p(std::make_unique<Impl>(ctx, geometry, theme)) {}
SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::setTheme(ThemeId theme) {
    _p->setTheme(theme);
}

void SceneRenderer::draw(const Camera& camera, const SceneDescription& scene, int viewportY,
                         int viewportH) {
    Impl& p = *_p;
    const SceneGeometry& g = p.g;
    const Theme& t = *p.current;
    glViewport(0, viewportY, p.ctx.width(), viewportH);
    glDisable(GL_BLEND);

    // Background gradient behind everything, in the 3D band only.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    GlShader& sky = *p.ctx.data().sky;
    sky.use();
    sky.setVec3("uTop", t.backgroundTop);
    sky.setVec3("uBottom", t.backgroundBottom);
    glBindVertexArray(p.skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    GlShader& s = *p.ctx.data().lit;
    s.use();
    s.setMat4("uView", camera.view());
    s.setMat4("uProj", camera.projection());
    s.setVec3("uLightDir", t.lightDir);
    s.setFloat("uAmbient", t.ambient);
    s.setBool("uFog", true);
    s.setVec3("uFogColor", t.fogColor);
    s.setFloat("uFogStart", t.fogStart);
    s.setFloat("uFogEnd", t.fogEnd);
    s.setVec4("uBandColor", glm::vec4(t.pinBand.color, 1.f));

    const glm::vec2 neck {0.55f * g.pinHeight - g.pinComHeight, 0.65f * g.pinHeight - g.pinComHeight};
    for (const Prop& prop : p.props)
        p.drawMaterial(p.meshFor(prop.shape), prop.transform.matrix(), prop.material, prop.stripes,
                       prop.pinBand ? neck : kNoBand);

    const float outerX = g.laneHalfWidth + g.gutterWidth;
    p.drawMaterial(p.lane, Impl::at(0.f, -0.05f, (g.laneLength - g.approach) / 2), t.lane, true);
    for (float sx : {-1.f, 1.f}) {
        p.drawMaterial(p.gutter, Impl::at(sx * (g.laneHalfWidth + g.gutterWidth / 2), -g.gutterDepth - 0.05f, g.laneLength / 2),
                       t.gutter);
        p.drawMaterial(p.wall, Impl::at(sx * (outerX + 0.02f), 0.05f, g.laneLength / 2), t.gutter);
    }
    for (int i = 0; i < 10; ++i)
        if (scene.pinVisible[i])
            p.drawMaterial(p.pin, scene.pins[i].matrix(), t.pinBody, false, neck);
    p.drawMaterial(p.ball, scene.ball.matrix(), t.ball);

    if (scene.showAimGuide) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        s.setBool("uFog", false);  // the guide keeps its colour and 0.40 alpha
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
