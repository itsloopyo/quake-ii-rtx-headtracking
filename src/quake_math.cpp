#include "quake_math.h"

#include <cmath>

#include "cameraunlock/rendering/aim_ndc_projection.h"

namespace Q2RTXHT {
namespace quake_math {

namespace {

// Nearer than this to the render eye and the perspective divide stops meaning
// anything: one Quake unit is roughly an inch, so no aim point the player can
// see is ever this close.
constexpr float kMinDepthUnits = 1.0f;

// Half-angle tangent below which the frame has no usable extent to divide by.
constexpr float kMinFovTangent = 1e-4f;

// Rodrigues with the angle's cosine and sine already in hand, so a rotation
// applied to two or three basis vectors evaluates them once instead of per
// vector.
Vec3 Rodrigues(const Vec3& v, const Vec3& k, float c, float s) {
    const Vec3 cross = Vec3::Cross(k, v);
    const float axial = Vec3::Dot(k, v) * (1.0f - c);
    return Vec3{
        v.x * c + cross.x * s + k.x * axial,
        v.y * c + cross.y * s + k.y * axial,
        v.z * c + cross.z * s + k.z * axial,
    };
}

}  // namespace

void AngleVectors(const float angles[3], Vec3& f, Vec3& r, Vec3& u) {
    const float sp = std::sin(angles[0] * kDeg2Rad), cp = std::cos(angles[0] * kDeg2Rad);
    const float sy = std::sin(angles[1] * kDeg2Rad), cy = std::cos(angles[1] * kDeg2Rad);
    const float sr = std::sin(angles[2] * kDeg2Rad), cr = std::cos(angles[2] * kDeg2Rad);

    f.x = cp * cy;
    f.y = cp * sy;
    f.z = -sp;
    r.x = (-sr * sp * cy + cr * sy);
    r.y = (-sr * sp * sy - cr * cy);
    r.z = -sr * cp;
    u.x = (cr * sp * cy + sr * sy);
    u.y = (cr * sp * sy - sr * cy);
    u.z = cr * cp;
}

void VectorsToAngles(const Vec3& f, const Vec3& r, const Vec3& u, float outAngles[3]) {
    outAngles[1] = std::atan2(f.y, f.x) * kRad2Deg;                        // yaw
    const float fxy = std::sqrt(f.x * f.x + f.y * f.y);
    outAngles[0] = std::atan2(-f.z, fxy) * kRad2Deg;                       // pitch
    outAngles[2] = std::atan2(-r.z, u.z) * kRad2Deg;                       // roll
}

void HorizonBasis(float yawDegrees, Vec3& f, Vec3& r, Vec3& u) {
    const float sy = std::sin(yawDegrees * kDeg2Rad), cy = std::cos(yawDegrees * kDeg2Rad);
    f = Vec3{ cy, sy, 0.0f };
    r = Vec3{ sy, -cy, 0.0f };
    u = Vec3{ 0.0f, 0.0f, 1.0f };
}

void RotateBasisByHeadPose(float headYaw, float headPitch, float headRoll,
                           bool worldSpaceYaw, Vec3& f, Vec3& r, Vec3& u) {
    // A copy, so world-locked yaw is unaffected by u being rotated below.
    const Vec3 yawAxis = worldSpaceYaw ? Vec3{ 0.0f, 0.0f, 1.0f } : u;
    const float yawRad = kYawTurn * headYaw * kDeg2Rad;
    const float cy = std::cos(yawRad), sy = std::sin(yawRad);
    f = Rodrigues(f, yawAxis, cy, sy);
    r = Rodrigues(r, yawAxis, cy, sy);
    u = Rodrigues(u, yawAxis, cy, sy);

    const float pitchRad = kPitchTilt * headPitch * kDeg2Rad;
    const float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
    f = Rodrigues(f, r, cp, sp);
    u = Rodrigues(u, r, cp, sp);

    const float rollRad = kRollTilt * headRoll * kDeg2Rad;
    const float cr = std::cos(rollRad), sr = std::sin(rollRad);
    r = Rodrigues(r, f, cr, sr);
    u = Rodrigues(u, f, cr, sr);
}

Projection ProjectToNdc(const Vec3& rel, const Vec3& f, const Vec3& r, const Vec3& u,
                        float fovXDeg, float fovYDeg, float& ndcX, float& ndcY) {
    if (Vec3::Dot(rel, f) <= kMinDepthUnits) return Projection::Behind;

    const float tanHx = std::tan(fovXDeg * 0.5f * kDeg2Rad);
    const float tanHy = std::tan(fovYDeg * 0.5f * kDeg2Rad);
    if (tanHx < kMinFovTangent || tanHy < kMinFovTangent) return Projection::Degenerate;

    const float aim[3] = { rel.x, rel.y, rel.z };
    const float fwd[3] = { f.x, f.y, f.z };
    const float right[3] = { r.x, r.y, r.z };
    const float up[3] = { u.x, u.y, u.z };
    return cameraunlock::rendering::ProjectAimToNdc(aim, fwd, right, up, tanHx, tanHy,
                                                    ndcX, ndcY)
               ? Projection::Ok
               : Projection::Degenerate;
}

float OverreachedReach(float maxDistance, float standoff) {
    return maxDistance + standoff * (1.0f / kMinApproachCos - 1.0f);
}

float ReportedDistance(float hitDistance, float approachCos, float standoff) {
    const float approach = approachCos < kMinApproachCos ? kMinApproachCos : approachCos;
    const float distance = hitDistance - standoff * (1.0f / approach - 1.0f);
    return distance > 0.0f ? distance : 0.0f;
}

}  // namespace quake_math
}  // namespace Q2RTXHT
