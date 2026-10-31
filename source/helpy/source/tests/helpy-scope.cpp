#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static std::string Read(const std::filesystem::path& path){
    std::ifstream input(path);assert(input);return {std::istreambuf_iterator<char>(input),{}};
}
int main(int argc,char** argv){
    assert(argc==2);const std::filesystem::path root(argv[1]);
    const auto desktop=Read(root/"source/helpy/source/src/Generator/HelpyStandaloneWindow.cpp");
    const auto plugin=Read(root/"source/helpy/source/plugins/helpy/HelpyPlugin.cpp");
    const auto build=Read(root/"source/helpy/CMakeLists.txt");
    assert(!desktop.contains("SchemaVision")&&!plugin.contains("SchemaVision"));
    assert(!plugin.contains("Runtime/HelpyNativeModule.h")&&!plugin.contains("helpy.vision.status"));
    assert(!build.contains("HelpyNativeModule")&&!build.contains("source/native/")&&!build.contains("schema-vision"));
    assert(!Read(root/"build/build.ps1").contains("RuneSchema.Helpy.Native.dll"));
    for(const auto* retired:{"source/raw/plugins/helpy/HelpyPlugin.cpp","source/helpy/source/native/SchemaVisionRuntime.cpp",
        "source/helpy/source/include/Generator/SchemaVisionPanel.h",
        "source/helpy/source/include/Runtime/HelpyNativeModule.h",
        "source/helpy/assets/SchemaVision","source/plugins/RuneSchema.Helpy/assets/SchemaVision"})
        assert(!std::filesystem::exists(root/retired));
    for(const auto* retained:{"ItemPreviewPanel::Render", "MapPanel::Render", "PlayerInventoryPanel::Render"})assert(desktop.contains(retained));
    const auto console=Read(root/"source/raw/include/Runtime/ConsoleMenu.h");
    for(const auto* retained:{"OpenWidgetIfClosed","CloseWidgetIfOpen","ToggleWidget","CloseConsole"})assert(console.contains(retained));
    assert(!desktop.contains("DrawHomeBackdrop("));
    const auto cache=desktop.substr(desktop.find("void RenderIconCacheSettings("),desktop.find("void RenderUpdatesAndRuntime(")-desktop.find("void RenderIconCacheSettings("));
    assert(!cache.contains("RefreshSnapshot(")&&!cache.contains("CurrentDiskStats()")&&cache.contains("Refresh usage")&&cache.contains("std::launch::async"));
    const auto settingsStart=desktop.find("void RenderSettingsPage(");assert(settingsStart!=std::string::npos);
    const auto settings=desktop.substr(settingsStart,desktop.find("void RenderWorkspace(",settingsStart)-settingsStart);
    assert(settings.contains("##settings-categories")&&settings.contains("##settings-details"));
    assert(!settings.contains("BeginTabItem(\"Item Cache\""));
    const auto tools=desktop.substr(desktop.find("case Page::Tools:{"));
    assert(tools.contains("case Tool::ItemCache:RenderIconCacheSettings(true,true)")&&tools.contains("case Tool::CacheBuilder:"));
    assert(!desktop.contains("page==Page::Tools&&!t.workspaces.Tools")&&!desktop.contains("CurrentPage==Page::Tools&&!t.workspaces.Tools"));
    const auto inventory=Read(root/"source/helpy/source/include/Generator/PlayerInventoryPanel.h");
    const auto repair=inventory.substr(inventory.find("inline void RestoreGear("),inventory.find("inline void PollRepair(")-inventory.find("inline void RestoreGear("));
    assert(!repair.contains("Request(player)")&&!repair.contains("Open=")&&!repair.contains("Target="));
    const auto status=Read(root/"source/raw/include/SDK/Helper/PlayerStatus.h");
    const auto platform=status.substr(status.find("inline std::string Platform("),status.find("// Game-thread")-status.find("inline std::string Platform("));
    for(const auto* required:{"DomPlatformData","PlatformName","FStrProperty","ReflectedContract::Fits","chars.Num()>65"})assert(platform.contains(required));
    for(const auto* forbidden:{"UniqueID","PlatformType","FunctionCall","Server_CacheClientPlatformData","Storefront::"})assert(!platform.contains(forbidden));
    const auto localPlatform=status.substr(status.find("inline std::string Platform(UObject* state,bool local)"),status.find("inline nlohmann::json Read(")-status.find("inline std::string Platform(UObject* state,bool local)"));
    assert(localPlatform.contains("Storefront::PlayerPlatform(Platform(state),local,")&&localPlatform.contains("Controller:IsLocalController"));
    const auto color=status.substr(status.find("inline void Color("),status.find("inline std::string Platform(")-status.find("inline void Color("));
    for(const auto* required:{"/Script/Dominion.PlayerColor","/Script/CoreUObject.LinearColor","ReflectedContract::Fits","NativePlayerColorVerified"})assert(color.contains(required));
    assert(!color.contains("FunctionCall")&&!color.contains("PlayerId"));
    const auto portraits=Read(root/"source/helpy/source/include/Generator/PlayerPortraitPicker.h");assert(portraits.contains("AddImageRounded")&&portraits.contains("tint,true"));
}
