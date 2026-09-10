#pragma once

namespace Q2RTXHT {

struct BuildProfile;

// Wraps SCR_DrawCrosshair so the game's crosshair is drawn at the clean-aim
// screen position inside the head-tracked view (aim decoupling feedback).
// Requires the render hook to be publishing frame state. Returns false on
// MinHook failure.
bool InstallCrosshairHook(void* moduleBase, const BuildProfile& profile);

}  // namespace Q2RTXHT
