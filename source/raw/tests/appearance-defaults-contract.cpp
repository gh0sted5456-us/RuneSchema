#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("source unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Need(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    Need(argc == 4, "defaults, pruner, and player rules required");
    const auto defaults = Read(argv[1]);
    const auto pruner = Read(argv[2]);
    const auto players = Read(argv[3]);

    Need(defaults.find("male_A_01") != defaults.npos
            && defaults.find("Preset_None") != defaults.npos
            && defaults.find("M_A_PresetNone") != defaults.npos,
        "the DLL does not contain a canonical male/A appearance profile");
    Need(defaults.find("Default.json") == defaults.npos
            && defaults.find("SettingsDirectory") == defaults.npos,
        "appearance recovery still depends on an external defaults file");
    Need(pruner.find("AppearanceDefaults::BuiltIn()") != pruner.npos,
        "load-time repair does not use baked defaults");
    Need(players.find("AppearanceDefaults::BuiltIn()") != players.npos
            && players.find("IsValidAppearanceReference") != players.npos,
        "runtime appearance recovery does not validate overrides before baked fallback");
    Need(players.find("ReconcileDeclaredPlayerAppearance") == players.npos
            && players.find("ObserveDeclaredAppearance") == players.npos,
        "runtime still performs periodic connected-player appearance scans");
}
