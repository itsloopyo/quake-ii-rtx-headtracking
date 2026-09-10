// Characterization tests for src/hooks/reticle.cpp - the step that turns a
// projected aim point into the pixel the game's crosshair is drawn at.
//
// This half used to sit inside the render hook behind engine memory, where no
// test could reach it, and it is where the two states that are NOT an offset
// get decided: Untouched leaves the game's own crosshair alone, Hidden draws
// nothing at all. Confusing them is silent in game - the reticle either stays
// at centre marking a point the round will not reach, or vanishes when it
// should have been left where the game put it.
//
// The pixel arithmetic is locked here too, because the viewport is not always
// the whole framebuffer: an off-origin rect that was ignored would put every
// reticle a fixed distance from where the shot lands.

#include "hooks/reticle.h"

#include <cmath>
#include <iostream>

#include "quake_math.h"

namespace {

namespace rt = Q2RTXHT::reticle;
namespace qm = Q2RTXHT::quake_math;
using Q2RTXHT::reticle::Vec3;

int g_failures = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        std::cout << "  [PASS] " << name << "\n";
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++g_failures;
    }
}

bool NearEqual(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

// A level 1280x720 frame at the framebuffer origin, tracking applied, with the
// aim point 200 units straight ahead of the render eye.
rt::FrameView LevelFrame() {
    rt::FrameView view;
    view.applied = true;
    view.aimValid = true;
    const float level[3] = {0.0f, 0.0f, 0.0f};
    qm::AngleVectors(level, view.f, view.r, view.u);
    view.eye[0] = view.eye[1] = view.eye[2] = 0.0f;
    view.aim[0] = 200.0f;
    view.fovX = 90.0f;
    view.fovY = 60.0f;
    view.rx = 0;
    view.ry = 0;
    view.rw = 1280;
    view.rh = 720;
    return view;
}

void TestAimStraightAheadLandsAtTheViewportCentre() {
    float px = -1.0f, py = -1.0f;
    Check(rt::Target(LevelFrame(), px, py) == rt::State::Offset,
          "an aim point inside the frame asks for an offset");
    Check(NearEqual(px, 640.0f) && NearEqual(py, 360.0f),
          "an aim point straight ahead is the centre pixel of the viewport");
}

// The 3D viewport is not always the whole framebuffer, and the pixel handed to
// the crosshair is measured from the framebuffer's top-left, so the rect origin
// has to be carried through. Ignoring it offsets every reticle by a constant.
void TestViewportOriginIsCarriedIntoThePixel() {
    rt::FrameView view = LevelFrame();
    view.rx = 100;
    view.ry = 40;
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Offset, "an offset rect still projects");
    Check(NearEqual(px, 740.0f) && NearEqual(py, 400.0f),
          "the viewport origin is added to the projected pixel");
}

// Screen y runs down and ndc y runs up, so a head that looks up must move the
// reticle DOWN the screen: the shot stays where the body is aiming while the
// picture rises. Getting this flip wrong is a reticle that mirrors the aim.
void TestLookingUpMovesTheReticleDownTheScreen() {
    rt::FrameView view = LevelFrame();
    const float level[3] = {0.0f, 0.0f, 0.0f};
    qm::AngleVectors(level, view.f, view.r, view.u);
    qm::RotateBasisByHeadPose(0.0f, 20.0f, 0.0f, /*worldSpaceYaw=*/true,
                              view.f, view.r, view.u);
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Offset, "a pitched frame projects");
    Check(NearEqual(px, 640.0f), "head pitch alone does not move the reticle sideways");
    Check(py > 360.0f, "a head that looks up drives the reticle down the screen");
}

// The two non-offset states are not interchangeable. Untouched means "we have
// nothing better than where the game would have drawn it"; Hidden means "we
// know the shot does not go anywhere on this screen".
void TestUntrackedFrameLeavesTheGameCrosshairAlone() {
    rt::FrameView view = LevelFrame();
    view.applied = false;
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Untouched,
          "a frame with no tracking applied is Untouched, not Hidden");
}

void TestUntracedAimPointLeavesTheGameCrosshairAlone() {
    rt::FrameView view = LevelFrame();
    view.aimValid = false;
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Untouched,
          "a frame whose aim point could not be traced is Untouched, not Hidden");
}

void TestAimBehindTheEyeIsHidden() {
    rt::FrameView view = LevelFrame();
    view.aim[0] = -200.0f;
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Hidden,
          "an aim point behind the render eye is Hidden");
}

// A head turn far enough to put the aim line off the frame leaves nothing to
// mark. Clamping to the edge would claim the shot goes there.
void TestAimOutsideTheFrameIsHidden() {
    rt::FrameView view = LevelFrame();
    const float level[3] = {0.0f, 0.0f, 0.0f};
    qm::AngleVectors(level, view.f, view.r, view.u);
    qm::RotateBasisByHeadPose(70.0f, 0.0f, 0.0f, true, view.f, view.r, view.u);
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Hidden,
          "an aim point swung outside the frame is Hidden rather than clamped to the edge");
}

