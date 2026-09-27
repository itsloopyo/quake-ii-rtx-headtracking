#pragma once

#include <cstdint>
#include <string>

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace Q2RTXHT {

// Beside q2rtx.exe. ConfigOwner reads and writes it; nothing else in the mod touches it.
constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file the dev pre-release read, beside kConfigFileName. Imported once while kConfigFileName
// is absent, and never written.
constexpr const char* kLegacyConfigFileName = "HeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Quake II RTX";

// Quake units per real-world metre. vieworg is in Quake units (~1 unit per inch), so the processed
// metre offset is scaled by this before it is added to the camera position. One metre is 39.37
// Quake units, and 40 is what every build has shipped.
constexpr float kUnitsPerMeter = 40.0f;

// Quake units held off a blocking surface. The player's own half-width is 16, so a standoff past
// four of those cuts every lean before it starts. The floor is the shipped 4, the only standoff
// confirmed in game to leave a wall rendering solid at the eye; smaller values have not been
// tried, and 0 rests the eye exactly on the surface, where the near plane culls it.
constexpr float kMinCollisionMargin = 4.0f;
constexpr float kMaxCollisionMargin = 64.0f;
constexpr float kDefaultCollisionMargin = 4.0f;

struct Config {
    uint16_t udpPort = 4242;
    bool enableOnStartup = true;
    // The startup tracking mode. The mode hotkey saves both.
    bool rotationEnabled = true;
    bool positionEnabled = true;

    // Smoothing is chosen per connection: local for a tracker on this machine (loopback), remote
    // for a device on the network. Both cover rotation and position.
    float localSmoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);
    // Yaw about world up (Quake's +Z), which keeps the horizon level when the head is pitched. The
    // yaw mode hotkey switches it and saves it.
    bool worldSpaceYaw = true;

    // How far the eye may leave the body, in metres.
    float posLimitX = cameraunlock::PositionSettings{}.limit_x;
    float posLimitY = cameraunlock::PositionSettings{}.limit_y;
    float posLimitYDown = cameraunlock::PositionSettings{}.limit_y_down;
    float posLimitZ = cameraunlock::PositionSettings{}.limit_z;
    float posLimitZBack = cameraunlock::PositionSettings{}.limit_z_back;

    // Keeps a lean from putting the eye inside the level.
    bool collisionEnabled = true;
    float collisionStandoff = kDefaultCollisionMargin;
    float collisionReleaseSmoothing = 0.9f;

    std::string toggleKey = "End, Ctrl+Shift+Y";
    std::string cycleTrackingModeKey = "PageUp, Ctrl+Shift+G";
    std::string yawModeKey = "PageDown, Ctrl+Shift+H";
};

// The rows of CameraUnlock.ini. The tracking mode pair and WorldSpaceYaw are Writable: the mode
// and yaw hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// HeadTracking.ini as the dev build read it (legacy_config/), mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from HeadTracking.ini. The mod passes DefaultsFile::PerUser()
// and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace Q2RTXHT
