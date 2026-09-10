#pragma once

#include <atomic>
#include <cstdint>

#include "core/config.h"
#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/tracking/head_tracking_session.h"
#include "cameraunlock/input/hotkey_poller.h"

namespace Q2RTXHT {

struct BuildProfile;

class Mod {
public:
    static Mod& Instance();

    // moduleBase is the q2rtx.exe base address. Loads config, fingerprints the
    // exe, installs the render hook if the build is known, starts the receiver
    // and hotkeys. Returns false on a fatal setup failure (mod then dormant).
    bool Initialize(void* moduleBase);

    bool IsEnabled() const { return m_enabled.load(std::memory_order_relaxed); }
    const Config& GetConfig() const { return m_config; }

    // Separate from Config::worldSpaceYaw, which is the loaded default and is
    // never written after Initialize. This one is flipped from the hotkey
    // thread and read from the render thread every frame, so it has to be
    // atomic - a plain bool written and read across two threads is a race, and
    // the rest of Config is read-only once loaded precisely so it is not one.
    bool WorldSpaceYaw() const { return m_worldSpaceYaw.load(std::memory_order_relaxed); }

    // Called once per rendered frame from the hook: advances the tracking
    // pipeline with the measured frame delta and caches the result.
    void UpdateTracking();

    bool GetProcessedRotation(float& yaw, float& pitch, float& roll) const;
    bool GetPositionOffset(float& x, float& y, float& z) const;

    // Seconds covered by the last UpdateTracking(), for anything else that has
    // to advance per frame (the lean clamp's release ease).
    float LastDeltaTime() const { return m_lastDeltaTime; }

    void Toggle();
    void CycleMode();
    void ToggleYawMode();

    Mod(const Mod&) = delete;
    Mod& operator=(const Mod&) = delete;

private:
    Mod() : m_session(m_receiver) {}
    ~Mod() = default;

    // The steps Initialize() walks, in the order it walks them.
    void LoadConfiguration();
    void StartFrameClock();
    // The matching profile, or nullptr when the running exe is not a build this
    // mod has offsets for - the caller then leaves the mod dormant.
    const BuildProfile* ResolveBuildProfile(void* moduleBase) const;
    void ApplyConfigToPipeline();
    void StartReceiver();
    void RegisterHotkeys();
    // Logs which smoothing parameter is in force when the session switches
    // between a local and a remote tracker. The session does the selection.
    void LogConnectionChange();

    Config m_config;
    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    // Without IsRemoteConnection() on the receiver the session silently falls
    // back to LocalSmoothing forever, with nothing at the call site to show it.
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() or remote smoothing never applies");
    cameraunlock::input::HotkeyPoller m_hotkeys;

    std::atomic<bool> m_enabled{true};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_worldSpaceYaw{true};

    int64_t m_lastQpc = 0;
    int64_t m_qpcFreq = 0;

    bool m_isRemoteConnection = false;
    // Tri-state: false/false is indistinguishable from a local tracker, so a
    // plain equality check never reports the (common) local case at all.
    bool m_remoteConnectionKnown = false;

    float m_yaw = 0.0f, m_pitch = 0.0f, m_roll = 0.0f;
    bool m_rotationValid = false;
    float m_posX = 0.0f, m_posY = 0.0f, m_posZ = 0.0f;
    bool m_positionValid = false;
    float m_lastDeltaTime = 0.0f;
};

}  // namespace Q2RTXHT
