#include "hooks/crosshair_hook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cmath>
#include <cstdint>

#include "core/build_profiles.h"
#include "core/mod.h"
#include "engine/cvar.h"
#include "hooks/reticle.h"
#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/logging/file_log.h"

namespace Q2RTXHT {

namespace {

namespace log = cameraunlock::logging;

using DrawCrosshairFn = void(*)();

DrawCrosshairFn g_orig = nullptr;
bool g_unplaceableLogged = false;

// SCR_DrawCrosshair centres the pic on scr.hud_width / scr.hud_height and then
// adds the ch_x / ch_y cvar integers, so those two integers are the whole of
// the reticle's position. hud space is the framebuffer scaled by the player's
// HUD scale, which is why r_config is needed to convert.
int* g_hudWidth = nullptr;
int* g_hudHeight = nullptr;
int* g_realWidth = nullptr;
int* g_realHeight = nullptr;
void* const* g_chX = nullptr;
void* const* g_chY = nullptr;
uint32_t g_cvarIntegerOffset = 0;

void DetourDrawCrosshair() {
    float px = 0.0f, py = 0.0f;
    const reticle::State state = Mod::Instance().IsEnabled()
                                     ? reticle::TargetForPublishedFrame(px, py)
                                     : reticle::State::Untouched;
    if (state == reticle::State::Hidden) return;
    if (state == reticle::State::Untouched) {
        g_orig();
        return;
    }

    int* chx = engine::CvarInteger(g_chX, g_cvarIntegerOffset);
    int* chy = engine::CvarInteger(g_chY, g_cvarIntegerOffset);
    if (!chx || !chy || *g_realWidth <= 0 || *g_realHeight <= 0) {
        // We know where the shot lands and cannot say so. Drawing the game's
        // crosshair at screen centre would mark a point the round will not
        // reach, which is the thing this hook exists to prevent, so draw
        // nothing instead and say once why.
        if (!g_unplaceableLogged) {
            g_unplaceableLogged = true;
            log::Line("[crosshair] ch_x/ch_y or the framebuffer size could not be read; "
                      "the crosshair is hidden while tracking rather than drawn off-target");
        }
        return;
    }

    const float toHudX = static_cast<float>(*g_hudWidth) / static_cast<float>(*g_realWidth);
    const float toHudY = static_cast<float>(*g_hudHeight) / static_cast<float>(*g_realHeight);

    // The player's own ch_x / ch_y nudge is kept: this adds to it rather than
    // replacing it, so a crosshair they had deliberately offset stays offset.
    const int oldX = *chx;
    const int oldY = *chy;
    *chx = oldX + static_cast<int>(std::lround(px * toHudX - *g_hudWidth * 0.5f));
    *chy = oldY + static_cast<int>(std::lround(py * toHudY - *g_hudHeight * 0.5f));
    g_orig();
    *chx = oldX;
    *chy = oldY;
}

}  // namespace

bool InstallCrosshairHook(void* moduleBase, const BuildProfile& profile) {
    using namespace cameraunlock::hooks;

    auto* base = static_cast<unsigned char*>(moduleBase);
    g_hudWidth = reinterpret_cast<int*>(base + profile.rvaScr + profile.offScrHudWidth);
    g_hudHeight = reinterpret_cast<int*>(base + profile.rvaScr + profile.offScrHudHeight);
    // r_config is { int width; int height; }, so the height is one int in.
    g_realWidth = reinterpret_cast<int*>(base + profile.rvaRConfig);
    g_realHeight = reinterpret_cast<int*>(base + profile.rvaRConfig + sizeof(int));
    g_chX = reinterpret_cast<void* const*>(base + profile.rvaChX);
    g_chY = reinterpret_cast<void* const*>(base + profile.rvaChY);
    g_cvarIntegerOffset = profile.offCvarInteger;
    void* target = base + profile.rvaScrDrawCrosshair;

    HookManager& hooks = HookManager::Instance();
    if (hooks.Initialize() != HookStatus::Ok) {
        log::Line("[crosshair] MinHook init failed");
        return false;
    }

    const HookStatus created = hooks.CreateHook(target,
                                                reinterpret_cast<void*>(&DetourDrawCrosshair),
                                                reinterpret_cast<void**>(&g_orig));
    if (created != HookStatus::Ok) {
        log::Line("[crosshair] CreateHook failed: %s", HookStatusToString(created));
        return false;
    }
    if (hooks.EnableHook(target) != HookStatus::Ok) {
        log::Line("[crosshair] EnableHook failed");
        hooks.RemoveHook(target);
        g_orig = nullptr;
        return false;
    }

    log::Line("[crosshair] SCR_DrawCrosshair hooked");
    return true;
}

}  // namespace Q2RTXHT
