#pragma once

namespace Q2RTXHT {

struct BuildProfile;

// Whether the client is somewhere tracking may be applied at all. Reads the
// engine's own client state, so it is a plain memory read with no hooks of its
// own.
namespace game_state {

// Resolves the cls / cl globals from the matched build. Runs before the render
// hook's slot is swapped, so IsInGameplay() is never reached before it.
void Init(void* moduleBase, const BuildProfile& profile);

// True when the running client is in single-player active gameplay: not a menu,
// console, loading screen or cinematic, and not a coop/deathmatch session.
bool IsInGameplay();

}  // namespace game_state
}  // namespace Q2RTXHT
