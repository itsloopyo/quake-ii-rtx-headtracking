#pragma once

// The HeadTracking.ini reader of the dev pre-release (b6665cf), the only build ever published,
// frozen so a player updating from it is converted exactly as that build read the file. Nothing in
// this folder is ever edited. Three things differ from the reader it was taken from: it fills this
// frozen copy of that build's Config and defaults rather than the runtime type, it never writes
// the file (a missing file reads as the defaults, which is what the old reader read from the file
// it created there), and it reports an absent file apart from one it read. The core default
// values the old Config took from PositionSettings and smoothing_utils.h, and the port check it
// took from port_utils.h, are written out here, so a later core cannot move what an old file
// converts to.

#include "cameraunlock/config/legacy_import.h"

#include <cstdint>
#include <vector>

namespace Q2RTXHT::legacy {

enum class ReadStatus {
    Read,
    // No file at the path, or none the old reader could open. Config holds the defaults.
    Absent,
};

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
    float localSmoothing = 0.0f;
    float remoteSmoothing = 0.15f;
    bool worldSpaceYaw = true;

    // [Position]
    bool positionEnabled = true;
    float posSensX = 1.0f;
    float posSensY = 1.0f;
    float posSensZ = 1.0f;
    float posLimitX = 0.30f;
    float posLimitY = 0.20f;
    float posLimitZ = 0.40f;
    float posLimitZBack = 0.10f;
    float positionUnitsPerMeter = 40.0f;

    // [Collision]
    bool collisionEnabled = true;
    float collisionStandoff = 4.0f;
    float collisionReleaseSmoothing = 0.9f;

    // [Controls]
    int keyToggle = 0x23;          // End
    int keyTogglePosition = 0x21;  // PageUp
    int keyToggleYaw = 0x22;       // PageDown
};

// Reads the file at `path`, the ANSI path the dev build opened it by, into a default-constructed
// `c`.
ReadStatus Read(const char* path, Config& c);

// Every section and key Read reads for a value, in the order it reads them. The retired keys it
// only warns about are left out, so the owner reports them as not carried over.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace Q2RTXHT::legacy
