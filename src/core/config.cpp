#include "core/config.h"

#include <sys/stat.h>

#include <cctype>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/protocol/port_utils.h"

namespace Q2RTXHT {

namespace guards = cameraunlock::config;

namespace {

// HeadTracking.ini is the one place user-typed text becomes a float that
// reaches the view basis, so every value crosses it through core's guards
// rather than through IniReader's raw readers, which return the default on a
// value they cannot make sense of and say nothing. strtod accepts "nan" and "inf",
// overflows a literal like 1e400 to +inf, and parses "0,15" as a whole-looking
// 0.0 - and none of those are caught downstream, because every comparison
// against NaN is false, so the position clamp is skipped and sin/cos of an
// infinite angle is NaN. The result would be a NaN viewangle written into
// cl.refdef every frame with nothing in the log.
constexpr guards::LogSink kLog = &cameraunlock::logging::Line;

// Quake units per real-world metre. This is a unit conversion, not a strength
// setting: one metre really is 39.37 Quake units, and the shipped 40 is that
// rounded. The band is wide enough to hold any rounding of the same physical
// fact and no wider, because the old order-of-magnitude one made this the mod's
// third position multiplier - the thing "the tracker owns pose shaping" exists
// to prevent, and the one a player reaches for after being told to raise it to
// lean further.
constexpr float kMinUnitsPerMeter = 32.0f;
constexpr float kMaxUnitsPerMeter = 48.0f;

// Quake units held off a blocking surface. The player's own half-width is 16,
// so a standoff past four of those cuts every lean before it starts. The floor
// is the shipped 4, the only standoff confirmed in game to leave a wall
// rendering solid at the eye; smaller values have not been tried, and 0 rests
// the eye exactly on the surface, where the near plane culls it and the player
// sees through the wall the clamp just stopped them entering. That is not the
// same guarantee as reading the engine's near plane, which the mod does not do.
constexpr float kMinCollisionStandoff = 4.0f;
constexpr float kMaxCollisionStandoff = 64.0f;

// A travel limit of zero passes every finite check and then leaves that axis
// unable to move the eye at all, which in game is indistinguishable from the
// mod not working. Same reasoning as kMinUnitsPerMeter, and the same lower
// bound the documented range starts at.
constexpr float kMinPositionLimit = 0.01f;

// The documented range, and the one the shipped ini names above these keys.
// core's guard admits ten metres, which is not a head leaning out of a body -
// it is 400 Quake units of camera, past anything the collision clamp was
// measured against.
constexpr float kMaxPositionLimit = 0.5f;

// Zero here is a dead axis rather than a disabled one; see ReadSensitivity.
constexpr float kMinSensitivity = 0.01f;

// Named readers for the three ranges the settings fall into. Spelling the
// guard's bounds out at each of the fourteen call sites made the one thing that
// differs between them - the key - the hardest part of the line to find.
// A sensitivity is a magnitude. Direction is what the Invert* keys are for, so
// a negative value here would be a second inversion composing invisibly with
// them - and the guard CLAMPS rather than falling back, so the user who typed
// -1 wanting a mirrored axis gets a dead one instead, with a log line naming
// the clamp that did it. The floor is the
// same small positive number the travel limits use, for the same reason: zero
// passes every finite check and then leaves that axis unable to move anything,
// which in game is indistinguishable from the mod not working.
float ReadSensitivity(const cameraunlock::IniReader& ini, const char* section,
                      const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, kMinSensitivity,
                                    guards::kMaxSensitivity, kLog);
}

// A negative travel limit inverts the clamp bounds in PositionProcessor -
// Clamp(v, -limit, limit) with limit < 0 returns the lower bound for every
// input - which pins the lean at a fixed offset instead of freeing it.
float ReadPositionLimit(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, "Position", key, fallback, kMinPositionLimit,
                                    kMaxPositionLimit, kLog);
}

