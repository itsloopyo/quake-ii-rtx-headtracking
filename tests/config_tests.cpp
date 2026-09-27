// CameraUnlock.ini and what the conversion moved into code: the committed file is the table's
// fresh render, the defaults the dev build ran on map to the defaults, a first start creates the
// committed file, the mode and yaw hotkeys save only their own lines and leave Defaults.ini and
// HeadTracking.ini alone, End's row cannot be saved, a folder the ANSI code page cannot name
// imports nothing (the dev build read no file there), and the version agrees everywhere it is
// written.
//
// `--render-config <path>` writes the committed file instead (pixi run render-config).

#include "core/config.h"
#include "legacy_config/legacy_config.h"
#include "version.h"

#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Q2RTXHT;

namespace {

namespace cfg = cameraunlock::config;
namespace fs = std::filesystem;

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// The file a first start creates.
std::string Rendered() {
    return cfg::RenderCanonicalFresh(MakeConfigTable(), {kConfigDisplayName});
}

std::string Committed() {
    return ReadBytes(fs::path(Q2RTXHT_COMMITTED_CONFIG));
}

void TestCommittedConfigIsRendered() {
    Check(Committed() == Rendered(), "config/HeadTracking.ini is the table's fresh render (pixi run render-config)");
}

// A scratch game folder, and a Defaults.ini of its own beside it that the first load creates.
struct Scratch {
    fs::path root;
    fs::path game;
    fs::path defaults;

    explicit Scratch(const std::wstring& gameFolder) {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        root = fs::path(temp) / (L"q2rtx-config-tests-" + std::to_wstring(GetCurrentProcessId()));
        game = root / gameFolder;
        fs::remove_all(game);
        fs::create_directories(game);
        fs::create_directories(root / L"user");
        defaults = root / L"user" / L"CameraUnlock" / L"Defaults.ini";
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    cfg::ConfigOwnerOptions<Config> Options() const {
        return MakeConfigOwnerOptions(game.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
    }
    fs::path ConfigPath() const { return game / kConfigFileName; }
    fs::path LegacyPath() const { return game / kLegacyConfigFileName; }
};

std::vector<std::string> Listing(const fs::path& dir) {
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

// With neither file there, the first start creates CameraUnlock.ini as the committed file and
// Defaults.ini with the built-in values, and no HeadTracking.ini.
void TestFirstStartCreatesTheCommittedFile() {
    const Scratch s(L"first-start");
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Created, "a first start with no file is Created");
    Check(ReadBytes(s.ConfigPath()) == Committed(), "a first start creates config/HeadTracking.ini's bytes");
    Check(Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini"}, "a first start creates CameraUnlock.ini and nothing else");
    Check(fs::exists(s.defaults), "a first start creates Defaults.ini");
    Check(AllValues(loaded.config) == AllValues(MakeConfigTable().defaults()), "a first start runs on the built-in values");
}

// A fresh install and an upgrade from the dev build's defaults start the same: the map of the
// frozen defaults holds every row at the table's default, leaves every global row to Defaults.ini,
// drops nothing, and every sensitivity, inversion and the unit scale is the shipped value.
void TestLegacyDefaultsMapToTheDefaults() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const fs::path missing = fs::path(temp) / L"q2rtx-no-such-folder" / kLegacyConfigFileName;
    const auto table = MakeConfigTable();
    Config mapped = table.defaults();
    const cfg::ImportResult result =
        MakeLegacyImport().run(cfg::LegacyInput{missing.wstring(), missing.string(), false}, mapped);
    Check(result.status == cfg::ImportStatus::Absent, "no file imports as Absent");
    Check(result.dropped.empty(), "the dev build's defaults drop nothing");
    Check(result.pose_shaping.size() == 10, "every sensitivity, inversion and the unit scale is recorded");
    Check(result.follows_defaults_ini.size() == 17, "the 17 global rows follow Defaults.ini");
    for (const cfg::PoseShapingValue& value : result.pose_shaping) {
        Check(value.folded, "[" + value.section + "] " + value.key + " at its shipped value is folded");
    }
    Check(AllValues(mapped) == AllValues(table.defaults()), "the dev build's defaults map to the defaults");
    Check(mapped.toggleKey == "End, Ctrl+Shift+Y" && mapped.cycleTrackingModeKey == "PageUp, Ctrl+Shift+G" &&
              mapped.yawModeKey == "PageDown, Ctrl+Shift+H",
          "the old hotkeys and their chords become key lists");
    Check(legacy::Config{}.collisionStandoff == kDefaultCollisionMargin,
          "the collision margin is the one the dev build shipped");
    Check(legacy::Config{}.positionUnitsPerMeter == kUnitsPerMeter, "the unit scale is the one the dev build shipped");
}

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < bytes.size()) {
        const size_t end = bytes.find("\r\n", start);
        lines.push_back(bytes.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

// The lines of `after` that differ from `before`, which must have as many lines.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before);
    const std::vector<std::string> b = Lines(after);
    if (a.size() != b.size()) return {"a line was added or removed"};
    std::vector<std::string> changed;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// A save changes the lines of its rows and no other byte, writes a value over default, and
// touches neither Defaults.ini nor HeadTracking.ini; the tracking mode and the yaw mode persist,
// and End's row cannot be saved at all.
void TestHotkeySaves() {
    const Scratch s(L"save");
    const std::string committed = Committed();
    const std::string legacyBytes = "[General]\r\nEnabled=0\r\n";
    WriteBytes(s.ConfigPath(), committed);
    WriteBytes(s.LegacyPath(), legacyBytes);

    {
        cfg::ConfigOwner<Config> owner(s.Options());
        const auto loaded = owner.Load();
        Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
        Check(loaded.config.enableOnStartup, "HeadTracking.ini is not read while CameraUnlock.ini exists");
        Check(Contains(loaded.log, "is left as it was and is not read"),
              "the log says HeadTracking.ini is not read while CameraUnlock.ini exists");
        const std::string defaultsBefore = ReadBytes(s.defaults);

        const auto rotationOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
        const cfg::ConfigSaveResult first = owner.Save([rotationOnly](Config& c) {
            c.rotationEnabled = rotationOnly.rotation_enabled;
            c.positionEnabled = rotationOnly.position_enabled;
        });
        Check(first.status == cfg::ConfigSaveStatus::Saved, "the tracking mode saves");
        Check(Contains(first.log, "no longer follows Defaults.ini"),
              "the save says the mode pair stopped following Defaults.ini");
        const std::string afterRotationOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(committed, afterRotationOnly) == std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"},
              "saving rotation only writes the mode pair over default and changes nothing else");

        const auto positionOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::PositionOnly);
        Check(owner.Save([positionOnly](Config& c) {
                  c.rotationEnabled = positionOnly.rotation_enabled;
                  c.positionEnabled = positionOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the third tracking mode saves");
        const std::string afterPositionOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterRotationOnly, afterPositionOnly) ==
                  std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"},
              "saving position only changes the mode pair and nothing else");

        Check(owner.Save([](Config& c) { c.worldSpaceYaw = false; }).status == cfg::ConfigSaveStatus::Saved,
              "the yaw mode saves");
        const std::string afterYaw = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterPositionOnly, afterYaw) == std::vector<std::string>{"WorldSpaceYaw=false"},
              "saving the yaw mode changes its own line and nothing else");

        bool refused = false;
        try {
            owner.Save([](Config& c) { c.enableOnStartup = false; });
        } catch (const std::logic_error&) {
            refused = true;
        }
        Check(refused, "EnableOnStartup is not Writable, so the End toggle cannot persist");
        Check(ReadBytes(s.ConfigPath()) == afterYaw, "a refused save writes nothing");

        Check(ReadBytes(s.defaults) == defaultsBefore, "saving leaves Defaults.ini as it was");
        Check(ReadBytes(s.LegacyPath()) == legacyBytes, "saving leaves HeadTracking.ini as it was");
    }

