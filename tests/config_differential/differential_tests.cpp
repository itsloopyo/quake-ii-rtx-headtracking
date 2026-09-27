// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read by:
//
//   oracle     the reader of the dev pre-release (b6665cf), the newest published build, with the
//              core sources it compiled at its pin fb55a6a (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// Inputs: no file, an empty file, the dev build's shipped config/HeadTracking.ini (which its
// install.cmd seeded and its launcher-manifest.json seeded byte for byte), the first-run output
// of the dev build, and core's corpus over the shipped file.
//
// `--extract-first-run <path>` writes what the oracle creates for a missing file to <path>, which
// is how data/dev-first-run.ini was made.

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/testing/ini_mutations.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
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

constexpr const char* kFileName = "HeadTracking.ini";

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

        const std::vector<Input> inputs = Inputs();
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (dev b6665cf) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) Comparison1(scratch, input);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
