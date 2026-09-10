#include "engine/lean_trace.h"

#include "core/build_profiles.h"
#include "quake_math.h"

namespace Q2RTXHT {
namespace lean_trace {

namespace {

// trace_t as the Quake II game ABI has declared it since 1997, confirmed
// against q2rtx.pdb: allsolid 0, startsolid 4, fraction 8, endpos 12,
// plane 24 (cplane_t, normal first), surface 48, contents 56, ent 64.
struct Q2Trace {
    int   allsolid;
    int   startsolid;
    float fraction;
    float endpos[3];
    float planeNormal[3];
    float planeDist;
    unsigned char planeType;
    unsigned char planeSignbits;
    unsigned char planePad[2];
    void* surface;
    int   contents;
    int   pad;
    void* ent;
};
static_assert(sizeof(Q2Trace) == 72, "trace_t layout does not match the shipped build");

using ClTraceFn = void(*)(Q2Trace*, const float*, const float*, const float*,
                          const float*, int);

ClTraceFn g_clTrace = nullptr;
void**    g_bspSlot = nullptr;
float     g_standoff = 4.0f;

}  // namespace

void Init(void* moduleBase, const BuildProfile& profile) {
    auto* base = static_cast<unsigned char*>(moduleBase);
    g_clTrace = reinterpret_cast<ClTraceFn>(base + profile.rvaClTrace);
    g_bspSlot = reinterpret_cast<void**>(base + profile.rvaCl + profile.offClBsp);
}

void SetStandoff(float units) {
    g_standoff = units;
}

bool TraceLine(const float start[3], const float end[3], int contentmask, Result& out) {
    if (!g_clTrace || !g_bspSlot || *g_bspSlot == nullptr) {
        return false;
    }

    const float zero[3] = {0.0f, 0.0f, 0.0f};
    Q2Trace tr = {};
    g_clTrace(&tr, start, zero, zero, end, contentmask);

    out.startsolid = tr.startsolid != 0 || tr.allsolid != 0;
    out.fraction = tr.fraction;
    out.hit = tr.fraction < 1.0f || out.startsolid;
    for (int i = 0; i < 3; ++i) {
        out.endpos[i] = tr.endpos[i];
        out.normal[i] = tr.planeNormal[i];
    }
    return true;
}

cameraunlock::camera::LeanObstruction Query(void*,
                                            const cameraunlock::math::Vec3& start,
                                            const cameraunlock::math::Vec3& direction,
                                            float maxDistance) {
    cameraunlock::camera::LeanObstruction obstruction;

    const float reach = quake_math::OverreachedReach(maxDistance, g_standoff);

    const float from[3] = { start.x, start.y, start.z };
    const float to[3] = { start.x + direction.x * reach,
                          start.y + direction.y * reach,
                          start.z + direction.z * reach };

    Result r;
    if (!TraceLine(from, to, kMaskSolid, r)) {
        return obstruction;  // queried stays false: no map, no answer
    }

    obstruction.queried = true;
    if (!r.hit) {
        return obstruction;
    }

    obstruction.blocked = true;
    if (r.startsolid) {
        obstruction.distance = 0.0f;
        return obstruction;
    }

    const float approach = -(direction.x * r.normal[0] + direction.y * r.normal[1] +
                             direction.z * r.normal[2]);
    obstruction.distance =
        quake_math::ReportedDistance(r.fraction * reach, approach, g_standoff);
    return obstruction;
}

}  // namespace lean_trace
}  // namespace Q2RTXHT
