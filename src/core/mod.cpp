#include "core/mod.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

#include "version.h"
#include "core/build_profiles.h"
#include "hooks/render_hook.h"
#include "hooks/crosshair_hook.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/math/smoothing_utils.h"
#include "cameraunlock/memory/pe_fingerprint.h"
#include "cameraunlock/input/chord_hotkeys.h"
#include "cameraunlock/os/module_paths.h"

namespace Q2RTXHT {

namespace log = cameraunlock::logging;
namespace os = cameraunlock::os;

namespace {

constexpr char kConfigFileName[] = "\\HeadTracking.ini";

// A frame this long was not rendered - the process was suspended by an alt-tab
// or a level load - so advancing smoothing by it would snap the view.
constexpr float kMaxFrameDeltaSeconds = 0.25f;

// ~60Hz, fast enough that a tap on a nav-cluster key is never missed.
constexpr int kHotkeyPollIntervalMs = 16;

}  // namespace

Mod& Mod::Instance() {
    // Deliberately never destroyed. A function-local static would put ~Mod on
    // the CRT's atexit list, which runs from DLL_PROCESS_DETACH - and ~Mod
    // destroys the hotkey poller and the UDP receiver, so it would join both
    // their threads and call closesocket and WSACleanup under the loader lock.
    // That is exactly what DllMain stopped doing by hand. The process is on its
    // way out; the OS reclaims this.
    static Mod* instance = new Mod();
    return *instance;
}

void Mod::LoadConfiguration() {
    // Narrow because IniReader wraps the ANSI GetPrivateProfile* API. An install
    // path with no ANSI form yields an empty string, and building a relative
    // path out of that would read and write a different folder entirely.
    const std::string dir = os::HostExeDirectoryNarrow();
    if (dir.empty()) {
        log::Line("[mod] the game directory has no ANSI form; running on built-in "
                  "defaults instead of HeadTracking.ini");
    } else if (!m_config.LoadOrCreate(dir + kConfigFileName)) {
        log::Line("[mod] HeadTracking.ini could not be opened; running on built-in defaults");
    }
    m_enabled.store(m_config.enabled, std::memory_order_relaxed);
    m_worldSpaceYaw.store(m_config.worldSpaceYaw, std::memory_order_relaxed);
}

void Mod::StartFrameClock() {
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_qpcFreq));
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m_lastQpc = now.QuadPart;
}

// Only a build with pinned offsets is hooked. Anything else - a patch we have
// not seen, a repacked exe - leaves the mod dormant and the game vanilla, with
// the log saying which direction the mismatch runs.
const BuildProfile* Mod::ResolveBuildProfile(void* moduleBase) const {
    cameraunlock::memory::PeFingerprint running{};
    if (!cameraunlock::memory::ReadPeFingerprint(moduleBase, running)) {
        log::Line("[mod] failed to read PE fingerprint; staying dormant");
        return nullptr;
    }
    log::Line("[mod] running fingerprint TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X",
              running.TimeDateStamp, running.SizeOfImage, running.CheckSum);

    const BuildProfile* profile = FindMatchingProfile(running);
    if (!profile) {
        const BuildProfile& primary = DiagnosticPrimary();
        auto mismatch = cameraunlock::memory::ClassifyMismatch(running, primary.fingerprint);
        const char* hint = "tampered/repacked exe; mod will not engage";
        if (mismatch == cameraunlock::memory::FingerprintMismatch::Newer)
            hint = "game is newer than this mod knows about; check for a mod update";
        else if (mismatch == cameraunlock::memory::FingerprintMismatch::Older)
            hint = "game is older; let Steam finish updating";
        log::Line("[mod] no matching build profile (%d known); %s", KnownProfileCount(), hint);
        return nullptr;
    }
    log::Line("[mod] matched build profile '%s'", profile->name);
    return profile;
}

