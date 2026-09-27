#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) std::exit(2);
    std::ostringstream value;
    value << file.rdbuf();
    return value.str();
}

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    Check(argc >= 8, "host, loaders, player rules, registrar, save viewer, and cleanup panel supplied");
    const auto host = Read(argv[1]);
    const auto mainLoader = Read(argv[2]);
    const auto buildingLoader = Read(argv[3]);
    const auto playerRules = Read(argv[4]);
    const auto registrar = Read(argv[5]);
    const auto saveViewer = Read(argv[6]);
    const auto cleanupPanel = Read(argv[7]);
    Check(host.find("RSDragonwilds") != std::string::npos
        && host.find("Saved") != std::string::npos
        && host.find("RuneSchema") != std::string::npos,
        "mutable state is rooted in the game's AppData Saved tree");
    Check(host.find("GetCurrentPackageFamilyName") != std::string::npos
        && host.find("LocalState") != std::string::npos
        && host.find("SystemAppData\\wgs") != std::string::npos,
        "Game Pass state is package-local and Xbox WGS is never treated as a normal save directory");
    Check(host.find("OwnedContentLedger.json") == std::string::npos,
        "runtime state migration does not create or move an ownership ledger");
    Check(mainLoader.find("OwnedContent::BeginSnapshot") == std::string::npos,
        "automatic cleanup does not create an ownership ledger");
    Check(buildingLoader.find("CustomBuildingData.json") == std::string::npos
        && buildingLoader.find("HistoricalIndex") == std::string::npos
        && buildingLoader.find("PersistenceId < right.PersistenceId") != std::string::npos,
        "building registration is deterministic and does not create a world manifest");
    Check(playerRules.find("StateDirectory() / \"players\"") != std::string::npos
        && playerRules.find("RuneSchemaPlayerAppearanceSnapshot") != std::string::npos
        && playerRules.find("WriteOnceFallback") != std::string::npos,
        "appearance-only write-once player snapshots use LocalAppData");
    Check(playerRules.find("SnapshotAppearanceFields") != std::string::npos
        && playerRules.find("\"FacialHairPreset\"") != std::string::npos
        && playerRules.find("\"EyebrowColor\"") != std::string::npos,
        "player snapshots cover the canonical appearance handles");
    Check(playerRules.find("if (error == \"player pawn or GUID was unavailable\") continue;") != std::string::npos
        && playerRules.find("PS::Log<LogLevel::Verbose>(") != std::string::npos,
        "normal pre-pawn appearance snapshot retries do not warn or fail");
    Check(registrar.find("ScrubLocalCharacterFiles") != std::string::npos
        && registrar.find("SaveCharacters") != std::string::npos
        && registrar.find("ConfigFiles::Write") != std::string::npos,
        "Steam character saves are pruned before character selection");
    Check(registrar.find("NativeLane::SteamNative") != std::string::npos
        && registrar.find("ScrubCharacterJsonBeforeLoad") != std::string::npos,
        "file and provider save lanes stay separated");
    Check(registrar.find("PublishRegistry") != std::string::npos
        && registrar.find("snapshot.Journals") != std::string::npos,
        "native item, recipe, quest, and journal registries feed pruning");
    Check(cleanupPanel.find("Remove invalid item/recipe/quest PersistenceIDs") != std::string::npos
        && cleanupPanel.find("ReadRegistry()") != std::string::npos,
        "Safe Clean exposes explicit live-registry orphan repair");
    const auto preRegistration=registrar.find("RegisterInitGameStatePreCallback");
    const auto registerAll=registrar.find("RegisterAll();",preRegistration);
    const auto scrubFiles=registrar.find("ScrubLocalCharacterFiles();",registerAll);
    Check(preRegistration != std::string::npos && registerAll < scrubFiles
        && registrar.find("OwnedContent::CompareSnapshot") == std::string::npos
        && registrar.find("OwnedContent::CommitSnapshot") == std::string::npos,
        "automatic pruning follows pre-world registration without a manifest or ledger");
    Check(saveViewer.find("Character-save file browsing is unavailable for Xbox WGS storage") != std::string::npos,
        "the file viewer does not mistake Steam saves for Game Pass saves");
    std::cout << "Mutable state storage contract passed.\n";
}