// The 0-1 scale every smoothing-shaped setting in the fleet is expressed on.
float ReadUnitInterval(const cameraunlock::IniReader& ini, const char* section,
                       const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, 0.0f, 1.0f, kLog);
}

// IniReader::ReadBool matches the whole raw value against a fixed list and
// returns the default on anything else, silently. GetPrivateProfileString does
// not strip an inline comment, so `CollisionEnabled=0 ; free lean` arrives as
// "0 ; free lean", matches nothing, and leaves the clamp on with an empty log.
// A typo does the same. This puts bools on the same boundary the numbers use:
// the value is trimmed of its comment first, and a token that still means
// nothing is named rather than swallowed.
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

bool FileExists(const std::string& path) {
    struct _stat st;
    return _stat(path.c_str(), &st) == 0;
}

// A hotkey is the one setting whose bad value is silent in game: the key simply
// never fires and there is nothing on screen to say why. GetAsyncKeyState only
// defines 0x01..0xFE, so ToggleKey=0x230 binds a key that cannot exist, and a
// binding on Ctrl/Shift/Alt is what the chord guard itself watches - the nav
// action would be suppressed for exactly as long as the key is held.
int ReadVirtualKey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const int vk = ini.ReadHex("Controls", key, fallback);
    if (guards::IsBindableVirtualKey(vk)) return vk;
    kLog("config: [Controls] %s=0x%X is not a key this mod can bind; using 0x%02X",
         key, vk, fallback);
    return fallback;
}

// A key that used to do something and now does not. Dropping it from the shipped
// .ini does nothing for a player who already has one on disk, because an update
// never rewrites their config: their InvertZ=1 would sit there looking effective.
void WarnRetiredKey(const cameraunlock::IniReader& ini, const char* section,
                    const char* key, const char* advice) {
    if (guards::ReadRawValue(ini, section, key).empty()) return;
    kLog("config: [%s] %s has been retired and is IGNORED. %s", section, key, advice);
}

}  // namespace

