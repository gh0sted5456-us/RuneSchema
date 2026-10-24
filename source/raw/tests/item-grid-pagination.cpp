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
    Require(picker.find("ImGuiListClipper")!=std::string::npos,"visible-window clipping missing");
    Require(picker.find("const int columns=ItemBayLayout::Columns(avail,minCardWidth,spacing,theme.itemGridColumns)")!=std::string::npos,"responsive preferred grid columns missing");
    Require(picker.find("LazyCatalog::FillTo(0,(state.PageIndex+1)*state.PageSize)")!=std::string::npos
        &&picker.find("ItemPaging::Slice")!=std::string::npos&&picker.find("PageControls(state,")!=std::string::npos,"bounded dynamic paging missing");
    Require(picker.find("LazyCatalog::Search(0,BackendQuery(state))")!=std::string::npos,"search does not request a new window");
    Require(picker.find("InternalName")!=std::string::npos && picker.find("PersistenceID")!=std::string::npos,
        "search metadata coverage regressed");
    Require(picker.find("std::snprintf(selected")!=std::string::npos,"grid selection no longer updates selected item");
    Require(panel.find("ImGui::BeginCombo(\"Loaded item\"")==std::string::npos,"legacy item combo still present");
    Require(picker.find("DrawPlaceholder")!=std::string::npos,"icon fallback placeholder missing");
    std::cout<<"PASS: Item browser uses bounded search, clipping and a responsive grid.\n";
}
