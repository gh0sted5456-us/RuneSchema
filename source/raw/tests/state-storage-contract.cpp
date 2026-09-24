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
    Check(argc >= 4, "host, main loader, and building loader supplied");
    const auto host = Read(argv[1]);
    const auto mainLoader = Read(argv[2]);
    const auto buildingLoader = Read(argv[3]);
    Check(host.find("RSDragonwilds") != std::string::npos
        && host.find("Saved") != std::string::npos
        && host.find("RuneSchema") != std::string::npos,
        "mutable state is rooted in the game's AppData Saved tree");
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
    std::cout << "Mutable state storage contract passed.\n";
}
