#pragma once

#include <cstdint>

namespace Q2RTXHT {
namespace engine {

// A cvar_t is reached through a cvar_t** the engine keeps in its own data, and
// the only member the mod reads is `integer`, at the offset the matched build
// pinned. Two settings arrive this way - the "fov" reference and the ch_x /
// ch_y crosshair nudge - and both carry the same trap: the slot is empty until
// the engine registers the cvar, which happens after the ASI is loaded.
//
// Returns the address of the cvar's `integer`, or nullptr while the slot is
// still empty.
inline int* CvarInteger(void* const* slot, uint32_t integerOffset) {
    if (!slot || !*slot) return nullptr;
    return reinterpret_cast<int*>(static_cast<unsigned char*>(*slot) + integerOffset);
}

}  // namespace engine
}  // namespace Q2RTXHT
