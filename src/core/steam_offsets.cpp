#include "core/build_profiles.h"

// Every Steam build this mod knows about, one profile per shipped exe, appended
// as patches land. Never edit an existing entry's RVAs in place: a player who
// has not taken the patch still routes to their own profile by fingerprint, and
// rewriting it strands them with no fix (AGENTS.md "Maintain compatibility
// across new patches"). A new store gets its own file next to this one.

namespace Q2RTXHT {

// Quake II RTX 1.8.0 (Steam), built 2025-03-26 (TimeDateStamp 0x67E454C2).
// R_RenderFrame is an 8-byte global function pointer in .data; every RVA and
// member offset below is read from the symbols the shipped q2rtx.exe and
// q2rtx.pdb carry. CheckSum is 0 in this build - the linker left it unset; that
// is still a valid (and stable) third fingerprint field.
extern const BuildProfile kSteamProfile_20250326 = {
    "steam-win64-20250326",
    { 0x67E454C2u, 0x03C31000u, 0x00000000u },
    0xC36C18u,  // rvaRRenderFrame (function pointer)
    24u,        // offViewOrg
    36u,        // offViewAngles
    16u,        // offFovX
    20u,        // offFovY
    0u,         // offRectX
    4u,         // offRectY
    8u,         // offWidth
    12u,        // offHeight
    2576832u,   // offClFovX
    0x3B3B440u, // rvaInfoFov
    0x3B3C5A0u, // rvaCls
    0u,         // offClsState
    4u,         // offClsKeyDest
    7,          // connstateActive (ca_active)
    0x333D040u, // rvaCl
    2580056u,   // offClMaxclients
    4324840u,   // offClBsp
    2572524u,   // offClPmType (cl.frame 2572476 + server_frame_t.ps 48 + pmove 0)
    4,          // pmtypeFreeze (PM_FREEZE)
    0x5A320u,   // rvaClTrace
    0x5C5C0u,   // rvaScrDrawCrosshair
    0xC36D40u,  // rvaScr
    156u,       // offScrHudWidth
    160u,       // offScrHudHeight
    0xC36EC8u,  // rvaChX
    0xC36ED0u,  // rvaChY
    48u,        // offCvarInteger
    0x333CF30u, // rvaRConfig
};

}  // namespace Q2RTXHT