// A degenerate FOV means the projection itself could not run, which is the
// Untouched case - there is no correction to make, so the game's own crosshair
// is the best answer available.
void TestUnusableFovLeavesTheGameCrosshairAlone() {
    rt::FrameView view = LevelFrame();
    view.fovX = 0.0f;
    view.fovY = 0.0f;
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Untouched,
          "a frame with no usable fov is Untouched, not Hidden");
}

// Screen x runs right and ndc x runs right, so a head that turns RIGHT must move
// the reticle LEFT: the shot stays where the body is aiming while the picture
// swings the other way. The vertical flip is pinned above; without this the
// horizontal half of the pixel mapping can be mirrored and every test still
// passes, because every other case here projects to ndc x = 0.
void TestHeadYawRightDrivesTheReticleLeftAcrossTheScreen() {
    rt::FrameView view = LevelFrame();
    qm::RotateBasisByHeadPose(20.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/true,
                              view.f, view.r, view.u);
    float px = 0.0f, py = 0.0f;
    Check(rt::Target(view, px, py) == rt::State::Offset, "a yawed frame projects");
    Check(NearEqual(py, 360.0f), "head yaw alone does not move the reticle vertically");
    // A level frame with the aim point straight ahead: 20 degrees of head yaw
    // puts the fixed aim point at ndc x = -tan(20)/tan(45), so the pixel is
    // 640 * (1 - tan 20). Written as the geometry rather than as the number a
    // run produced, which is the same rule the parallax tests below follow.
    const float expected = 640.0f * (1.0f - std::tan(20.0f * qm::kDeg2Rad));
    Check(NearEqual(px, expected, 0.01f),
          "a head that turns right drives the reticle left across the screen");
}

// ---- Litmus 5 and 6: the positional release gate (AGENTS.md) ----------------
//
// With rotation centred, a lean moves the render eye off the eye the shot leaves
// from, and the reticle has to follow the parallax of the IMPACT POINT, which
// scales as 1/depth. A projection built on a fixed, smoothed or stale depth
// agrees at exactly one range and crosses to the other side of the shot either
// side of it - AGENTS.md calls that a failed release gate.
//
// Every other test in this file leaves the render eye at the origin, so without
// these two the whole `aim - eye` term in reticle.cpp can be deleted and the
// suite still passes clean.
//
// The expected pixels are computed from the frame geometry, not by calling the
// projection: an expectation routed through the code under test moves with it.

// Half the frame width times the tangent ratio: the pixel a point `lean` off the
// view axis at `depth` sits at, measured from the centre.
float ParallaxPixels(float lean, float depth, float halfExtentPixels, float fovDeg) {
    return halfExtentPixels * (lean / depth) / std::tan(fovDeg * 0.5f * qm::kDeg2Rad);
}

void TestSidewaysLeanMovesTheReticleByTheImpactPointParallax() {
    const float lean = 12.0f;  // 0.30 m, the shipped LimitX
    for (float depth : {50.0f, 400.0f}) {
        rt::FrameView left = LevelFrame(), right = LevelFrame();
        left.aim[0] = right.aim[0] = depth;
        left.eye[1] = lean;    // Quake +y is left
        right.eye[1] = -lean;

        float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
        Check(rt::Target(left, lx, ly) == rt::State::Offset &&
                  rt::Target(right, rx, ry) == rt::State::Offset,
              "litmus 5: a leaned frame projects");

        const float expected = ParallaxPixels(lean, depth, 640.0f, 90.0f);
        Check(NearEqual(lx, 640.0f + expected, 0.02f) &&
                  NearEqual(rx, 640.0f - expected, 0.02f),
              "litmus 5: the reticle moves by the impact point's parallax at this depth");
        Check(NearEqual(ly, 360.0f) && NearEqual(ry, 360.0f),
              "litmus 5: a sideways lean does not move the reticle vertically");
    }
}

void TestVerticalLeanMovesTheReticleByTheImpactPointParallax() {
    const float lean = 8.0f;  // 0.20 m, the shipped LimitY
    for (float depth : {50.0f, 400.0f}) {
        rt::FrameView up = LevelFrame(), down = LevelFrame();
        up.aim[0] = down.aim[0] = depth;
        up.eye[2] = lean;
        down.eye[2] = -lean;

        float ux = 0.0f, uy = 0.0f, dx = 0.0f, dy = 0.0f;
        Check(rt::Target(up, ux, uy) == rt::State::Offset &&
                  rt::Target(down, dx, dy) == rt::State::Offset,
              "litmus 6: a leaned frame projects");

        const float expected = ParallaxPixels(lean, depth, 360.0f, 60.0f);
        Check(NearEqual(uy, 360.0f + expected, 0.02f) &&
                  NearEqual(dy, 360.0f - expected, 0.02f),
              "litmus 6: an eye that rises drives the reticle down by the parallax");
        Check(NearEqual(ux, 640.0f) && NearEqual(dx, 640.0f),
              "litmus 6: a vertical lean does not move the reticle sideways");
    }
}

