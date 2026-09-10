#include "core/build_profiles.h"

namespace Q2RTXHT {

// Newest first. Append new builds at the top (diagnostic primary); never edit
// an existing entry's RVAs in place.
static const BuildProfile* const kKnownProfiles[] = {
    &kSteamProfile_20250326,
};

const BuildProfile* FindMatchingProfile(const cameraunlock::memory::PeFingerprint& running) {
    for (const BuildProfile* p : kKnownProfiles) {
        if (p->fingerprint.Matches(running)) {
            return p;
        }
    }
    return nullptr;
}

const BuildProfile& DiagnosticPrimary() {
    return *kKnownProfiles[0];
}

const BuildProfile* ProfileAt(int index) {
    return kKnownProfiles[index];
}

int KnownProfileCount() {
    return static_cast<int>(sizeof(kKnownProfiles) / sizeof(kKnownProfiles[0]));
}

}  // namespace Q2RTXHT
