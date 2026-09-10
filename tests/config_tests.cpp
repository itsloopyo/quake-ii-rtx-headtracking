// Boundary tests for HeadTracking.ini parsing.
//
// The .ini is user-typed text and the only place it becomes a float that
// reaches the view basis. IniReader's numeric readers are strtod-based, so
// "nan", "inf", "1e400" and the European decimal comma all parse without
// complaint, and nothing downstream catches them: every comparison against NaN
// is false, so the position clamp is skipped and sin/cos of an infinite angle
// is NaN. What that looks like in game is a frame rendered from a NaN camera
// with an empty log, which is why these are locked here rather than left to be
// noticed by a player.
//
// Every assertion names the exact value the user is left with. A range check
// against the guard's own clamp range cannot fail for anything the guard is
// able to produce, so it would pass just as happily if a rejected value fell
// back to the bottom of that range instead of to the documented default.

#include "core/config.h"
#include "version.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        std::cout << "  [PASS] " << name << "\n";
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++g_failures;
    }
}

// A path in the OS temp directory that does not exist yet, so LoadOrCreate's
// write-a-default branch can be exercised as well as its read branch.
std::string TempIniPath(const char* stem) {
    char dir[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, dir);
    return std::string(dir) + "q2rtxht-" + stem + ".ini";
}

void WriteFile(const std::string& path, const char* body) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << body;
}

// Line endings are a checkout property, not a content difference: .gitattributes
// stores the shipped .ini LF while IniWriter emits the platform's own.
std::string ReadFileWithoutCarriageReturns(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string out;
    for (char c : buf.str()) {
        if (c != '\r') out += c;
    }
    return out;
}

std::string DecodeBase64(const std::string& text) {
    static const char* kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    unsigned bits = 0;
    int held = 0;
    for (char c : text) {
        if (c == '=') break;
        const char* at = std::strchr(kAlphabet, c);
        if (c == '\n' || at == nullptr) continue;
        bits = (bits << 6) | static_cast<unsigned>(at - kAlphabet);
        held += 6;
        if (held >= 8) {
            held -= 8;
            out += static_cast<char>((bits >> held) & 0xFF);
        }
    }
    return out;
}

// Every hostile value that reaches a float in one file: non-finite literals, an
// overflowing exponent, a decimal comma, and negative travel limits (which
// invert PositionProcessor's clamp bounds and pin the lean at a fixed offset).
constexpr const char* kHostileIni =
    "[General]\n"
    "Enabled=1\n"
    "UdpPort=70000\n"
    "[Rotation]\n"
    "YawSensitivity=nan\n"
    "PitchSensitivity=inf\n"
    "RollSensitivity=1e400\n"
    "LocalSmoothing=5.0\n"
    // The comma goes on the key whose default is NOT zero. strtod reads "0,25"
    // as a whole-looking 0.0, and on LocalSmoothing (default 0.0) the broken
    // and the corrected result are the same number, so the check would pass
    // either way and prove nothing.
    "RemoteSmoothing=0,25\n"
    "[Position]\n"
    "SensitivityX=nan\n"
    "LimitX=-0.30\n"
    "LimitY=nan\n"
    "LimitZ=99999\n"
    "LimitZBack=inf\n"
    "UnitsPerMeter=0\n"
    "[Collision]\n"
    "CollisionMargin=nan\n"
    "CollisionReleaseSmoothing=42\n"
    "[Controls]\n"
    "ToggleKey=0x230\n"
    "TogglePositionKey=0x10\n"
    "ToggleYawKey=0\n";

// Every key valid on its own, two of them the same key. One press would
// otherwise toggle tracking and cycle the tracking mode together.
constexpr const char* kCollidingKeysIni =
    "[Controls]\n"
    "ToggleKey=0x76\n"
    "TogglePositionKey=0x76\n"
    "ToggleYawKey=0x77\n";

