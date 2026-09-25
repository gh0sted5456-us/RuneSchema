#include "Utility/ConfigCodec.h"
#include "Core/ConfigFiles.h"
#include <cassert>
#include <chrono>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace fs = std::filesystem;
#undef assert
#define assert(expression) do { if(!(expression)) throw std::runtime_error("Assertion failed: " #expression); } while(false)
template<class F> void Rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
int RunConfigSettings() {
    using namespace PS;
    const auto root = fs::current_path() / ("config-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(fs::create_directory(root));
    const auto file = root / "config.json";
    ConfigFiles::Write(file,"1234");
    assert(ConfigFiles::Read(file,4)=="1234");
    assert(ConfigFiles::Read(file,8)=="1234");
    Rejects([&]{ConfigFiles::Read(file,3);});
    Rejects([&]{ConfigFiles::Read(root/"missing",4);});
    ConfigFiles::Write(file,"");
    assert(ConfigFiles::Read(file,0).empty());
    Rejects([&]{ConfigFiles::Read(file,8*1024*1024+1);});
    auto settings = DecodeSettings(R"({"advancedLogging":true,"colorCodeLoaderAnnotations":false})");
    assert(settings.advancedLogging && settings.loaders.buildings);
    assert(!settings.colorCodeLoaderAnnotations);
    assert(!DecodeSettings("{}").advancedLogging);
    assert(!DecodeSettings("{}").persistence.characterCustomization);
    assert(!DecodeSettings("{}").persistence.journal);
    assert(!DecodeSettings("{}").persistence.recipes);
    auto persistenceSettings=DecodeSettings(R"({"persistence":{"characterCustomization":true,"journal":true,"recipes":true}})");
    assert(persistenceSettings.persistence.characterCustomization
        && persistenceSettings.persistence.journal && persistenceSettings.persistence.recipes);
    assert(DecodeSettings(EncodeSettings(persistenceSettings)).persistence.journal);
    Rejects([&]{DecodeSettings(R"({"persistence":{"characterCustomization":"yes"}})");});
    assert(DecodeSettings("{}").authoringTools);
    assert(DecodeSettings("{}").loaders.dialogue);
    assert(DecodeSettings("{}").loaders.quests);
    assert(DecodeSettings("{}").loaders.events);
    assert(DecodeSettings("{}").loaders.lore);
    assert(!DecodeSettings(R"({"loaders":{"lore":false}})").loaders.lore);
    assert(!DecodeSettings(R"({"loaders":{"events":false}})").loaders.events);
    assert(!DecodeSettings(R"({"loaders":{"quests":false}})").loaders.quests);
    assert(!DecodeSettings(R"({"loaders":{"dialogue":false}})").loaders.dialogue);
    assert(!DecodeSettings("{}").npcDiagnostics.statusExport);
    assert(!DecodeSettings("{}").npcDiagnostics.interactionTraceExport);
    assert(!DecodeSettings("{}").diagnosticJobs.enabled);
    assert(!DecodeSettings("{}").diagnosticJobs.characterEditorPreset);
    auto jobSettings=DecodeSettings(R"({"advancedRuntime":true,"diagnosticJobs":{"enabled":true,"characterEditorPreset":true}})");
    assert(jobSettings.advancedRuntime && jobSettings.diagnosticJobs.enabled
        && jobSettings.diagnosticJobs.characterEditorPreset);
    assert(DecodeSettings(EncodeSettings(jobSettings)).diagnosticJobs.enabled);
    assert(!DecodeSettings(R"({"advancedLogging":true})").npcDiagnostics.statusExport);
    auto diagnosticSettings=DecodeSettings(R"({"npcDiagnostics":{"statusExport":true,"interactionTraceExport":false}})");
    assert(diagnosticSettings.npcDiagnostics.statusExport && !diagnosticSettings.npcDiagnostics.interactionTraceExport);
    diagnosticSettings.npcDiagnostics.interactionTraceExport=true;
    assert(DecodeSettings(EncodeSettings(diagnosticSettings)).npcDiagnostics.interactionTraceExport);
    Rejects([&]{DecodeSettings(R"({"npcDiagnostics":{"statusExport":"yes"}})");});
    assert(DecodeSettings("{}").colorCodeLoaderAnnotations);
    assert(DecodeSettings(R"({"loaders":{"retired_feature":true,"assets":false}})").loaders.assets == false);
    ConfigFiles::Write(file, EncodeSettings(settings));
    assert(DecodeSettings(ConfigFiles::Read(file)).advancedLogging);
    settings.advancedLogging = false;
    settings.colorCodeLoaderAnnotations = true;
    ConfigFiles::Write(file, EncodeSettings(settings));
    assert(!DecodeSettings(ConfigFiles::Read(file)).advancedLogging);
    const std::string broken = "{\"advancedLogging\":true,\"loaders\":{\"assets\":false},\"plugins\": [";
    ConfigFiles::Write(file, broken);
    PSConfigSettings unchanged{};
    Rejects([&] { unchanged = DecodeSettings(ConfigFiles::Read(file)); });
    assert(!unchanged.advancedLogging && unchanged.loaders.assets);
    const auto backup = ConfigFiles::Backup(file);
    const auto secondBackup = ConfigFiles::Backup(file);
    assert(backup != secondBackup && ConfigFiles::Read(backup) == broken);
    ConfigFiles::Write(file, EncodeSettings(unchanged));
    assert(ConfigFiles::Read(backup) == broken);
    assert(DecodeSettings(ConfigFiles::Read(file)).loaders.assets);
    Rejects([&] { DecodeSettings(R"({"advancedLogging":"yes"})"); });
    Rejects([&] { DecodeSettings(""); });
    const auto blocked = root / "directory.json";
    fs::create_directory(blocked);
    Rejects([&] { ConfigFiles::Write(blocked, "{}"); });
    Rejects([&] { ConfigFiles::Backup(blocked); });
    assert(fs::is_directory(blocked));
#ifdef _WIN32
    // A non-sharing handle forces backup and replacement failures, independent of ACLs.
    const auto original = ConfigFiles::Read(file);
    HANDLE locked = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(locked != INVALID_HANDLE_VALUE);
    Rejects([&] { ConfigFiles::Backup(file); });
    Rejects([&] { ConfigFiles::Write(file, "replacement"); });
    CloseHandle(locked);
    assert(ConfigFiles::Read(file) == original);
#endif
    for (const auto& entry : fs::directory_iterator(root)) {
        assert(entry.path().filename().string().find(".write-") == std::string::npos);
        fs::remove(entry.path());
    }
    fs::remove(root);
    std::cout << "Configuration round-trip, defaults, malformed input, backups, failed writes and staging cleanup passed.\n";
    return 0;
}
int main() {
    try{return RunConfigSettings();}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
