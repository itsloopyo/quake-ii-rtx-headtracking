#pragma once

#include "cameraunlock/math/vec3.h"

// Quake II's own angle conventions, the tracker-to-Quake axis boundary, the
// perspective projection the reticle is placed with, and the lean clamp's
// approach-angle geometry.
//
// Everything here is pure: no engine memory, no globals, no logging, no Windows.
// That is what lets tests/quake_math_tests.cpp exercise it directly, which
// matters because a sign error in this file is invisible until a player reports
// the whole frame rendering upside down.
namespace Q2RTXHT {
namespace quake_math {

using cameraunlock::math::Vec3;

constexpr float kDeg2Rad = 0.01745329251994329577f;
constexpr float kRad2Deg = 57.29577951308232087680f;

// ---- Tracker -> Quake axis mapping -----------------------------------------
//
// Every sign correction between the tracker frame and Quake lives here, at the
// engine boundary, and nowhere else. It is deliberately not expressed as an INI
// Invert* default: the position processor applies inversion BEFORE the
// asymmetric Z clamp, so an InvertZ used to flip the engine convention silently
// moves the generous LimitZ (0.40m) allowance onto the backward lean and leaves
// LimitZBack (0.10m) for leaning in.
//
// Quake II and Source share AngleVectors exactly - same basis, same angle order,
// same signs - so these are the conversions half-life-2-headtracking verified in
// game rather than a re-derivation from handedness:
//
//   yaw   > 0 = head turns right    Quake yaw   > 0 = turn left    -> negate
//   pitch > 0 = head looks up       Quake pitch > 0 = look down    -> negate
//   roll  > 0 = head tilts left     Quake roll  > 0 = tilt right   -> negate
//   x     > 0 = head moves left     Quake right vector             -> negate
//   y     > 0 = head moves up       Quake up vector                -> as-is
//   z     < 0 = head leans forward  Quake forward vector           -> negate
//
// The three rotation constants are rotations of the view BASIS, not additions to
// the Euler angles, so a sign reads opposite to the Quake-angle delta wherever
// the two run the other way: a positive rotation about the right vector tilts
// the view UP, which is a negative Quake pitch delta.
constexpr float kYawTurn   = -1.0f;  // about the yaw axis, + turns the view left
constexpr float kPitchTilt =  1.0f;  // about the right vector, + tilts the view up
constexpr float kRollTilt  = -1.0f;  // about the forward vector, + rolls up toward screen right

constexpr float kPosXSign = -1.0f;
constexpr float kPosYSign =  1.0f;
constexpr float kPosZSign = -1.0f;

/// Quake's AngleVectors: `angles` is [PITCH, YAW, ROLL] in degrees.
void AngleVectors(const float angles[3], Vec3& f, Vec3& r, Vec3& u);

/// Inverse of AngleVectors: recovers [PITCH, YAW, ROLL] from an orthonormal
/// basis. Roll comes out of r.z = -sin(roll)*cos(pitch) and
/// u.z = cos(roll)*cos(pitch), so it is atan2(-r.z, u.z). Negating both
/// arguments instead returns roll + 180 for every pose, a level one included,
/// and renders the whole frame upside down.
void VectorsToAngles(const Vec3& f, const Vec3& r, const Vec3& u, float outAngles[3]);

/// The horizon-locked basis at `yawDegrees`: forward flattened onto the world
/// plane, right beside it, up along Quake's +Z. A lean is applied in this basis
/// rather than in the view basis so it follows the body rather than the aim -
/// otherwise looking at the floor and leaning forward drives the eye down into
/// it, and Quake's own strafe roll (cl_rollangle) tips a vertical head movement
/// sideways by a couple of degrees every time the player strafes.
void HorizonBasis(float yawDegrees, Vec3& f, Vec3& r, Vec3& u);

/// Rotates the clean view basis in `f`/`r`/`u` by the head pose, in place:
/// intrinsic yaw -> pitch -> roll about the current basis, with
/// `worldSpaceYaw` swinging the yaw about Quake's world up (+Z) instead of the
/// camera's own up. The axis constants above are applied here, so callers pass
/// the pose as the tracker sends it. The caller supplies the basis (from
/// AngleVectors) because it needs the clean one anyway - for the lean and the
/// aim trace - and deriving it twice from the same angles is wasted trigonometry.
void RotateBasisByHeadPose(float headYaw, float headPitch, float headRoll,
                           bool worldSpaceYaw, Vec3& f, Vec3& r, Vec3& u);

/// Outcome of ProjectToNdc. `Behind` and `Degenerate` are kept apart because
/// they call for opposite handling: a point behind the eye has no on-screen
/// position at all, while an unusable FOV means the projection itself could not
/// run and the caller has nothing to correct with.
enum class Projection {
    Ok,          ///< ndcX / ndcY written. Still off-screen when |ndc| > 1.
    Behind,      ///< at or behind the render eye.
    Degenerate,  ///< the frame's FOV is too narrow to project through.
};

/// A world point at `rel` from the render eye, projected through the view basis
/// into normalised device coordinates (x right, y up, +-1 at the frame edges).
/// `fovXDeg` / `fovYDeg` are the full angles the frame is rendered with.
///
/// The arithmetic is core's ProjectAimToNdc. This adds the tri-state, which core
/// has no room for behind a bool and which the caller needs because Behind and
/// Degenerate drive opposite reticle states, and it gates on Quake units and
/// whole-angle degrees rather than on core's engine-neutral thresholds - both
/// of which are stricter, so nothing that passes here is rejected there.
Projection ProjectToNdc(const Vec3& rel, const Vec3& f, const Vec3& r, const Vec3& u,
                        float fovXDeg, float fovYDeg, float& ndcX, float& ndcY);

// ---- Lean clamp geometry ---------------------------------------------------
//
// The mod owns the collision query; core's LeanClamp owns the policy. These two
// convert between the distance along the lean ray that a trace reports and the
// clearance the clamp has to leave along the surface NORMAL, which is what the
// standoff is measured in.

/// Smallest approach cosine the correction is evaluated at. A surface met more
/// obliquely than this would demand an unbounded overreach, so it is capped. At
/// the cap the correction is still exact - the clearance is the full standoff -
/// and past it the eye is held closer in proportion to the approach: at a
/// cosine of 0.05, a fifth of the standoff.
///
/// The cosine is measured against the surface NORMAL, and it is the LEAN's
/// angle, not the view's. Decoupled look and aim leaves those two free of each
/// other, so a wall met this obliquely by the lean can still be square in
/// frame, which is why the shortfall matters at all.
constexpr float kMinApproachCos = 0.25f;

/// How far a line trace must run to see the surface the lean is about to come
/// to rest against. A ray that stops where the lean stops cannot see it at all.
float OverreachedReach(float maxDistance, float standoff);

/// The obstruction distance to report to LeanClamp, which subtracts its own skin
/// along the ray. Holding the eye `standoff` off the surface along the normal
/// costs standoff/cos along the ray, so that difference is folded in here.
/// `approachCos` is clamped to kMinApproachCos before use.
float ReportedDistance(float hitDistance, float approachCos, float standoff);

}  // namespace quake_math
}  // namespace Q2RTXHT
