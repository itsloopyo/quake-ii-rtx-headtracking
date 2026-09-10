#include "hooks/render_hook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "core/build_profiles.h"
#include "core/mod.h"
#include "engine/fov_zoom.h"
#include "engine/game_state.h"
#include "engine/lean_trace.h"
#include "engine/slot_swap.h"
#include "hooks/reticle.h"
#include "quake_math.h"
#include "cameraunlock/camera/lean_clamp.h"
#include "cameraunlock/camera/zoom_compensation.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/math/vec3.h"

namespace Q2RTXHT {

namespace {

namespace log = cameraunlock::logging;
using cameraunlock::math::Vec3;

using RenderFrameFn = void(*)(void*);

const BuildProfile* g_profile = nullptr;
std::atomic<void*> g_orig{nullptr};

// Quake's world is 8192 units across at most, so this reaches any surface the
// aim line can meet. It is the trace's own far end, not a stand-in convergence
// distance: a trace that reaches it has definitely hit nothing.
constexpr float kAimRange = 8192.0f;

// A member of the refdef the engine is about to render, at the offset the
// matched build pinned. Typed so a viewport int can never be read as a float.
template <typename T>
T ReadMember(const unsigned char* refdef, uint32_t offset) {
    return *reinterpret_cast<const T*>(refdef + offset);
}

// What the injection did this frame, for the diagnostic line.
struct InjectionResult {
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    Vec3 leanOffset;
    bool leanApplied = false;
};

// ---- Lean clamp ------------------------------------------------------------

cameraunlock::camera::LeanClamp g_lean;

// A jump in the eye position big enough to mean a teleporter or a level load
// rather than walking. A rocket jump or a frame hitch can cross it too, which
// costs nothing: Reset() only drops the current allowance and tightening is
// never eased, so a false positive is one frame of release pacing. Missing a
// real cut is the expensive direction, because the destination room's first
// fifth of a second of lean would still be rationed by the wall just left.
constexpr float kCameraCutUnits = 128.0f;
float g_lastCleanOrg[3] = {0.0f, 0.0f, 0.0f};
bool g_haveLastCleanOrg = false;
bool g_leanContact = false;
bool g_leanQueryFailed = false;
bool g_aimTraceWarned = false;

void ReportLeanState(bool contact, bool queryFailed) {
    if (contact == g_leanContact && queryFailed == g_leanQueryFailed) return;
    g_leanContact = contact;
    g_leanQueryFailed = queryFailed;
    log::Line("[lean] contact=%d queryFailed=%d", contact ? 1 : 0, queryFailed ? 1 : 0);
}

void ResetLeanOnCameraCut(const float cleanOrg[3]) {
    if (g_haveLastCleanOrg) {
        const Vec3 step{ cleanOrg[0] - g_lastCleanOrg[0], cleanOrg[1] - g_lastCleanOrg[1],
                         cleanOrg[2] - g_lastCleanOrg[2] };
        if (Vec3::Dot(step, step) > kCameraCutUnits * kCameraCutUnits) g_lean.Reset();
    }
    std::memcpy(g_lastCleanOrg, cleanOrg, sizeof(g_lastCleanOrg));
    g_haveLastCleanOrg = true;
}

// ---- Diagnostics -----------------------------------------------------------
//
// Dense at first, where install-time faults show, then a wall-clock sample.
// The steady sample is on a timer rather than a frame count because a frame
// count makes the log's size a function of the player's framerate: at 600
// frames a 240Hz machine writes eight times the lines a 30Hz one does for the
// same session, and the ~20 lines that say what the mod actually did end up
// buried under thousands of identical-shaped ones. The lean clamp still reports
// every contact and query-failure transition the instant it happens; this
// sample only has to separate "the sweep runs and the room is open" from "the
// sweep is not running", which one line per half minute does.
constexpr uint64_t kBurstFrames = 5;
constexpr uint64_t kSteadyIntervalMs = 30000;

uint64_t g_frameCount = 0;
uint64_t g_lastSteadyLogMs = 0;

void DiagnosticLog(const reticle::FrameView& frame, float zoomFactor,
                   const float cleanAngles[3], const float cleanOrg[3],
                   const InjectionResult& injected) {
    ++g_frameCount;
    if (g_frameCount > kBurstFrames) {
        const uint64_t nowMs = GetTickCount64();
        if (nowMs - g_lastSteadyLogMs < kSteadyIntervalMs) return;
        g_lastSteadyLogMs = nowMs;
    }

    float px = 0.0f, py = 0.0f;
    const bool aimOnScreen = reticle::Target(frame, px, py) == reticle::State::Offset;
    log::Line("[view] rect=%d,%d %dx%d fov=%.2f/%.2f zoom=%.4f clean=(p%.2f y%.2f r%.2f) "
              "org=(%.1f,%.1f,%.1f) | head=(y%.2f p%.2f r%.2f) lean=%d(%.1f,%.1f,%.1f) "
              "contact=%d qfail=%d aim=%d(%.1f,%.1f,%.1f) px=%d(%.1f,%.1f)",
              frame.rx, frame.ry, frame.rw, frame.rh, frame.fovX, frame.fovY,
              zoomFactor,
              cleanAngles[0], cleanAngles[1], cleanAngles[2],
              cleanOrg[0], cleanOrg[1], cleanOrg[2],
              injected.yaw, injected.pitch, injected.roll,
              injected.leanApplied ? 1 : 0,
              injected.leanOffset.x, injected.leanOffset.y, injected.leanOffset.z,
              g_lean.InContact() ? 1 : 0, g_lean.LastQueryFailed() ? 1 : 0,
              frame.aimValid ? 1 : 0, frame.aim[0], frame.aim[1], frame.aim[2],
              aimOnScreen ? 1 : 0, px, py);
}

// ---- Per-frame injection ---------------------------------------------------

// The viewport and field of view the frame is being rendered with. Also the
// basis the reticle projection divides by, so it is captured whether or not
// anything is injected.
void CaptureFrameGeometry(const unsigned char* refdef, reticle::FrameView& frame) {
    frame.rx = ReadMember<int>(refdef, g_profile->offRectX);
    frame.ry = ReadMember<int>(refdef, g_profile->offRectY);
    frame.rw = ReadMember<int>(refdef, g_profile->offWidth);
    frame.rh = ReadMember<int>(refdef, g_profile->offHeight);
    frame.fovX = ReadMember<float>(refdef, g_profile->offFovX);
    frame.fovY = ReadMember<float>(refdef, g_profile->offFovY);
}

// The reticle has to sit on the point the shot reaches, and the shot leaves the
// CLEAN eye along the CLEAN aim line - so the depth is traced fresh every frame
// rather than assumed.
void CaptureCleanAimPoint(const float cleanOrg[3], const Vec3& cleanF,
                          reticle::FrameView& frame) {
    const float aimEnd[3] = { cleanOrg[0] + cleanF.x * kAimRange,
                              cleanOrg[1] + cleanF.y * kAimRange,
                              cleanOrg[2] + cleanF.z * kAimRange };
    lean_trace::Result aim;
    if (lean_trace::TraceLine(cleanOrg, aimEnd, lean_trace::kMaskShot, aim)) {
        std::memcpy(frame.aim, aim.endpos, sizeof(frame.aim));
        frame.aimValid = !aim.startsolid;
    } else if (!g_aimTraceWarned) {
        g_aimTraceWarned = true;
        log::Line("[aim] collision model unavailable; the reticle is left uncompensated");
    }
}

// The lean the world leaves room for, in Quake units along the horizon-locked
// basis at the clean view yaw. `applied` comes back false when the tracker is
// sending rotation only, in which case the clamp's allowance is dropped rather
// than left stale.
Vec3 ComputeLeanOffset(const Config& cfg, Mod& mod, const float cleanOrg[3],
                       float cleanYaw, float zoomFactor, bool& applied) {
    applied = false;

    float px, py, pz;
    if (!mod.GetPositionOffset(px, py, pz)) {
        g_lean.Reset();
        ReportLeanState(false, false);
        return Vec3{};
    }

    // A lean slides the picture too, and linearly, so it carries the same zoom
    // factor. But the travel limits that keep the eye inside the player's body
    // were applied upstream in metres, BEFORE this multiply, so scaling alone
    // would stop them bounding the excursion they exist to bound: at the
    // shipped UnitsPerMeter of 40 and the widest factor fov_zoom allows, a full
    // forward lean would reach 157 Quake units. The factor may redistribute
    // screen displacement; it may not move the eye further out than the limits
    // authorise. So the scaled pose is held inside the same box the processor
    // clamped it to - per axis, and keeping z's asymmetry, where the tracker's
    // negative z is the forward lean and leaning back is allowed a quarter of
    // leaning in. At factor 1.0 every axis is already inside and this is a
    // no-op.
    px = std::clamp(px * zoomFactor, -cfg.posLimitX, cfg.posLimitX);
    py = std::clamp(py * zoomFactor, -cfg.posLimitY, cfg.posLimitY);
    pz = std::clamp(pz * zoomFactor, -cfg.posLimitZ, cfg.posLimitZBack);

    const float upm = cfg.positionUnitsPerMeter;
    const float xs = quake_math::kPosXSign * upm;
    const float ys = quake_math::kPosYSign * upm;
    const float zs = quake_math::kPosZSign * upm;

    // Horizon-locked, not the view basis: a lean moves the body, and the body
    // does not pitch with the aim or tip with Quake's strafe roll. Off this
    // basis, looking at the floor and leaning in drives the eye down into it.
    Vec3 leanF, leanR, leanU;
    quake_math::HorizonBasis(cleanYaw, leanF, leanR, leanU);
    const Vec3 desired = leanR * (px * xs) + leanU * (py * ys) + leanF * (pz * zs);

    const Vec3 offset = g_lean.Apply(Vec3(cleanOrg[0], cleanOrg[1], cleanOrg[2]),
                                     desired, mod.LastDeltaTime(),
                                     cfg.collisionEnabled ? &lean_trace::Query : nullptr,
                                     nullptr);
    applied = true;
    ReportLeanState(g_lean.InContact(), g_lean.LastQueryFailed());
    return offset;
}

// Writes the head-tracked basis and eye into the refdef the engine is about to
// render, and fills in the frame state the crosshair reprojection reads.
InjectionResult ApplyTracking(Mod& mod, const float cleanAngles[3], const float cleanOrg[3],
                              float zoomFactor, float* viewangles, float* vieworg,
                              reticle::FrameView& frame) {
    InjectionResult out;
    const Config& cfg = mod.GetConfig();

    Vec3 cleanF, cleanR, cleanU;
    quake_math::AngleVectors(cleanAngles, cleanF, cleanR, cleanU);
    Vec3 f = cleanF, r = cleanR, u = cleanU;

    // Into locals, not straight into `out`: the getter writes its cached pose
    // whether or not it is valid, and the diagnostic line reads `out`. Leaving
    // the stale pose there had the log report a head angle on frames where
    // nothing was rotated by one.
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    const bool rotated = mod.GetProcessedRotation(yaw, pitch, roll);
    if (rotated) {
        // Yaw and pitch slide the picture across the frame, so both are
        // rescaled to displace it by as much as they did at the player's own
        // FOV. Roll spins the picture about the view axis by its own angle at
        // every FOV there is, so scaling it would only flatten a head tilt the
        // player is holding.
        out.yaw = cameraunlock::camera::ScaleAngleForZoom(yaw, zoomFactor);
        out.pitch = cameraunlock::camera::ScaleAngleForZoom(pitch, zoomFactor);
        out.roll = roll;
        quake_math::RotateBasisByHeadPose(out.yaw, out.pitch, out.roll,
                                          mod.WorldSpaceYaw(), f, r, u);
    }

    out.leanOffset =
        ComputeLeanOffset(cfg, mod, cleanOrg, cleanAngles[1], zoomFactor, out.leanApplied);

    if (!rotated && !out.leanApplied) return out;

    const float eye[3] = { cleanOrg[0] + out.leanOffset.x,
                           cleanOrg[1] + out.leanOffset.y,
                           cleanOrg[2] + out.leanOffset.z };

    CaptureCleanAimPoint(cleanOrg, cleanF, frame);

    if (rotated) {
        quake_math::VectorsToAngles(f, r, u, viewangles);
    }
    std::memcpy(vieworg, eye, sizeof(eye));

    std::memcpy(frame.eye, eye, sizeof(eye));
    frame.f = f;
    frame.r = r;
    frame.u = u;
    frame.applied = true;
    return out;
}

void Detour(void* fd) {
    Mod& mod = Mod::Instance();

    // Drive the pipeline once per rendered frame regardless of gameplay state,
    // so smoothing and interpolation timing stays continuous.
    mod.UpdateTracking();

    // Not null-checked: slot_swap release-stores this before it writes the
    // detour into the slot, so any thread that got here has already observed
    // the store that put it here.
    auto orig = reinterpret_cast<RenderFrameFn>(g_orig.load(std::memory_order_acquire));

    auto* bytes = static_cast<unsigned char*>(fd);
    auto* viewangles = reinterpret_cast<float*>(bytes + g_profile->offViewAngles);
    auto* vieworg = reinterpret_cast<float*>(bytes + g_profile->offViewOrg);

    float cleanAngles[3], cleanOrg[3];
    std::memcpy(cleanAngles, viewangles, sizeof(cleanAngles));
    std::memcpy(cleanOrg, vieworg, sizeof(cleanOrg));

    reticle::FrameView frame;
    CaptureFrameGeometry(bytes, frame);
    const float zoomFactor =
        fov_zoom::Update({ frame.fovX, frame.fovY, frame.rw, frame.rh });

    ResetLeanOnCameraCut(cleanOrg);

    InjectionResult injected;
    if (mod.IsEnabled() && game_state::IsInGameplay()) {
        injected =
            ApplyTracking(mod, cleanAngles, cleanOrg, zoomFactor, viewangles, vieworg, frame);
    } else {
        // A menu, a loading screen or a closed gameplay gate must not carry the
        // previous room's allowance into the next lean.
        g_lean.Reset();
        ReportLeanState(false, false);
    }
    reticle::Publish(frame);

    DiagnosticLog(frame, zoomFactor, cleanAngles, cleanOrg, injected);

    orig(fd);

    // fd is &cl.refdef, which outlives the frame. Everything downstream of the
    // renderer - sound placement, the next frame's deltas - has to see the
    // camera the game placed, not the one the player looked through.
    std::memcpy(viewangles, cleanAngles, sizeof(cleanAngles));
    std::memcpy(vieworg, cleanOrg, sizeof(cleanOrg));
}

}  // namespace

bool InstallRenderHook(void* moduleBase, const BuildProfile& profile) {
    g_profile = &profile;

    game_state::Init(moduleBase, profile);
    lean_trace::Init(moduleBase, profile);
    fov_zoom::Init(moduleBase, profile);

    const Config& cfg = Mod::Instance().GetConfig();
    cameraunlock::camera::LeanClampSettings lean;
    lean.skin = cfg.collisionStandoff;
    lean.release_smoothing = cfg.collisionReleaseSmoothing;
    g_lean.SetSettings(lean);
    lean_trace::SetStandoff(cfg.collisionStandoff);
    log::Line("[lean] collision clamp %s (standoff=%.1f units, release=%.2f)",
              cfg.collisionEnabled ? "enabled" : "disabled",
              cfg.collisionStandoff, cfg.collisionReleaseSmoothing);

    void** slot = reinterpret_cast<void**>(
        static_cast<unsigned char*>(moduleBase) + profile.rvaRRenderFrame);
    if (!slot_swap::BeginAsync(moduleBase, slot, reinterpret_cast<void*>(&Detour), g_orig,
                               "render_hook", "R_RenderFrame")) {
        return false;
    }

    // The swap itself happens on that thread, whenever R_Init gets around to
    // populating the slot, so say here that the wait has started: without this
    // line an install that later times out and one that succeeded a second
    // afterwards look identical up to the point the first one gives up.
    log::Line("[render_hook] waiting for R_Init to populate R_RenderFrame");
    return true;
}

}  // namespace Q2RTXHT
