// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read three ways:
//
//   oracle     the reader of the dev pre-release (b6665cf), the newest published build, with the
//              core sources it compiled at its pin fb55a6a (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//   migration  the config owner in a folder holding only HeadTracking.ini, the legacy file,
//              importing it into a new CameraUnlock.ini, then the canonical reader and table on
//              that file
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// Comparison 2, import against migration, is the proof for the migration: the settings the mod
// starts on are the import's, apart from the approved change, which the import must record as
// dropped: a sensitivity, inversion or unit scale the player set away from its shipped value is
// dropped (pose_shaping). The hotkeys fire exactly as the dev build fired them.
//
// A row the player never changed from what the dev build ran on with no file follows Defaults.ini:
// the import lists it in follows_defaults_ini and the migration writes it default, the tracking
// mode pair as one unit. The test derives that list from what the import read and holds the
// import's list to it on every input; the shipped file, the dev build's first-run output and the
// empty file list every row and migrate to the committed file byte for byte.
//
// Comparison 2 runs twice, once over a Defaults.ini at the built-in values, where the session runs
// as the import read, and once over one a player changed, where a row the player never changed
// takes Defaults.ini's value and a changed row keeps the player's. After every load
// HeadTracking.ini keeps its bytes, its write time and its attributes, Defaults.ini is never
// written, and the folder holds the legacy file and CameraUnlock.ini and nothing else. The next
// load reads CameraUnlock.ini, imports nothing and writes nothing, and a read-only legacy file
// imports as a writable one does.
//
// The distinct migrated files are written beside the executable under migrated\, for
// lint-migrated.mjs to run core's canonical config lint over.
//
// Inputs: no file, an empty file, the dev build's shipped config/HeadTracking.ini (which its
// install.cmd seeded and its launcher-manifest.json seeded byte for byte), the first-run output
// of the dev build, two sets of hotkeys the dev reader had to separate, and core's corpus over
// the shipped file.
//
// `--extract-first-run <path>` writes what the oracle creates for a missing file to <path>, which
// is how data/dev-first-run.ini was made.

#include "core/config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace Q2RTXHT;

namespace {

// The dev build and the frozen import read the file with the same source (src/core/config.cpp did
// not change between the dev pre-release and the commit that froze it) and the same core
// IniReader and value guards (unchanged since fb55a6a), so comparison 1 has no differences to
// record.
const char* const kComparison1Differences[] = {
    "none",
};

constexpr const char* kFileName = kLegacyConfigFileName;

int g_failures = 0;
int g_checks = 0;

void Check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        if (g_failures < 200) std::printf("  FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("could not read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("could not write " + path.string());
}

void SetReadOnly(const fs::path& path, bool readOnly) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("no attributes for " + path.string());
    const DWORD next = readOnly ? (attrs | FILE_ATTRIBUTE_READONLY) : (attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!SetFileAttributesW(path.c_str(), next)) throw std::runtime_error("could not set attributes on " + path.string());
}

using Listing = std::vector<std::pair<std::string, std::string>>;

Listing List(const fs::path& dir) {
    Listing l;
    for (const auto& e : fs::directory_iterator(dir)) {
        l.emplace_back(e.path().filename().string(), ReadBytes(e.path()));
    }
    std::sort(l.begin(), l.end());
    return l;
}

struct Input {
    std::string name;
    std::optional<std::string> bytes;  // nullopt: no file
};

// Startup state as the dev build derives it from the config (Mod::LoadConfiguration and
// ApplyConfigToPipeline): enabled, the yaw mode, and RotationAndPosition when [Position] Enabled
// else RotationOnly.
struct Startup {
    bool enabled;
    bool worldSpaceYaw;
    int mode;  // 0 rotation and position, 1 rotation only
    bool operator==(const Startup& o) const {
        return enabled == o.enabled && worldSpaceYaw == o.worldSpaceYaw && mode == o.mode;
    }
};

Startup StartupOf(const q2_oracle_view::OracleConfig& c) { return {c.enabled, c.worldSpaceYaw, c.positionEnabled ? 0 : 1}; }

Startup StartupOf(const legacy::Config& c) { return {c.enabled, c.worldSpaceYaw, c.positionEnabled ? 0 : 1}; }

std::string FirstFireDifference(const q2_oracle_view::FireTable& expected, const q2_oracle_view::FireTable& got) {
    for (std::size_t i = 0; i < expected.size() && i < got.size(); ++i) {
        if (expected[i] != got[i]) {
            char text[160];
            std::snprintf(text, sizeof text, "key 0x%02X held %d fires %d/%d/%d, not %d/%d/%d",
                          static_cast<int>(i / q2_oracle_view::kHeldStates) + q2_oracle_view::kFirstKey,
                          static_cast<int>(i % q2_oracle_view::kHeldStates), got[i][0], got[i][1], got[i][2],
                          expected[i][0], expected[i][1], expected[i][2]);
            return text;
        }
    }
    return expected.size() == got.size() ? "none" : "the tables differ in size";
}

