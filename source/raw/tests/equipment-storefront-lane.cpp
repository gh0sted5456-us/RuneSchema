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
    Check(argc == 3, "equipment and Shadowveil implementations supplied");
    const auto equipment = Read(argv[1]);
    const auto shadowveil = Read(argv[2]);
    Check(equipment.find("NativeLane::SteamNative") != std::string::npos
        && equipment.find("Surge/Dash native contract is currently verified only") != std::string::npos,
        "Dash/Surge hooks are explicitly isolated to the Steam/GOG lane");
    Check(equipment.find("GrantedEffects") != std::string::npos
        && equipment.find("LoadAsset_Blocking") != std::string::npos,
        "Windstep and other cooked GrantedEffects remain reflection-driven and storefront-neutral");
    Check(shadowveil.find("native hook profile does not match the selected storefront lane") != std::string::npos
        && shadowveil.find("GamePassTimestamp") != std::string::npos
        && shadowveil.find("CurrentNativeLane") != std::string::npos,
        "Shadowveil chooses only the profile belonging to the active storefront lane");
    std::cout << "Equipment storefront lane contract passed.\n";
}
