#include "legacy_config/legacy_config.h"

#include <cctype>
#include <string>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"
#include "cameraunlock/logging/file_log.h"

namespace Q2RTXHT::legacy {

namespace guards = cameraunlock::config;

namespace {

constexpr guards::LogSink kLog = &cameraunlock::logging::Line;

constexpr float kMinUnitsPerMeter = 32.0f;
constexpr float kMaxUnitsPerMeter = 48.0f;

constexpr float kMinCollisionStandoff = 4.0f;
constexpr float kMaxCollisionStandoff = 64.0f;

constexpr float kMinPositionLimit = 0.01f;
constexpr float kMaxPositionLimit = 0.5f;

constexpr float kMinSensitivity = 0.01f;

// port_utils.h NormalizeUdpPort at the dev build's core pin (fb55a6a).
uint16_t NormalizeUdpPort(int raw, uint16_t fallback, bool& valid) {
    valid = (raw >= 1024 && raw <= 65535);
    return valid ? static_cast<uint16_t>(raw) : fallback;
}

float ReadSensitivity(const cameraunlock::IniReader& ini, const char* section,
                      const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, kMinSensitivity,
                                    guards::kMaxSensitivity, kLog);
}

float ReadPositionLimit(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, "Position", key, fallback, kMinPositionLimit,
                                    kMaxPositionLimit, kLog);
}

float ReadUnitInterval(const cameraunlock::IniReader& ini, const char* section,
                       const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, 0.0f, 1.0f, kLog);
}

bool ReadBoolChecked(const cameraunlock::IniReader& ini, const char* section,
                     const char* key, bool fallback) {
    const std::string raw = guards::ReadRawValue(ini, section, key);
    if (raw.empty()) return fallback;

    std::string token;
    for (const char c : raw) {
        token += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (token == "1" || token == "true" || token == "yes" || token == "on") return true;
    if (token == "0" || token == "false" || token == "no" || token == "off") return false;

    kLog("config: [%s] %s=%s is not a yes/no value; using %d", section, key, raw.c_str(),
         fallback ? 1 : 0);
    return fallback;
}

int ReadVirtualKey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const int vk = ini.ReadHex("Controls", key, fallback);
    if (guards::IsBindableVirtualKey(vk)) return vk;
    kLog("config: [Controls] %s=0x%X is not a key this mod can bind; using 0x%02X",
         key, vk, fallback);
    return fallback;
}

void WarnRetiredKey(const cameraunlock::IniReader& ini, const char* section,
                    const char* key, const char* advice) {
    if (guards::ReadRawValue(ini, section, key).empty()) return;
    kLog("config: [%s] %s has been retired and is IGNORED. %s", section, key, advice);
}

void RejectDuplicateKeys(Config& c) {
    const Config defaults;
    struct Binding { const char* name; int* value; int fallback; };
    const Binding bindings[] = {
        { "ToggleKey", &c.keyToggle, defaults.keyToggle },
        { "TogglePositionKey", &c.keyTogglePosition, defaults.keyTogglePosition },
        { "ToggleYawKey", &c.keyToggleYaw, defaults.keyToggleYaw },
    };
    constexpr size_t kCount = sizeof(bindings) / sizeof(bindings[0]);

    int taken[kCount] = {};
    size_t takenCount = 0;
    auto isTaken = [&](int vk) {
        for (size_t k = 0; k < takenCount; ++k) {
            if (taken[k] == vk) return true;
        }
        return false;
    };

    for (size_t i = 0; i < kCount; ++i) {
        int* value = bindings[i].value;
        if (!isTaken(*value)) {
            taken[takenCount++] = *value;
            continue;
        }
        const int clashed = *value;
        if (!isTaken(bindings[i].fallback)) {
            *value = bindings[i].fallback;
            kLog("config: [Controls] %s=0x%02X is already bound to another action; "
                 "using 0x%02X instead",
                 bindings[i].name, clashed, *value);
            taken[takenCount++] = *value;
        } else {
            *value = 0;
            kLog("config: [Controls] %s=0x%02X is already bound to another action, and "
                 "its default 0x%02X is taken too; leaving it unbound. Use its "
                 "Ctrl+Shift chord, or pick a free key code",
                 bindings[i].name, clashed, bindings[i].fallback);
        }
    }
}

}  // namespace

