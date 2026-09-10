#pragma once

namespace Q2RTXHT {

struct BuildProfile;

// Starts the R_RenderFrame pointer swap. moduleBase is the q2rtx.exe base.
// Returns false only when the installer thread could not be started. The swap
// itself is asynchronous - the engine assigns the pointer during R_Init, after
// our init thread runs - so a true return means "waiting", not "installed", and
// the outcome either way is a [render_hook] line in the log.
//
// Once installed, each frame's state is published through hooks/reticle.h,
// which is where the crosshair hook reads it.
bool InstallRenderHook(void* moduleBase, const BuildProfile& profile);

}  // namespace Q2RTXHT