// The same gate against the in-game measurement in .lab/NOTES.md "Near/far
// release gate": demo1 spawn, level view, 1280x720 at fov_x 91.31, a 12-unit
// sideways lean at three aim depths. These are that table's "mod px offset"
// column, which its "drawn crosshair" and "world patch" columns corroborate to
// within a pixel - so this is a regression pin on numbers that were checked
// against the screen, not an independent oracle. A fixed impact depth can be
// made to match any single row; matching all three is what says it is live.
//
// Only fov_x is set from the measurement. The vertical half of the frame plays
// no part in a sideways lean, so LevelFrame's fovY is left alone.
void TestMeasuredNearFarReleaseGate() {
    struct Row { float depth; float offset; };
    const Row measured[] = { {46.2f, 162.6f}, {95.8f, 78.3f}, {362.0f, 20.7f} };

    for (const Row& row : measured) {
        rt::FrameView view = LevelFrame();
        view.fovX = 91.31f;
        view.aim[0] = row.depth;
        view.eye[1] = 12.0f;
        float px = 0.0f, py = 0.0f;
        Check(rt::Target(view, px, py) == rt::State::Offset,
              "the measured frame projects");
        Check(NearEqual(px - 640.0f, row.offset, 0.5f),
              "the in-game near/far measurement reproduces at this aim depth");
    }
}

// The crosshair hook runs in the engine's 2D pass, after the render hook has
// published the frame it drew, and reads it through this pair.
void TestPublishedFrameIsWhatTheCrosshairHookReads() {
    rt::FrameView view = LevelFrame();
    view.rx = 100;
    view.ry = 40;
    rt::Publish(view);

    float px = 0.0f, py = 0.0f;
    Check(rt::TargetForPublishedFrame(px, py) == rt::State::Offset,
          "the published frame answers the same as the frame itself");
    Check(NearEqual(px, 740.0f) && NearEqual(py, 400.0f),
          "the published frame answers with the same pixel");

    rt::FrameView untracked;
    rt::Publish(untracked);
    Check(rt::TargetForPublishedFrame(px, py) == rt::State::Untouched,
          "publishing a frame with no tracking applied clears the offset");
}

// The render hook publishes once per frame and this hook reads once per frame,
// so a read that finds nothing new means the render hook did not run - the
// engine took its slot back on a renderer restart, say. Holding the last frame
// would pin the crosshair to a placement computed for a camera that is no
// longer being written, which is worse than not compensating at all: the view
// is untracked and the reticle is nailed off-centre with no way back.
void TestAPublishedFrameIsConsumedSoAStaleOneCannotPersist() {
    rt::Publish(LevelFrame());

    float px = 0.0f, py = 0.0f;
    Check(rt::TargetForPublishedFrame(px, py) == rt::State::Offset,
          "the frame just published answers with an offset");
    Check(rt::TargetForPublishedFrame(px, py) == rt::State::Untouched,
          "reading it a second time with nothing republished falls back to the "
          "game's own crosshair rather than repeating the last placement");
}

}  // namespace

int RunReticleTests() {
    std::cout << "\nReticle placement tests\n";
    TestAimStraightAheadLandsAtTheViewportCentre();
    TestViewportOriginIsCarriedIntoThePixel();
    TestLookingUpMovesTheReticleDownTheScreen();
    TestUntrackedFrameLeavesTheGameCrosshairAlone();
    TestUntracedAimPointLeavesTheGameCrosshairAlone();
    TestAimBehindTheEyeIsHidden();
    TestAimOutsideTheFrameIsHidden();
    TestUnusableFovLeavesTheGameCrosshairAlone();
    TestHeadYawRightDrivesTheReticleLeftAcrossTheScreen();
    TestSidewaysLeanMovesTheReticleByTheImpactPointParallax();
    TestVerticalLeanMovesTheReticleByTheImpactPointParallax();
    TestMeasuredNearFarReleaseGate();
    TestPublishedFrameIsWhatTheCrosshairHookReads();
    TestAPublishedFrameIsConsumedSoAStaleOneCannotPersist();

    if (g_failures == 0) {
        std::cout << "Reticle placement tests: all passed\n";
    } else {
        std::cout << "Reticle placement tests: " << g_failures << " failure(s)\n";
    }
    return g_failures;
}
