#include "engine/fov_zoom.h"

#include <cmath>

#include "core/build_profiles.h"
#include "engine/cvar.h"
#include "quake_math.h"
#include "cameraunlock/camera/zoom_compensation.h"
#include "cameraunlock/logging/file_log.h"

namespace Q2RTXHT {
namespace fov_zoom {

namespace {

namespace log = cameraunlock::logging;

const unsigned char* g_moduleBase = nullptr;
const BuildProfile* g_profile = nullptr;

float g_loggedLiveFov = -1.0f;
float g_loggedBaseFov = -1.0f;
bool g_fovUnreadableLogged = false;
bool g_zoomClampLogged = false;

// ClientUserinfoChanged's own clamp on the "fov" userinfo.
constexpr int kFovClampFloor = 1;
constexpr int kFovClampFloorReplacement = 90;
constexpr int kFovClampCeiling = 160;

// Outside this band the value is not a field of view the game could be
// rendering, so it is not a denominator either.
constexpr float kMinUsableFovDegrees = 1.0f;
constexpr float kMaxUsableFovDegrees = 179.0f;

// tan(80)/tan(30): the widest ratio between two field-of-view values the game's
// own 60-160 menu offers, either way round. A sanity bound rather than a tight
// one. The "fov" cvar is settable from the console well outside that menu and it
// is the denominator, so `fov 2` against the 90 the game forces for the
// intermission gives a factor of 57, which would turn a 5 degree head turn into
// 89 degrees of engine yaw and a 0.30 m lean into 684 Quake units.
constexpr float kMaxZoomFactor = 9.8229f;
constexpr float kMinZoomFactor = 1.0f / kMaxZoomFactor;

// The "fov" cvar under the game's own clamp. 0 means the cvar is not registered
// yet.
float BaseFovDegrees() {
    const int* fovSlot = engine::CvarInteger(
        reinterpret_cast<void* const*>(g_moduleBase + g_profile->rvaInfoFov),
        g_profile->offCvarInteger);
    if (!fovSlot) return 0.0f;
    const int fov = *fovSlot;
    if (fov < kFovClampFloor) return static_cast<float>(kFovClampFloorReplacement);
    if (fov > kFovClampCeiling) return static_cast<float>(kFovClampCeiling);
    return static_cast<float>(fov);
}

bool FovUsable(float deg) {
    return deg > kMinUsableFovDegrees && deg < kMaxUsableFovDegrees;
}

float LiveFovDegrees() {
    return *reinterpret_cast<const float*>(g_moduleBase + g_profile->rvaCl +
                                           g_profile->offClFovX);
}

}  // namespace

void Init(void* moduleBase, const BuildProfile& profile) {
    g_moduleBase = static_cast<const unsigned char*>(moduleBase);
    g_profile = &profile;
}

float Update(const RenderedFrame& frame) {
    const float live = LiveFovDegrees();
    const float base = BaseFovDegrees();

    if (!FovUsable(live) || !FovUsable(base)) {
        if (!g_fovUnreadableLogged) {
            g_fovUnreadableLogged = true;
            log::Line("[fov] live=%.2f base=%.2f is not a usable pair; "
                      "no zoom compensation applied", live, base);
        }
        return 1.0f;
    }
    g_fovUnreadableLogged = false;

    float factor = cameraunlock::camera::FovZoomFactor(
        std::tan(live * 0.5f * quake_math::kDeg2Rad),
        std::tan(base * 0.5f * quake_math::kDeg2Rad));

    if (factor < kMinZoomFactor || factor > kMaxZoomFactor) {
        const float raw = factor;
        factor = factor < kMinZoomFactor ? kMinZoomFactor : kMaxZoomFactor;
        // Latched until the factor comes back inside the band, not re-armed
        // every usable frame: a console `fov 2` holds the clamp for as long as
        // it is set, and each line here is flushed through to disk, so re-arming
        // wrote one per rendered frame for the duration.
        if (!g_zoomClampLogged) {
            g_zoomClampLogged = true;
            log::Line("[fov] live=%.2f base=%.2f gives a zoom factor of %.4f, wider than "
                      "the game's own 60-160 menu spans; held at %.4f",
                      live, base, raw, factor);
        }
    } else {
        g_zoomClampLogged = false;
    }

    if (std::fabs(live - g_loggedLiveFov) > 0.01f ||
        std::fabs(base - g_loggedBaseFov) > 0.01f) {
        g_loggedLiveFov = live;
        g_loggedBaseFov = base;
        log::Line("[fov] live=%.2f base=%.2f (Quake fov degrees, horizontal at 4:3) "
                  "rendered=%.2f/%.2f over %dx%d px -> zoom factor %.4f",
                  live, base, frame.fovX, frame.fovY, frame.width, frame.height, factor);
    }

    return factor;
}

}  // namespace fov_zoom
}  // namespace Q2RTXHT
