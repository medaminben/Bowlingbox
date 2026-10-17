#include <Bowlingbox/Render/Render.h>
#include "ContextData.h"
#include "GlMesh.h"
#include "Shaders.h"
#include <Bowlingbox/Render/Lighting.h>
#include <glad/gl.h>
#include <stb_easy_font.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace Bowlingbox {
namespace Render {
namespace {

const glm::vec2 kNoBand {1.f, -1.f};  // min > max: matches no y
const Material kGuide {{0.f, 0.898f, 1.f}, true, 1.f, 0.f, 1.f, Pattern::None};

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
    auto lit = GlShader::build(shaders::kLitVs, shaders::litFs().c_str());
    if (lit.isError())
        return Bowlingbox::Core::Error<>("lit shader: " + lit.error());
    auto flat = GlShader::build(shaders::kFlatVs, shaders::kFlatFs);
    if (flat.isError())
        return Bowlingbox::Core::Error<>("flat shader: " + flat.error());
    auto sky = GlShader::build(shaders::kSkyVs, shaders::skyFs().c_str());
    if (sky.isError())
        return Bowlingbox::Core::Error<>("sky shader: " + sky.error());
    std::unique_ptr<Context> ctx(new Context());
    ctx->_d->lit = lit();
    ctx->_d->flat = flat();
    ctx->_d->sky = sky();
    // Samplers of different types must not share a texture unit.
    ctx->_d->lit->use();
    ctx->_d->lit->setInt("uShadowMap", 1);
    ctx->_d->lit->setInt("uReflection", 2);
    auto text = GlShader::build(shaders::kTextVs, shaders::kTextFs);
    auto font = FontAtlas::bakeEmbedded();
    if (text.isError()) {
        std::fprintf(stderr, "render: text shader failed (%s); using the built-in font\n", text.error().c_str());
    } else if (font.isError()) {
        std::fprintf(stderr, "render: HUD font unavailable (%s); using the built-in font\n", font.error().c_str());
    } else {
        ContextData& d = *ctx->_d;
        d.text = text();
        d.font = font();
        d.fontTexture.create();
        glBindTexture(GL_TEXTURE_2D, d.fontTexture.id());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FontAtlas::kSize, FontAtlas::kSize, 0, GL_RED, GL_UNSIGNED_BYTE,
                     d.font->pixels().data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, FontAtlas::kMaxMipLevel);  // see kPadding
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glUseProgram(0);
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
#ifndef NDEBUG
    if (const GLenum err = glGetError(); err != GL_NO_ERROR && !_d->reportedGlError) {
        std::fprintf(stderr, "render: GL error 0x%04x\n", err);
        _d->reportedGlError = true;
    }
#endif
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
namespace {
/// Per-pass settings for the lit shader.
struct PassParams {
    glm::mat4 view {1.f}, proj {1.f};
    glm::vec3 eye {0.f};
    bool toneMap = false;  // basic path: tone map + sRGB in the lit shader
    bool shadows = false;
    glm::mat4 lightViewProj {1.f};
    glm::vec4 clipPlane {0.f, 0.f, 0.f, 1.f};  // keeps everything
    glm::vec2 viewportSize {1.f};
    bool reflection = false;  // the main lane samples the reflection texture
};
}  // namespace

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
          ball(sphereData(geo.ballRadius, 48)),
          pin(shapeMesh(Shape::Pin, geo)),
          guide(boxData(0.01f, 0.003f, 0.5f)),
          unitBox(shapeMesh(Shape::Box, geo)),
          unitCylinder(shapeMesh(Shape::Cylinder, geo)),
          unitCone(shapeMesh(Shape::Cone, geo)) {
        glGenVertexArrays(1, &skyVao);
        setTheme(id);
    }
    ~Impl() { glDeleteVertexArrays(1, &skyVao); }

