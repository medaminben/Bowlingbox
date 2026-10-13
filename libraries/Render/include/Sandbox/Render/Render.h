#ifndef SANDBOX_RENDER_H
#define SANDBOX_RENDER_H
#include <Sandbox/Render/Sandbox_Render_export.h>
#include <Sandbox/Render/ScoreboardLayout.h>
#include <Sandbox/Core/Core.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace Sandbox {
namespace Render {

struct SANDBOX_RENDER_API Transform {
    glm::vec3 position {0.f};
    glm::quat rotation {1.f, 0.f, 0.f, 0.f};  // w, x, y, z
    glm::vec3 scale {1.f};
    glm::mat4 matrix() const;
};

/// Physical sizes (metres); copied from Alley::kLane by the application.
struct SceneGeometry {
    float laneHalfWidth, laneLength, approach, gutterWidth, gutterDepth;
    float ballRadius, pinHeight, pinMaxRadius, pinComHeight;
};

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
class SANDBOX_RENDER_API Context {
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

class SANDBOX_RENDER_API Camera {
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

/// Lane, gutters, ball, pins, aim guide.
class SANDBOX_RENDER_API SceneRenderer {
public:
    SceneRenderer(Context& ctx, const SceneGeometry& geometry);
    ~SceneRenderer();
    void draw(const Camera& camera, const SceneDescription& scene, int viewportY, int viewportH);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

/// 2D layer in pixels, origin top-left, drawn after the scene.
class SANDBOX_RENDER_API Overlay {
public:
    explicit Overlay(Context& ctx);
    ~Overlay();
    void begin(int width, int height);
    void drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb);
    glm::vec2 measureText(std::string_view text, float scale) const;
    void drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba);
    void strokeRect(const Rect& r, float thickness, glm::vec4 rgba);
    void drawScoreboard(const std::vector<int>& rolls, const std::vector<int>& scores, int total,
                        const ScoreboardLayout& layout);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_H