std::vector<std::string> FieldDifferences(const q2_oracle_view::OracleConfig& o, const legacy::Config& i) {
    std::vector<std::string> d;
    auto b = [&d](const char* n, bool x, bool y) { if (x != y) d.push_back(n); };
    auto f = [&d](const char* n, float x, float y) { if (!SameBits(x, y)) d.push_back(n); };
    auto n = [&d](const char* name, long long x, long long y) { if (x != y) d.push_back(name); };
    b("enabled", o.enabled, i.enabled);
    n("udpPort", o.udpPort, i.udpPort);
    f("yawSensitivity", o.yawSensitivity, i.yawSensitivity);
    f("pitchSensitivity", o.pitchSensitivity, i.pitchSensitivity);
    f("rollSensitivity", o.rollSensitivity, i.rollSensitivity);
    b("invertYaw", o.invertYaw, i.invertYaw);
    b("invertPitch", o.invertPitch, i.invertPitch);
    b("invertRoll", o.invertRoll, i.invertRoll);
    f("localSmoothing", o.localSmoothing, i.localSmoothing);
    f("remoteSmoothing", o.remoteSmoothing, i.remoteSmoothing);
    b("worldSpaceYaw", o.worldSpaceYaw, i.worldSpaceYaw);
    b("positionEnabled", o.positionEnabled, i.positionEnabled);
    f("posSensX", o.posSensX, i.posSensX);
    f("posSensY", o.posSensY, i.posSensY);
    f("posSensZ", o.posSensZ, i.posSensZ);
    f("posLimitX", o.posLimitX, i.posLimitX);
    f("posLimitY", o.posLimitY, i.posLimitY);
    f("posLimitZ", o.posLimitZ, i.posLimitZ);
    f("posLimitZBack", o.posLimitZBack, i.posLimitZBack);
    f("positionUnitsPerMeter", o.positionUnitsPerMeter, i.positionUnitsPerMeter);
    b("collisionEnabled", o.collisionEnabled, i.collisionEnabled);
    f("collisionStandoff", o.collisionStandoff, i.collisionStandoff);
    f("collisionReleaseSmoothing", o.collisionReleaseSmoothing, i.collisionReleaseSmoothing);
    n("keyToggle", o.keyToggle, i.keyToggle);
    n("keyTogglePosition", o.keyTogglePosition, i.keyTogglePosition);
    n("keyToggleYaw", o.keyToggleYaw, i.keyToggleYaw);
    return d;
}