void TestHostileValuesNeverReachTheCamera() {
    const std::string path = TempIniPath("hostile");
    WriteFile(path, kHostileIni);

    const Q2RTXHT::Config defaults;
    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(path), "a readable ini loads");

    Check(cfg.yawSensitivity == defaults.yawSensitivity,
          "YawSensitivity=nan falls back to the default");
    Check(cfg.pitchSensitivity == defaults.pitchSensitivity,
          "PitchSensitivity=inf falls back to the default");
    Check(cfg.rollSensitivity == defaults.rollSensitivity,
          "RollSensitivity=1e400 falls back to the default");
    Check(cfg.posSensX == defaults.posSensX, "SensitivityX=nan falls back to the default");

    Check(cfg.localSmoothing == 1.0f, "LocalSmoothing=5.0 is clamped to 1.0");
    // A decimal comma is the expected user error - IniReader pins the C locale -
    // and strtod reads it as a whole-looking 0.0 that passes every range check.
    Check(cfg.remoteSmoothing == defaults.remoteSmoothing,
          "RemoteSmoothing=0,25 falls back to the default rather than a silent 0");

    // Negative or non-finite travel limits invert the processor's clamp bounds.
    // Zero is refused for the same reason UnitsPerMeter refuses it: an axis
    // clamped to zero cannot move the eye at all, which in game is
    // indistinguishable from the mod not working.
    Check(cfg.posLimitX == 0.01f, "a negative LimitX is held at the smallest usable limit");
    Check(cfg.posLimitY == defaults.posLimitY, "LimitY=nan falls back to the default");
    Check(cfg.posLimitZ == 0.5f, "LimitZ=99999 is clamped to the largest allowed limit");
    Check(cfg.posLimitZBack == defaults.posLimitZBack,
          "LimitZBack=inf falls back to the default");

    // Zero here is a lean that never moves the eye, with nothing to say why, so
    // a finite out-of-range value is clamped up to the smallest usable one.
    Check(cfg.positionUnitsPerMeter == 32.0f,
          "UnitsPerMeter=0 is raised to the smallest usable value");

    Check(cfg.collisionStandoff == defaults.collisionStandoff,
          "CollisionMargin=nan falls back to the default");
    Check(cfg.collisionReleaseSmoothing == 1.0f,
          "CollisionReleaseSmoothing=42 is clamped to 1.0");

    // 70000 cast straight to uint16_t is 4464 - a mod listening on a port
    // nothing sends to, reported in the log as a successful bind.
    Check(cfg.udpPort == defaults.udpPort, "UdpPort=70000 falls back to the default");

    // GetAsyncKeyState only defines 0x01..0xFE, and a binding on Shift is
    // suppressed by the chord guard for as long as the key is held.
    Check(cfg.keyToggle == defaults.keyToggle, "ToggleKey=0x230 cannot be bound");
    Check(cfg.keyTogglePosition == defaults.keyTogglePosition,
          "TogglePositionKey=0x10 (Shift) is refused");
    Check(cfg.keyToggleYaw == defaults.keyToggleYaw, "ToggleYawKey=0 is refused");

    std::remove(path.c_str());
}

// A standoff under the shipped 4 units rests the eye close enough to a surface
// for the near plane to cull it, so the player sees through the wall the clamp
// just stopped them leaning into - the same bug, reached through the setting
// that exists to prevent it.
void TestCollisionMarginCannotBeSetBelowTheMeasuredStandoff() {
    const std::string path = TempIniPath("standoff");
    WriteFile(path, "[Collision]\nCollisionMargin=0.0\n");

    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(path), "an ini with only a collision margin loads");
    Check(cfg.collisionStandoff == 4.0f, "CollisionMargin=0 is raised to the 4-unit standoff");

    std::remove(path.c_str());
}

void TestCollidingHotkeysAreSeparated() {
    const std::string path = TempIniPath("keys");
    WriteFile(path, kCollidingKeysIni);

    const Q2RTXHT::Config defaults;
    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(path), "an ini with colliding hotkeys loads");
    Check(cfg.keyToggle == 0x76, "the first of a colliding pair keeps the key");
    Check(cfg.keyTogglePosition == defaults.keyTogglePosition,
          "the second of a colliding pair reverts to its default");
    Check(cfg.keyToggleYaw == 0x77, "a key that collides with nothing is left alone");

    std::remove(path.c_str());
}

