#include "engine/slot_swap.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <cstdint>

#include "cameraunlock/logging/file_log.h"

namespace Q2RTXHT {
namespace slot_swap {

namespace {

namespace log = cameraunlock::logging;

// The engine assigns the pointer during its own renderer init, which can be
// well after an ASI's init thread runs; a cold start off a hard disk is the
// slow case.
constexpr int kPollIntervalMs = 100;
constexpr int kPollAttempts = 600;  // 60s

// After the first swap the same thread keeps watching the slot. The engine does
// not write it once: CL_InitRefresh re-registers the renderer's entry points,
// and CL_RunRefresh reaches that on any client frame where a CVAR_REFRESH cvar
// has been touched - `vid_restart`, or a renderer or video-mode change from the
// options menu. A detour dropped that way is silent, so it is watched for
// instead.
//
// The price of polling rather than hooking the two registration functions is
// that a renderer restart leaves up to a quarter second of the game's own
// camera before the detour is back - fifteen frames at 60Hz, and visible as the
// view settling to centre and back. That is the right trade against hooking two
// more RVAs per build, and it costs four wakeups a second.
constexpr int kSuperviseIntervalMs = 250;

struct Request {
    void* moduleBase;
    void** slot;
    void* detour;
    std::atomic<void*>* original;
    const char* logTag;
    const char* slotName;
};

// A populated slot points at engine code. Anything else is the slot still
// holding its initial value, so capturing it would swap in a pointer the game
// never calls.
bool PointerInText(void* moduleBase, void* p) {
    if (!p) return false;
    auto* dos = static_cast<IMAGE_DOS_HEADER*>(moduleBase);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        static_cast<unsigned char*>(moduleBase) + dos->e_lfanew);
    uintptr_t base = reinterpret_cast<uintptr_t>(moduleBase);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if ((sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
            uintptr_t lo = base + sec[i].VirtualAddress;
            uintptr_t hi = lo + sec[i].Misc.VirtualSize;
            if (addr >= lo && addr < hi) return true;
        }
    }
    return false;
}

// A failed VirtualProtect is reported rather than written through: the store
// would fault inside whichever thread called this and take the game down with
// nothing in the log.
bool WriteSlot(const Request& req, void* value) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(req.slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        log::Line("[%s] VirtualProtect on the %s slot failed: %lu",
                  req.logTag, req.slotName, GetLastError());
        return false;
    }
    *req.slot = value;
    VirtualProtect(req.slot, sizeof(void*), oldProtect, &oldProtect);
    return true;
}

// WriteSlot, but only landing while the slot still holds `expected`, and
// reporting what it actually held. Compare-exchange rather than exchange so the
// caller can publish `original` FIRST and still not race the engine: the detour
// can only become reachable against the value that was published.
bool CompareExchangeSlot(const Request& req, void* value, void* expected, void*& prior) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(req.slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        log::Line("[%s] VirtualProtect on the %s slot failed: %lu",
                  req.logTag, req.slotName, GetLastError());
        return false;
    }
    prior = InterlockedCompareExchangePointer(req.slot, value, expected);
    VirtualProtect(req.slot, sizeof(void*), oldProtect, &oldProtect);
    return true;
}

// Re-swaps whenever the engine has put its own function back in the slot. The
// address it puts back is not necessarily the one captured the first time - a
// GL/RTX toggle registers a different function - so `original` is re-captured
// before the detour becomes reachable again, never reused.
void Supervise(const Request& req) {
    bool foreignLogged = false;
    for (;;) {
        Sleep(kSuperviseIntervalMs);
        void* current = *req.slot;
        if (current == req.detour) continue;
        if (!PointerInText(req.moduleBase, current)) {
            // Zeroed during a renderer shutdown, or another injector chaining
            // through us - both are cases to leave alone. But a hook that has
            // quietly stopped hooking looks exactly like one that never
            // engaged, so say it once.
            if (!foreignLogged) {
                foreignLogged = true;
                log::Line("[%s] %s holds %p, which is neither our detour nor engine code; "
                          "leaving it alone", req.logTag, req.slotName, current);
            }
            continue;
        }
        foreignLogged = false;

        // Published before the detour goes back in, keeping the ordering the
        // header promises and render_hook.cpp relies on to skip its null check.
        // The install is then a compare-exchange rather than a store, so it can
        // only land while the slot still holds the value just published: a
        // plain write would leave `original` naming the renderer that was shut
        // down for as long as the second VirtualProtect takes, and a GL/RTX
        // toggle is precisely when that is the wrong function to call. Losing
        // the compare costs one tick.
        req.original->store(current, std::memory_order_release);
        void* prior = nullptr;
        if (!CompareExchangeSlot(req, req.detour, current, prior)) continue;
        if (prior != current) continue;

        log::Line("[%s] the engine reassigned %s; swapped back (orig=%p)",
                  req.logTag, req.slotName, current);
    }
}

unsigned __stdcall PollAndSwap(void* arg) {
    const Request req = *static_cast<Request*>(arg);
    delete static_cast<Request*>(arg);

    for (int i = 0; i < kPollAttempts; ++i) {
        void* current = *req.slot;
        if (PointerInText(req.moduleBase, current)) {
            req.original->store(current, std::memory_order_release);
            if (!WriteSlot(req, req.detour)) return 1;

            log::Line("[%s] %s swapped (orig=%p)", req.logTag, req.slotName, current);
            Supervise(req);
            return 0;
        }
        Sleep(kPollIntervalMs);
    }
    log::Line("[%s] timed out waiting for %s to be populated", req.logTag, req.slotName);
    return 1;
}

}  // namespace

bool BeginAsync(void* moduleBase, void** slot, void* detour,
                std::atomic<void*>& original, const char* logTag, const char* slotName) {
    auto* req = new Request{ moduleBase, slot, detour, &original, logTag, slotName };

    const uintptr_t thread = _beginthreadex(nullptr, 0, PollAndSwap, req, 0, nullptr);
    if (thread == 0) {
        delete req;
        return false;
    }
    CloseHandle(reinterpret_cast<HANDLE>(thread));
    return true;
}

}  // namespace slot_swap
}  // namespace Q2RTXHT
