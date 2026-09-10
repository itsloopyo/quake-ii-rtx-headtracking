#include "hooks/reticle.h"

#include <cmath>

#include "quake_math.h"

namespace Q2RTXHT {
namespace reticle {

namespace {

FrameView g_published;

}  // namespace

State Target(const FrameView& view, float& px, float& py) {
    // Nothing was injected this frame, or the collision model could not be
    // asked where the shot lands: the reticle is already where the game wants
    // it, so leave it there rather than guess at an offset.
    if (!view.applied || !view.aimValid) return State::Untouched;

    const Vec3 rel{ view.aim[0] - view.eye[0],
                    view.aim[1] - view.eye[1],
                    view.aim[2] - view.eye[2] };

    float ndcx = 0.0f, ndcy = 0.0f;
    switch (quake_math::ProjectToNdc(rel, view.f, view.r, view.u,
                                     view.fovX, view.fovY, ndcx, ndcy)) {
        case quake_math::Projection::Behind:
            return State::Hidden;  // behind, or on top of, the render eye
        case quake_math::Projection::Degenerate:
            return State::Untouched;
        case quake_math::Projection::Ok:
            break;
    }

    // A head turn far enough to put the aim line outside the frame leaves
    // nothing to mark; drawing it at the edge would claim the shot goes there.
    if (std::fabs(ndcx) > 1.0f || std::fabs(ndcy) > 1.0f) return State::Hidden;

    px = view.rx + view.rw * 0.5f * (1.0f + ndcx);
    py = view.ry + view.rh * 0.5f * (1.0f - ndcy);
    return State::Offset;
}

void Publish(const FrameView& view) {
    g_published = view;
}

State TargetForPublishedFrame(float& px, float& py) {
    const State state = Target(g_published, px, py);
    // Consumed, not just read. If the render hook stops running - the engine
    // took its slot back on a renderer restart - the last frame it published
    // would otherwise stay valid forever, and this hook would keep pinning the
    // crosshair to a placement computed for a camera that is no longer being
    // written. Clearing it falls back to the game's own crosshair instead.
    // SCR_DrawCrosshair has one call site and runs once per frame, after
    // R_RenderFrame, so a live frame is always published before it is read.
    g_published.applied = false;
    return state;
}

}  // namespace reticle
}  // namespace Q2RTXHT
