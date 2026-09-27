#include "core/config.h"

#include <string>
#include <utility>
#include <vector>

#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"
#include "legacy_config/legacy_config.h"

namespace Q2RTXHT {

namespace {

namespace cfg = cameraunlock::config;
using cfg::DroppedValue;
using cfg::ImportResult;
using cfg::LegacyFollowsDefaultsIni;
using cfg::LegacyInput;
using cfg::LegacyPoseShaping;
using cfg::PoseShapingValue;
using cfg::schema::Concept;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and the Ctrl+Shift chord the dev build always registered beside it, as one
// key list: the code's binding (none for a code no hotkey can hold, N1 and N3, or for the 0 the dev
// reader left on a key it could not separate from another action's), then the chord.
std::string KeyList(int vk, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    const std::string code = cfg::LegacyVirtualKeyToBindings(vk, "Controls", key, dropped);
    const std::string chord = cameraunlock::input::FormatKeyBindings(
        std::vector<KeyBinding>{{KeyModifiers::kCtrl | KeyModifiers::kShift, letter}});
    return code.empty() ? chord : code + ", " + chord;
}

ImportResult Import(const LegacyInput& input, Config& out) {
    legacy::Config c;
    // The dev build opened the file by the exe folder's name in the ANSI code page, and where the
    // code page could not hold that name it read no file at all and ran on its defaults.
    const legacy::ReadStatus read =
        input.ansi_lossy ? legacy::ReadStatus::Absent : legacy::Read(input.ansi_path.c_str(), c);
    const legacy::Config shipped;

    std::vector<DroppedValue> dropped;
    std::vector<PoseShapingValue> shaping;

    out.udpPort = c.udpPort;
    out.enableOnStartup = c.enabled;

    // [Position] Enabled chose only the startup mode: the cycle key reached every mode either way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.positionEnabled ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);
    out.rotationEnabled = mode.rotation_enabled;
    out.positionEnabled = mode.position_enabled;

    out.localSmoothing = c.localSmoothing;
    out.remoteSmoothing = c.remoteSmoothing;
    out.worldSpaceYaw = c.worldSpaceYaw;

    // The old file had one vertical limit, which the old runtime applied both ways.
    out.posLimitX = c.posLimitX;
    out.posLimitY = c.posLimitY;
    out.posLimitYDown = c.posLimitY;
    out.posLimitZ = c.posLimitZ;
    out.posLimitZBack = c.posLimitZBack;

    out.collisionEnabled = c.collisionEnabled;
    out.collisionStandoff = c.collisionStandoff;
    out.collisionReleaseSmoothing = c.collisionReleaseSmoothing;

    // Every sensitivity and inversion shipped at identity, and the unit scale shipped at 40, which
    // is now kUnitsPerMeter. Nothing else folds, and a value the player changed is dropped.
    LegacyPoseShaping(c.yawSensitivity, shipped.yawSensitivity, "Rotation", "YawSensitivity", shaping, dropped);
    LegacyPoseShaping(c.pitchSensitivity, shipped.pitchSensitivity, "Rotation", "PitchSensitivity", shaping, dropped);
    LegacyPoseShaping(c.rollSensitivity, shipped.rollSensitivity, "Rotation", "RollSensitivity", shaping, dropped);
    LegacyPoseShaping(c.invertYaw, shipped.invertYaw, "Rotation", "InvertYaw", shaping, dropped);
    LegacyPoseShaping(c.invertPitch, shipped.invertPitch, "Rotation", "InvertPitch", shaping, dropped);
    LegacyPoseShaping(c.invertRoll, shipped.invertRoll, "Rotation", "InvertRoll", shaping, dropped);
    LegacyPoseShaping(c.posSensX, shipped.posSensX, "Position", "SensitivityX", shaping, dropped);
    LegacyPoseShaping(c.posSensY, shipped.posSensY, "Position", "SensitivityY", shaping, dropped);
    LegacyPoseShaping(c.posSensZ, shipped.posSensZ, "Position", "SensitivityZ", shaping, dropped);
    LegacyPoseShaping(c.positionUnitsPerMeter, shipped.positionUnitsPerMeter, "Position", "UnitsPerMeter", shaping,
                      dropped);

    out.toggleKey = KeyList(c.keyToggle, 'Y', "ToggleKey", dropped);
    out.cycleTrackingModeKey = KeyList(c.keyTogglePosition, 'G', "TogglePositionKey", dropped);
    out.yawModeKey = KeyList(c.keyToggleYaw, 'H', "ToggleYawKey", dropped);

    // A row still at what the dev build ran on with no file is no player's choice, so it follows
    // Defaults.ini. The chords were fixed in code, so each hotkey's code decides alone.
    // CollisionMargin is not global, so it is not given.
    LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, c.udpPort, shipped.udpPort);
    follows.Setting(Concept::EnableOnStartup, c.enabled, shipped.enabled);
    follows.TrackingMode(c.positionEnabled, shipped.positionEnabled);
    follows.Setting(Concept::LocalSmoothing, c.localSmoothing, shipped.localSmoothing);
    follows.Setting(Concept::RemoteSmoothing, c.remoteSmoothing, shipped.remoteSmoothing);
    follows.Setting(Concept::WorldSpaceYaw, c.worldSpaceYaw, shipped.worldSpaceYaw);
    follows.Setting(Concept::PositionLimitX, c.posLimitX, shipped.posLimitX);
    follows.Setting(Concept::PositionLimitY, c.posLimitY, shipped.posLimitY);
    follows.Setting(Concept::PositionLimitYDown, c.posLimitY, shipped.posLimitY);
    follows.Setting(Concept::PositionLimitZ, c.posLimitZ, shipped.posLimitZ);
    follows.Setting(Concept::PositionLimitZBack, c.posLimitZBack, shipped.posLimitZBack);
    follows.Setting(Concept::CollisionEnabled, c.collisionEnabled, shipped.collisionEnabled);
    follows.Setting(Concept::CollisionReleaseSmoothing, c.collisionReleaseSmoothing,
                    shipped.collisionReleaseSmoothing);
    follows.Setting(Concept::ToggleKey, c.keyToggle, shipped.keyToggle);
    follows.Setting(Concept::CycleTrackingModeKey, c.keyTogglePosition, shipped.keyTogglePosition);
    follows.Setting(Concept::YawModeKey, c.keyToggleYaw, shipped.keyToggleYaw);

