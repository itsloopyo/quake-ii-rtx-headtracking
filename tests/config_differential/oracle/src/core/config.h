#pragma once

#include <string>
#include <cstdint>

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace Q2RTXHT {

// All defaults follow the CameraUnlock doctrine (AGENTS.md "Configuration
// Defaults"). Loaded from HeadTracking.ini next to the game exe; a default
// file is written if none exists.
struct Config {
    // [General]
    bool enabled = true;
    uint16_t udpPort = 4242;

    // [Rotation]
    float yawSensitivity = 1.0f;
    float pitchSensitivity = 1.0f;
    float rollSensitivity = 1.0f;
    bool invertYaw = false;
    bool invertPitch = false;
    bool invertRoll = false;
    // Smoothing is chosen per connection: local for a tracker on this machine
    // (loopback), remote for a device on the network. Both cover rotation and
    // position.
    float localSmoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);
    // Yaw about world up (Quake's +Z), which keeps the horizon level when the
    // head is pitched. Page Down / Ctrl+Shift+H switches to camera-local.
    bool worldSpaceYaw = true;

    // [Position]
    bool positionEnabled = true;
    float posSensX = 1.0f;
    float posSensY = 1.0f;
    float posSensZ = 1.0f;
    // meters
    float posLimitX = cameraunlock::PositionSettings{}.limit_x;
    float posLimitY = cameraunlock::PositionSettings{}.limit_y;
    // forward
    float posLimitZ = cameraunlock::PositionSettings{}.limit_z;
    // backward
    float posLimitZBack = cameraunlock::PositionSettings{}.limit_z_back;
    // Quake units per real-world meter. vieworg is in Quake units (~1 unit per
    // inch), so the processed metre offset is scaled by this before it is added
    // to the camera position.
    float positionUnitsPerMeter = 40.0f;

    // [Collision] - keeps a lean from putting the eye inside the level.
    bool collisionEnabled = true;
    // Quake units held off a blocking surface. The path tracer starts primary
    // rays at the eye, so this only has to keep the eye out of solid; 4 units
    // is a quarter of the player's own 16-unit half-width.
    float collisionStandoff = 4.0f;
    float collisionReleaseSmoothing = 0.9f;

    // [Controls] - nav-cluster virtual key codes (chords are fixed in code).
    int keyToggle = 0x23;          // End
    int keyTogglePosition = 0x21;  // PageUp
    int keyToggleYaw = 0x22;       // PageDown

    // Loads from path, writing a documented default file first if it is absent.
    // Returns false only on unrecoverable IO failure.
    bool LoadOrCreate(const std::string& path);

private:
    void WriteDefault(const std::string& path) const;
    // Two actions on one key fire both from one press. Reverts the later of any
    // colliding pair to its compiled default.
    void RejectDuplicateKeys();
};

}  // namespace Q2RTXHT