    const auto again = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical && again.diagnostics.empty() &&
              !again.config.rotationEnabled && again.config.positionEnabled && !again.config.worldSpaceYaw &&
              again.config.enableOnStartup,
          "the saved tracking mode and yaw mode come back at the next start");
    Check((Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini", "HeadTracking.ini"}),
          "the game folder holds CameraUnlock.ini and HeadTracking.ini and nothing else");
}

// The dev build named the exe folder in the ANSI code page and, where the code page could not hold
// the name, read no file and ran on its defaults. The import does the same, and HeadTracking.ini
// stays as it was.
void TestAFolderTheCodepageCannotNameImportsNothing() {
    if (GetACP() == CP_UTF8) {
        std::printf("note: the ANSI code page is UTF-8, which names every folder; skipping\n");
        return;
    }
    const Scratch s(L"q2rtx-\x4E2D");
    const std::string legacyBytes = "[General]\r\nUdpPort=5000\r\n";
    WriteBytes(s.LegacyPath(), legacyBytes);
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(loaded.config.udpPort == 4242, "the port is the default the dev build ran on there");
    Check(ReadBytes(s.LegacyPath()) == legacyBytes, "HeadTracking.ini stays as it was");
}

// The value of the first `"key": "value"` pair in `json`.
std::string JsonStringValue(const std::string& json, const char* key) {
    const std::string marker = std::string("\"") + key + "\"";
    const size_t at = json.find(marker);
    if (at == std::string::npos) return {};
    const size_t open = json.find('"', json.find(':', at + marker.size()) + 1);
    const size_t close = json.find('"', open + 1);
    return json.substr(open + 1, close - open - 1);
}

void TestTheVersionAgreesEverywhereItIsWritten() {
    Check(std::string(MOD_VERSION) == Q2RTXHT_PROJECT_VERSION, "src/version.h and CMakeLists.txt name the same version");
    Check(JsonStringValue(ReadBytes(Q2RTXHT_MANIFEST), "version") == MOD_VERSION,
          "launcher-manifest.json names the version src/version.h does");
    const std::string install = ReadBytes(Q2RTXHT_INSTALL_CMD);
    const std::string marker = "set \"MOD_VERSION=";
    const size_t at = install.find(marker);
    Check(at != std::string::npos, "install.cmd carries a MOD_VERSION");
    if (at == std::string::npos) return;
    const size_t from = at + marker.size();
    Check(install.substr(from, install.find('"', from) - from) == MOD_VERSION,
          "install.cmd names the version src/version.h does");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
            WriteBytes(argv[2], Rendered());
            return 0;
        }
        if (argc != 1) {
            std::printf("usage: %s [--render-config <path>]\n", argv[0]);
            return 2;
        }

        TestCommittedConfigIsRendered();
        TestLegacyDefaultsMapToTheDefaults();
        TestFirstStartCreatesTheCommittedFile();
        TestHotkeySaves();
        TestAFolderTheCodepageCannotNameImportsNothing();
        TestTheVersionAgreesEverywhereItIsWritten();
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config tests: all passed\n");
        return 0;
    }
    std::printf("config tests: %d failure(s)\n", g_failures);
    return 1;
}