    return read == legacy::ReadStatus::Absent
               ? ImportResult::Absent(std::move(dropped), std::move(shaping), follows.Concepts())
               : ImportResult::Imported(std::move(dropped), std::move(shaping), follows.Concepts());
}

}  // namespace

cfg::ConfigTable<Config> MakeConfigTable() {
    cfg::ConfigTable<Config> table{Config{}};
    table.Concept<Concept::UdpPort>(&Config::udpPort)
        .Concept<Concept::EnableOnStartup>(&Config::enableOnStartup)
        .Concept<Concept::RotationEnabled>(&Config::rotationEnabled)
        .Writable()
        .Concept<Concept::LocalSmoothing>(&Config::localSmoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remoteSmoothing)
        .Concept<Concept::WorldSpaceYaw>(&Config::worldSpaceYaw)
        .Writable()
        .Concept<Concept::PositionEnabled>(&Config::positionEnabled)
        .Writable()
        .Concept<Concept::PositionLimitX>(&Config::posLimitX)
        .Concept<Concept::PositionLimitY>(&Config::posLimitY)
        .Concept<Concept::PositionLimitYDown>(&Config::posLimitYDown)
        .Concept<Concept::PositionLimitZ>(&Config::posLimitZ)
        .Concept<Concept::PositionLimitZBack>(&Config::posLimitZBack)
        .Concept<Concept::CollisionEnabled>(&Config::collisionEnabled)
        .Concept<Concept::CollisionMargin>(&Config::collisionStandoff)
        .Comment("How far, in Quake units (about 1 per inch), a lean holds the eye off a wall. 4 to 64.")
        .Concept<Concept::CollisionReleaseSmoothing>(&Config::collisionReleaseSmoothing)
        .Concept<Concept::ToggleKey>(&Config::toggleKey)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycleTrackingModeKey)
        .Concept<Concept::YawModeKey>(&Config::yawModeKey);
    return table;
}

cfg::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cfg::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder, cfg::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cfg::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace Q2RTXHT
