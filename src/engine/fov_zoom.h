#pragma once

namespace Q2RTXHT {

struct BuildProfile;

// Quake II RTX renders at whatever field of view the player set (the "field of
// view" slider in the video menu, or the fov cvar), and the game moves it out from
// under them in places they did not ask for: ClientEndServerFrame forces 90 for
// the end-of-level intermission, and a demo renders the field of view it was
// recorded at. A narrower field of view magnifies everything in the frame, head
// tracking included, so the same head angle would sweep further across the
// screen the moment the rendered FOV left the player's setting.
//
// The live value is cl.fov_x, the interpolated player-state FOV that
// V_RenderView turns into the refdef.fov_x / fov_y pair it hands the renderer.
// The reference is the "fov" cvar, clamped the way ClientUserinfoChanged clamps
// it. Both are Quake fov degrees - horizontal, referenced to 4:3, which is what
// cl.fov_y = V_CalcFov(cl.fov_x, 4, 3) establishes - because they are the same
// quantity read at two moments, so no aspect conversion sits between them.
//
// The aspect and "fov scaling" step that turns either of them into the
// horizontal FOV actually rendered is linear in the tangent (V_CalcFov gives
// tan(out/2) = height/width * tan(in/2)), so it cancels out of the ratio
// exactly. That is what keeps the factor at 1.0 for every resolution, both
// cl_adjustfov positions, and every fov the player picks - the reference has to
// follow their setting, because a fixed 90 here would quietly scale all of
// normal play by tan(45)/tan(fov/2).
namespace fov_zoom {

// The geometry of the frame being rendered. Carried in only so the diagnostic
// line can state the whole basis of the factor at once: a factor that is wrong
// by a constant reads exactly like one that is right, and the only check that
// catches it is that line reading 1.0000 in ordinary play.
struct RenderedFrame {
    float fovX;
    float fovY;
    int width;
    int height;
};

// Resolves cl.fov_x and the "fov" cvar from the matched build.
void Init(void* moduleBase, const BuildProfile& profile);

// Recomputes the factor from the live FOV pair and returns it. Called once per
// rendered frame, so a mid-game move of the FOV slider is picked up without a
// restart. Logging is driven off the camera rather than off the pose, so the
// basis is on record from the first camera frame with no tracker connected.
float Update(const RenderedFrame& frame);

}  // namespace fov_zoom
}  // namespace Q2RTXHT
