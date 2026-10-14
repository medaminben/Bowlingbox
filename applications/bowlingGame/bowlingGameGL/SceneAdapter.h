#ifndef BOWLINGGL_SCENE_ADAPTER_H
#define BOWLINGGL_SCENE_ADAPTER_H
#include <Bowlingbox/Alley/Alley.h>
#include <Bowlingbox/Render/Render.h>

/// The only place that converts Alley plain floats into Render/GLM types.
namespace SceneAdapter {
Bowlingbox::Render::SceneGeometry geometry(const Bowlingbox::Alley::LaneDimensions& d);
Bowlingbox::Render::Transform toTransform(const Bowlingbox::Alley::Pose& p);
Bowlingbox::Render::SceneDescription toScene(const Bowlingbox::Alley::Snapshot& s);
}  // namespace SceneAdapter
#endif  // BOWLINGGL_SCENE_ADAPTER_H
