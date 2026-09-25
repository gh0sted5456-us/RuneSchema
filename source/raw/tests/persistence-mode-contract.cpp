#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Persistence-mode contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Config, journal, recipe and player sources are required");
    const auto config = Read(argv[1]);
    const auto journal = Read(argv[2]);
    const auto recipes = Read(argv[3]);
    const auto players = Read(argv[4]);
    Require(config.find("bool characterCustomization = false;") != std::string::npos,
        "Character-customization persistence must default off");
    Require(config.find("bool journal = false;") != std::string::npos,
        "Journal persistence must default off");
    Require(config.find("bool recipes = false;") != std::string::npos,
        "Recipe persistence must default off");
    Require(journal.find("persistence.journal && !m_ownedIds.empty()") != std::string::npos,
        "Journal native persistence is not independently gated");
    Require(journal.find("if (!PS::PSConfig::Get()->GetSettings().persistence.journal)") != std::string::npos,
        "Journal unlock delivery is not independently gated");
    Require(journal.find("RegisterHooks();") != std::string::npos,
        "Journal loader lifecycle was disabled with persistence");
    Require(recipes.find("RecipesUnlockedThatShouldNotPersist") != std::string::npos,
        "Transient recipe unlock set is not used");
    Require(recipes.find("GetSettings().persistence.recipes") != std::string::npos,
        "Permanent recipe unlock set is not independently gated");
    Require(players.find("GetSettings().persistence.characterCustomization") != std::string::npos,
        "Automatic character-customization writes are not independently gated");
    std::cout << "Journal and recipe persistence modes remain independent from loader activation.\n";
}
