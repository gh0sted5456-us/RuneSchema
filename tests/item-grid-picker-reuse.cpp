#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}
static void Require(bool value,const char* what){if(!value)throw std::runtime_error(std::string("Item picker regression: ")+what);}

int main(int argc,char** argv) {
    if(argc!=3)throw std::runtime_error("Expected SpawnToolsPanel and ItemGridPicker sources");
    const auto panel=Read(argv[1]);const auto picker=Read(argv[2]);
    Require(panel.find("ItemGridPicker::Render(\"inventory\"")!=std::string::npos,
        "Items tab no longer uses the shared picker");
    Require(panel.find("ItemGridPicker::Render(\"additional-drop\"")!=std::string::npos,
        "Additional Loot no longer uses the shared picker");
    Require(panel.find("ImGui::BeginCombo(\"Loaded item\"")==std::string::npos,
        "legacy Additional Loot item combo returned");
    Require(panel.find("Advanced item path override")!=std::string::npos,
        "manual asset-path escape hatch disappeared");
    Require(panel.find("AdditionalDropList")!=std::string::npos && panel.find("ChancePercent")!=std::string::npos,
        "Additional Loot summary table is missing");
    Require(picker.find("int PageSize=25")!=std::string::npos && picker.find("constexpr int columns=5")!=std::string::npos,
        "fixed 5x5 page/grid contract regressed");
    Require(picker.find("ItemIconCache::Draw(iconPath")!=std::string::npos,
        "shared picker no longer renders real icons");
    Require(picker.find("runeSchema?\"RS\":\"LOADED\"")!=std::string::npos,
        "source badge is missing");
    Require(picker.find("PersistenceID:")!=std::string::npos && picker.find("Internal:")!=std::string::npos,
        "hover metadata lost internal name or PersistenceID");
    Require(picker.find("state.Page=0")!=std::string::npos,
        "search/page-size changes no longer reset pagination");
    std::cout<<"PASS: Item grid is shared by inventory and Additional Loot with fixed 5x5 paging, real icons, source badges and metadata.\n";
}
