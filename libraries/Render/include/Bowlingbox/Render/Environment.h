#ifndef BOWLINGBOX_RENDER_ENVIRONMENT_H
#define BOWLINGBOX_RENDER_ENVIRONMENT_H
#include <Bowlingbox/Render/Bowlingbox_Render_export.h>
#include <Bowlingbox/Render/Geometry.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace Bowlingbox {
namespace Render {

struct BOWLINGBOX_RENDER_API Transform {
    glm::vec3 position {0.f};
    glm::quat rotation {1.f, 0.f, 0.f, 0.f};  // w, x, y, z
    glm::vec3 scale {1.f};
    glm::mat4 matrix() const;
};

/// Physical sizes (metres); copied from Alley::kLane by the application.
struct SceneGeometry {
    float laneHalfWidth, laneLength, approach, gutterWidth, gutterDepth;
    float ballRadius, pinHeight, pinMaxRadius, pinComHeight;
    float pinSpacing, headPinZ;
};

enum class ThemeId { NeonHall, ClassicAlley, Outdoor };
inline constexpr std::array<ThemeId, 3> kThemes {ThemeId::NeonHall, ThemeId::ClassicAlley, ThemeId::Outdoor};

/// Surface pattern drawn by the lit shader. The values are the shader's uPattern.
enum class Pattern { None = 0, Wood = 1, BallSwirl = 2 };

struct Material {
    glm::vec3 color {1.f};          // sRGB as authored; the renderer converts to linear
    bool emissive = false;          // glows: ignores the lights, fogged at half strength
    float roughness = 0.6f;         // 0 = sharp highlight, 1 = chalky
    float specular = 0.04f;         // reflectance at normal incidence
    float emissiveStrength = 1.f;   // > 1 pushes the glow over the bloom threshold
    Pattern pattern = Pattern::None;
};

/// A light without shadows; falls off with distance and stops at `range`.
struct PointLight {
    glm::vec3 position {0.f};
    glm::vec3 color {1.f};  // linear
    float intensity = 0.f;
    float range = 1.f;      // metres
};
inline constexpr int kMaxPointLights = 4;

struct PropSet {
    bool pillars = false, neonStrips = false, ceilingLights = false, wallScreens = false;
    bool scoreMonitor = false, fences = false, trees = false;
};

struct Theme {
    ThemeId id;
    std::string_view name;   // "neon", "classic", "outdoor": the only name table
    std::string_view label;  // "Neon Hall", "Classic Alley", "Outdoor"
    glm::vec3 backgroundTop, backgroundBottom;
    glm::vec3 fogColor;
    float fogStart, fogEnd;  // metres from the camera
    glm::vec3 lightDir;      // direction the light travels
    glm::vec3 keyColor;      // linear; the key light casts the shadows
    float keyIntensity;
    glm::vec3 skyAmbient, groundAmbient;  // linear; hemisphere ambient from above / below
    std::array<PointLight, kMaxPointLights> pointLights;
    int pointLightCount;     // 0 .. kMaxPointLights
    float exposure;          // > 0
    float bloomStrength;     // 0 .. 1
    Material lane, gutter, ball, pinBody, pinBand;
    bool indoor;             // walls + ceiling; otherwise open ground
    int neighbourLanesPerSide;
    Material floor, wall, ceiling, trim, accentA, accentB, screen;
    PropSet props;
};

/// Box, Cylinder and Cone are unit shapes (1 m across, centred on the origin), so
/// `transform.scale` is their size in metres. Pin is the real pin mesh, base at y = -pinComHeight.
enum class Shape { Box, Cylinder, Cone, Pin };

struct Prop {
    Shape shape = Shape::Box;
    Transform transform;
    Material material;
    bool pinBand = false;  // draw the theme's pinBand on the neck (Pin only)
};

struct Aabb {
    glm::vec3 min {0.f};
    glm::vec3 max {0.f};
};

// Layout constants, metres.
inline constexpr float kReturnGap = 0.30f;     // between the gutters of adjacent lanes
inline constexpr float kReturnWidth = 0.14f;   // ball-return box in the middle of that gap
inline constexpr float kReturnHeight = 0.3f;
inline constexpr float kHallHeight = 4.5f;     // ceiling's inner face
inline constexpr float kHallFrontZ = -8.f;
inline constexpr float kHallBackZ = 22.f;
inline constexpr float kWallThickness = 0.2f;
inline constexpr float kFloorTopY = -0.15f;
inline constexpr float kPitLength = 2.4f;      // BulletWorld's pit behind the deck
inline constexpr float kPitTopY = -0.25f;

BOWLINGBOX_RENDER_API const Theme& theme(ThemeId id);
/// ASCII case-insensitive; empty for an unknown name.
BOWLINGBOX_RENDER_API std::optional<ThemeId> themeFromName(std::string_view name);
BOWLINGBOX_RENDER_API ThemeId nextTheme(ThemeId id);  // Neon -> Classic -> Outdoor -> Neon
/// Distance between lane centres: 2 * (laneHalfWidth + gutterWidth) + kReturnGap.
BOWLINGBOX_RENDER_API float lanePitch(const SceneGeometry& g);
/// Inner half-width of an indoor hall: lanePitch * (neighbourLanesPerSide + 0.5) + 0.6.
BOWLINGBOX_RENDER_API float hallHalfWidth(const Theme& theme, const SceneGeometry& g);
BOWLINGBOX_RENDER_API std::vector<Prop> buildProps(const Theme& theme, const SceneGeometry& g);
/// The mesh drawn for a shape, and its local box (min/max over that mesh's vertices).
BOWLINGBOX_RENDER_API MeshData shapeMesh(Shape shape, const SceneGeometry& g);
BOWLINGBOX_RENDER_API Aabb localBounds(Shape shape, const SceneGeometry& g);
/// World box: localBounds' eight corners through transform.matrix().
BOWLINGBOX_RENDER_API Aabb bounds(const Prop& prop, const SceneGeometry& g);

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_ENVIRONMENT_H