void Mod::StartReceiver() {
    // The receiver latches its own one-shot lines (first packet, bind after
    // retry, a second tracker on the port). Without this callback a working
    // install and a mod nothing is sending to produce identical logs.
    m_receiver.SetLog([](const std::string& msg) { log::Line("[udp] %s", msg.c_str()); });

    // Start() returns false when the port is momentarily unavailable (e.g.
    // another OpenTrack consumer still holds it); the receiver keeps retrying
    // in the background, so this is a warning, not a fatal error.
    if (m_receiver.Start(m_config.udpPort)) {
        log::Line("[mod] UDP receiver listening on port %u", m_config.udpPort);
        return;
    }
    // What went wrong is on the [udp] line above, in the OS's own words.
    // Restating it here as a port conflict would be a guess: a bind also
    // fails inside a Hyper-V/WSL reserved range with nothing holding the
    // port, and naming the wrong cause sends the user hunting an app that
    // is not running.
    log::Line("[mod] UDP port %u not bound yet; the receiver retries every %dms until it is",
              m_config.udpPort, cameraunlock::UdpReceiver::kRetryIntervalMs);
}

bool Mod::Initialize(void* moduleBase) {
    if (m_initialized.load(std::memory_order_acquire)) return true;

    LoadConfiguration();
    StartFrameClock();

    const BuildProfile* profile = ResolveBuildProfile(moduleBase);
    if (!profile) return false;

    ApplyConfigToPipeline();
    StartReceiver();

    if (!InstallRenderHook(moduleBase, *profile)) {
        // The receiver is already listening at this point and nothing else will
        // stop it: an ASI is never unloaded, so there is no later teardown to
        // catch a socket and supervisor thread left running with nothing to
        // feed.
        m_receiver.Stop();
        log::Line("[mod] failed to start render hook installer");
        return false;
    }

    // Crosshair compensation is a refinement; a failure here must not take down
    // head tracking, so it is logged but not fatal.
    if (!InstallCrosshairHook(moduleBase, *profile)) {
        log::Line("[mod] crosshair hook unavailable; aim feedback uncompensated");
    }

    RegisterHotkeys();

    m_initialized.store(true, std::memory_order_release);
    log::Line("[mod] %s v%s initialized", MOD_NAME, MOD_VERSION);
    return true;
}

void Mod::ApplyConfigToPipeline() {
    auto& proc = m_session.GetProcessor();

    cameraunlock::SensitivitySettings sens;
    sens.yaw = m_config.yawSensitivity;
    sens.pitch = m_config.pitchSensitivity;
    sens.roll = m_config.rollSensitivity;
    sens.invert_yaw = m_config.invertYaw;
    sens.invert_pitch = m_config.invertPitch;
    sens.invert_roll = m_config.invertRoll;
    proc.SetSensitivity(sens);

    auto& posProc = m_session.GetPositionProcessor();

    cameraunlock::PositionSettings pos;
    pos.sensitivity_x = m_config.posSensX;
    pos.sensitivity_y = m_config.posSensY;
    pos.sensitivity_z = m_config.posSensZ;
    pos.limit_x = m_config.posLimitX;
    // The clamp is [-limit_y_down, +limit_y] and limit_y_down carries its own
    // default, so mirror the one configured vertical limit the way
    // PositionSettings::Symmetric does. Left unset, raising LimitY widened the
    // upward budget only and downward travel stayed pinned at 0.20m.
    pos.limit_y = m_config.posLimitY;
    pos.limit_y_down = m_config.posLimitY;
    pos.limit_z = m_config.posLimitZ;
    pos.limit_z_back = m_config.posLimitZBack;
    // The tracker-to-Quake axis signs live in quake_math and are applied at the
    // engine boundary, after the clamp. The processor inverts BEFORE the
    // asymmetric Z clamp, so flipping an axis here would swap the generous
    // 0.40m forward allowance onto the backward lean.
    pos.invert_x = false;
    pos.invert_y = false;
    pos.invert_z = false;
    posProc.SetSettings(pos);

    // After SetSettings: the session writes both smoothing values into the
    // position settings too, so a later settings rebuild would drop them. The
    // session feeds the connection flag that picks between them, from the
    // receiver's source address, every update.
    m_session.SetLocalSmoothing(m_config.localSmoothing);
    m_session.SetRemoteSmoothing(m_config.remoteSmoothing);

    m_session.SetMode(m_config.positionEnabled
                          ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);
}

