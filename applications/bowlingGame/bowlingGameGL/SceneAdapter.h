#ifndef BOWLINGGL_SCENE_ADAPTER_H
#define BOWLINGGL_SCENE_ADAPTER_H
#include <Sandbox/Alley/Alley.h>
#include <Sandbox/Render/Render.h>

/// The only place that converts Alley plain floats into Render/GLM types.
namespace SceneAdapter {
Sandbox::Render::SceneGeometry geometry(const Sandbox::Alley::LaneDimensions& d);
Sandbox::Render::Transform toTransform(const Sandbox::Alley::Pose& p);
Sandbox::Render::SceneDescription toScene(const Sandbox::Alley::Snapshot& s);
}  // namespace SceneAdapter
#endif  // BOWLINGGL_SCENE_ADAPTER_H
