#pragma once

#include "cameraunlock/math/vec3.h"

namespace Q2RTXHT {

// Aim decoupling feedback: the shot leaves the CLEAN eye along the CLEAN aim
// line, and the player is looking through the head-tracked one, so the reticle
// has to be drawn where that line lands inside the tracked frame.
namespace reticle {

using cameraunlock::math::Vec3;

// What this frame asks of the game's crosshair.
enum class State {
    Untouched,  // no tracking applied, or no aim point: draw where the game would
    Offset,     // draw at (px, py)
    Hidden,     // the aim line is not inside the tracked frame: draw nothing
};

// Everything the placement needs from the frame the renderer is about to draw.
// The render hook fills it as the frame is built; nothing here reads engine
// memory a second time, which is what keeps the reticle glued to the camera
// that was actually written rather than to a re-derivation of it.
struct FrameView {
    bool applied = false;      // tracking was written into this frame
    bool aimValid = false;     // the clean aim line's impact point is known
    float aim[3] = {0, 0, 0};  // world point the clean aim line lands on
    float eye[3] = {0, 0, 0};  // world position the frame is rendered from
    Vec3 f, r, u;              // tracked view basis
    float fovX = 90.0f;
    float fovY = 60.0f;
    int rx = 0, ry = 0, rw = 0, rh = 0;
};

// Where the clean aim line lands inside `view`, in framebuffer pixels from the
// top-left. px/py are written only for State::Offset. Pure: the same view
// always gives the same answer, which is what lets the litmus tests in
// tests/reticle_tests.cpp exercise it off the game.
State Target(const FrameView& view, float& px, float& py);

// The render hook publishes the frame it drew; the crosshair hook places
// against it when the engine gets to the 2D pass. Both run on the render
// thread, so this needs no synchronisation.
void Publish(const FrameView& view);
State TargetForPublishedFrame(float& px, float& py);

}  // namespace reticle
}  // namespace Q2RTXHT
