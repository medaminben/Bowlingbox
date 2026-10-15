#ifndef BOWLINGBOX_RENDER_H
#define BOWLINGBOX_RENDER_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Render/Canvas2D.h>
#include <Bowlingbox/Render/Environment.h>
#include <Bowlingbox/Render/ScoreboardLayout.h>
#include <Bowlingbox/Core/Core.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace Bowlingbox {
namespace Render {

struct SceneDescription {
    Transform ball;
    std::array<Transform, 10> pins;
    std::array<bool, 10> pinVisible {};
    bool showAimGuide = false;
    float aimOffset = 0.f;    // metres, ball start x
    float aimAngleDeg = 0.f;  // rotation about y
};

struct ContextData;

/// One per GL context. Loads GL 3.3 core via GLAD and owns the shader programs.
class BOWLINGBOX_RENDER_API Context {
public:
    static Result<std::unique_ptr<Context>> create();  // needs a current GL context
    ~Context();
    void resize(int width, int height);
    int width() const;
    int height() const;
    void beginFrame(const glm::vec3& clearColor);
    void endFrame();
    ContextData& data();

private:
    Context();
    std::unique_ptr<ContextData> _d;
};

class BOWLINGBOX_RENDER_API Camera {
public:
    void setViewport(int width, int height);
    void lookAt(glm::vec3 eye, glm::vec3 target);
    glm::mat4 view() const;
    glm::mat4 projection() const;

private:
    glm::vec3 _eye {0.f, 1.8f, -3.5f};
    glm::vec3 _target {0.f, 0.f, 6.f};
    float _aspect = 16.f / 9.f;
};

/// Background gradient, theme props, lane, gutters, ball, pins, aim guide.
class BOWLINGBOX_RENDER_API SceneRenderer {
public:
    SceneRenderer(Context& ctx, const SceneGeometry& geometry, ThemeId theme = ThemeId::NeonHall);
    ~SceneRenderer();
    /// Switches decor and colours. Rebuilds the prop list on the CPU; makes no GL calls.
    void setTheme(ThemeId theme);
    void draw(const Camera& camera, const SceneDescription& scene, int viewportY, int viewportH);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

/// OpenGL implementation of ICanvas2D, drawn after the scene.
class BOWLINGBOX_RENDER_API Overlay final : public ICanvas2D {
public:
    explicit Overlay(Context& ctx);
    ~Overlay() override;
    void begin(int width, int height);
    void drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb) override;
    glm::vec2 measureText(std::string_view text, float scale) const override;
    void drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_H
