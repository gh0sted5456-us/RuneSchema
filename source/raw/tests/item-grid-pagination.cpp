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
static void Require(bool value,const char* what){if(!value)throw std::runtime_error(std::string("Item grid regression: ")+what);}

int main(int argc,char** argv) {
    if(argc!=3)throw std::runtime_error("Expected SpawnToolsPanel.h and ItemGridPicker.h");
    const auto panel=Read(argv[1]);const auto picker=Read(argv[2]);
    Require(panel.find("ItemGridPicker::Render(\"inventory\"")!=std::string::npos,"Items tab no longer uses shared grid");
    Require(picker.find("int PageSize=25")!=std::string::npos,"25-item default pagination missing");
    Require(picker.find("ImGui::BeginTable(\"ItemGrid\"")!=std::string::npos,"item grid table missing");
    Require(picker.find("constexpr int columns=5")!=std::string::npos,"fixed five-column grid missing");
    Require(picker.find("< Previous##item-grid")!=std::string::npos && picker.find("Next >##item-grid")!=std::string::npos,
        "page navigation missing");
    Require(picker.find("state.LastFilter!=filter")!=std::string::npos,"search does not reset pagination");
    Require(picker.find("InternalName")!=std::string::npos && picker.find("PersistenceID")!=std::string::npos,
        "search metadata coverage regressed");
    Require(picker.find("std::snprintf(selected")!=std::string::npos,"grid selection no longer updates selected item");
    Require(panel.find("ImGui::BeginCombo(\"Loaded item\"")==std::string::npos,"legacy item combo still present");
    Require(picker.find("\"ITEM\"")!=std::string::npos,"icon fallback placeholder missing");
    std::cout<<"PASS: Settings item browser uses reusable full-catalog search, 25-item pagination and fixed 5x5 grid.\n";
}
