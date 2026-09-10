// Characterization tests for src/core/build_profiles.cpp and the store profile
// files beside it.
//
// This is the highest-consequence path in the mod and the one with no visible
// symptom when it goes wrong in the safe direction. Hooking a patched build
// against stale RVAs takes the player's game down seconds after the map loads,
// so the failsafe - no match, no hooks, no process modification - is mandatory
// rather than defensive. Nothing else in the suite reaches it, and a routing
// key that has quietly stopped discriminating looks exactly like one that
// never had to.

#include "core/build_profiles.h"

#include <iostream>

namespace {

namespace mem = cameraunlock::memory;

int g_failures = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        std::cout << "  [PASS] " << name << "\n";
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++g_failures;
    }
}

// The fingerprint of the build the mod was developed against, written out here
// rather than read from the profile: an expectation taken from the value under
// test moves with it, and this is the one number that says which exe the pinned
// RVAs were read from (.lab/NOTES.md, "Offsets").
constexpr mem::PeFingerprint kSteam20250326{ 0x67E454C2u, 0x03C31000u, 0x00000000u };

void TestTheShippedProfileStillNamesTheBuildItsOffsetsCameFrom() {
    const Q2RTXHT::BuildProfile* p = Q2RTXHT::FindMatchingProfile(kSteam20250326);
    Check(p == &Q2RTXHT::kSteamProfile_20250326,
          "the 2025-03-26 Steam fingerprint routes to its own profile");
}

// All three fields are load-bearing. Two of them matching is a different exe,
// and routing to it would hook the wrong addresses.
void TestEachFingerprintFieldAloneIsEnoughToMiss() {
    mem::PeFingerprint stamp = kSteam20250326;
    stamp.TimeDateStamp += 1u;
    Check(Q2RTXHT::FindMatchingProfile(stamp) == nullptr,
          "a different TimeDateStamp matches nothing");

    mem::PeFingerprint size = kSteam20250326;
    size.SizeOfImage += 0x1000u;
    Check(Q2RTXHT::FindMatchingProfile(size) == nullptr,
          "a different SizeOfImage matches nothing");

    // This build's CheckSum is 0 because the linker left it unset, which makes
    // it the field most likely to be dropped from the comparison as useless.
    mem::PeFingerprint sum = kSteam20250326;
    sum.CheckSum = 0x1234u;
    Check(Q2RTXHT::FindMatchingProfile(sum) == nullptr,
          "a different CheckSum matches nothing, zero-valued though ours is");
}

// The registry is append-at-the-top, and the top entry is what words the
// newer/older hint when nothing matches. If a new profile is appended at the
// bottom instead, every unrecognised build is told it is newer than a build the
// mod has known about for years.
void TestDiagnosticPrimaryIsTheNewestKnownBuild() {
    const Q2RTXHT::BuildProfile& primary = Q2RTXHT::DiagnosticPrimary();
    Check(Q2RTXHT::KnownProfileCount() >= 1, "at least one build is pinned");

    uint32_t newest = 0;
    for (int i = 0; i < Q2RTXHT::KnownProfileCount(); ++i) {
        const Q2RTXHT::BuildProfile* p = Q2RTXHT::ProfileAt(i);
        if (p->fingerprint.TimeDateStamp > newest) newest = p->fingerprint.TimeDateStamp;
    }
    Check(primary.fingerprint.TimeDateStamp == newest,
          "the diagnostic primary is the newest build in the registry");
    Check(Q2RTXHT::ProfileAt(0) == &primary, "the diagnostic primary is the first entry");
}

// The three directions drive three different user-facing sentences, and getting
// them the wrong way round tells a player on a patched game to wait for Steam.
void TestMismatchDirectionsAreDistinguished() {
    mem::PeFingerprint newer = kSteam20250326;
    newer.TimeDateStamp += 1u;
    Check(mem::ClassifyMismatch(newer, kSteam20250326) == mem::FingerprintMismatch::Newer,
          "a later TimeDateStamp classifies as Newer");

    mem::PeFingerprint older = kSteam20250326;
    older.TimeDateStamp -= 1u;
    Check(mem::ClassifyMismatch(older, kSteam20250326) == mem::FingerprintMismatch::Older,
          "an earlier TimeDateStamp classifies as Older");

    mem::PeFingerprint repacked = kSteam20250326;
    repacked.SizeOfImage += 0x1000u;
    Check(mem::ClassifyMismatch(repacked, kSteam20250326) == mem::FingerprintMismatch::Differs,
          "the same TimeDateStamp with a different size classifies as Differs");
}

// Every offset the hook writes through comes from the profile, so a zeroed one
// is a write at the module base. Walked over the whole registry rather than the
// entry that exists today: the profile that gets a field left blank is the next
// one pasted in for a patched build, which is the only one nobody has run yet.
void TestNoKnownProfileHasAnUnfilledRva() {
    for (int i = 0; i < Q2RTXHT::KnownProfileCount(); ++i) {
        const Q2RTXHT::BuildProfile& p = *Q2RTXHT::ProfileAt(i);
        Check(p.rvaRRenderFrame != 0 && p.rvaCls != 0 && p.rvaCl != 0 && p.rvaClTrace != 0 &&
                  p.rvaScrDrawCrosshair != 0 && p.rvaScr != 0 && p.rvaChX != 0 &&
                  p.rvaChY != 0 && p.rvaRConfig != 0 && p.rvaInfoFov != 0,
              "no RVA in this profile is left at zero");
        Check(p.name != nullptr && p.name[0] != '\0', "the profile names itself for the log");
    }
}

}  // namespace

int RunBuildProfileTests() {
    std::cout << "\nBuild profile registry tests\n";
    TestTheShippedProfileStillNamesTheBuildItsOffsetsCameFrom();
    TestEachFingerprintFieldAloneIsEnoughToMiss();
    TestDiagnosticPrimaryIsTheNewestKnownBuild();
    TestMismatchDirectionsAreDistinguished();
    TestNoKnownProfileHasAnUnfilledRva();

    if (g_failures == 0) {
        std::cout << "Build profile registry tests: all passed\n";
    } else {
        std::cout << "Build profile registry tests: " << g_failures << " failure(s)\n";
    }
    return g_failures;
}
