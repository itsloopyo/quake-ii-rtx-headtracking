#pragma once

#include "cameraunlock/camera/lean_clamp.h"
#include "cameraunlock/math/vec3.h"

namespace Q2RTXHT {

struct BuildProfile;

// The engine half of the positional-lean clamp, and the aim raycast the
// reticle projection needs. Both run through the client's own collision model
// (CL_Trace: world brushes plus the client's solid entities, local player
// excluded), so they agree with what the server will hit.
namespace lean_trace {

// Quake content bits. MASK_SOLID stops the eye at level geometry and brush
// model entities (doors, platforms); MASK_SHOT is what a hitscan weapon uses
// and additionally catches monsters and corpses.
constexpr int kMaskSolid = 1 | 2;                          // SOLID | WINDOW
constexpr int kMaskShot  = 1 | 2 | 0x2000000 | 0x4000000;  // + MONSTER | DEADMONSTER

struct Result {
    bool  hit = false;         // something was struck before the end point
    bool  startsolid = false;  // the start point was already inside geometry
    float fraction = 1.0f;
    float endpos[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
};

// Resolves CL_Trace and the cl.bsp slot from the matched build. Traces report
// unavailable until this has run and a map is loaded.
void Init(void* moduleBase, const BuildProfile& profile);

// Point trace. Returns false when no map is loaded, which is a query that could
// not run rather than a clear path - callers must not read it as either.
bool TraceLine(const float start[3], const float end[3], int contentmask, Result& out);

// How far off a surface the clamp holds the eye, in Quake units. Kept here as
// well as in LeanClampSettings because the query has to overreach by it.
void SetStandoff(float units);

// cameraunlock::camera::LeanQueryFn. `context` is unused: the collision model
// is a client global, not a per-camera object.
cameraunlock::camera::LeanObstruction Query(void* context,
                                            const cameraunlock::math::Vec3& start,
                                            const cameraunlock::math::Vec3& direction,
                                            float maxDistance);

}  // namespace lean_trace
}  // namespace Q2RTXHT
