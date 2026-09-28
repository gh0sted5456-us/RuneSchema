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
    if (!value) { std::cerr << message << '\n'; throw std::runtime_error(message); }
}

int main(int argc, char** argv) {
    if (argc != 6) throw std::runtime_error("Config, journal, recipe, quest and player sources are required");
    const auto config = Read(argv[1]);
    const auto journal = Read(argv[2]);
    const auto recipes = Read(argv[3]);
    const auto quests = Read(argv[4]);
    const auto players = Read(argv[5]);
    Require(config.find("bool characterCustomization = false;") != std::string::npos,
        "Character-customization persistence must default off");
    const auto persistenceStart=config.find("struct PersistenceSettings");
    const auto persistenceEnd=config.find("};",persistenceStart);
    Require(persistenceStart!=std::string::npos && persistenceEnd!=std::string::npos,
        "Persistence settings structure is missing");
    const auto persistenceConfig=config.substr(persistenceStart,persistenceEnd-persistenceStart);
    Require(persistenceConfig.find("bool journal =") == std::string::npos,
        "Journal persistence must not be configurable");
    Require(persistenceConfig.find("bool recipes =") == std::string::npos,
        "Recipe persistence must not be configurable");
    Require(config.find("bool quests = true;") != std::string::npos,
        "Quest persistence must default on");
    Require(journal.find("if (!m_ownedIds.empty())") != std::string::npos,
        "Journal native adapter is not installed for transient unlock filtering");
    Require(journal.find("if (!m_nativePersistenceReady)") != std::string::npos,
        "Journal temporary unlock delivery is not guarded by the save adapter");
    Require(journal.find("RegisterHooks();") != std::string::npos,
        "Journal loader lifecycle was disabled with persistence");
    Require(journal.find("UnlockOnAcquire requires a string ItemData reference") != std::string::npos,
        "Journal item-acquisition unlock validation is missing");
    Require(journal.find("/Script/Dominion.InventoryComponent:GetNumItemsByData") != std::string::npos,
        "Journal acquisition does not use the native inventory count contract");
    Require(journal.find("|| !m_nativePersistenceReady)return;") != std::string::npos,
        "Journal acquisition is not guarded when transient filtering is unavailable");
    Require(recipes.find("RecipesUnlockedThatShouldNotPersist") != std::string::npos,
        "Transient recipe unlock set is not used");
    Require(recipes.find("std::vector<const TCHAR*> targetSets{TEXT(\"RecipesUnlocked\")}") != std::string::npos,
        "Runtime recipes must always enter the visible unlock set");
    Require(recipes.find("targetSets.push_back(TEXT(\"RecipesUnlockedThatShouldNotPersist\"));") != std::string::npos,
        "Nonpersistent recipes must be marked for save exclusion");
    Require(recipes.find("GetSettings().persistence.recipes") == std::string::npos,
        "Recipe persistence must not be configurable");
    Require(recipes.find("if (m_hooksActive || m_recipes.empty())") != std::string::npos,
        "Recipe-unlocker consumables are not observed when automatic unlock is disabled");
    Require(recipes.find("if(recipe && unlocked.Contains(&recipe))transient.Add(&recipe);") != std::string::npos,
        "Consumable-granted RuneSchema recipes are not marked transient");
    Require(quests.find("GetSettings().persistence.quests") != std::string::npos,
        "Quest actions are not gated by the quest persistence setting");
    Require(players.find("GetSettings().persistence.characterCustomization") != std::string::npos,
        "Automatic character-customization writes are not independently gated");
    std::cout << "Journal and recipe unlocks remain transient without disabling their loaders.\n";
}