// The shape a real user reaches first, and the one a revert-to-default check
// cannot fix on its own: a single key moved onto ANOTHER action's default. Here
// ToggleKey takes Page Down, which is ToggleYawKey's default, so reverting
// ToggleYawKey to that default would leave both on Page Down and one press would
// still fire two actions.
void TestKeyMovedOntoAnotherActionsDefaultIsSeparated() {
    const std::string path = TempIniPath("keys-onto-default");
    WriteFile(path, "[Controls]\nToggleKey=0x22\n");

    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(path), "an ini binding one action onto another's default loads");
    Check(cfg.keyToggle == 0x22, "the key the user asked for is honoured");
    Check(cfg.keyTogglePosition == 0x21, "the untouched binding keeps its default");
    // Exactly zero, not merely different: ToggleYawKey's own default IS 0x22,
    // so it has nowhere free to fall back to and RejectDuplicateKeys leaves it
    // unbound. That branch has no other test, and asserting a difference would
    // be satisfied by a fallback outcome too, pinning neither.
    Check(cfg.keyToggleYaw == 0,
          "a displaced binding whose own default is taken is left unbound");

    std::remove(path.c_str());
}

// All three on one key: the first keeps it and the other two fall back to their
// own compiled defaults, which are free because the key that collided was not
// one of them. Asserted as the three exact values rather than as "they differ" -
// a difference test is satisfied by both the fallback outcome and an unbound one,
// so it pins neither, which is the shape this file's header warns about.
void TestThreeWayHotkeyCollisionLeavesNoSharedKey() {
    const std::string path = TempIniPath("keys-all-three");
    WriteFile(path,
              "[Controls]\nToggleKey=0x76\nTogglePositionKey=0x76\nToggleYawKey=0x76\n");

    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(path), "an ini with three colliding hotkeys loads");
    Check(cfg.keyToggle == 0x76, "the first keeps the key");
    Check(cfg.keyTogglePosition == 0x21,
          "the second falls back to its own default, Page Up");
    Check(cfg.keyToggleYaw == 0x22,
          "the third falls back to its own default, Page Down");

    std::remove(path.c_str());
}

// The documented failure mode for a path that can be neither written nor read:
// compiled defaults, and false so the caller can say so in the log.
void TestUnusablePathReportsFailureAndKeepsDefaults() {
    const Q2RTXHT::Config defaults;
    Q2RTXHT::Config cfg;
    Check(!cfg.LoadOrCreate("\\\\?\\Q2RTXHT-no-such-volume\\HeadTracking.ini"),
          "an unusable path reports failure rather than pretending to load");
    Check(cfg.udpPort == defaults.udpPort, "the compiled defaults survive that failure");
}

void TestAbsentFileWritesDefaultsThatReadBackUnchanged() {
    const std::string path = TempIniPath("roundtrip");
    std::remove(path.c_str());

    const Q2RTXHT::Config defaults;
    Q2RTXHT::Config written;
    Check(written.LoadOrCreate(path), "a missing ini is created and then read");

    // The strict parser the guards use is narrower than strtod, so a file the
    // mod writes itself is not automatically a file it can read: a value
    // emitted with a locale comma or a stray space would fall back to the
    // compiled default and the shipped .ini would be silently inert.
    Check(written.udpPort == defaults.udpPort, "round-trip: UdpPort");
    Check(written.yawSensitivity == defaults.yawSensitivity, "round-trip: YawSensitivity");
    Check(written.localSmoothing == defaults.localSmoothing, "round-trip: LocalSmoothing");
    Check(written.remoteSmoothing == defaults.remoteSmoothing, "round-trip: RemoteSmoothing");
    Check(written.worldSpaceYaw == defaults.worldSpaceYaw, "round-trip: WorldSpaceYaw");
    Check(written.posLimitX == defaults.posLimitX, "round-trip: LimitX");
    Check(written.posLimitY == defaults.posLimitY, "round-trip: LimitY");
    Check(written.posLimitZ == defaults.posLimitZ, "round-trip: LimitZ");
    Check(written.posLimitZBack == defaults.posLimitZBack, "round-trip: LimitZBack");
    Check(written.positionUnitsPerMeter == defaults.positionUnitsPerMeter,
          "round-trip: UnitsPerMeter");
    Check(written.collisionEnabled == defaults.collisionEnabled,
          "round-trip: CollisionEnabled");
    Check(written.collisionStandoff == defaults.collisionStandoff,
          "round-trip: CollisionMargin");
    Check(written.collisionReleaseSmoothing == defaults.collisionReleaseSmoothing,
          "round-trip: CollisionReleaseSmoothing");
    Check(written.keyToggle == defaults.keyToggle, "round-trip: ToggleKey");
    Check(written.keyTogglePosition == defaults.keyTogglePosition,
          "round-trip: TogglePositionKey");
    Check(written.keyToggleYaw == defaults.keyToggleYaw, "round-trip: ToggleYawKey");

    std::remove(path.c_str());
}