std::string Join(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

// The corpus descriptor of every key the frozen reader reads.
std::vector<cameraunlock::config::testing::MutationKey> MutationKeys() {
    using cameraunlock::config::testing::MutationKey;
    auto plain = [](const char* s, const char* k, const char* alt, std::vector<std::string> oor = {}) {
        MutationKey m;
        m.section = s;
        m.key = k;
        m.alternate = alt;
        m.out_of_range = std::move(oor);
        return m;
    };
    auto hotkey = [](const char* k, const char* alt) {
        MutationKey m;
        m.section = "Controls";
        m.key = k;
        m.alternate = alt;
        m.out_of_range = {"0x100", "0x10"};
        m.hotkey = true;
        return m;
    };
    return {
        plain("General", "Enabled", "0"),
        plain("General", "UdpPort", "4243", {"1023", "65536"}),
        plain("Rotation", "YawSensitivity", "0.5", {"0.001", "100.5"}),
        plain("Rotation", "PitchSensitivity", "0.5", {"0.001", "100.5"}),
        plain("Rotation", "RollSensitivity", "0.5", {"0.001", "100.5"}),
        plain("Rotation", "InvertYaw", "1"),
        plain("Rotation", "InvertPitch", "1"),
        plain("Rotation", "InvertRoll", "1"),
        plain("Rotation", "LocalSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Rotation", "RemoteSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Rotation", "WorldSpaceYaw", "0"),
        plain("Position", "Enabled", "0"),
        plain("Position", "SensitivityX", "0.5", {"0.001", "100.5"}),
        plain("Position", "SensitivityY", "0.5", {"0.001", "100.5"}),
        plain("Position", "SensitivityZ", "0.5", {"0.001", "100.5"}),
        plain("Position", "LimitX", "0.45", {"0.001", "0.6"}),
        plain("Position", "LimitY", "0.45", {"0.001", "0.6"}),
        plain("Position", "LimitZ", "0.45", {"0.001", "0.6"}),
        plain("Position", "LimitZBack", "0.2", {"0.001", "0.6"}),
        plain("Position", "UnitsPerMeter", "39.37", {"31.5", "48.5"}),
        plain("Collision", "CollisionEnabled", "0"),
        plain("Collision", "CollisionMargin", "8", {"3.5", "64.5"}),
        plain("Collision", "CollisionReleaseSmoothing", "0.5", {"-0.5", "1.5"}),
        hotkey("ToggleKey", "0x70"),
        hotkey("TogglePositionKey", "0x71"),
        hotkey("ToggleYawKey", "0x72"),
    };
}

// One folder per reading under a root of this process's own, emptied before each input so the
// test never holds more than one input's files.
class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() / ("q2rtx-config-differential-" + std::to_string(GetCurrentProcessId()));
        Remove(root_);
        fs::create_directories(root_);
    }
    ~Scratch() { Remove(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    fs::path Clean(const std::string& leaf) {
        const fs::path dir = root_ / leaf;
        Remove(dir);
        fs::create_directories(dir);
        return dir;
    }

private:
    // Read-only files included, which remove_all will not delete.
    static void Remove(const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
            if (e.is_regular_file()) SetFileAttributesW(e.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(dir, ec);
        if (ec) throw std::runtime_error("could not empty " + dir.string() + ": " + ec.message());
    }

    fs::path root_;
};

fs::path Place(const fs::path& dir, const Input& input) {
    const fs::path file = dir / kFileName;
    if (input.bytes) WriteBytes(file, *input.bytes);
    return file;
}

q2_oracle_view::OracleConfig RunOracleOn(Scratch& scratch, const Input& input) {
    const fs::path file = Place(scratch.Clean("oracle"), input);
    return q2_oracle_view::RunOracle(file.string());
}

struct ImportRun {
    legacy::Config config;
    legacy::ReadStatus status = legacy::ReadStatus::Read;
};

// The import on a read-only copy of the input, which must leave its folder as it found it.
ImportRun RunImport(Scratch& scratch, const Input& input) {
    const fs::path dir = scratch.Clean("import");
    const fs::path file = Place(dir, input);
    if (input.bytes) SetReadOnly(file, true);
    const Listing before = List(dir);
    ImportRun run;
    run.status = legacy::Read(file.string().c_str(), run.config);
    Check(List(dir) == before, input.name + ": the import changed its folder");
    return run;
}

q2_oracle_view::FireTable FiresOf(const legacy::Config& c) {
    return q2_oracle_view::OracleFires(c.keyToggle, c.keyTogglePosition, c.keyToggleYaw);
}

ImportRun Comparison1(Scratch& scratch, const Input& input) {
    const q2_oracle_view::OracleConfig oracle = RunOracleOn(scratch, input);
    const ImportRun import = RunImport(scratch, input);

    // The dev build created a missing file and read it back, so it loaded every input; the import
    // writes nothing and reports the missing file as absent, with the same defaults.
    Check(oracle.loaded, input.name + ": the oracle did not load");
    Check(input.bytes.has_value() == (import.status == legacy::ReadStatus::Read),
          input.name + ": the import's status does not say whether there was a file");
    const std::vector<std::string> fields = FieldDifferences(oracle, import.config);
    Check(fields.empty(), input.name + ": fields differ: " + Join(fields));
    Check(StartupOf(oracle) == StartupOf(import.config), input.name + ": startup state differs");
    const q2_oracle_view::FireTable oracleFires =
        q2_oracle_view::OracleFires(oracle.keyToggle, oracle.keyTogglePosition, oracle.keyToggleYaw);
    const q2_oracle_view::FireTable importFires = FiresOf(import.config);
    Check(oracleFires == importFires,
          input.name + ": hotkeys fire differently: " + FirstFireDifference(oracleFires, importFires));
    return import;
}

// ---------------------------------------------------------------------------
// Comparison 2
// ---------------------------------------------------------------------------

namespace cfg = cameraunlock::config;
using cfg::ConfigLoadStatus;
using cfg::DropRule;
using cfg::DroppedValue;
using cfg::ImportResult;
using cfg::ImportStatus;
using cfg::schema::Concept;

cameraunlock::input::KeyModifiers g_currentHeld = cameraunlock::input::KeyModifiers::kNone;

cameraunlock::input::KeyModifiers CurrentHeld() { return g_currentHeld; }

cameraunlock::input::KeyModifiers ModifiersOf(int held) {
    using cameraunlock::input::KeyModifiers;
    KeyModifiers m = KeyModifiers::kNone;
    if ((held & 1) != 0) m = m | KeyModifiers::kCtrl;
    if ((held & 2) != 0) m = m | KeyModifiers::kShift;
    if ((held & 4) != 0) m = m | KeyModifiers::kAlt;
    return m;
}

// OracleFires' table for the current build. Mod::RegisterHotkeys parses each key list and hands
// it to RegisterKeyBindings, which puts one detail::GuardKey callback per distinct key on the
// poller, holding that key's bindings in list order. The same callbacks are built here with the
// held modifiers read from the test rather than the keyboard, since the poller keeps its
// callbacks to itself. Actions in the order toggle, mode cycle, yaw mode, as OracleFires counts
// them.
q2_oracle_view::FireTable CurrentFires(const Config& m) {
    using q2_oracle_view::kFirstKey;
    using q2_oracle_view::kHeldStates;
    using q2_oracle_view::kLastKey;
    std::array<int, q2_oracle_view::kActions> fired{};
    std::vector<std::pair<int, std::function<void()>>> registered;
    const std::string* lists[q2_oracle_view::kActions] = {&m.toggleKey, &m.cycleTrackingModeKey, &m.yawModeKey};
    for (int action = 0; action < q2_oracle_view::kActions; ++action) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*lists[action]);
        if (!parsed.ok()) throw std::logic_error("migrated hotkey list '" + *lists[action] + "' does not parse");
        std::vector<int> keys;
        std::vector<std::vector<cameraunlock::input::KeyModifiers>> modifiers;
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            const auto at = std::find(keys.begin(), keys.end(), b.vk);
            if (at == keys.end()) {
                keys.push_back(b.vk);
                modifiers.push_back({b.modifiers});
            } else {
                modifiers[static_cast<std::size_t>(at - keys.begin())].push_back(b.modifiers);
            }
        }
        for (std::size_t i = 0; i < keys.size(); ++i) {
            registered.emplace_back(keys[i], cameraunlock::input::detail::GuardKey(
                                                 std::move(modifiers[i]), [&fired, action] { ++fired[action]; },
                                                 &CurrentHeld));
        }
    }

    q2_oracle_view::FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            g_currentHeld = ModifiersOf(held);
            for (const auto& r : registered) {
                if (r.first == vk) r.second();
            }
            table.push_back(fired);
        }
    }
    g_currentHeld = cameraunlock::input::KeyModifiers::kNone;
    return table;
}

