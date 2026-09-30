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
    Check(playerRules.find("RuneSchemaPlayerAppearanceSnapshot") == std::string::npos
        && playerRules.find("WriteOnceFallback") == std::string::npos
        && playerRules.find("SaveCharacters\" / \"Default.json") != std::string::npos,
        "appearance recovery does not retain player snapshots and uses Default.json");
    Check(playerRules.find("CanonicalAppearanceFields") != std::string::npos
        && playerRules.find("\"FacialHairPreset\"") != std::string::npos
        && playerRules.find("\"EyebrowColor\"") != std::string::npos,
        "Default.json recovery covers the canonical appearance handles");
    Check(playerRules.find("IsValidAppearanceReference") != std::string::npos
        && playerRules.find("ReadDefaultPlayerAppearance") != std::string::npos,
        "appearance is replaced only after validation fails");
    Check(registrar.find("ScrubLocalCharacterFiles") == std::string::npos
        && registrar.find("ConfigFiles::Write") == std::string::npos,
        "automatic cleanup writes directly to stored character files");
    Check(registrar.find("Default.json") != std::string::npos
        && registrar.find("ConfigFiles::Read") != std::string::npos,
        "appearance recovery does not read the canonical default profile");
    Check(registrar.find("ScrubCharacterJsonBeforeLoad") != std::string::npos,
        "shared native character-load preflight is missing");
    Check(registrar.find("m_startupCleanupPending") != std::string::npos
        && registrar.find("EnsureCharacterJsonPreflightHook") != std::string::npos
        && registrar.find("[SAVE-CLEANER][BOUNDARY-READY]") != std::string::npos,
        "automatic recovery cannot reach the first eligible native character load");
    Check(registrar.find("PublishRegistry") != std::string::npos
        && registrar.find("snapshot.Journals") != std::string::npos,
        "native item, recipe, quest, and journal registries feed pruning");
    Check(cleanupPanel.find("Remove invalid item/recipe/quest PersistenceIDs") != std::string::npos
        && cleanupPanel.find("ReadRegistry()") != std::string::npos,
        "Safe Clean exposes explicit live-registry orphan repair");
    const auto preRegistration=registrar.find("RegisterInitGameStatePreCallback");
    const auto registerAll=registrar.find("RegisterAll();",preRegistration);
    Check(preRegistration != std::string::npos && registerAll != std::string::npos
        && registrar.find("OwnedContent::CompareSnapshot") == std::string::npos
        && registrar.find("OwnedContent::CommitSnapshot") == std::string::npos,
        "automatic pruning does not follow pre-world registration without a manifest or ledger");
    Check(registrar.find("fingerprint != m_registryCandidateFingerprint") != std::string::npos
        && registrar.find("m_checkedCharacters") == std::string::npos
        && registrar.find("m_startupCleanupPending = false;") != std::string::npos
        && registrar.find("if (cleaned.Removed.empty())") != std::string::npos,
        "automatic pruning is not globally gated by a stable registry and a nonempty removal plan");
    Check(registrar.find("for (auto* subsystem : subsystems)") != std::string::npos
        && registrar.find("registrationsComplete = RegisterMissing(dataClass, subsystem)") != std::string::npos,
        "a world transition can leave a live native registry unpopulated");
    Check(saveViewer.find("Character-save file browsing is unavailable for Xbox WGS storage") != std::string::npos,
        "the file viewer does not mistake Steam saves for Game Pass saves");
    std::cout << "Mutable state storage contract passed.\n";
}