void Mod::LogConnectionChange() {
    const bool isRemote = m_session.IsRemoteConnection();
    if (m_remoteConnectionKnown && isRemote == m_isRemoteConnection) return;
    m_isRemoteConnection = isRemote;
    m_remoteConnectionKnown = true;

    const double effective = cameraunlock::math::GetEffectiveSmoothing(
        m_config.localSmoothing, m_config.remoteSmoothing, isRemote);
    log::Line("[mod] tracker connection is %s; smoothing=%.3f",
              isRemote ? "remote" : "local", effective);
}

void Mod::RegisterHotkeys() {
    using namespace cameraunlock::input;
    // Nav-cluster keys (fire only when the chord modifier is NOT held).
    m_hotkeys.AddHotkey(m_config.keyToggle, NavGuarded([] { Mod::Instance().Toggle(); }));
    m_hotkeys.AddHotkey(m_config.keyTogglePosition, NavGuarded([] { Mod::Instance().CycleMode(); }));
    m_hotkeys.AddHotkey(m_config.keyToggleYaw, NavGuarded([] { Mod::Instance().ToggleYawMode(); }));

    // Chord alternatives: Ctrl+Shift+ Y / G / H (T/Y/U/G/H/J cluster).
    m_hotkeys.AddHotkey('Y', ChordGuarded([] { Mod::Instance().Toggle(); }));
    m_hotkeys.AddHotkey('G', ChordGuarded([] { Mod::Instance().CycleMode(); }));
    m_hotkeys.AddHotkey('H', ChordGuarded([] { Mod::Instance().ToggleYawMode(); }));

    m_hotkeys.Start(kHotkeyPollIntervalMs);
}

void Mod::UpdateTracking() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = static_cast<float>(static_cast<double>(now.QuadPart - m_lastQpc) /
                                  static_cast<double>(m_qpcFreq));
    m_lastQpc = now.QuadPart;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > kMaxFrameDeltaSeconds) dt = kMaxFrameDeltaSeconds;
    m_lastDeltaTime = dt;

    if (m_session.Update(dt)) {
        LogConnectionChange();
        m_rotationValid = m_session.GetRotation(m_yaw, m_pitch, m_roll);
        m_positionValid = m_session.GetPositionOffset(m_posX, m_posY, m_posZ);
    } else {
        m_rotationValid = false;
        m_positionValid = false;
    }
}

bool Mod::GetProcessedRotation(float& yaw, float& pitch, float& roll) const {
    yaw = m_yaw; pitch = m_pitch; roll = m_roll;
    return m_rotationValid;
}

bool Mod::GetPositionOffset(float& x, float& y, float& z) const {
    x = m_posX; y = m_posY; z = m_posZ;
    return m_positionValid;
}

void Mod::Toggle() {
    bool now = !m_enabled.load(std::memory_order_relaxed);
    m_enabled.store(now, std::memory_order_relaxed);
    log::Line("[mod] tracking %s", now ? "enabled" : "disabled");
}

void Mod::CycleMode() {
    switch (m_session.CycleMode()) {
        case cameraunlock::TrackingMode::RotationOnly:
            log::Line("[mod] tracking mode: rotation only");
            break;
        case cameraunlock::TrackingMode::PositionOnly:
            log::Line("[mod] tracking mode: position only");
            break;
        case cameraunlock::TrackingMode::RotationAndPosition:
            log::Line("[mod] tracking mode: rotation and position");
            break;
    }
}

void Mod::ToggleYawMode() {
    const bool worldSpace = !m_worldSpaceYaw.load(std::memory_order_relaxed);
    m_worldSpaceYaw.store(worldSpace, std::memory_order_relaxed);
    log::Line("[mod] yaw mode: %s", worldSpace ? "world-locked" : "camera-local");
}

}  // namespace Q2RTXHT
