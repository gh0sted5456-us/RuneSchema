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
    Need(argc == 7, "defaults, pruner, player rules, build, instructions, and example required");
    const auto defaults = Read(argv[1]);
    const auto pruner = Read(argv[2]);
    const auto players = Read(argv[3]);
    const auto build = Read(argv[4]);
    const auto instructions = Read(argv[5]);
    const auto example = Read(argv[6]);

    Need(defaults.find("male_A_01") != defaults.npos
            && defaults.find("Preset_None") != defaults.npos
            && defaults.find("M_A_PresetNone") != defaults.npos,
        "the DLL does not contain a canonical male/A appearance profile");
    Need(defaults.find("SettingsDirectory() / \"defaults\" / \"Default.json\"")
            != defaults.npos,
        "the override is not isolated under settings/defaults");
    Need(defaults.find("SaveCharacters") == defaults.npos,
        "appearance recovery still depends on a character-save Default.json");
    Need(defaults.find("RequiredFields") != defaults.npos
            && defaults.find("fields.size() != RequiredFields.size()") != defaults.npos,
        "external defaults do not require the complete canonical field set");
    Need(pruner.find("AppearanceDefaults::BuiltIn()") != pruner.npos
            && pruner.find("DEFAULT-OVERRIDE-IGNORED") != pruner.npos,
        "load-time repair cannot reject an unavailable override and use baked defaults");
    Need(players.find("AppearanceDefaults::BuiltIn()") != players.npos
            && players.find("IsValidAppearanceReference") != players.npos,
        "runtime appearance recovery does not validate overrides before baked fallback");
    Need(build.find("settings\\defaults") != build.npos
            && instructions.find("No external file") != instructions.npos
            && example.find("\"EyebrowColor\"") != example.npos,
        "override instructions and example are not packaged");
}