// config/HeadTracking.ini ships in the installer ZIP and is what install.cmd
// seeds; WriteDefault is what a player who deletes it gets instead. They are
// two copies of one document, kept in step by hand, and they have already
// drifted once - a comment on one side only. Comparing the bytes is the only
// thing that catches it.
void TestShippedIniIsExactlyWhatTheModWritesForItself() {
    // In the working directory rather than the temp directory: this is the one
    // test that reads a written file back through the CRT rather than through
    // GetPrivateProfileString, and a sandboxed or locked-down temp directory
    // reads back empty, which would look like WriteDefault writing nothing.
    const std::string generated = "q2rtxht-shipped-compare.ini";
    std::remove(generated.c_str());

    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(generated), "the default ini is written for comparison");

    const std::string shipped = ReadFileWithoutCarriageReturns(Q2RTXHT_SHIPPED_INI);
    const std::string emitted = ReadFileWithoutCarriageReturns(generated);
    Check(!shipped.empty(), "config/HeadTracking.ini is readable");
    const bool same = shipped == emitted;
    Check(same, "config/HeadTracking.ini is byte-identical to what Config::WriteDefault emits");
    if (!same) {
        std::istringstream a(shipped), b(emitted);
        std::string la, lb;
        for (int line = 1;; ++line) {
            const bool ha = static_cast<bool>(std::getline(a, la));
            const bool hb = static_cast<bool>(std::getline(b, lb));
            if (!ha && !hb) break;
            if (ha == hb && la == lb) continue;
            std::cout << "        first difference at line " << line << "\n"
                      << "        shipped:      " << (ha ? la : std::string("<end of file>"))
                      << "\n"
                      << "        WriteDefault: " << (hb ? lb : std::string("<end of file>"))
                      << "\n";
            break;
        }
    }

    std::remove(generated.c_str());
}

// The version exists in five places and release.ps1 stamps all of them, so the
// way it goes wrong is a hand-edit to one. Three are worth failing the build
// over: the string the mod logs on attach, the version the launcher installs
// by, and install.cmd's MOD_VERSION, which the install body writes into
// .headtracking-state.json as the installed version. A log line or a receipt
// naming a version that was never released sends triage at the wrong binary.
//
// The two JSON scans skip whatever whitespace follows the colon rather than
// pinning one space, because a writer that reformats the file is a release
// problem and not a reason to fail the suite.
std::string JsonStringValue(const std::string& document, const std::string& key) {
    const std::string quoted = "\"" + key + "\"";
    size_t at = document.find(quoted);
    if (at == std::string::npos) return {};
    at = document.find(':', at + quoted.size());
    if (at == std::string::npos) return {};
    at = document.find('"', at + 1);
    if (at == std::string::npos) return {};
    const size_t end = document.find('"', at + 1);
    if (end == std::string::npos) return {};
    return document.substr(at + 1, end - at - 1);
}