ReadStatus Read(const char* path, Config& c) {
    cameraunlock::IniReader ini;
    if (!ini.Open(path)) {
        return ReadStatus::Absent;
    }

    c.enabled = ReadBoolChecked(ini, "General", "Enabled", c.enabled);

    bool portValid = false;
    c.udpPort = NormalizeUdpPort(ini.ReadInt("General", "UdpPort", c.udpPort), c.udpPort, portValid);
    if (!portValid) {
        kLog("config: [General] UdpPort is outside 1024-65535; using %u", c.udpPort);
    }

    c.yawSensitivity = ReadSensitivity(ini, "Rotation", "YawSensitivity", c.yawSensitivity);
    c.pitchSensitivity = ReadSensitivity(ini, "Rotation", "PitchSensitivity", c.pitchSensitivity);
    c.rollSensitivity = ReadSensitivity(ini, "Rotation", "RollSensitivity", c.rollSensitivity);
    c.invertYaw = ReadBoolChecked(ini, "Rotation", "InvertYaw", c.invertYaw);
    c.invertPitch = ReadBoolChecked(ini, "Rotation", "InvertPitch", c.invertPitch);
    c.invertRoll = ReadBoolChecked(ini, "Rotation", "InvertRoll", c.invertRoll);
    c.localSmoothing = ReadUnitInterval(ini, "Rotation", "LocalSmoothing", c.localSmoothing);
    c.remoteSmoothing = ReadUnitInterval(ini, "Rotation", "RemoteSmoothing", c.remoteSmoothing);
    guards::WarnRetiredSmoothingKey(ini, "Rotation", "Smoothing", kLog);
    guards::WarnRetiredSmoothingKey(ini, "Position", "Smoothing", kLog);
    WarnRetiredKey(ini, "Rotation", "Deadzone",
                   "Set a deadzone in your tracker instead, so one profile covers every game.");
    for (const char* axis : { "InvertX", "InvertY", "InvertZ" }) {
        WarnRetiredKey(ini, "Position", axis,
                       "The tracker-to-Quake axis directions are fixed in the mod. If an axis "
                       "arrives mirrored, correct it in your tracker's own profile.");
    }
    c.worldSpaceYaw = ReadBoolChecked(ini, "Rotation", "WorldSpaceYaw", c.worldSpaceYaw);

    c.positionEnabled = ReadBoolChecked(ini, "Position", "Enabled", c.positionEnabled);
    c.posSensX = ReadSensitivity(ini, "Position", "SensitivityX", c.posSensX);
    c.posSensY = ReadSensitivity(ini, "Position", "SensitivityY", c.posSensY);
    c.posSensZ = ReadSensitivity(ini, "Position", "SensitivityZ", c.posSensZ);
    c.posLimitX = ReadPositionLimit(ini, "LimitX", c.posLimitX);
    c.posLimitY = ReadPositionLimit(ini, "LimitY", c.posLimitY);
    c.posLimitZ = ReadPositionLimit(ini, "LimitZ", c.posLimitZ);
    c.posLimitZBack = ReadPositionLimit(ini, "LimitZBack", c.posLimitZBack);
    c.positionUnitsPerMeter = guards::ReadFloatChecked(ini, "Position", "UnitsPerMeter",
                                                       c.positionUnitsPerMeter, kMinUnitsPerMeter,
                                                       kMaxUnitsPerMeter, kLog);

    c.collisionEnabled = ReadBoolChecked(ini, "Collision", "CollisionEnabled", c.collisionEnabled);
    c.collisionStandoff = guards::ReadFloatChecked(ini, "Collision", "CollisionMargin",
                                                   c.collisionStandoff, kMinCollisionStandoff,
                                                   kMaxCollisionStandoff, kLog);
    c.collisionReleaseSmoothing = ReadUnitInterval(ini, "Collision", "CollisionReleaseSmoothing",
                                                   c.collisionReleaseSmoothing);

    c.keyToggle = ReadVirtualKey(ini, "ToggleKey", c.keyToggle);
    c.keyTogglePosition = ReadVirtualKey(ini, "TogglePositionKey", c.keyTogglePosition);
    c.keyToggleYaw = ReadVirtualKey(ini, "ToggleYawKey", c.keyToggleYaw);
    RejectDuplicateKeys(c);

    return ReadStatus::Read;
}

std::vector<cameraunlock::config::LegacyKey> ReadKeys() {
    return {
        {"General", "Enabled"},
        {"General", "UdpPort"},
        {"Rotation", "YawSensitivity"},
        {"Rotation", "PitchSensitivity"},
        {"Rotation", "RollSensitivity"},
        {"Rotation", "InvertYaw"},
        {"Rotation", "InvertPitch"},
        {"Rotation", "InvertRoll"},
        {"Rotation", "LocalSmoothing"},
        {"Rotation", "RemoteSmoothing"},
        {"Rotation", "WorldSpaceYaw"},
        {"Position", "Enabled"},
        {"Position", "SensitivityX"},
        {"Position", "SensitivityY"},
        {"Position", "SensitivityZ"},
        {"Position", "LimitX"},
        {"Position", "LimitY"},
        {"Position", "LimitZ"},
        {"Position", "LimitZBack"},
        {"Position", "UnitsPerMeter"},
        {"Collision", "CollisionEnabled"},
        {"Collision", "CollisionMargin"},
        {"Collision", "CollisionReleaseSmoothing"},
        {"Controls", "ToggleKey"},
        {"Controls", "TogglePositionKey"},
        {"Controls", "ToggleYawKey"},
    };
}

}  // namespace Q2RTXHT::legacy
