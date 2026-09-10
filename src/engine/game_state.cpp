#include "engine/game_state.h"

#include "core/build_profiles.h"

namespace Q2RTXHT {
namespace game_state {

namespace {

// keydest_t. Anything else means the console, a menu or a message prompt has
// the keyboard, which is not gameplay.
constexpr int kKeyDestGame = 0;

// cl.maxclients in a single-player session. Above it the client is in coop or
// deathmatch.
constexpr int kSinglePlayerMaxClients = 1;

const unsigned char* g_moduleBase = nullptr;
const BuildProfile* g_profile = nullptr;

int ReadInt(unsigned int rva, unsigned int offset) {
    return *reinterpret_cast<const int*>(g_moduleBase + rva + offset);
}

}  // namespace

void Init(void* moduleBase, const BuildProfile& profile) {
    g_moduleBase = static_cast<const unsigned char*>(moduleBase);
    g_profile = &profile;
}

bool IsInGameplay() {
    const int state = ReadInt(g_profile->rvaCls, g_profile->offClsState);
    const int keyDest = ReadInt(g_profile->rvaCls, g_profile->offClsKeyDest);
    if (state != g_profile->connstateActive || keyDest != kKeyDestGame) return false;

    // Coop and deathmatch: a view that can look behind you while your aim stays
    // forward is a competitive advantage, so the mod stands down.
    if (ReadInt(g_profile->rvaCl, g_profile->offClMaxclients) > kSinglePlayerMaxClients) {
        return false;
    }

    // The end-of-level intermission keeps cls.state at ca_active and leaves the
    // keyboard with the game, so nothing above it notices. What has happened is
    // that MoveClientToIntermission parked the camera on an
    // info_player_intermission and set pm_type to PM_FREEZE, while
    // ClientEndServerFrame forces the field of view to 90 for the duration. So
    // tracking here would move a camera the player no longer owns, and do it at
    // whatever exaggeration that forced FOV implies against their own setting.
    return ReadInt(g_profile->rvaCl, g_profile->offClPmType) != g_profile->pmtypeFreeze;
}

}  // namespace game_state
}  // namespace Q2RTXHT