// Two actions on one key fire both callbacks from a single press, so a toggle
// and a mode cycle happen together. It is silent in game in the same way an
// unbindable key is: nothing on screen says why the key did two things.
//
// Reverting to the binding's own compiled default is not enough by itself,
// because the collision is usually WITH a default: ToggleKey=0x22 on its own
// collides with ToggleYawKey, whose default is 0x22, so reverting it changes
// nothing and the log would claim it had. The fallback is therefore re-checked,
// and a binding with nowhere free is left unbound (0, which HotkeyPoller skips)
// rather than left colliding. Its Ctrl+Shift chord is unaffected either way.
void Config::RejectDuplicateKeys() {
    const Config defaults;
    struct Binding { const char* name; int* value; int fallback; };
    const Binding bindings[] = {
        { "ToggleKey", &keyToggle, defaults.keyToggle },
        { "TogglePositionKey", &keyTogglePosition, defaults.keyTogglePosition },
        { "ToggleYawKey", &keyToggleYaw, defaults.keyToggleYaw },
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

bool Config::LoadOrCreate(const std::string& path) {
    if (!FileExists(path)) {
        WriteDefault(path);
    }

    cameraunlock::IniReader ini;
    if (!ini.Open(path)) {
        // Boundary failure: keep compiled defaults rather than guessing.
        return false;
    }

    enabled = ReadBoolChecked(ini, "General", "Enabled", enabled);

    // Out of range falls back to the default rather than being cast straight to
    // uint16_t, which truncates silently: UdpPort=70000 would bind 4464 and the
    // player would be left with a mod listening on a port nothing sends to.
    // ReadInt is the reader that yields 0 rather than the default on a present
    // but unparseable value (see ini_reader.h rule 4), and port 0 binds an
    // OS-assigned ephemeral port, so the log would claim success either way.
    bool portValid = false;
    udpPort = cameraunlock::NormalizeUdpPort(
        ini.ReadInt("General", "UdpPort", udpPort), udpPort, portValid);
    if (!portValid) {
        kLog("config: [General] UdpPort is outside 1024-65535; using %u", udpPort);
    }

    yawSensitivity = ReadSensitivity(ini, "Rotation", "YawSensitivity", yawSensitivity);
    pitchSensitivity = ReadSensitivity(ini, "Rotation", "PitchSensitivity", pitchSensitivity);
    rollSensitivity = ReadSensitivity(ini, "Rotation", "RollSensitivity", rollSensitivity);
    invertYaw = ReadBoolChecked(ini, "Rotation", "InvertYaw", invertYaw);
    invertPitch = ReadBoolChecked(ini, "Rotation", "InvertPitch", invertPitch);
    invertRoll = ReadBoolChecked(ini, "Rotation", "InvertRoll", invertRoll);
    localSmoothing = ReadUnitInterval(ini, "Rotation", "LocalSmoothing", localSmoothing);
    remoteSmoothing = ReadUnitInterval(ini, "Rotation", "RemoteSmoothing", remoteSmoothing);
    guards::WarnRetiredSmoothingKey(ini, "Rotation", "Smoothing", kLog);
    guards::WarnRetiredSmoothingKey(ini, "Position", "Smoothing", kLog);
    // Pose shaping belongs to the tracker, so one profile behaves the same in
    // every game instead of being re-tuned per mod.
    WarnRetiredKey(ini, "Rotation", "Deadzone",
                   "Set a deadzone in your tracker instead, so one profile covers every game.");
    for (const char* axis : { "InvertX", "InvertY", "InvertZ" }) {
        WarnRetiredKey(ini, "Position", axis,
                       "The tracker-to-Quake axis directions are fixed in the mod. If an axis "
                       "arrives mirrored, correct it in your tracker's own profile.");
    }
    worldSpaceYaw = ReadBoolChecked(ini, "Rotation", "WorldSpaceYaw", worldSpaceYaw);

    positionEnabled = ReadBoolChecked(ini, "Position", "Enabled", positionEnabled);
    posSensX = ReadSensitivity(ini, "Position", "SensitivityX", posSensX);
    posSensY = ReadSensitivity(ini, "Position", "SensitivityY", posSensY);
    posSensZ = ReadSensitivity(ini, "Position", "SensitivityZ", posSensZ);
    posLimitX = ReadPositionLimit(ini, "LimitX", posLimitX);
    posLimitY = ReadPositionLimit(ini, "LimitY", posLimitY);
    posLimitZ = ReadPositionLimit(ini, "LimitZ", posLimitZ);
    posLimitZBack = ReadPositionLimit(ini, "LimitZBack", posLimitZBack);
    positionUnitsPerMeter = guards::ReadFloatChecked(ini, "Position", "UnitsPerMeter",
                                                     positionUnitsPerMeter, kMinUnitsPerMeter,
                                                     kMaxUnitsPerMeter, kLog);

    collisionEnabled = ReadBoolChecked(ini, "Collision", "CollisionEnabled", collisionEnabled);
    collisionStandoff = guards::ReadFloatChecked(ini, "Collision", "CollisionMargin",
                                                 collisionStandoff, kMinCollisionStandoff,
                                                 kMaxCollisionStandoff, kLog);
    collisionReleaseSmoothing = ReadUnitInterval(ini, "Collision", "CollisionReleaseSmoothing",
                                                 collisionReleaseSmoothing);

    keyToggle = ReadVirtualKey(ini, "ToggleKey", keyToggle);
    keyTogglePosition = ReadVirtualKey(ini, "TogglePositionKey", keyTogglePosition);
    keyToggleYaw = ReadVirtualKey(ini, "ToggleYawKey", keyToggleYaw);
    RejectDuplicateKeys();

    return true;
}

void Config::WriteDefault(const std::string& path) const {
    cameraunlock::IniWriter w;
    if (!w.Open(path)) {
        // The read that follows falls back to the compiled defaults, so the mod
        // still runs - but it runs on numbers the player cannot see or edit,
        // and without this line the log would only say the file could not be
        // opened, which reads as a file that exists and is locked.
        kLog("config: could not create %s; running on built-in defaults, and any "
             "settings written to that path will not be picked up",
             path.c_str());
        return;
    }

    w.WriteComment("Quake II RTX Head Tracking configuration");
    w.WriteComment("Send OpenTrack UDP output to 127.0.0.1:4242 (Output: UDP over network).");
    w.WriteBlankLine();

    w.WriteSection("General");
    w.WriteBool("Enabled", enabled);
    w.WriteInt("UdpPort", udpPort);
    w.WriteBlankLine();

    w.WriteSection("Rotation");
    w.WriteDouble("YawSensitivity", yawSensitivity);
    w.WriteDouble("PitchSensitivity", pitchSensitivity);
    w.WriteDouble("RollSensitivity", rollSensitivity);
    w.WriteBool("InvertYaw", invertYaw);
    w.WriteBool("InvertPitch", invertPitch);
    w.WriteBool("InvertRoll", invertRoll);
    w.WriteComment("Smoothing is picked per connection from the tracker's source address");
    w.WriteComment("and covers rotation and position. 0 = none, 1 = heavy.");
    w.WriteComment("LocalSmoothing: tracker runs on this machine (loopback)");
    w.WriteDouble("LocalSmoothing", localSmoothing);
    w.WriteComment("RemoteSmoothing: tracker is a remote device on the network");
    w.WriteDouble("RemoteSmoothing", remoteSmoothing);
    w.WriteComment("WorldSpaceYaw: head yaw about world up, so the horizon stays level however");
    w.WriteComment("far the mouse is pitched. 0 turns it about the camera's own up axis instead.");
    w.WriteComment("Page Down switches it in game.");
    w.WriteBool("WorldSpaceYaw", worldSpaceYaw);
    w.WriteBlankLine();

    w.WriteSection("Position");
    w.WriteBool("Enabled", positionEnabled);
    w.WriteDouble("SensitivityX", posSensX);
    w.WriteDouble("SensitivityY", posSensY);
    w.WriteDouble("SensitivityZ", posSensZ);
    w.WriteComment("How far the eye may leave the body, in meters. 0.01 - 0.5.");
    w.WriteDouble("LimitX", posLimitX);
    w.WriteDouble("LimitY", posLimitY);
    w.WriteDouble("LimitZ", posLimitZ);
    w.WriteDouble("LimitZBack", posLimitZBack);
    w.WriteComment("Position uses the [Rotation] LocalSmoothing / RemoteSmoothing values");
    w.WriteComment("Quake units per metre (vieworg is in Quake units, ~1 per inch).");
    w.WriteComment("This converts units. It does not set how far you lean - the");
    w.WriteComment("Limit values above do that, and your tracker sets the rest.");
    w.WriteDouble("UnitsPerMeter", positionUnitsPerMeter);
    w.WriteBlankLine();

    w.WriteSection("Collision");
    w.WriteComment("Stops a lean putting the view inside a wall. Rotation is unaffected.");
    w.WriteBool("CollisionEnabled", collisionEnabled);
    w.WriteComment("Quake units the eye is held off a blocking surface");
    w.WriteDouble("CollisionMargin", collisionStandoff);
    w.WriteComment("How fast the lean reopens once the obstruction clears (0 = instant)");
    w.WriteDouble("CollisionReleaseSmoothing", collisionReleaseSmoothing);
    w.WriteBlankLine();

    w.WriteSection("Controls");
    w.WriteComment("Virtual key codes in hex. Chord alternatives (no edit needed):");
    w.WriteComment("  Toggle Ctrl+Shift+Y, Cycle tracking mode Ctrl+Shift+G,");
    w.WriteComment("  Toggle yaw mode Ctrl+Shift+H");
    w.WriteHex("ToggleKey", keyToggle);
    w.WriteHex("TogglePositionKey", keyTogglePosition);
    w.WriteHex("ToggleYawKey", keyToggleYaw);

    w.Close();
}

}  // namespace Q2RTXHT
