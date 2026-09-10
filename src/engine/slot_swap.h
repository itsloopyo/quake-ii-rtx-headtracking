#pragma once

#include <atomic>

namespace Q2RTXHT {

// Swapping a function pointer the engine keeps in its own data, once the engine
// has populated it. There is no trampoline and no instruction patching here:
// the slot IS the call target, so writing it is the whole hook.
namespace slot_swap {

// Polls `slot` until it holds an address inside an executable section of
// `moduleBase`, then release-stores that address into `original` and writes
// `detour` into the slot. The slot is polled rather than written at load time
// because the engine assigns it well after an ASI is injected, and a pointer
// captured before then is not the function the game will call.
//
// The store into `original` happens BEFORE the detour becomes reachable, so any
// thread that reaches the detour has already observed it and never has to
// null-check.
//
// The thread does not exit after the swap. The engine may write the slot again
// at any point in the session - a renderer restart re-registers its own entry
// points - so the slot is watched for the life of the process and swapped back
// whenever that happens, re-capturing `original` each time because a restart can
// register a different function.
//
// Returns false only when the polling thread could not be started - the swap
// itself is asynchronous, so a true return means "waiting", not "installed".
// Its outcome either way is a log line reading `[<logTag>] ... <slotName> ...`.
bool BeginAsync(void* moduleBase, void** slot, void* detour,
                std::atomic<void*>& original, const char* logTag, const char* slotName);

}  // namespace slot_swap
}  // namespace Q2RTXHT
