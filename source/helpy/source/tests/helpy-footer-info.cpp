#include <fstream>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include "Core/HelpyBranding.h"
int main(int argc,char** argv){
    if(std::string(PS::HelpyBranding::Credits).find("Abyss (RSDW Modding Community)")==std::string::npos)
        throw std::runtime_error("Corrupt-save bypass origin credit is missing");
    if(argc!=2)throw std::runtime_error("Expected standalone UI source");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("UI source unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    if(source.find("Page CurrentPage=Page::Home;")==source.npos)throw std::runtime_error("The landing page must be the initial workspace");
    const auto begin=source.find("void RenderRuntimeBadge(const HelpyImGuiTheme::Theme& t) {"),end=source.find("void ApplyCaptionTheme(",begin);
    if(begin==source.npos||end==source.npos)throw std::runtime_error("Runtime icon implementation missing");
    const auto badge=source.substr(begin,end-begin);
    for(const auto* required:{"HelpyEnvironment::Label()","F2  close / reopen","EpicTexture","BeginDetailsTooltip()","RuntimeDetailsPanel::Request()","IsItemHovered()","IsItemFocused()"})
        if(badge.find(required)==badge.npos)throw std::runtime_error("Runtime icon details, platform mark or Info action missing");
    if(source.find("RenderRuntimeFooter(")!=source.npos||source.find("RuntimeFooterHeight(")!=source.npos)
        throw std::runtime_error("Runtime information must not leave a duplicate bottom footer");
    const auto bar=source.substr(source.find("void RenderInformationBar("),begin-source.find("void RenderInformationBar("));
    for(const auto* required:{"RenderRuntimeBadge(t)","PageTitle(CurrentPage)","width-badge.x","height-badge.y"})
        if(bar.find(required)==bar.npos)throw std::runtime_error("Runtime icon must be right-aligned in the page heading row");
    const auto panel=std::filesystem::path(argv[1]).parent_path().parent_path().parent_path()/"include"/"Generator"/"RuntimeDetailsPanel.h";
    std::ifstream infoFile(panel);if(!infoFile)throw std::runtime_error("Runtime details panel unavailable");
    const std::string info{std::istreambuf_iterator<char>(infoFile),{}};
    for(const auto* required:{"RuneSchemaVersion","GameVersion","UE4SS","BuildId","Helpy (RSMK)","Attributions","Legal","StartupStats","Detach window","Dock back","RenderDocked","RenderGameLogo()","ImGui::Image(GameLogo"})
        if(info.find(required)==info.npos)throw std::runtime_error("Info does not contain runtime/build metadata");
    for(const auto* removed:{"SoftButton(\"Info\")","SoftButton(\"Credits\")","SoftButton(\"Legal\")"})
        if(source.find(removed)!=source.npos)throw std::runtime_error("Versions, attribution and legal must live in the runtime details window");
    if(source.find("RuntimeDetailsPanel::GameLogo=ImTextureRef(GameBrandTexture)")==source.npos||source.find("RuntimeDetailsPanel::GameLogo={}")==source.npos)throw std::runtime_error("Runtime details must reuse and release the already-loaded Dragonwilds logo");
}
