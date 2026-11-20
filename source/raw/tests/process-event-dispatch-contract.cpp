// The name-only ProcessEvent hooks go through the shared dispatcher, not one global callback each.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Missing source: " + path.string());
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}
void Check(bool ok, const std::string& what) { if (!ok) throw std::runtime_error(what); }
// The route for `label` is added through the dispatcher with `names`, and the file no longer
// registers a global callback under that label.
void Routed(const std::string& source, const std::string& file, const std::string& label, const std::string& names) {
    const auto label_at = source.find("\"" + label + "\"");
    Check(label_at != std::string::npos, file + ": route " + label + " is missing");
    const auto add = source.rfind("PS::ProcessEventDispatch::Add(", label_at);
    Check(add != std::string::npos && label_at - add < 400, file + ": " + label + " is not added through the dispatcher");
    Check(source.find(names, add) < label_at, file + ": " + label + " is not routed by " + names);
    Check(source.find("TEXT(\"" + label + "\")") == std::string::npos, file + ": " + label + " is still a global callback name");
}
}

int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("Expected the raw source root");
    const std::filesystem::path root = argv[1];
    const auto dispatch = Read(root / "include/Runtime/ProcessEventDispatch.h");
    Check(dispatch.find("RemoveSelf()") == std::string::npos, "The dispatcher must never remove its shared callback per handler");
    Check(dispatch.find("Hook::RegisterProcessEventPreCallback") != std::string::npos
        && dispatch.find("Hook::RegisterProcessEventPostCallback") != std::string::npos, "Shared callbacks are not registered");

    const auto assets = Read(root / "src/Loader/DragonWildsAssetModLoader.cpp");
    Routed(assets, "assets", "CharacterMenuAssetPatchReplay", "{TEXT(\"Construct\"), TEXT(\"BP_OnOpen\")}");
    Check(assets.find("PS::ProcessEventDispatch::Remove(m_characterMenuPatchHook)") != std::string::npos, "assets: route is not removed");

    const auto npc = Read(root / "src/Loader/DragonWildsNpcLoader.cpp");
    Routed(npc, "npc", "RuneSchemaSummonKillCredit", "{TEXT(\"OnAIKilled\")}");

    const auto journal = Read(root / "src/Loader/DragonWildsJournalModLoader.cpp");
    Routed(journal, "journal", "RuneSchemaJournalAcquisition", "TEXT(\"OnRep_HasLoadedFromSave\")");
    Check(journal.find("PS::ProcessEventDispatch::Remove(m_acquisitionCallbackId)") != std::string::npos, "journal: route is not removed");

    const auto spawn = Read(root / "src/Loader/DragonWildsSpawnLoader.cpp");
    Routed(spawn, "spawn", "ConfirmedPlayerRespawn", "{TEXT(\"Multicast_Respawn\")}");
    Routed(spawn, "spawn", "PlayerActivityObservation", "std::move(activityNames)");
    // Activity names follow the player rules: the route is rebuilt, never left with stale names.
    const auto activity = spawn.find("std::move(activityNames)");
    Check(spawn.rfind("PS::ProcessEventDispatch::Remove(m_activityObserver)", activity) != std::string::npos
        && spawn.rfind("m_playerProcessEventActivityNames.end()", activity) != std::string::npos, "spawn: activity route is not rebuilt from the current names");

    const auto quick = Read(root / "src/Runtime/QuickLoad.cpp");
    Routed(quick, "quick-load", "QuickLoadPossession", "{TEXT(\"HandlePossessedPawnChanged\")}");
    Check(quick.find("PS::ProcessEventDispatch::Remove(possessHook)") != std::string::npos, "quick-load: route is not removed");

    const auto registrar = Read(root / "src/Misc/DragonWildsDataRegistrar.cpp");
    Routed(registrar, "registrar", "CombatComponentWorldReady", "TEXT(\"OnPersonalInventoryLoadedFromSave\")");

    Check(Read(root / "src/dllmain.cpp").find("PS::ProcessEventDispatch::Shutdown()") != std::string::npos, "Shared callbacks are not unregistered at shutdown");
    std::cout << "Name-only ProcessEvent hooks are routed through the shared dispatcher.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
