#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <cstdint>
#include <cwctype>
#include <string>

#include "version.h"
#include "core/mod.h"
#include "window_centering.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/os/module_paths.h"

namespace {

namespace log = cameraunlock::logging;
namespace os = cameraunlock::os;

// Ultimate ASI Loader injects into whatever process it is renamed next to, so
// the mod has to establish it is inside the game before it writes anything.
constexpr wchar_t kGameExeName[] = L"q2rtx.exe";

// The exe's own file name, not a substring of the whole path: this decides
// whether the mod writes a log next to a foreign process's executable and starts
// reading its memory at pinned offsets, so a directory that happens to contain
// the string must not answer yes.
bool ProcessIsGame() {
    const std::wstring path = os::ModuleFilePath(nullptr);
    const size_t sep = path.find_last_of(L"\\/");
    std::wstring name = sep == std::wstring::npos ? path : path.substr(sep + 1);
    for (auto& c : name) c = static_cast<wchar_t>(towlower(c));
    return name == kGameExeName;
}

unsigned __stdcall InitThread(void*) {
    if (!ProcessIsGame()) {
        // No log file: this is a foreign process and the mod does not write next
        // to someone else's exe. The debugger channel costs nothing and still
        // separates "wrong host" from "the loader never loaded us", which
        // otherwise look identical from outside - both are no tracking and no
        // QuakeIIRTXHeadTracking.log.
        OutputDebugStringA(
            "QuakeIIRTXHeadTracking: host process is not q2rtx.exe; staying dormant\r\n");
        return 1;
    }

    // Open() truncates, and first rotates the outgoing session to
    // QuakeIIRTXHeadTracking.prev.log - the render hook can take the game down
    // with it and the player relaunches before sending the log, so the session
    // worth reading is the one that has to survive. It reports a failed
    // rotation itself; rotating here as well would report it twice.
    log::Open(os::HostExeDirectory() + L"\\QuakeIIRTXHeadTracking.log");
    log::Line("====================================================");
    log::Line("%s v%s attaching", Q2RTXHT::MOD_NAME, Q2RTXHT::MOD_VERSION);

    // No wait for the engine here. The one thing that is not ready at load is
    // the R_RenderFrame function pointer, and the render hook's own installer
    // thread already polls for it - waiting a second time only delayed the
    // config load, the fingerprint check and every log line behind it.
    if (!Q2RTXHT::Mod::Instance().Initialize(GetModuleHandleW(nullptr))) {
        // An unrecognised build leaves the mod dormant and the game vanilla, and
        // that has to include its window: a mod doing nothing must not move it.
        return 1;
    }

    // Last, because it blocks until the game has a window that has stopped
    // moving - up to a minute on a cold start. Nothing above may wait on it.
    Q2RTXHT::CenterWindowWhenReady();
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // No DisableThreadLibraryCalls: this DLL links the static CRT, which the
        // Win32 documentation says must keep its DLL_THREAD_ATTACH and
        // DLL_THREAD_DETACH notifications. Skipping the notifications for one
        // ASI buys nothing measurable, and the game creates renderer, sound and
        // driver threads that touch per-thread CRT state.
        const uintptr_t thread =
            _beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (thread == 0) {
            // The log lives inside InitThread, so a failure here produces a mod
            // that does nothing and writes no file - indistinguishable from the
            // ASI never being loaded at all. A bare OutputDebugStringA of a
            // literal needs no handle, no lock, and no formatting call into
            // user32, which is what makes it usable from here.
            OutputDebugStringA("QuakeIIRTXHeadTracking: init thread could not start\r\n");
        } else {
            CloseHandle(reinterpret_cast<HANDLE>(thread));
        }
    }

    // No teardown on DLL_PROCESS_DETACH, deliberately: no log line, no hook
    // removal, no joins. On process exit every other thread has already been
    // terminated, possibly holding the log's mutex, so writing a shutdown line
    // there can hang the game on quit. On an explicit FreeLibrary the loader
    // lock is held, which makes joining the receiver, hotkey and render-slot
    // threads a deadlock rather than a cleanup. Ultimate ASI Loader never frees
    // a plugin, so there is no third case to write teardown for. Mod::Instance
    // is heap-allocated and never destroyed for the same reason, see mod.cpp.
    return TRUE;
}
