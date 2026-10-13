#ifndef BOWLINGGL_SCENE_ADAPTER_H
#define BOWLINGGL_SCENE_ADAPTER_H
#include <Gamebox/Alley/Alley.h>
#include <Gamebox/Render/Render.h>

/// The only place that converts Alley plain floats into Render/GLM types.
namespace SceneAdapter {
Gamebox::Render::SceneGeometry geometry(const Gamebox::Alley::LaneDimensions& d);
Gamebox::Render::Transform toTransform(const Gamebox::Alley::Pose& p);
Gamebox::Render::SceneDescription toScene(const Gamebox::Alley::Snapshot& s);
}  // namespace SceneAdapter
#endif  // BOWLINGGL_SCENE_ADAPTER_H
