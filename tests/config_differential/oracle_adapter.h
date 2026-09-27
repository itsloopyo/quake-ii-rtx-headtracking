#pragma once

// The oracle: the HeadTracking.ini reader and hotkey registration of the dev pre-release
// (b6665cf), the newest published build, compiled from oracle/ with the core sources they
// included at its pin (fb55a6a). Two libraries build it, each with its namespaces renamed at
// compile time so it links beside the current core: the reader, and the registration against
// oracle_fake's recording poller. This header names no core type, so the test includes it
// without the renaming.

#include <array>
#include <string>
#include <vector>

namespace q2_oracle_view {

struct OracleConfig {
    bool enabled;
    int udpPort;
    float yawSensitivity, pitchSensitivity, rollSensitivity;
    bool invertYaw, invertPitch, invertRoll;
    float localSmoothing, remoteSmoothing;
    bool worldSpaceYaw;
    bool positionEnabled;
    float posSensX, posSensY, posSensZ;
    float posLimitX, posLimitY, posLimitZ, posLimitZBack;
    float positionUnitsPerMeter;
    bool collisionEnabled;
    float collisionStandoff, collisionReleaseSmoothing;
    int keyToggle, keyTogglePosition, keyToggleYaw;
    // LoadOrCreate's result: false when the file could not be opened, the mod then running on
    // the defaults.
    bool loaded;
};

// Config::LoadOrCreate on `path` as the dev build ran it, from a default Config. It creates the
// file when there is none, as that build did.
OracleConfig RunOracle(const std::string& path);

// Which actions a key press fires, for every key a binding can name (0x01-0xFE) under every set
// of held modifiers. Entry (vk - kFirstKey) * kHeldStates + held counts the toggle, mode cycle
// and yaw mode actions fired, in that order. held: 1 Ctrl, 2 Shift, 4 Alt.
constexpr int kFirstKey = 0x01;
constexpr int kLastKey = 0xFE;
constexpr int kHeldStates = 8;
constexpr int kActions = 3;
using FireTable = std::vector<std::array<int, kActions>>;

// The dev build's Mod::RegisterHotkeys run on the three codes, pressing each key under each held
// set.
FireTable OracleFires(int keyToggle, int keyTogglePosition, int keyToggleYaw);

}  // namespace q2_oracle_view
