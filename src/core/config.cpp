#include "core/config.h"

#include <sys/stat.h>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"
#include "cameraunlock/logging/file_log.h"
#include "legacy_config/legacy_config.h"

namespace Q2RTXHT {

namespace {

bool FileExists(const std::string& path) {
    struct _stat st;
    return _stat(path.c_str(), &st) == 0;
}

constexpr cameraunlock::config::LogSink kLog = &cameraunlock::logging::Line;

}  // namespace

bool Config::LoadOrCreate(const std::string& path) {
    if (!FileExists(path)) {
        WriteDefault(path);
    }

    legacy::Config c;
    if (legacy::Read(path.c_str(), c) == legacy::ReadStatus::Absent) {
        // Boundary failure: keep compiled defaults rather than guessing.
        return false;
    }

    enabled = c.enabled;
    udpPort = c.udpPort;
    yawSensitivity = c.yawSensitivity;
    pitchSensitivity = c.pitchSensitivity;
    rollSensitivity = c.rollSensitivity;
    invertYaw = c.invertYaw;
    invertPitch = c.invertPitch;
    invertRoll = c.invertRoll;
    localSmoothing = c.localSmoothing;
    remoteSmoothing = c.remoteSmoothing;
    worldSpaceYaw = c.worldSpaceYaw;
    positionEnabled = c.positionEnabled;
    posSensX = c.posSensX;
    posSensY = c.posSensY;
    posSensZ = c.posSensZ;
    posLimitX = c.posLimitX;
    posLimitY = c.posLimitY;
    posLimitZ = c.posLimitZ;
    posLimitZBack = c.posLimitZBack;
    positionUnitsPerMeter = c.positionUnitsPerMeter;
    collisionEnabled = c.collisionEnabled;
    collisionStandoff = c.collisionStandoff;
    collisionReleaseSmoothing = c.collisionReleaseSmoothing;
    keyToggle = c.keyToggle;
    keyTogglePosition = c.keyTogglePosition;
    keyToggleYaw = c.keyToggleYaw;
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
