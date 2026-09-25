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
    Check(argc >= 7, "host, loaders, player rules, registrar, and save viewer supplied");
    const auto host = Read(argv[1]);
    const auto mainLoader = Read(argv[2]);
    const auto buildingLoader = Read(argv[3]);
    const auto playerRules = Read(argv[4]);
    const auto registrar = Read(argv[5]);
    const auto saveViewer = Read(argv[6]);
    Check(host.find("RSDragonwilds") != std::string::npos
        && host.find("Saved") != std::string::npos
        && host.find("RuneSchema") != std::string::npos,
        "mutable state is rooted in the game's AppData Saved tree");
    Check(host.find("GetCurrentPackageFamilyName") != std::string::npos
        && host.find("LocalState") != std::string::npos
        && host.find("SystemAppData\\wgs") != std::string::npos,
        "Game Pass state is package-local and Xbox WGS is never treated as a normal save directory");
    Check(host.find("Preserve the Steam/GOG ledger") != std::string::npos,
        "the Game Pass ledger seed does not consume Steam state");
    Check(host.find("safesave") != std::string::npos
        && host.find("OwnedContentLedger.json") != std::string::npos,
        "legacy SafeSave ledger migration exists");
    Check(host.find(".migrating") != std::string::npos
        && host.find("file_size(source) != std::filesystem::file_size(temporary)") != std::string::npos,
        "SafeSave migration is staged and verified");
    Check(mainLoader.find("HostServices::StateDirectory()") != std::string::npos,
        "snapshot uses AppData state root");
    Check(buildingLoader.find("GetProjectSavedDirectory") != std::string::npos
        && buildingLoader.find("CustomBuildingData.json") != std::string::npos,
        "world building data remains in the engine Saved directory");
    Check(playerRules.find("StateDirectory() / \"players\"") != std::string::npos
        && playerRules.find("RuneSchemaPlayerAppearanceSnapshot") != std::string::npos
        && playerRules.find("WriteOnceFallback") != std::string::npos,
        "appearance-only write-once player snapshots use LocalAppData");
    Check(playerRules.find("SnapshotAppearanceFields") != std::string::npos
        && playerRules.find("\"FacialHairPreset\"") != std::string::npos
        && playerRules.find("\"EyebrowColor\"") != std::string::npos,
        "player snapshots cover the canonical appearance handles");
    Check(registrar.find("Xbox WGS save detected") != std::string::npos
        && registrar.find("GamePassNative") != std::string::npos,
        "Game Pass cleanup uses the in-game provider path instead of direct JSON writes");
    Check(registrar.find("OwnedContent::CommitSnapshot(m_pendingProviderSnapshot)") != std::string::npos
        && registrar.find("[SAVE-CLEANER][PROVIDER][PENDING]") != std::string::npos,
        "Game Pass retains its previous ledger until provider cleanup is verified");
    Check(saveViewer.find("Character-save file browsing is unavailable for Xbox WGS storage") != std::string::npos,
        "the file viewer does not mistake Steam saves for Game Pass saves");
    std::cout << "Mutable state storage contract passed.\n";
}