// launcher-manifest.json carries the same file a third time, base64 in its
// loader.seed block, because that is how the launcher seeds a config where
// install.cmd copies one. Nothing else compares them: the manifest validator
// counts seeds without reading them, so a change to WriteDefault would move the
// .ini (the test above forces that) and leave the manifest seeding the old one,
// which is the two-delivery-paths-disagree bug the seed was added to fix.
void TestLauncherManifestSeedsTheSameIni() {
    const std::string manifest = ReadFileWithoutCarriageReturns(Q2RTXHT_MANIFEST);
    Check(!manifest.empty(), "launcher-manifest.json is readable");

    const std::string encoded = JsonStringValue(manifest, "content_b64");
    Check(!encoded.empty(), "launcher-manifest.json carries a loader.seed content_b64");
    if (encoded.empty()) return;

    const std::string decoded = DecodeBase64(encoded);
    const std::string shipped = ReadFileWithoutCarriageReturns(Q2RTXHT_SHIPPED_INI);
    Check(decoded == shipped,
          "the launcher-manifest.json seed decodes to exactly config/HeadTracking.ini");
}

void TestTheVersionAgreesEverywhereItIsWritten() {
    Check(std::string(Q2RTXHT::MOD_VERSION) == Q2RTXHT_PROJECT_VERSION,
          "src/version.h and CMakeLists.txt name the same version");

    const std::string manifest = ReadFileWithoutCarriageReturns(Q2RTXHT_MANIFEST);
    Check(JsonStringValue(manifest, "version") == Q2RTXHT::MOD_VERSION,
          "launcher-manifest.json names the version src/version.h does");

    const std::string install = ReadFileWithoutCarriageReturns(Q2RTXHT_INSTALL_CMD);
    const std::string marker = "set \"MOD_VERSION=";
    const size_t at = install.find(marker);
    Check(at != std::string::npos, "install.cmd carries a MOD_VERSION");
    if (at == std::string::npos) return;
    const size_t from = at + marker.size();
    Check(install.substr(from, install.find('"', from) - from) == Q2RTXHT::MOD_VERSION,
          "install.cmd names the version src/version.h does");
}

void TestValidSettingsSurviveUntouched() {
    const std::string path = TempIniPath("valid");
    WriteFile(path,
              "[General]\n"
              "Enabled=0\n"
              "UdpPort=5967\n"
              "[Rotation]\n"
              "YawSensitivity=1.5\n"
              "LocalSmoothing=0.25\n"
              "[Position]\n"
              "LimitZ=0.35\n"
              "UnitsPerMeter=40.0\n"
              "[Controls]\n"
              "ToggleKey=0x76\n");

    Q2RTXHT::Config cfg;
    Check(cfg.LoadOrCreate(path), "a valid ini loads");
    Check(!cfg.enabled, "Enabled=0 is honoured");
    Check(cfg.udpPort == 5967, "a non-default in-range port is honoured");
    Check(cfg.yawSensitivity == 1.5f, "an in-range sensitivity is untouched");
    Check(cfg.localSmoothing == 0.25f, "an in-range smoothing is untouched, not floored");
    Check(cfg.posLimitZ == 0.35f, "an in-range travel limit is untouched");
    Check(cfg.positionUnitsPerMeter == 40.0f, "UnitsPerMeter is untouched");
    Check(cfg.keyToggle == 0x76, "a bindable key code is honoured");

    std::remove(path.c_str());
}

}  // namespace

int RunConfigTests() {
    std::cout << "\nConfig boundary tests\n";
    TestHostileValuesNeverReachTheCamera();
    TestCollisionMarginCannotBeSetBelowTheMeasuredStandoff();
    TestCollidingHotkeysAreSeparated();
    TestKeyMovedOntoAnotherActionsDefaultIsSeparated();
    TestThreeWayHotkeyCollisionLeavesNoSharedKey();
    TestUnusablePathReportsFailureAndKeepsDefaults();
    TestAbsentFileWritesDefaultsThatReadBackUnchanged();
    TestShippedIniIsExactlyWhatTheModWritesForItself();
    TestLauncherManifestSeedsTheSameIni();
    TestTheVersionAgreesEverywhereItIsWritten();
    TestValidSettingsSurviveUntouched();

    if (g_failures == 0) {
        std::cout << "Config boundary tests: all passed\n";
    } else {
        std::cout << "Config boundary tests: " << g_failures << " failure(s)\n";
    }
    return g_failures;
}