// A file as the test holds it to: its bytes, its last write time and its attributes.
struct FileStamp {
    std::string bytes;
    FILETIME written{};
    DWORD attributes = 0;

    bool operator==(const FileStamp& o) const {
        return bytes == o.bytes && CompareFileTime(&written, &o.written) == 0 && attributes == o.attributes;
    }
};

FileStamp Stamp(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        throw std::runtime_error("cannot stat " + path.string());
    }
    FileStamp s;
    s.bytes = ReadBytes(path);
    s.written = data.ftLastWriteTime;
    s.attributes = data.dwFileAttributes;
    return s;
}

// Where Defaults.ini is for each run of comparison 2: at the built-in values, which the first
// load creates, and with the values a player changed, written from it.
fs::path g_builtinDefaults;
fs::path g_alteredDefaults;

cfg::ConfigOwnerOptions<Config> OwnerOptions(const fs::path& dir, const fs::path& defaults) {
    return MakeConfigOwnerOptions(dir.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
}

// The import with its map, for the values it records.
ImportResult RunMappedImport(Scratch& scratch, const Input& input) {
    const fs::path file = Place(scratch.Clean("mapped"), input);
    Config out = MakeConfigTable().defaults();
    return MakeLegacyImport().run(cfg::LegacyInput{file.wstring(), file.string(), false}, out);
}

const DroppedValue* FindDrop(const std::vector<DroppedValue>& dropped, DropRule rule, const char* section,
                             const char* key) {
    for (const DroppedValue& d : dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return &d;
    }
    return nullptr;
}

struct Tally {
    std::string committed;
    std::set<std::string> migrated;
    struct Run {
        int created = 0;
        int imported = 0;
        // Migrated files holding at least one default row.
        int with_default_rows = 0;
        // Migrated files that differ from the committed file.
        int with_values = 0;
    } builtin, altered;
    int with_pose_shaping_dropped = 0;
    int with_unbound_key = 0;
    // Inputs that change a row from the dev build's default, and those among them that change the
    // mode.
    int touched = 0;
    int mode_touched = 0;
};

// Every row the table binds that follows Defaults.ini: every concept row but CollisionMargin,
// which is not global.
const std::set<Concept>& AllRows() {
    static const std::set<Concept> all = {
        Concept::UdpPort,          Concept::EnableOnStartup,    Concept::RotationEnabled,
        Concept::LocalSmoothing,   Concept::RemoteSmoothing,    Concept::WorldSpaceYaw,
        Concept::PositionEnabled,  Concept::PositionLimitX,     Concept::PositionLimitY,
        Concept::PositionLimitYDown, Concept::PositionLimitZ,   Concept::PositionLimitZBack,
        Concept::CollisionEnabled, Concept::CollisionReleaseSmoothing, Concept::ToggleKey,
        Concept::CycleTrackingModeKey, Concept::YawModeKey,
    };
    return all;
}

// The rows the player never changed: each reads as the dev build ran on with no file. One LimitY
// gave both vertical rows, and [Position] Enabled gave the mode pair.
std::set<Concept> UntouchedRows(const legacy::Config& l) {
    const legacy::Config d;
    std::set<Concept> u;
    const auto row = [&u](bool same, std::initializer_list<Concept> ids) {
        if (same) u.insert(ids.begin(), ids.end());
    };
    row(l.udpPort == d.udpPort, {Concept::UdpPort});
    row(l.enabled == d.enabled, {Concept::EnableOnStartup});
    row(l.positionEnabled == d.positionEnabled, {Concept::RotationEnabled, Concept::PositionEnabled});
    row(l.localSmoothing == d.localSmoothing, {Concept::LocalSmoothing});
    row(l.remoteSmoothing == d.remoteSmoothing, {Concept::RemoteSmoothing});
    row(l.worldSpaceYaw == d.worldSpaceYaw, {Concept::WorldSpaceYaw});
    row(l.posLimitX == d.posLimitX, {Concept::PositionLimitX});
    row(l.posLimitY == d.posLimitY, {Concept::PositionLimitY, Concept::PositionLimitYDown});
    row(l.posLimitZ == d.posLimitZ, {Concept::PositionLimitZ});
    row(l.posLimitZBack == d.posLimitZBack, {Concept::PositionLimitZBack});
    row(l.collisionEnabled == d.collisionEnabled, {Concept::CollisionEnabled});
    row(l.collisionReleaseSmoothing == d.collisionReleaseSmoothing, {Concept::CollisionReleaseSmoothing});
    row(l.keyToggle == d.keyToggle, {Concept::ToggleKey});
    row(l.keyTogglePosition == d.keyTogglePosition, {Concept::CycleTrackingModeKey});
    row(l.keyToggleYaw == d.keyToggleYaw, {Concept::YawModeKey});
    return u;
}

std::string Names(const std::set<Concept>& rows) {
    std::string text;
    for (const Concept row : rows) {
        text += (text.empty() ? "" : ", ") + std::string(cfg::schema::kConcepts[static_cast<std::size_t>(row)].name);
    }
    return text.empty() ? "none" : text;
}

// What the session runs on over the changed Defaults.ini (WriteAlteredDefaults), in the frozen
// reader's terms: the import's values, with each row the import left to Defaults.ini as that
// file gives it.
legacy::Config OverAlteredDefaults(legacy::Config l, const std::set<Concept>& follows) {
    const auto f = [&follows](Concept id) { return follows.count(id) != 0; };
    if (f(Concept::UdpPort)) l.udpPort = 4243;
    if (f(Concept::EnableOnStartup)) l.enabled = false;
    if (f(Concept::RotationEnabled)) l.positionEnabled = false;
    if (f(Concept::LocalSmoothing)) l.localSmoothing = 0.3f;
    if (f(Concept::RemoteSmoothing)) l.remoteSmoothing = 0.3f;
    if (f(Concept::WorldSpaceYaw)) l.worldSpaceYaw = false;
    if (f(Concept::PositionLimitX)) l.posLimitX = 0.45f;
    if (f(Concept::PositionLimitY)) l.posLimitY = 0.45f;
    if (f(Concept::PositionLimitZ)) l.posLimitZ = 0.45f;
    if (f(Concept::PositionLimitZBack)) l.posLimitZBack = 0.2f;
    if (f(Concept::CollisionEnabled)) l.collisionEnabled = false;
    if (f(Concept::CollisionReleaseSmoothing)) l.collisionReleaseSmoothing = 0.5f;
    if (f(Concept::ToggleKey)) l.keyToggle = 0x70;
    if (f(Concept::CycleTrackingModeKey)) l.keyTogglePosition = 0x71;
    if (f(Concept::YawModeKey)) l.keyToggleYaw = 0x72;
    return l;
}

// The dev build's shipped file, its first-run output and the empty file hold no value the dev
// build did not run on with no file, so every row follows Defaults.ini and the migration gives
// the committed file.
bool IsUnedited(const std::string& name) {
    return name == "empty file" || name == "dev-shipped.ini" || name == "dev-first-run.ini";
}

// Every pose-shaping value the frozen reader read is listed in its place, folded where it holds
// the shipped value and dropped as PoseShaping where it does not; a hotkey the dev reader left
// unbound stays unbound; and nothing is dropped by any other rule.
void CheckDrops(const std::string& name, const legacy::Config& l, const ImportResult& imported, Tally& tally) {
    const legacy::Config shipped;
    struct Read {
        const char* section;
        const char* key;
        bool atShipped;
    };
    const Read reads[] = {
        {"Rotation", "YawSensitivity", SameBits(l.yawSensitivity, shipped.yawSensitivity)},
        {"Rotation", "PitchSensitivity", SameBits(l.pitchSensitivity, shipped.pitchSensitivity)},
        {"Rotation", "RollSensitivity", SameBits(l.rollSensitivity, shipped.rollSensitivity)},
        {"Rotation", "InvertYaw", l.invertYaw == shipped.invertYaw},
        {"Rotation", "InvertPitch", l.invertPitch == shipped.invertPitch},
        {"Rotation", "InvertRoll", l.invertRoll == shipped.invertRoll},
        {"Position", "SensitivityX", SameBits(l.posSensX, shipped.posSensX)},
        {"Position", "SensitivityY", SameBits(l.posSensY, shipped.posSensY)},
        {"Position", "SensitivityZ", SameBits(l.posSensZ, shipped.posSensZ)},
        {"Position", "UnitsPerMeter", SameBits(l.positionUnitsPerMeter, shipped.positionUnitsPerMeter)},
    };
    Check(imported.pose_shaping.size() == std::size(reads),
          name + ": the import lists " + std::to_string(imported.pose_shaping.size()) + " pose-shaping values, not 10");
    if (imported.pose_shaping.size() != std::size(reads)) return;
    bool anyDropped = false;
    for (size_t k = 0; k < std::size(reads); ++k) {
        const cfg::PoseShapingValue& v = imported.pose_shaping[k];
        const std::string label = std::string("[") + reads[k].section + "] " + reads[k].key;
        Check(v.section == reads[k].section && v.key == reads[k].key, name + ": " + label + " is not listed in its place");
        Check(v.folded == reads[k].atShipped, name + ": " + label + " is " + (v.folded ? "folded" : "dropped") + " wrongly");
        const bool listed = FindDrop(imported.dropped, DropRule::PoseShaping, reads[k].section, reads[k].key) != nullptr;
        Check(listed != reads[k].atShipped,
              name + ": " + label + (listed ? " is dropped at its shipped value" : " is changed and not dropped"));
        if (!reads[k].atShipped) anyDropped = true;
    }
    if (anyDropped) ++tally.with_pose_shaping_dropped;
    if (l.keyToggle == 0 || l.keyTogglePosition == 0 || l.keyToggleYaw == 0) ++tally.with_unbound_key;

    for (const DroppedValue& d : imported.dropped) {
        Check(d.rule == DropRule::PoseShaping,
              name + ": the import drops [" + d.section + "] " + d.key + " by a rule this map never applies");
    }
}

// The settings the mod starts on after the migration against the ones the frozen reader's build
// started on, with the approved changes applied: identity pose shaping and the unit scale in code
// (CheckDrops holds the import to recording every value it leaves out), and the hotkeys as the
// dev build fired them. The dev build applied its one LimitY both ways.
std::vector<std::string> StartupDifferences(const legacy::Config& l, const Config& m) {
    std::vector<std::string> d;
    if (m.enableOnStartup != l.enabled) d.push_back("EnableOnStartup");
    if (m.udpPort != l.udpPort) d.push_back("UdpPort");
    const auto mode = cameraunlock::DecodeTrackingMode(m.rotationEnabled, m.positionEnabled);
    if (!mode || *mode != (l.positionEnabled ? cameraunlock::TrackingMode::RotationAndPosition
                                             : cameraunlock::TrackingMode::RotationOnly)) {
        d.push_back("tracking mode");
    }
    if (m.worldSpaceYaw != l.worldSpaceYaw) d.push_back("WorldSpaceYaw");
    if (!SameBits(m.localSmoothing, l.localSmoothing)) d.push_back("LocalSmoothing");
    if (!SameBits(m.remoteSmoothing, l.remoteSmoothing)) d.push_back("RemoteSmoothing");
    if (!SameBits(m.posLimitX, l.posLimitX)) d.push_back("PositionLimitX");
    if (!SameBits(m.posLimitY, l.posLimitY)) d.push_back("PositionLimitY");
    if (!SameBits(m.posLimitYDown, l.posLimitY)) d.push_back("PositionLimitYDown");
    if (!SameBits(m.posLimitZ, l.posLimitZ)) d.push_back("PositionLimitZ");
    if (!SameBits(m.posLimitZBack, l.posLimitZBack)) d.push_back("PositionLimitZBack");
    if (m.collisionEnabled != l.collisionEnabled) d.push_back("CollisionEnabled");
    if (!SameBits(m.collisionStandoff, l.collisionStandoff)) d.push_back("CollisionMargin");
    if (!SameBits(m.collisionReleaseSmoothing, l.collisionReleaseSmoothing)) d.push_back("CollisionReleaseSmoothing");

    const q2_oracle_view::FireTable before = FiresOf(l);
    const q2_oracle_view::FireTable after = CurrentFires(m);
    if (before != after) d.push_back("hotkeys: " + FirstFireDifference(before, after));
    return d;
}

// Every field the table binds, as the canonical renderer writes it, so two Configs compare whole.
std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

void Comparison2(Scratch& scratch, const Input& input, const ImportRun& import, const ImportResult* mapped,
                 const fs::path& defaults, Tally& tally) {
    const bool builtin = defaults == g_builtinDefaults;
    Tally::Run& run = builtin ? tally.builtin : tally.altered;
    const std::string name =
        input.name + (builtin ? " (Defaults.ini at the built-in values)" : " (Defaults.ini changed)");

    const fs::path dir = scratch.Clean("migration");
    const fs::path config = dir / kConfigFileName;
    const fs::path legacyFile = Place(dir, input);
    const FileStamp defaultsBefore = Stamp(defaults);
    FileStamp legacyBefore;
    if (input.bytes) legacyBefore = Stamp(legacyFile);

    const cfg::ConfigLoadResult<Config> loaded = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
    const Listing after = List(dir);
    Check(Stamp(defaults) == defaultsBefore, name + ": the load wrote Defaults.ini");
    if (input.bytes) {
        Check(Stamp(legacyFile) == legacyBefore, name + ": HeadTracking.ini did not keep its bytes, write time and attributes");
    }

    // Over the changed Defaults.ini the rows the import left to it take its values.
    const legacy::Config expected =
        builtin || !mapped ? import.config
                           : OverAlteredDefaults(import.config, std::set<Concept>(mapped->follows_defaults_ini.begin(),
                                                                                  mapped->follows_defaults_ini.end()));

    if (!input.bytes) {
        // A fresh install, which follows Defaults.ini.
        ++run.created;
        Check(loaded.status == ConfigLoadStatus::Created, name + ": no file is not Created");
        Check(after == Listing{{kConfigFileName, tally.committed}},
              name + ": the folder does not hold CameraUnlock.ini as config/HeadTracking.ini and nothing else");
        if (builtin) {
            const std::vector<std::string> d = StartupDifferences(import.config, loaded.config);
            Check(d.empty(), name + ": comparison 2: " + Join(d));
        }
        return;
    }

    if (builtin) CheckDrops(name, import.config, *mapped, tally);

    {
        const std::vector<std::string> d = StartupDifferences(expected, loaded.config);
        Check(d.empty(), name + ": comparison 2: " + Join(d));
    }

    ++run.imported;
    Check(loaded.status == ConfigLoadStatus::Migrated,
          name + ": the migration is " + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
    if (loaded.status != ConfigLoadStatus::Migrated) return;
    Check(after.size() == 2 && after[0].first == kConfigFileName && after[1].first == kLegacyConfigFileName &&
              after[1].second == *input.bytes,
          name + ": the folder does not hold CameraUnlock.ini and HeadTracking.ini and nothing else");
    Check(Contains(loaded.log, "created from"), name + ": the log does not say where CameraUnlock.ini came from");
    const std::string migrated = ReadBytes(config);
    tally.migrated.insert(migrated);
    if (migrated.find("=default\r\n") != std::string::npos) ++run.with_default_rows;
    if (migrated != tally.committed) ++run.with_values;
    for (const Concept row : mapped->follows_defaults_ini) {
        const std::string key = cfg::schema::kConcepts[static_cast<std::size_t>(row)].key;
        Check(migrated.find("\r\n" + key + "=default\r\n") != std::string::npos, name + ": " + key + " is not written default");
    }
    if (builtin && IsUnedited(input.name)) {
        Check(migrated == tally.committed, name + ": does not migrate to the committed file");
    }

    // The next launch reads CameraUnlock.ini over the same Defaults.ini, with nothing to report,
    // to the same settings, does not import, and writes neither file.
    {
        const cfg::ConfigLoadResult<Config> reread = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
        Check(reread.status == ConfigLoadStatus::Canonical && reread.diagnostics.empty(),
              name + ": the next launch does not read CameraUnlock.ini cleanly");
        Check(AllValues(reread.config) == AllValues(loaded.config), name + ": the next launch runs on other settings");
        Check(!Contains(reread.log, "created from"), name + ": the next launch imports again");
        Check(Contains(reread.log, "is left as it was and is not read"),
              name + ": the next launch does not say HeadTracking.ini is not read");
        Check(List(dir) == after && Stamp(legacyFile) == legacyBefore && Stamp(defaults) == defaultsBefore,
              name + ": the next launch changed a file");
    }

    // A read-only HeadTracking.ini imports as a writable one does and keeps its attribute, bytes
    // and write time.
    if (builtin) {
        const fs::path roDir = scratch.Clean("read-only");
        const fs::path roLegacy = Place(roDir, input);
        SetReadOnly(roLegacy, true);
        const FileStamp roBefore = Stamp(roLegacy);
        const cfg::ConfigLoadResult<Config> fromReadOnly = cfg::ConfigOwner<Config>(OwnerOptions(roDir, defaults)).Load();
        Check(fromReadOnly.status == ConfigLoadStatus::Migrated && AllValues(fromReadOnly.config) == AllValues(loaded.config) &&
                  ReadBytes(roDir / kConfigFileName) == migrated,
              name + ": a read-only HeadTracking.ini does not import as a writable one does");
        Check(Stamp(roLegacy) == roBefore && (roBefore.attributes & FILE_ATTRIBUTE_READONLY) != 0,
              name + ": a read-only HeadTracking.ini did not keep its attribute, bytes and write time");
    }
}

// Defaults.ini as a player may have changed it, from the one the owner created: every value this
// game takes from it differs from the built-in one, each set to the corpus's alternate for the
// legacy key it comes from, so a corpus input holding that alternate migrates as default.
void WriteAlteredDefaults() {
    std::string text = ReadBytes(g_builtinDefaults);
    const std::pair<const char*, const char*> changes[] = {
        {"UdpPort=4242", "UdpPort=4243"},
        {"EnableOnStartup=true", "EnableOnStartup=false"},
        {"PositionEnabled=true", "PositionEnabled=false"},
        {"LocalSmoothing=0.0", "LocalSmoothing=0.3"},
        {"RemoteSmoothing=0.15", "RemoteSmoothing=0.3"},
        {"WorldSpaceYaw=true", "WorldSpaceYaw=false"},
        {"PositionLimitX=0.3", "PositionLimitX=0.45"},
        {"PositionLimitY=0.2", "PositionLimitY=0.45"},
        {"PositionLimitYDown=0.2", "PositionLimitYDown=0.45"},
        {"PositionLimitZ=0.4", "PositionLimitZ=0.45"},
        {"PositionLimitZBack=0.1", "PositionLimitZBack=0.2"},
        {"CollisionEnabled=true", "CollisionEnabled=false"},
        {"CollisionReleaseSmoothing=0.9", "CollisionReleaseSmoothing=0.5"},
        {"ToggleKey=End, Ctrl+Shift+Y", "ToggleKey=F1, Ctrl+Shift+Y"},
        {"CycleTrackingModeKey=PageUp, Ctrl+Shift+G", "CycleTrackingModeKey=F2, Ctrl+Shift+G"},
        {"YawModeKey=PageDown, Ctrl+Shift+H", "YawModeKey=F3, Ctrl+Shift+H"},
    };
    for (const auto& [from, to] : changes) {
        const std::string line = std::string("\r\n") + from + "\r\n";
        const size_t at = text.find(line);
        if (at == std::string::npos) throw std::runtime_error(std::string("the created Defaults.ini has no line ") + from);
        text.replace(at + 2, std::strlen(from), to);
    }
    fs::create_directories(g_alteredDefaults.parent_path());
    WriteBytes(g_alteredDefaults, text);
}

std::string Data(const char* name) {
    const std::string bytes = ReadBytes(fs::path(Q2RTXHT_DIFFERENTIAL_DATA) / name);
    Check(!bytes.empty(), std::string("data/") + name + " is empty");
    return bytes;
}

std::vector<Input> Inputs() {
    using cameraunlock::config::testing::GenerateIniMutations;
    std::vector<Input> inputs;
    inputs.push_back({"no file", std::nullopt});
    inputs.push_back({"empty file", std::string()});
    inputs.push_back({"dev-shipped.ini", Data("dev-shipped.ini")});
    inputs.push_back({"dev-first-run.ini", Data("dev-first-run.ini")});
    // Hotkeys the dev reader separated: one moved back to its default, and one left unbound
    // because its default was taken too.
    inputs.push_back({"colliding hotkeys", std::string("[Controls]\r\nToggleKey=0x76\r\nTogglePositionKey=0x76\r\n")});
    inputs.push_back({"colliding hotkeys, default taken",
                      std::string("[Controls]\r\nToggleKey=0x21\r\nTogglePositionKey=0x21\r\n")});
    for (auto& m : GenerateIniMutations(Data("dev-shipped.ini"), legacy::ReadKeys(), MutationKeys())) {
        inputs.push_back({std::string("corpus over dev-shipped.ini: ") + m.name, std::move(m.bytes)});
    }
    return inputs;
}

// What the oracle creates for a missing file.
std::string OracleFirstRun(Scratch& scratch) {
    const fs::path file = scratch.Clean("first-run") / kFileName;
    q2_oracle_view::RunOracle(file.string());
    return ReadBytes(file);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Scratch scratch;
        if (argc == 3 && std::strcmp(argv[1], "--extract-first-run") == 0) {
            WriteBytes(argv[2], OracleFirstRun(scratch));
            return 0;
        }
        if (argc != 1) {
            std::printf("usage: %s [--extract-first-run <path>]\n", argv[0]);
            return 2;
        }

        // The dev build's first-run output, committed once as test data, is what the oracle
        // still writes for a missing file.
        Check(OracleFirstRun(scratch) == Data("dev-first-run.ini"),
              "the oracle's first-run output differs from data/dev-first-run.ini");

        Tally tally;
        tally.committed = ReadBytes(fs::path(Q2RTXHT_COMMITTED_CONFIG));
        Check(!tally.committed.empty(), "config/HeadTracking.ini is missing");

        // Each Defaults.ini sits outside the game folder, in a user folder of its own whose
        // parent exists, as the owner requires before it creates the file.
        g_builtinDefaults = scratch.Clean("user-builtin") / "CameraUnlock" / "Defaults.ini";
        g_alteredDefaults = scratch.Clean("user-altered") / "CameraUnlock" / "Defaults.ini";
        {
            const fs::path dir = scratch.Clean("first-load");
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Created,
                  "the first load is not Created");
            Check(fs::exists(g_builtinDefaults), "the first load did not create Defaults.ini");
        }
        WriteAlteredDefaults();

        // Fresh equals upgrade: over Defaults.ini at the built-in values, the dev build's shipped
        // file and its first-run output each import into a CameraUnlock.ini that is the committed
        // file, which is what a fresh install creates.
        for (const char* name : {"dev-shipped.ini", "dev-first-run.ini"}) {
            const fs::path dir = scratch.Clean("fresh-equals-upgrade");
            WriteBytes(dir / kFileName, Data(name));
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Migrated &&
                      ReadBytes(dir / kConfigFileName) == tally.committed,
                  std::string(name) + " does not import into the committed file");
        }

        const std::vector<Input> inputs = Inputs();
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (dev b6665cf) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) {
            const ImportRun import = Comparison1(scratch, input);
            std::optional<ImportResult> mapped;
            if (input.bytes) {
                mapped = RunMappedImport(scratch, input);
                Check(mapped->status == ImportStatus::Imported, input.name + ": the mapped import is not Imported");
                const std::set<Concept> follows(mapped->follows_defaults_ini.begin(), mapped->follows_defaults_ini.end());
                Check(follows.size() == mapped->follows_defaults_ini.size(),
                      input.name + ": follows_defaults_ini names a row twice");
                const std::set<Concept> untouched = UntouchedRows(import.config);
                Check(follows == untouched, input.name + ": follows Defaults.ini " + Names(follows) +
                                                ", but the rows the player never changed are " + Names(untouched));
                if (untouched != AllRows()) ++tally.touched;
                if (!untouched.count(Concept::RotationEnabled)) ++tally.mode_touched;
                if (IsUnedited(input.name)) Check(untouched == AllRows(), input.name + ": a row is changed");
            }
            for (const fs::path& defaults : {g_builtinDefaults, g_alteredDefaults}) {
                Comparison2(scratch, input, import, mapped ? &*mapped : nullptr, defaults, tally);
            }
        }

        std::printf("comparison 2, the import against the migration, %zu distinct files:\n", tally.migrated.size());
        for (const auto& [over, run] : {std::pair<const char*, const Tally::Run*>{"at the built-in values", &tally.builtin},
                                        std::pair<const char*, const Tally::Run*>{"changed", &tally.altered}}) {
            std::printf("  over Defaults.ini %s: %d created, %d imported (%d holding a default row, %d differing from "
                        "the committed file)\n",
                        over, run->created, run->imported, run->with_default_rows, run->with_values);
            Check(run->with_default_rows > 0, std::string("no import writes default over ") + over);
            Check(run->with_values > 0, std::string("no import writes a value over ") + over);
        }
        std::printf("  %d with a changed sensitivity, inversion or unit scale dropped (pose_shaping)\n",
                    tally.with_pose_shaping_dropped);
        std::printf("  %d with a hotkey the dev reader left unbound\n", tally.with_unbound_key);
        std::printf("  %d inputs changed a row from the dev build's default, %d of them the tracking mode\n", tally.touched,
                    tally.mode_touched);
        Check(tally.with_pose_shaping_dropped > 0, "no input drops a changed pose-shaping value");
        Check(tally.with_unbound_key > 0, "no input leaves a hotkey unbound");
        Check(tally.touched > 0 && tally.mode_touched > 0,
              "no input changes a row, the tracking mode among them, which then does not follow Defaults.ini");
        Check(tally.migrated.count(tally.committed) == 1, "no input migrated to the committed file");

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const fs::path lintDir = fs::path(exe).parent_path() / "migrated";
        fs::remove_all(lintDir);
        fs::create_directories(lintDir);
        int n = 0;
        for (const std::string& file : tally.migrated) WriteBytes(lintDir / (std::to_string(n++) + ".ini"), file);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
