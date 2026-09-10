// Characterization tests for src/quake_math.cpp - the tracker-to-Quake axis
// boundary and the projection the reticle is placed with.
//
// These lock behaviour that was established by measurement in a running game
// (see .lab/NOTES.md, "Verified in game"), not behaviour derived from first
// principles. A sign here is not a matter of taste: flipping one renders the
// frame upside down, sends the lean the wrong way, or walks the reticle off the
// shot, and none of it shows up until somebody is playing.

#include "quake_math.h"

#include <cmath>
#include <cstdio>
#include <iostream>

namespace {

using Q2RTXHT::quake_math::Vec3;
namespace qm = Q2RTXHT::quake_math;

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

bool NearEqual(const Vec3& a, const Vec3& b, float eps = 1e-3f) {
    return NearEqual(a.x, b.x, eps) && NearEqual(a.y, b.y, eps) && NearEqual(a.z, b.z, eps);
}

// A level pose: looking down +X, which is Quake's world forward.
constexpr float kLevel[3] = {0.0f, 0.0f, 0.0f};

// The pair the render hook runs: the clean basis from the view angles, then the
// head pose rotated into it.
void TrackedBasis(const float cleanAngles[3], float headYaw, float headPitch, float headRoll,
                  bool worldSpaceYaw, Vec3& f, Vec3& r, Vec3& u) {
    qm::AngleVectors(cleanAngles, f, r, u);
    qm::RotateBasisByHeadPose(headYaw, headPitch, headRoll, worldSpaceYaw, f, r, u);
}

void TestLevelPoseGivesQuakeWorldBasis() {
    Vec3 f, r, u;
    qm::AngleVectors(kLevel, f, r, u);
    // Quake's world is x-forward, y-LEFT, z-up, so the RIGHT vector is -y.
    Check(NearEqual(f, Vec3(1.0f, 0.0f, 0.0f)), "level pose forward is +x");
    Check(NearEqual(r, Vec3(0.0f, -1.0f, 0.0f)), "level pose right is -y");
    Check(NearEqual(u, Vec3(0.0f, 0.0f, 1.0f)), "level pose up is +z");
}

void TestVectorsToAnglesInvertsAngleVectors() {
    const float poses[][3] = {
        {0.0f, 0.0f, 0.0f},
        {20.0f, -35.0f, 0.0f},
        {-15.0f, 170.0f, 25.0f},
        {45.0f, 90.0f, -40.0f},
    };
    for (const auto& pose : poses) {
        Vec3 f, r, u;
        qm::AngleVectors(pose, f, r, u);
        float back[3] = {0, 0, 0};
        qm::VectorsToAngles(f, r, u, back);
        const bool same = NearEqual(back[0], pose[0], 1e-2f) &&
                          NearEqual(back[1], pose[1], 1e-2f) &&
                          NearEqual(back[2], pose[2], 1e-2f);
        // Named per pose: {45, 90, -40} sits near the pitch-recovery edge, and a
        // shared label said only how many failed, not which.
        char label[96];
        std::snprintf(label, sizeof(label),
                      "VectorsToAngles round-trips AngleVectors at (p%.0f y%.0f r%.0f)",
                      pose[0], pose[1], pose[2]);
        Check(same, label);
    }
}

// The first build recovered roll as atan2(r.z, -u.z), which is roll + 180 for
// EVERY pose including a level one, so the whole frame rendered upside down the
// moment tracking applied. A level basis must come back at roll 0.
void TestLevelBasisRecoversZeroRollNotOneEighty() {
    Vec3 f, r, u;
    qm::AngleVectors(kLevel, f, r, u);
    float back[3] = {0, 0, 0};
    qm::VectorsToAngles(f, r, u, back);
    Check(NearEqual(back[2], 0.0f), "a level basis recovers roll 0, not 180");
}

// Head yaw is negated at the boundary: the tracker's +yaw is a turn to the
// RIGHT, and Quake's +yaw turns LEFT.
void TestPositiveHeadYawTurnsTheViewRight() {
    Vec3 f, r, u;
    TrackedBasis(kLevel, 30.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/true, f, r, u);
    float angles[3] = {0, 0, 0};
    qm::VectorsToAngles(f, r, u, angles);
    Check(NearEqual(angles[1], -30.0f, 1e-2f), "+30 head yaw is -30 Quake yaw");
}

// Quake's +pitch looks DOWN, so a head that looks up must produce a negative
// Quake pitch. kPitchTilt is +1 because it rotates the BASIS about the right
// vector, which runs opposite to the Quake angle.
void TestPositiveHeadPitchLooksUp() {
    Vec3 f, r, u;
    TrackedBasis(kLevel, 0.0f, 20.0f, 0.0f, true, f, r, u);
    float angles[3] = {0, 0, 0};
    qm::VectorsToAngles(f, r, u, angles);
    Check(NearEqual(angles[0], -20.0f, 1e-2f), "+20 head pitch is -20 Quake pitch (looking up)");
    Check(f.z > 0.0f, "+head pitch raises the forward vector");
}

void TestPositiveHeadRollTiltsTheViewLeft() {
    Vec3 f, r, u;
    TrackedBasis(kLevel, 0.0f, 0.0f, 15.0f, true, f, r, u);
    float angles[3] = {0, 0, 0};
    qm::VectorsToAngles(f, r, u, angles);
    Check(NearEqual(angles[2], -15.0f, 1e-2f), "+15 head roll is -15 Quake roll");
}

void TestTrackedBasisStaysOrthonormal() {
    const float clean[3] = {12.0f, -47.0f, 3.0f};
    Vec3 f, r, u;
    TrackedBasis(clean, 25.0f, -18.0f, 30.0f, true, f, r, u);
    Check(NearEqual(f.Magnitude(), 1.0f) && NearEqual(r.Magnitude(), 1.0f) &&
              NearEqual(u.Magnitude(), 1.0f),
          "tracked basis vectors stay unit length");
    Check(NearEqual(Vec3::Dot(f, r), 0.0f) && NearEqual(Vec3::Dot(f, u), 0.0f) &&
              NearEqual(Vec3::Dot(r, u), 0.0f),
          "tracked basis vectors stay mutually perpendicular");
}

// Looking straight down, world-locked yaw swings the basis about the world up
// axis the forward vector is already parallel to, so the view direction does
// not move at all - that is the whole point of the mode. Camera-local yaw does
// move it, which is the leaning artifact the mode exists to avoid.
void TestWorldYawLeavesForwardFixedWhenLookingDown() {
    const float lookingDown[3] = {90.0f, 0.0f, 0.0f};
    Vec3 f0, r0, u0;
    qm::AngleVectors(lookingDown, f0, r0, u0);

    Vec3 fw, rw, uw;
    TrackedBasis(lookingDown, 40.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/true, fw, rw, uw);
    Check(NearEqual(fw, f0), "world yaw looking down leaves the view direction fixed");

    Vec3 fl, rl, ul;
    TrackedBasis(lookingDown, 40.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/false, fl, rl, ul);
    Check(!NearEqual(fl, f0, 1e-2f), "camera-local yaw looking down does move the view");
}

// ---- Position axis boundary ------------------------------------------------
//
// The three rotation signs each have a test above. These three pin the sign
// constants only, not render_hook.cpp's use of them, which sits behind engine
// memory no test can reach. It is still the half that goes wrong: a mirrored
// lean looks like it works, it just goes the wrong way.
//
// The lean is built in the horizon-locked basis, so at a level yaw of 0 that is
// forward +x, right -y, up +z.
void TestPositionSignsMoveTheEyeTheWayTheHeadDoes() {
    Vec3 f, r, u;
    qm::HorizonBasis(0.0f, f, r, u);

    // Tracker +x is a head that moves LEFT, and Quake's left is +y.
    const Vec3 left = r * (0.30f * qm::kPosXSign);
    Check(left.y > 0.0f && NearEqual(left.x, 0.0f) && NearEqual(left.z, 0.0f),
          "+x on the tracker moves the eye to Quake's left (+y)");

    // Tracker +y is a head that moves UP, and Quake's up is +z.
    const Vec3 up = u * (0.20f * qm::kPosYSign);
    Check(up.z > 0.0f, "+y on the tracker moves the eye up (+z)");

    // The processor's forward lean is NEGATIVE z, and it must land on the
    // generous LimitZ budget rather than the 0.10 m backward one.
    const Vec3 forward = f * (-0.40f * qm::kPosZSign);
    Check(forward.x > 0.0f, "the processor's negative z leans the eye forward (+x)");

    const Vec3 back = f * (0.10f * qm::kPosZSign);
    Check(back.x < 0.0f, "the processor's positive z leans the eye backward (-x)");
}

// The lean must follow the body, not the aim. Off the view basis, looking at the
// floor and leaning in drives the eye down into it, and Quake's own strafe roll
// tips a vertical head movement sideways every time the player strafes.
void TestHorizonBasisIgnoresPitchAndRoll() {
    Vec3 f, r, u;
    qm::HorizonBasis(0.0f, f, r, u);
    Check(NearEqual(f, Vec3(1.0f, 0.0f, 0.0f)), "horizon basis at yaw 0 is forward +x");
    Check(NearEqual(r, Vec3(0.0f, -1.0f, 0.0f)), "horizon basis at yaw 0 is right -y");
    Check(NearEqual(u, Vec3(0.0f, 0.0f, 1.0f)), "horizon basis up is world +z");

    // A level view at the same yaw agrees with it exactly; a pitched and rolled
    // one at that yaw still gives the same lean basis, which is the point.
    const float pitchedAndRolled[3] = {55.0f, 90.0f, 25.0f};
    Vec3 vf, vr, vu;
    qm::AngleVectors(pitchedAndRolled, vf, vr, vu);
    Vec3 hf, hr, hu;
    qm::HorizonBasis(pitchedAndRolled[1], hf, hr, hu);
    Check(NearEqual(hf, Vec3(0.0f, 1.0f, 0.0f)) && NearEqual(hu, Vec3(0.0f, 0.0f, 1.0f)),
          "horizon basis at yaw 90 is forward +y, up +z, whatever the pitch and roll");
    Check(!NearEqual(hf, vf, 1e-2f),
          "the view basis at that pose is not the lean basis, so pitch cannot tip the lean");
    Check(NearEqual(hf.Magnitude(), 1.0f) && NearEqual(hr.Magnitude(), 1.0f) &&
              NearEqual(Vec3::Dot(hf, hr), 0.0f),
          "horizon basis is orthonormal");
}

// Camera-local yaw turns about the camera's own up vector. Only asserting "it
// moved" would pass for any axis at all, including the wrong one.
void TestCameraLocalYawTurnsAboutTheCameraUp() {
    // Rolled 90 degrees the camera's up vector is (0, -1, 0), which is Quake's
    // RIGHT, so a camera-local yaw there pitches the view instead of turning it.
    // Comparing the two modes at a LEVEL view would prove nothing: the axes
    // coincide there, so both branches are handed the same vector.
    const float rolled[3] = {0.0f, 0.0f, 90.0f};
    Vec3 f1, r1, u1;
    TrackedBasis(rolled, 35.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/false, f1, r1, u1);

    // Written out rather than recomputed through the rotation helper and
    // kYawTurn: an expectation built from the constant under test flips with it
    // and passes either way, so it pins the axis and not the sign. Rolled 90
    // degrees the camera up is (0, -1, 0), so 35 degrees of head yaw swings
    // forward through the x/z plane to (cos 35, 0, -sin 35).
    Check(NearEqual(f1, Vec3(0.81915f, 0.0f, -0.57358f)),
          "camera-local yaw turns the forward vector about the camera's own up axis, "
          "and in the direction the head turned");
}

// ---- Lean clamp geometry ---------------------------------------------------
//
// The mod owns the collision query; core owns the policy. These two functions
// are the whole of the arithmetic between them, and the only other check on
// them is a manual in-game measurement that is expensive to repeat.
void TestLeanClampGeometry() {
    const float standoff = 8.0f;

    // The trace has to see past where the lean stops, or the eye travels the
    // whole lean, arrives against the wall, and only pops back out once the
    // head pushes far enough for the ray itself to cross the surface.
    Check(NearEqual(qm::OverreachedReach(10.0f, standoff), 10.0f + 3.0f * standoff),
          "the trace overreaches the lean by standoff/kMinApproachCos - standoff");
    Check(qm::OverreachedReach(10.0f, 0.0f) == 10.0f,
          "a zero standoff needs no overreach");

    // Head-on, LeanClamp's own skin is the whole standoff, so nothing is folded
    // in and the reported distance is the raw hit distance.
    Check(NearEqual(qm::ReportedDistance(30.0f, 1.0f, standoff), 30.0f),
          "a head-on hit reports the hit distance unchanged");

    // The measured case from .lab/NOTES.md: standoff 8, wall 22.9 units away,
    // lean cut to 11.6 after LeanClamp subtracts its 8-unit skin along the ray.
    Check(NearEqual(qm::ReportedDistance(22.9f, 0.708f, standoff) - standoff, 11.6f, 0.05f),
          "the in-game measured clamp reproduces exactly");

    // An oblique surface costs more along the ray than along its normal, so the
    // reported distance must SHRINK, never grow.
    Check(qm::ReportedDistance(30.0f, 0.5f, standoff) < 30.0f,
          "an oblique hit reports less than the hit distance, not more");
    Check(qm::ReportedDistance(1.0f, 0.3f, standoff) == 0.0f,
          "a correction larger than the hit distance floors at zero rather than going negative");

    // Below kMinApproachCos the correction is capped, so a near-parallel wall
    // does not demand an unbounded overreach.
    Check(qm::ReportedDistance(30.0f, 0.01f, standoff) ==
              qm::ReportedDistance(30.0f, qm::kMinApproachCos, standoff),
          "an approach shallower than kMinApproachCos is capped there");
}

// ---- Projection ------------------------------------------------------------

void TestPointAheadProjectsToCentre() {
    Vec3 f, r, u;
    qm::AngleVectors(kLevel, f, r, u);
    float x = 9.0f, y = 9.0f;
    const auto result = qm::ProjectToNdc(f * 100.0f, f, r, u, 90.0f, 60.0f, x, y);
    Check(result == qm::Projection::Ok, "a point straight ahead projects");
    Check(NearEqual(x, 0.0f) && NearEqual(y, 0.0f), "a point straight ahead is at ndc (0, 0)");
}

void TestFrameEdgeIsNdcOne() {
    Vec3 f, r, u;
    qm::AngleVectors(kLevel, f, r, u);
    const float depth = 100.0f;
    const float half = std::tan(45.0f * qm::kDeg2Rad) * depth;  // 90 degree horizontal fov
    float x = 0.0f, y = 0.0f;
    qm::ProjectToNdc(f * depth + r * half, f, r, u, 90.0f, 60.0f, x, y);
    Check(NearEqual(x, 1.0f), "the right frame edge is ndc x = 1");
}

void TestPointBehindTheEyeIsReported() {
    Vec3 f, r, u;
    qm::AngleVectors(kLevel, f, r, u);
    float x = 0.0f, y = 0.0f;
    Check(qm::ProjectToNdc(f * -50.0f, f, r, u, 90.0f, 60.0f, x, y) == qm::Projection::Behind,
          "a point behind the eye reports Behind");
    Check(qm::ProjectToNdc(f * 1.0f, f, r, u, 90.0f, 60.0f, x, y) == qm::Projection::Behind,
          "a point exactly one unit ahead is still too close to project");
}

void TestUnusableFovIsReportedSeparately() {
    Vec3 f, r, u;
    qm::AngleVectors(kLevel, f, r, u);
    float x = 0.0f, y = 0.0f;
    Check(qm::ProjectToNdc(f * 100.0f, f, r, u, 0.0f, 0.0f, x, y) == qm::Projection::Degenerate,
          "a zero fov reports Degenerate, not Behind");
}

// ---- Reticle litmus tests (AGENTS.md) --------------------------------------
//
// The world point is the same in every case: the clean aim line's impact, which
// head tracking never moves. Only the basis it is projected through changes.

// A square fov so ndc is isotropic and an angle in ndc space is a real angle.
constexpr float kSquareFov = 90.0f;
constexpr float kAimDepth = 200.0f;

// Projects the clean-aim point through the basis a head pose produces.
void ProjectAim(float headYaw, float headPitch, float headRoll, bool worldYaw,
                const float cleanAngles[3], float& ndcX, float& ndcY,
                qm::Projection& outResult) {
    Vec3 cleanF, cleanR, cleanU;
    qm::AngleVectors(cleanAngles, cleanF, cleanR, cleanU);
    const Vec3 aim = cleanF * kAimDepth;

    Vec3 f, r, u;
    TrackedBasis(cleanAngles, headYaw, headPitch, headRoll, worldYaw, f, r, u);
    outResult = qm::ProjectToNdc(aim, f, r, u, kSquareFov, kSquareFov, ndcX, ndcY);
}

void TestPureRollKeepsTheReticleCentred() {
    float x = 9.0f, y = 9.0f;
    qm::Projection result{};
    ProjectAim(0.0f, 0.0f, 25.0f, true, kLevel, x, y, result);
    Check(result == qm::Projection::Ok && NearEqual(x, 0.0f) && NearEqual(y, 0.0f),
          "litmus 1: pure roll leaves the reticle at centre");
}

void TestPurePitchMovesTheReticleVertically() {
    float x = 9.0f, y = 9.0f;
    qm::Projection result{};
    ProjectAim(0.0f, 20.0f, 0.0f, true, kLevel, x, y, result);
    Check(result == qm::Projection::Ok && NearEqual(x, 0.0f),
          "litmus 2: pure pitch does not move the reticle horizontally");
    Check(std::fabs(y) > 0.1f, "litmus 2: pure pitch does move it vertically");
}

// This camera composes roll OUTERMOST (roll is applied last, about the final
// forward vector), so combining pitch with roll must rotate the pure-pitch
// offset about screen centre by exactly the roll angle rather than leaving it
// vertical. Verified in game at .lab/NOTES.md "Reticle".
void TestPitchPlusRollRotatesTheOffsetByTheRollAngle() {
    float px = 0.0f, py = 0.0f;
    float rx = 0.0f, ry = 0.0f;
    qm::Projection a{}, b{};
    const float roll = 30.0f;
    ProjectAim(0.0f, 20.0f, 0.0f, true, kLevel, px, py, a);
    ProjectAim(0.0f, 20.0f, roll, true, kLevel, rx, ry, b);

    const float pitchOnly = std::sqrt(px * px + py * py);
    const float combined = std::sqrt(rx * rx + ry * ry);
    Check(a == qm::Projection::Ok && b == qm::Projection::Ok &&
              NearEqual(pitchOnly, combined, 1e-2f),
          "litmus 3: roll does not change how far the reticle sits from centre");

    // Signed and wrapped, not fabs: getting the roll 180 degrees out of phase
    // between the camera and the projection is the documented failure here, and
    // an unsigned comparison passes for both phases.
    float turned = (std::atan2(ry, rx) - std::atan2(py, px)) * qm::kRad2Deg;
    while (turned > 180.0f) turned -= 360.0f;
    while (turned <= -180.0f) turned += 360.0f;
    Check(NearEqual(turned, -roll, 0.5f),
          "litmus 3: roll turns the pure-pitch offset by exactly the roll angle, "
          "in the direction the camera rolls");
}

void TestWorldYawLookingDownKeepsTheReticleCentred() {
    const float lookingDown[3] = {90.0f, 0.0f, 0.0f};
    float x = 9.0f, y = 9.0f;
    qm::Projection result{};
    ProjectAim(45.0f, 0.0f, 0.0f, /*worldYaw=*/true, lookingDown, x, y, result);
    Check(result == qm::Projection::Ok && NearEqual(x, 0.0f) && NearEqual(y, 0.0f),
          "litmus 4: world yaw looking straight down leaves the reticle at centre");
}

}  // namespace

