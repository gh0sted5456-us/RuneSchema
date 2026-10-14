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
    Require(picker.find("ImGuiListClipper")!=std::string::npos && picker.find("const int columns=std::max(1")!=std::string::npos,
        "bounded responsive grid contract regressed");
    Require(picker.find("ItemIconCache::Draw(iconPath")!=std::string::npos,
        "shared picker no longer renders real icons");
    Require(picker.find("PersistenceID")!=std::string::npos && picker.find("InternalName")!=std::string::npos,
        "hover metadata lost internal name or PersistenceID");
    Require(picker.find("state.CatalogRevision=0")!=std::string::npos,
        "search changes no longer invalidate the visible window");
    std::cout<<"PASS: Shared item picker retains bounded windows, real icons and metadata.\n";
}
