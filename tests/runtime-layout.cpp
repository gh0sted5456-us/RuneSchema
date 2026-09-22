#include "Runtime/Layout.h"
#include <cassert>
#include <chrono>

int main() {
    namespace fs = std::filesystem;
    using namespace PS;
    const auto root = fs::temp_directory_path() / ("runeschema-layout-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(fs::create_directory(root));
    ConfigFiles::Write(root / "config/config.json", "legacy-settings");
    ConfigFiles::Write(root / "config.json", "fallback-settings");
    ConfigFiles::Write(root / "config/diagnostics/presets/test.json", "preset");
    ConfigFiles::Write(root / "runtime/searches/presets/test.json", "new-preset");
    ConfigFiles::Write(root / "runtime/reference/ai-catalog.json", "ai");
    ConfigFiles::Write(root / "runtime/cache/icon.bin", "icon");
    ConfigFiles::Write(root / "runtime/progress/quests/player.json", "progress");
    ConfigFiles::Write(root / "settings/F2-catalog-cache.json", "cache");
    ConfigFiles::Write(root / "config/diagnostics/trace-profiles/trace.jsonc", "trace");
    ConfigFiles::Write(root / "config/diagnostics/schema/schema.json", "schema");
    ConfigFiles::Write(root / "config/diagnostics/exports/result.json", "result");
    ConfigFiles::Write(root / "config/diagnostics/report.json", "report");
    ConfigFiles::Write(root / "dlls/runtime/state.json", "state");
    RuntimeLayout::Migrate(root);
    assert(ConfigFiles::Read(root / "settings/settings.jsonc") == "legacy-settings");
    assert(ConfigFiles::Read(root / "runtime/live/jobs/searches/presets/test.json") == "new-preset");
    assert(ConfigFiles::Read(root / "runtime/live/jobs/searches/trace-profiles/trace.jsonc") == "trace");
    assert(ConfigFiles::Read(root / "runtime/live/jobs/exports/schema/schema.json") == "schema");
    assert(ConfigFiles::Read(root / "runtime/live/jobs/exports/result.json") == "result");
    assert(ConfigFiles::Read(root / "runtime/live/jobs/exports/report.json") == "report");
    assert(ConfigFiles::Read(root / "runtime/live/jobs/exports/state.json") == "state");
    assert(ConfigFiles::Read(root / "runtime/live/saved/references/Helpy-catalog-ai.json") == "ai");
    assert(ConfigFiles::Read(root / "runtime/live/saved/cache/icon.bin") == "icon");
    assert(ConfigFiles::Read(root / "runtime/live/saved/cache/Helpy-catalog-cache.json") == "cache");
    assert(ConfigFiles::Read(root / "runtime/live/saved/progress/quests/player.json") == "progress");
    assert(!fs::exists(root / "settings/settings.json"));
    assert(!fs::exists(root / "settings/F2-catalog-cache.json"));
    assert(fs::exists(root / "config/config.json"));
    ConfigFiles::Write(root / "settings/settings.jsonc", "new-settings");
    RuntimeLayout::Migrate(root);
    assert(ConfigFiles::Read(root / "settings/settings.jsonc") == "new-settings");
    // Test-owned, unique temporary directory only.
    assert(fs::equivalent(root.parent_path(), fs::temp_directory_path()));
    fs::remove_all(root);
}
