#pragma once

#include <cstdint>
#include "cameraunlock/memory/pe_fingerprint.h"

namespace Q2RTXHT {

// Per-build offsets pinned to a specific shipped q2rtx.exe. The PE fingerprint
// (TimeDateStamp + SizeOfImage + CheckSum) is the authoritative routing key;
// the date in the name is for humans. Append a new profile when a patch breaks
// the RVAs - never edit an existing one in place (see AGENTS.md "Maintain
// compatibility across new patches").
struct BuildProfile {
    const char* name;
    cameraunlock::memory::PeFingerprint fingerprint;

    // RVA of R_RenderFrame(refdef_t *fd) - the renderer entry the hook wraps.
    uint32_t rvaRRenderFrame;

    // Byte offsets inside refdef_t (the *fd argument).
    uint32_t offViewOrg;     // vec3_t  (3 floats) - camera world position
    uint32_t offViewAngles;  // vec3_t  (3 floats) - pitch, yaw, roll (degrees)
    uint32_t offFovX;        // float   - horizontal fov in degrees
    uint32_t offFovY;        // float   - vertical fov in degrees
    uint32_t offRectX;       // int     - 3D viewport left edge, real pixels
    uint32_t offRectY;       // int     - 3D viewport top edge, real pixels
    uint32_t offWidth;       // int     - render viewport width
    uint32_t offHeight;      // int     - render viewport height

    // Zoom compensation. cl.fov_x is the interpolated player-state FOV in
    // Quake's own fov units - horizontal degrees referenced to 4:3, which is
    // what V_CalcFov(cl.fov_x, 4, 3) deriving cl.fov_y establishes. info_fov is
    // the client's "fov" cvar, the un-zoomed reference the player themselves
    // set (Options > Video, or the console). Both are the same quantity at
    // different moments, so the ratio needs no aspect term.
    uint32_t offClFovX;      // float inside client_state_t
    uint32_t rvaInfoFov;     // cvar_t** - the "fov" cvar

    // Gameplay gate: read cls.state / cls.key_dest to suppress tracking outside
    // active gameplay (menu, console, loading, cinematic).
    uint32_t rvaCls;            // RVA of the client_static_t global (cls)
    uint32_t offClsState;       // int - connstate_t
    uint32_t offClsKeyDest;     // int - keydest_t (0 == KEY_GAME)
    int32_t  connstateActive;   // value of ca_active

    // Multiplayer gate and the collision model the aim/lean traces run against.
    uint32_t rvaCl;             // RVA of the client_state_t global (cl)
    uint32_t offClMaxclients;   // int - 1 in single player, >1 in coop/deathmatch
    uint32_t offClBsp;          // bsp_t* - null until a map is loaded

    // End-of-level intermission. cls.state stays ca_active and the keyboard
    // stays with the game, so only the player state says the camera has been
    // parked on an info_player_intermission and is no longer the player's to
    // move. cl.frame.ps.pmove.pm_type carries it.
    uint32_t offClPmType;       // int - pmtype_t inside cl.frame.ps.pmove
    int32_t  pmtypeFreeze;      // value of PM_FREEZE

    // CL_Trace(trace_t *tr, const vec3_t start, const vec3_t mins,
    //          const vec3_t maxs, const vec3_t end, int contentmask).
    // World brushes plus the client's solid entities; skips the local player.
    uint32_t rvaClTrace;

    // Crosshair reprojection (aim decoupling). SCR_DrawCrosshair centres the pic
    // on scr.hud_width/hud_height and adds the ch_x / ch_y cvar integers, so the
    // reticle is moved by writing those two integers around the original call.
    uint32_t rvaScrDrawCrosshair;  // function to wrap
    uint32_t rvaScr;               // the screen state struct
    uint32_t offScrHudWidth;       // int - 2D drawing space width
    uint32_t offScrHudHeight;      // int - 2D drawing space height
    uint32_t rvaChX;               // cvar_t** - crosshair x nudge
    uint32_t rvaChY;               // cvar_t** - crosshair y nudge
    uint32_t offCvarInteger;       // int inside cvar_t
    uint32_t rvaRConfig;           // { int width; int height; } - framebuffer size
};

// Defined in steam_offsets.cpp, one file per store. The registry in
// build_profiles.cpp is the only thing that lists them.
extern const BuildProfile kSteamProfile_20250326;

// Returns the profile whose fingerprint matches the running module, or nullptr
// when the build is unknown (mod then stays fully dormant).
const BuildProfile* FindMatchingProfile(const cameraunlock::memory::PeFingerprint& running);

// Top of the registry: the newest build the mod knows about, used only to word
// the "your build is newer/older" diagnostic when nothing matches.
const BuildProfile& DiagnosticPrimary();

// Number of known profiles (0 means none pinned yet).
int KnownProfileCount();

// The registry entry at `index`, which must be below KnownProfileCount().
// Exists so tests/build_profile_tests.cpp can check the append-at-the-top rule
// still holds once there is more than one build to get it wrong with.
const BuildProfile* ProfileAt(int index);

}  // namespace Q2RTXHT