    void setTheme(ThemeId id) {  // CPU only
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
    static glm::mat4 at(float x, float y, float z) {
        return glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
    }
    glm::vec2 neck() const {
        return {0.55f * g.pinHeight - g.pinComHeight, 0.65f * g.pinHeight - g.pinComHeight};
    }

    PassParams cameraPass(const Camera& camera, glm::ivec2 size) const {
        PassParams pp;
        pp.view = camera.view();
        pp.proj = camera.projection();
        pp.eye = camera.eye();
        pp.viewportSize = glm::vec2(size);
        return pp;
    }

    void setFrameUniforms(GlShader& s, const PassParams& pp) const {
        const Theme& t = *current;
        s.use();
        s.setMat4("uView", pp.view);
        s.setMat4("uProj", pp.proj);
        s.setVec3("uCameraPos", pp.eye);
        s.setVec3("uLightDir", glm::normalize(t.lightDir));
        s.setVec3("uKeyColor", t.keyColor * t.keyIntensity);
        s.setVec3("uSkyAmbient", t.skyAmbient);
        s.setVec3("uGroundAmbient", t.groundAmbient);
        static constexpr std::array<const char*, kMaxPointLights> kPos {"uPointPos[0]", "uPointPos[1]",
                                                                         "uPointPos[2]", "uPointPos[3]"};
        static constexpr std::array<const char*, kMaxPointLights> kColor {"uPointColor[0]", "uPointColor[1]",
                                                                           "uPointColor[2]", "uPointColor[3]"};
        static constexpr std::array<const char*, kMaxPointLights> kRange {"uPointRange[0]", "uPointRange[1]",
                                                                           "uPointRange[2]", "uPointRange[3]"};
        s.setInt("uPointCount", t.pointLightCount);
        for (std::size_t i = 0; i < static_cast<std::size_t>(t.pointLightCount); ++i) {
            const PointLight& l = t.pointLights[i];
            s.setVec3(kPos[i], l.position);
            s.setVec3(kColor[i], l.color * l.intensity);
            s.setFloat(kRange[i], l.range);
        }
        s.setBool("uFog", true);
        s.setVec3("uFogColor", srgbToLinear(t.fogColor));
        s.setFloat("uFogStart", t.fogStart);
        s.setFloat("uFogEnd", t.fogEnd);
        s.setVec3("uBandColor", srgbToLinear(t.pinBand.color));
        s.setBool("uToneMap", pp.toneMap);
        s.setFloat("uExposure", t.exposure);
        s.setBool("uShadows", pp.shadows);
        s.setMat4("uLightVP", pp.lightViewProj);
        s.setVec4("uClipPlane", pp.clipPlane);
        s.setVec2("uViewportSize", pp.viewportSize);
        s.setBool("uReflect", false);
        s.setFloat("uLaneHalfWidth", g.laneHalfWidth);
        s.setFloat("uBoard", 2.f * g.laneHalfWidth / 39.f);  // a real lane has 39 boards
        s.setFloat("uDeckZ", g.headPinZ - 0.3f);
        s.setFloat("uAlpha", 1.f);
    }

    void drawMesh(GlShader& s, const GlMesh& mesh, const glm::mat4& model, const Material& m,
                  glm::vec2 band = kNoBand) const {
        s.setMat4("uModel", model);
        s.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        s.setVec3("uColor", srgbToLinear(m.color));
        s.setVec2("uBandRange", band);
        s.setBool("uEmissive", m.emissive);
        s.setFloat("uEmissiveStrength", m.emissiveStrength);
        s.setFloat("uRoughness", m.roughness);
        s.setFloat("uSpecular", m.specular);
        s.setInt("uPattern", static_cast<int>(m.pattern));
        mesh.draw();
    }

    /// Props (all, or only the listed indices), then lane + gutters (if withLane), gutter walls,
    /// standing pins and the ball. Works with any program built on kLitVs.
    void drawOpaque(GlShader& s, const PassParams& pp, const SceneDescription& scene,
                    const std::vector<std::size_t>* only, bool withLane) const {
        const Theme& t = *current;
        const auto drawProp = [&](const Prop& prop) {
            drawMesh(s, meshFor(prop.shape), prop.transform.matrix(), prop.material, prop.pinBand ? neck() : kNoBand);
        };
        if (only)
            for (std::size_t i : *only)
                drawProp(props[i]);
        else
            for (const Prop& prop : props)
                drawProp(prop);

        const float outerX = g.laneHalfWidth + g.gutterWidth;
        if (withLane) {
            s.setBool("uReflect", pp.reflection);
            drawMesh(s, lane, at(0.f, -0.05f, (g.laneLength - g.approach) / 2), t.lane);
            s.setBool("uReflect", false);
            for (float sx : {-1.f, 1.f})
                drawMesh(s, gutter, at(sx * (g.laneHalfWidth + g.gutterWidth / 2), -g.gutterDepth - 0.05f, g.laneLength / 2),
                         t.gutter);
        }
        for (float sx : {-1.f, 1.f})
            drawMesh(s, wall, at(sx * (outerX + 0.02f), 0.05f, g.laneLength / 2), t.gutter);
        for (std::size_t i = 0; i < scene.pins.size(); ++i)
            if (scene.pinVisible[i])
                drawMesh(s, pin, scene.pins[i].matrix(), t.pinBody, neck());
        drawMesh(s, ball, scene.ball.matrix(), t.ball);
    }

    void drawSky(bool toneMap) {
        const Theme& t = *current;
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        GlShader& sky = *ctx.data().sky;
        sky.use();
        sky.setVec3("uTop", srgbToLinear(t.backgroundTop));
        sky.setVec3("uBottom", srgbToLinear(t.backgroundBottom));
        sky.setBool("uToneMap", toneMap);
        sky.setFloat("uExposure", t.exposure);
        glBindVertexArray(skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void drawGuide(GlShader& s, const SceneDescription& scene) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        s.setBool("uFog", false);  // the guide keeps its colour
        s.setFloat("uAlpha", 0.40f);
        const float length = g.laneLength + 1.f;  // from the ball start (z = -1) to the deck end
        const float a = glm::radians(scene.aimAngleDeg);
        const glm::vec3 dir(std::sin(a), 0.f, std::cos(a));
        const glm::vec3 start(scene.aimOffset, 0.004f, -1.f);
        const glm::mat4 model = glm::translate(glm::mat4(1.f), start + dir * (length / 2)) *
                                glm::rotate(glm::mat4(1.f), a, glm::vec3(0.f, 1.f, 0.f)) *
                                glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, length));
        drawMesh(s, guide, model, kGuide);
        s.setFloat("uAlpha", 1.f);
        s.setBool("uFog", true);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    /// Sky, everything opaque, then the aim guide, into whatever framebuffer is bound.
    void mainPass(const PassParams& pp, const SceneDescription& scene) {
        glDisable(GL_BLEND);
        drawSky(pp.toneMap);
        GlShader& s = *ctx.data().lit;
        setFrameUniforms(s, pp);
        drawOpaque(s, pp, scene, nullptr, true);
        if (scene.showAimGuide)
            drawGuide(s, scene);
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
    if (viewportH < 1)
        return;  // minimised, or the HUD bands take the whole window
    glViewport(0, viewportY, p.ctx.width(), viewportH);
    PassParams pp = p.cameraPass(camera, {p.ctx.width(), viewportH});
    pp.toneMap = true;
    p.mainPass(pp, scene);
}

// ---------------------------------------------------------------- Overlay
struct Overlay::Impl {
    Context& ctx;
    unsigned vao = 0, vbo = 0, ebo = 0;
    glm::mat4 proj {1.f};
    std::vector<char> textBuffer = std::vector<char>(200000);
    std::vector<unsigned> indices;
    struct TextVertex {
        float x, y, u, v;
    };
    unsigned textVao = 0, textVbo = 0;
    std::vector<TextVertex> textVertices;

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
        glGenVertexArrays(1, &textVao);
        glGenBuffers(1, &textVbo);
        glBindVertexArray(textVao);
        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                              reinterpret_cast<void*>(static_cast<std::uintptr_t>(2 * sizeof(float))));
        glBindVertexArray(0);
    }
    ~Impl() {
        glDeleteBuffers(1, &textVbo);
        glDeleteVertexArrays(1, &textVao);
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

    void drawGlyphs(const std::vector<GlyphQuad>& quads, glm::vec2 offset, glm::vec4 color) {
        textVertices.clear();
        for (const GlyphQuad& q : quads) {
            const float x0 = q.p0.x + offset.x, y0 = q.p0.y + offset.y;
            const float x1 = q.p1.x + offset.x, y1 = q.p1.y + offset.y;
            textVertices.insert(textVertices.end(), {{x0, y0, q.uv0.x, q.uv0.y}, {x1, y0, q.uv1.x, q.uv0.y},
                                                     {x1, y1, q.uv1.x, q.uv1.y}, {x0, y0, q.uv0.x, q.uv0.y},
                                                     {x1, y1, q.uv1.x, q.uv1.y}, {x0, y1, q.uv0.x, q.uv1.y}});
        }
        GlShader& s = *ctx.data().text;
        s.use();
        s.setMat4("uMVP", proj);
        s.setVec4("uColor", color);
        s.setInt("uAtlas", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx.data().fontTexture.id());
        glBindVertexArray(textVao);
        glBindBuffer(GL_ARRAY_BUFFER, textVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(textVertices.size() * sizeof(TextVertex)),
                     textVertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textVertices.size()));
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
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
    if (const auto& font = _p->ctx.data().font) {
        const auto quads = font->layout(text, px, font->pixelHeight(scale));
        _p->drawGlyphs(quads, {1.f, 1.f}, {0.f, 0.f, 0.f, 0.5f});  // drop shadow
        _p->drawGlyphs(quads, {0.f, 0.f}, glm::vec4(rgb, 1.f));
        return;
    }
    std::string s(text);
    const int quads = stb_easy_font_print(0.f, 0.f, s.data(), nullptr, _p->textBuffer.data(),
                                          static_cast<int>(_p->textBuffer.size()));
    const glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(px, 0.f)) *
                            glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, 1.f));
    _p->drawQuads(_p->textBuffer.data(), quads, model, glm::vec4(rgb, 1.f));
}

glm::vec2 Overlay::measureText(std::string_view text, float scale) const {
    if (const auto& font = _p->ctx.data().font)
        return font->measure(text, font->pixelHeight(scale));
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