int RunQuakeMathTests() {
    std::cout << "\nQuake math tests\n";
    TestLevelPoseGivesQuakeWorldBasis();
    TestVectorsToAnglesInvertsAngleVectors();
    TestLevelBasisRecoversZeroRollNotOneEighty();
    TestPositiveHeadYawTurnsTheViewRight();
    TestPositiveHeadPitchLooksUp();
    TestPositiveHeadRollTiltsTheViewLeft();
    TestTrackedBasisStaysOrthonormal();
    TestWorldYawLeavesForwardFixedWhenLookingDown();
    TestPositionSignsMoveTheEyeTheWayTheHeadDoes();
    TestHorizonBasisIgnoresPitchAndRoll();
    TestCameraLocalYawTurnsAboutTheCameraUp();
    TestLeanClampGeometry();
    TestPointAheadProjectsToCentre();
    TestFrameEdgeIsNdcOne();
    TestPointBehindTheEyeIsReported();
    TestUnusableFovIsReportedSeparately();
    TestPureRollKeepsTheReticleCentred();
    TestPurePitchMovesTheReticleVertically();
    TestPitchPlusRollRotatesTheOffsetByTheRollAngle();
    TestWorldYawLookingDownKeepsTheReticleCentred();

    if (g_failures == 0) {
        std::cout << "Quake math tests: all passed\n";
    } else {
        std::cout << "Quake math tests: " << g_failures << " failure(s)\n";
    }
    return g_failures;
}
