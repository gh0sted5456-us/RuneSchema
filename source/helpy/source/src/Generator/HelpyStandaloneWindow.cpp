#include "Core/ProcessExitFuture.h"
#include "Generator/HelpyStandaloneWindow.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyBackgrounds.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/MapPanel.h"
#include "Generator/AuthoringPanel.h"
#include "Generator/SaveEditorPanel.h"
#include "Generator/ProfilePanel.h"
#include "Generator/BugReportPanel.h"
#include "Generator/ItemPreviewPanel.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/RuntimeDetailsPanel.h"
#if defined(HELPY_PREVIEW)
#include "../../tests/AuthoringPreviewFixture.h"
#endif
#include "Generator/LazyCatalog.h"
#include "Generator/HelpyCachedCatalog.h"
#include "Generator/ItemGridPicker.h"
#include "Generator/ToolRequest.h"
#include "Core/HelpyPopupInput.h"
#include "Core/HelpyFramePacing.h"
#include "Core/HelpyNativeFrame.h"
#include "Generator/HelpySurfaceDraw.h"
#include "Core/HelpyNavigation.h"
#include "Generator/NavigationOrder.h"
#include "Core/HelpyEnvironment.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Core/HelpyBranding.h"
#include "Core/HelpySplash.h"
#include "Runtime/HelpyDownloads.h"
#include "Runtime/HelpyMapCache.h"
#include <limits>
#include "Core/RuneSchemaUpdateInfo.h"
#include "Runtime/HelpyUpdates.h"
#include "Core/RsmkResources.h"
#include "Core/DesktopBayLayout.h"
#include "Core/HelpyTaskbar.h"
#include "Generator/JournalPanel.h"
#include "Generator/HelpyIconPacks.h"
#include "Generator/LiveLogPanel.h"
#include "Generator/DiagnosticsPanel.h"
#include "Generator/StructuredLogFeed.h"
#include "Generator/HelpyDiagnosticOverlay.h"
#include "Generator/ModsPanel.h"
#include "Generator/DedicatedServerPanel.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/PluginRuntimeServices.h"
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <imgui.h>
#include <chrono>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <random>
#include <filesystem>
#include <wincodec.h>
#include <wrl/client.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);

namespace PS {
namespace {
constexpr UINT ToggleMessage=WM_APP+0x521,ReloadMessage=WM_APP+0x522,StopMessage=WM_APP+0x523;
#if defined(HELPY_PREVIEW)
bool PreviewUpdatesTab=false;
bool PreviewBackgroundsTab=false;
#endif
HelpyPopupInput::HotkeyLatch Hotkey;
bool GameWindow(HWND hwnd) {
    if(HelpyEnvironment::Server())return hwnd&&hwnd==GetConsoleWindow();
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    wchar_t name[128]{};GetClassNameW(hwnd,name,128);
    return hwnd&&pid==GetCurrentProcessId()&&std::wstring_view(name)==L"UnrealWindow";
}
using Page=HelpyNavigation::Page;
Page CurrentPage=Page::Home;
Page LastPlayersPage=Page::Items,LastWorldPage=Page::Map,LastToolsPage=Page::Tools;
std::array<bool,2> DetachedWorkspaces{}; // Map, Recipes; presentation state only.
bool& WorkspaceDetached(size_t index){return index==4?PlayerInventoryPanel::Detached:index==3?MapPanel::Data.PartyDetached:index==2?RuntimeDetailsPanel::Detached:DetachedWorkspaces.at(index);}
HelpyTaskbar::Window WorkspaceTask(size_t index){return std::array{HelpyTaskbar::Window::Map,HelpyTaskbar::Window::Recipes,HelpyTaskbar::Window::Runtime,HelpyTaskbar::Window::Party,HelpyTaskbar::Window::Inventory}.at(index);}
const char* WorkspaceTitle(size_t index){return std::array{"Map","Recipes","Runtime details","Party","Inventory inspector"}.at(index);}
const char* WorkspaceImGuiId(size_t index){return std::array{"##DetachedMap","##DetachedRecipes","##DetachedRuntime","##DetachedParty","##DetachedInventory"}.at(index);}
const wchar_t* WorkspaceCaption(size_t index){return std::array{L"RSMK - Map",L"RSMK - Recipes",L"RSMK - Runtime details",L"RSMK - Party",L"RSMK - Inventory inspector"}.at(index);}
DesktopBayLayout::Rect WorkspaceRect(size_t index,const DesktopBayLayout::Canvas& canvas){return index==0?canvas.Map:index==1?canvas.Recipes:index==2?canvas.Runtime:index==3?canvas.Party:canvas.Inventory;}
bool SharedCartVisible=false;
ImVec2 MainSurfacePosition{},MainSurfaceSize{};
HelpyNativeFrame::Metrics NativeCaption;
HelpyNavigation::SpawnSection CurrentSpawn=HelpyNavigation::SpawnSection::Resources;
HelpyNavigation::ToolSection CurrentTool=HelpyNavigation::ToolSection::ItemCache;
std::array<std::string,HelpyNavigation::Pages.size()> PageErrors;
ID3D11ShaderResourceView* BrandTexture=nullptr;
ID3D11ShaderResourceView* BrandAccentTexture=nullptr;
ID3D11ShaderResourceView* GameBrandTexture=nullptr;
ID3D11ShaderResourceView* LoadingRingTexture=nullptr;
ID3D11ShaderResourceView* HomeBannerTexture=nullptr;
struct HomeDetailImage {ID3D11ShaderResourceView* Texture=nullptr;UINT Width=0,Height=0;};
std::array<HomeDetailImage,3> HomeDetailImages{};
int HomeLinkDetails=-1;
std::array<ID3D11ShaderResourceView*,4> HomeLinkTextures{};
ID3D11ShaderResourceView* HomeNexusAccentTexture=nullptr;
struct HomeLink {const char* Name;const char* Description;const char* Url;const wchar_t* NativeUrl;const char* Asset;};
constexpr std::array HomeLinks{
    HomeLink{"GitHub","RuneSchema source, releases, and issue tracking.","https://github.com/gh0sted5456-us/RuneSchema",L"https://github.com/gh0sted5456-us/RuneSchema","github.png"},
    HomeLink{"Nexus Mods","RuneSchema downloads and mod-page information.","https://www.nexusmods.com/runescapedragonwilds/mods/570",L"https://www.nexusmods.com/runescapedragonwilds/mods/570","nexus.png"},
    HomeLink{"CurseForge","Visit RuneSchema on CurseForge.","https://www.curseforge.com/runescape-dragonwilds/ue4ss-mods/runeschema",L"https://www.curseforge.com/runescape-dragonwilds/ue4ss-mods/runeschema","curseforge.png"},
    HomeLink{"RSDW Modding Community","Join the RuneScape: Dragonwilds modding community on Discord.","https://discord.gg/AhCuNJ8sg7",L"https://discord.gg/AhCuNJ8sg7","discord.png"}
};
#if defined(HELPY_PREVIEW)
std::string PreviewHomeOpenedLink;
#endif
void OpenHomeLink(size_t index){
    HomeLinkDetails=-1;
#if defined(HELPY_PREVIEW)
    PreviewHomeOpenedLink=HomeLinks.at(index).Url;
#else
    ShellExecuteW(nullptr,L"open",HomeLinks.at(index).NativeUrl,nullptr,nullptr,SW_SHOWNORMAL);
#endif
}
double SplashStarted=std::numeric_limits<double>::quiet_NaN();
bool SplashEntered=false;
HelpySplash::Exit SplashExit;
HelpySplash::Exit HomeExit;
Page HomeExitTarget=Page::Home;
double HomeRevealStarted=std::numeric_limits<double>::quiet_NaN();
struct SplashSprite {ID3D11ShaderResourceView* Texture=nullptr;ImVec4 Bounds{};};
std::array<SplashSprite,3> SplashSprites;
ID3D11ShaderResourceView* SteamTexture=nullptr;
ID3D11ShaderResourceView* EpicTexture=nullptr;
ID3D11ShaderResourceView* XboxTexture=nullptr;
#if defined(HELPY_PREVIEW)
bool PreviewSelectSections=false;
HelpyNavigation::SpawnSection PreviewWantedSpawn=HelpyNavigation::SpawnSection::Resources;
HelpyNavigation::ToolSection PreviewWantedTool=HelpyNavigation::ToolSection::Maintenance;
std::filesystem::path PreviewCapture;
#endif
ImGuiTabItemFlags SectionFlags(bool selected) {
#if defined(HELPY_PREVIEW)
    if(PreviewSelectSections&&selected)return ImGuiTabItemFlags_SetSelected;
#else
    (void)selected;
#endif
    return ImGuiTabItemFlags_None;
}
float SidebarWidthLogical=-1.0f;
ImFont* UiRegularFont=nullptr;
ImFont* UiBoldFont=nullptr;
struct UiFontFamily {std::string Name;ImFont* Regular{};ImFont* Bold{};std::string Source,BoldSource;bool Available{};};
std::vector<UiFontFamily> UiFontFamilies;
UiFontFamily* FindUiFontFamily(const std::string& name){for(auto& f:UiFontFamilies)if(f.Name==name)return &f;return nullptr;}
UiFontFamily* ActiveUiFontFamily(const std::string& requested){
    if(auto* f=FindUiFontFamily(requested);f&&f->Available)return f;
    if(auto* f=FindUiFontFamily("Segoe UI");f&&f->Available)return f;
    for(auto& f:UiFontFamilies)if(f.Available)return &f;return nullptr;
}
void SelectUiFontFamily(const std::string& requested){
    auto& io=ImGui::GetIO();
    for(size_t attempt=0;attempt<UiFontFamilies.size();++attempt){
        auto* f=ActiveUiFontFamily(requested);if(!f)break;
        if(!f->Regular){
            f->Regular=io.Fonts->AddFontFromFileTTF(f->Source.c_str(),13.0f);
            if(!f->Regular){f->Available=false;continue;}
            f->Bold=f->BoldSource.empty()||f->BoldSource==f->Source?f->Regular:io.Fonts->AddFontFromFileTTF(f->BoldSource.c_str(),13.0f);
            if(!f->Bold)f->Bold=f->Regular;
        }
        UiRegularFont=f->Regular;UiBoldFont=f->Bold;return;
    }
    if(!UiRegularFont)UiRegularFont=io.Fonts->AddFontDefault();UiBoldFont=UiRegularFont;
}
std::filesystem::path FindRuneScapeFontFile() {
    std::vector<std::filesystem::path> roots{HelpyPaths::PluginRoot()/"assets"/"fonts"};
    std::error_code ec;
    for(const auto& root:roots) {
        if(!std::filesystem::is_directory(root,ec))continue;
        static constexpr const char* preferred[]{"runescape_uf.ttf","runescape.ttf","runescape uf.ttf","runescape-uf.ttf","runescape_uf.otf","runescape.otf","runescape uf.otf","runescape-uf.otf"};
        for(const auto* name:preferred) {
            const auto candidate=root/name;if(std::filesystem::is_regular_file(candidate,ec))return candidate;
        }
        for(std::filesystem::directory_iterator it(root,ec),last;!ec&&it!=last;it.increment(ec)) {
            if(!it->is_regular_file(ec))continue;auto name=it->path().filename().string();
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            const auto ext=it->path().extension().string();std::string lowerExt=ext;std::transform(lowerExt.begin(),lowerExt.end(),lowerExt.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if((lowerExt==".ttf"||lowerExt==".otf")&&name.find("runescape")!=std::string::npos)return it->path();
        }
    }
    return {};
}
void LoadUiFontFamilies(ImGuiIO& io){
    UiFontFamilies.clear();UiRegularFont=nullptr;UiBoldFont=nullptr;
    const auto add=[&](const char* name,std::vector<std::filesystem::path> regular,std::vector<std::filesystem::path> bold){
        UiFontFamily f;f.Name=name;std::filesystem::path rp,bp;std::error_code ec;
        for(const auto& x:regular)if(!x.empty()&&std::filesystem::is_regular_file(x,ec)){rp=x;break;}
        for(const auto& x:bold)if(!x.empty()&&std::filesystem::is_regular_file(x,ec)){bp=x;break;}
        // Discover every choice, but read/rasterize only the selected family.
        // Settings can load another family on demand before the next frame.
        if(!rp.empty()){f.Available=true;f.Source=rp.string();f.BoldSource=bp.string();}
        UiFontFamilies.push_back(std::move(f));
    };
    const std::filesystem::path winFonts="C:/Windows/Fonts";
    add("Segoe UI",{winFonts/"segoeui.ttf"},{winFonts/"segoeuib.ttf"});
    add("Arial",{winFonts/"arial.ttf"},{winFonts/"arialbd.ttf"});
    add("Tahoma",{winFonts/"tahoma.ttf"},{winFonts/"tahomabd.ttf"});
    add("Verdana",{winFonts/"verdana.ttf"},{winFonts/"verdanab.ttf"});
    add("Trebuchet MS",{winFonts/"trebuc.ttf"},{winFonts/"trebucbd.ttf"});
    add("Georgia",{winFonts/"georgia.ttf"},{winFonts/"georgiab.ttf"});
    add("Consolas",{winFonts/"consola.ttf"},{winFonts/"consolab.ttf"});
    add("Courier New",{winFonts/"cour.ttf"},{winFonts/"courbd.ttf"});
    add("Bahnschrift",{winFonts/"bahnschrift.ttf"},{winFonts/"bahnschrift.ttf"});
    const auto runeScape=FindRuneScapeFontFile();
    add("RuneScape",{runeScape},{runeScape});
    SelectUiFontFamily(HelpyImGuiTheme::Current.fontFamily);io.FontDefault=UiRegularFont;
}
std::atomic<bool> FontRescanRequested{false};
struct Toast {std::string Text;std::chrono::steady_clock::time_point Until;};
std::vector<Toast> Toasts;
DiagnosticToastFeed::Feed DiagnosticFeed;
DiagnosticCenterModel::Store DiagnosticStore;
DiagnosticCenterHistory::Service DiagnosticHistory;
DiagnosticCenterMutes::Manager DiagnosticMutes;
HelpyDiagnosticOverlay DiagnosticOverlay;
DiagnosticToastFeed::Options DiagnosticOptions(){const auto& t=HelpyImGuiTheme::Current;return {t.diagnosticToastsEnabled,t.diagnosticToastLimit,t.diagnosticToastDuration};}
std::chrono::steady_clock::time_point NextPlayerRefresh{};
void PushToast(std::string text,double seconds=3.0){Toasts.push_back({std::move(text),std::chrono::steady_clock::now()+std::chrono::milliseconds(static_cast<int>(seconds*1000.0))});if(Toasts.size()>4)Toasts.erase(Toasts.begin());}
bool QueuePlayerRefresh(bool automatic){
    NextPlayerRefresh=std::chrono::steady_clock::now()+std::chrono::minutes(1);
    // The regular Read service now coalesces a dedicated read-only status hint.
    // Opening Helpy/background refresh must not compete with item mutations.
    if(automatic)return SpawnToolRequests::IsAvailable();
    return SpawnToolRequests::TrySubmit({{"Action","QuickPlayers"},{"_Source","HelpyStandalone"}});
}
void RenderToasts(const HelpyImGuiTheme::Theme& t){
    const auto now=std::chrono::steady_clock::now();Toasts.erase(std::remove_if(Toasts.begin(),Toasts.end(),[&](const Toast& x){return x.Until<=now;}),Toasts.end());if(Toasts.empty())return;
    float y=ImGui::GetIO().DisplaySize.y-t.Y(12.0f);for(int i=static_cast<int>(Toasts.size())-1;i>=0;--i){
        ImGui::SetNextWindowPos(ImVec2(t.X(12.0f),y),ImGuiCond_Always,ImVec2(0,1));ImGui::SetNextWindowBgAlpha(0.94f);ImGui::SetNextWindowSizeConstraints(ImVec2(t.X(180.0f),0),ImVec2(t.X(420.0f),t.Y(120.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,t.X(t.childRounding));ImGui::PushStyleColor(ImGuiCol_WindowBg,t.panelAlt);const std::string id="##helpy-toast-"+std::to_string(i);
        if(ImGui::Begin(id.c_str(),nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)){ImGui::TextWrapped("%s",Toasts[static_cast<size_t>(i)].Text.c_str());y=ImGui::GetWindowPos().y-t.Y(8.0f);}
        ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar();
    }
}
void RenderDiagnosticIcon(ImDrawList* dl,ImVec2 p,float size,ImU32 color,
    DiagnosticToastPresentation::Display::Icon type){
    const ImVec2 c{p.x+size*0.5f,p.y+size*0.5f};
    const float r=size*0.44f,stroke=std::max(1.0f,size*0.105f);
    using Icon=DiagnosticToastPresentation::Display::Icon;
    if(type==Icon::Warning){
        dl->AddTriangle({c.x,p.y+size*.06f},{p.x+size*.94f,p.y+size*.92f},
            {p.x+size*.06f,p.y+size*.92f},color,stroke);
        dl->AddLine({c.x,c.y-.17f*size},{c.x,c.y+.13f*size},color,stroke);
        dl->AddCircleFilled({c.x,c.y+.30f*size},stroke*.55f,color);
    }else if(type==Icon::Activity){
        for(int i=0;i<3;++i){const float n=i*size*.095f;
            dl->AddRect({p.x+size*.10f+n,p.y+size*.10f+n},
                {p.x+size*.76f+n,p.y+size*.76f+n},color,1.0f,0,stroke*.75f);}
    }else{
        dl->AddCircle(c,r,color,16,stroke);
        if(type==Icon::Error){
            dl->AddLine({c.x-r*.40f,c.y-r*.40f},{c.x+r*.40f,c.y+r*.40f},color,stroke);
            dl->AddLine({c.x+r*.40f,c.y-r*.40f},{c.x-r*.40f,c.y+r*.40f},color,stroke);
        }else if(type==Icon::Debug){
            dl->AddLine({c.x-r*.30f,c.y-r*.45f},{c.x-r*.78f,c.y},color,stroke);
            dl->AddLine({c.x-r*.78f,c.y},{c.x-r*.30f,c.y+r*.45f},color,stroke);
            dl->AddLine({c.x+r*.30f,c.y-r*.45f},{c.x+r*.78f,c.y},color,stroke);
            dl->AddLine({c.x+r*.78f,c.y},{c.x+r*.30f,c.y+r*.45f},color,stroke);
        }else if(type==Icon::Diagnostic){
            const ImVec2 wave[]={{c.x-r*.78f,c.y},{c.x-r*.36f,c.y},
                {c.x-r*.12f,c.y-r*.55f},{c.x+r*.12f,c.y+r*.55f},
                {c.x+r*.35f,c.y},{c.x+r*.78f,c.y}};
            dl->AddPolyline(wave,6,color,ImDrawFlags_None,stroke);
        }else{
            dl->AddCircleFilled({c.x,c.y-r*.48f},stroke*.52f,color);
            dl->AddLine({c.x,c.y-r*.12f},{c.x,c.y+r*.53f},color,stroke);
        }
    }
}
void RenderDiagnosticToasts(const HelpyImGuiTheme::Theme& t){
    if(!t.diagnosticToastsEnabled||DiagnosticFeed.Empty())return;
    float y=MainSurfacePosition.y+t.Y(12.0f);
    const float right=MainSurfacePosition.x+MainSurfaceSize.x-t.X(12.0f);
    const float width=std::min(t.X(390.0f),std::max(t.X(220.0f),MainSurfaceSize.x-t.X(24.0f)));
    const float wrapWidth=std::max(t.X(110.0f),width-t.X(38.0f));
    const float gap=t.Y(7.0f);
    size_t index=0;
    for(const auto& notice:DiagnosticFeed.Visible()){
        const auto display=DiagnosticToastPresentation::Format(notice);
        const auto color=notice.Summary?t.accent:notice.Severity==LiveLog::Level::Error?t.error:
            notice.Severity==LiveLog::Level::Warning?t.warning:
            notice.Severity==LiveLog::Level::Diagnostic?t.accent:t.info;
        const float iconSize=t.Y(15.0f),padY=t.Y(10.0f),padX=t.X(16.0f);
        const float headerHeight=std::max(iconSize,ImGui::GetTextLineHeight());
        const float titleHeight=[&](){if(UiBoldFont)ImGui::PushFont(UiBoldFont);
            const float h=ImGui::CalcTextSize(display.Title.c_str()).y;
            if(UiBoldFont)ImGui::PopFont();return h;}();
        const float detailHeight=ImGui::CalcTextSize(display.Detail.c_str(),nullptr,false,wrapWidth).y;
        const float footerHeight=display.Subtitle.empty()?0.0f:ImGui::CalcTextSize(display.Subtitle.c_str(),nullptr,false,wrapWidth).y;
        const float height=padY+headerHeight+t.Y(6.0f)+titleHeight+t.Y(6.0f)+detailHeight+
            (footerHeight?t.Y(5.0f)+footerHeight:0.0f)+t.Y(14.0f);
        // The open Helpy panel must not put a partial toast under the taskbar.
        if(y+height>MainSurfacePosition.y+MainSurfaceSize.y-t.Y(10.0f))break;
        ImGui::SetNextWindowPos({right,y},ImGuiCond_Always,{1,0});
        ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,t.panelAlt);
        ImGui::PushStyleColor(ImGuiCol_Border,t.border);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,t.X(t.childRounding));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,t.X(1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{padX,padY});
        const std::string id="##runeschema-log-toast-"+std::to_string(index++);
        if(ImGui::Begin(id.c_str(),nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoScrollbar)){
            const auto min=ImGui::GetWindowPos();
            const auto max=ImVec2(min.x+width,min.y+height);
            auto* dl=ImGui::GetWindowDrawList();
            dl->AddRectFilled({min.x+t.X(3),min.y+t.Y(8)},
                {min.x+t.X(7),max.y-t.Y(8)},ImGui::GetColorU32(color),t.X(2));
            const ImVec2 anchor{min.x+padX+t.X(5.0f),min.y+padY};
            RenderDiagnosticIcon(dl,anchor,iconSize,ImGui::GetColorU32(color),display.Type);
            dl->AddText({anchor.x+iconSize+t.X(8.0f),anchor.y},ImGui::GetColorU32(t.textDisabled),"RuneSchema");
            std::string level=notice.Summary?"ACTIVITY":display.Level;
            if(notice.Repetitions>1&&!notice.Summary&&!(notice.BlueprintBatch&&notice.BatchCount>1))
                level+="  x"+std::to_string(notice.Repetitions);
            const auto labelSize=ImGui::CalcTextSize(level.c_str());
            dl->AddText({max.x-padX-labelSize.x,min.y+padY},ImGui::GetColorU32(color),level.c_str());
            ImGui::SetCursorScreenPos({anchor.x,anchor.y+headerHeight+t.Y(6.0f)});
            if(UiBoldFont)ImGui::PushFont(UiBoldFont);
            ImGui::TextUnformatted(display.Title.c_str());
            if(UiBoldFont)ImGui::PopFont();
            ImGui::SetCursorScreenPos({anchor.x,anchor.y+headerHeight+t.Y(6.0f)+titleHeight+t.Y(6.0f)});
            ImGui::PushStyleColor(ImGuiCol_Text,t.textDisabled);
            ImGui::PushTextWrapPos(anchor.x+wrapWidth);
            ImGui::TextUnformatted(display.Detail.c_str());
            ImGui::PopTextWrapPos();
            if(footerHeight){
                ImGui::SetCursorScreenPos({anchor.x,anchor.y+headerHeight+t.Y(6.0f)+titleHeight+
                    t.Y(6.0f)+detailHeight+t.Y(5.0f)});
                ImGui::PushTextWrapPos(anchor.x+wrapWidth);
                ImGui::TextUnformatted(display.Subtitle.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::PopStyleColor();
        }
        ImGui::End();ImGui::PopStyleVar(3);ImGui::PopStyleColor(2);
        y+=height+gap;
    }
}
const char* PageTitle(Page page) {
    return HelpyNavigation::Title(HelpyNavigation::Parent(page));
}
HelpyImGuiTheme::Icon PageIcon(Page page) {
    using I=HelpyImGuiTheme::Icon;switch(page){case Page::Home:return I::Building;case Page::Items:return I::Item;case Page::Spawns:return I::Resource;
    case Page::Creation:case Page::Recipes:return I::Lab;case Page::Map:return I::Map;case Page::Mods:return I::Tools;
    case Page::Profile:return I::Appearance;case Page::Authoring:case Page::SaveEditor:return I::Authoring;case Page::Tools:return I::Tools;case Page::Settings:return I::Settings;case Page::LiveLog:return I::LiveLog;case Page::DedicatedServer:return I::Server;default:return I::Info;}
}
bool NavItem(Page page,const HelpyImGuiTheme::Theme& t,const std::vector<int>& sidebarDefaults) {
    const bool active=HelpyNavigation::Parent(CurrentPage)==page;ImGui::PushID(static_cast<int>(page));
    ImGui::PushStyleColor(ImGuiCol_Header,active?t.navigationActive:t.navigation);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,t.navigationHover);ImGui::PushStyleColor(ImGuiCol_HeaderActive,t.navigationActive);
    const float w=ImGui::GetContentRegionAvail().x;const bool compact=w<t.X(120.0f);
    const float textWidth=std::max(1.0f,w-t.X(46.0f));
    const auto textSize=ImGui::CalcTextSize(PageTitle(page),nullptr,false,textWidth);
    const float h=std::max(t.Y(t.navigationItemHeight),textSize.y+t.Y(18.0f));
    const bool clicked=ImGui::Selectable("##nav",active,ImGuiSelectableFlags_None,ImVec2(w,h));
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::Navigation[PageTitle(page)]=HelpyPreviewHooks::LastRect();
#endif
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const float icon=t.Y(t.navigationIconSize);
    const ImVec2 iconAt{compact?(a.x+b.x)*.5f:a.x+t.X(18),(a.y+b.y)*.5f};
    bool portrait=false;if(page==Page::Profile){const auto id=RuneSchemaProfile::Id();portrait=PlayerPortraitPicker::DrawCached(PlayerPortraitPicker::Choices.Get(id),{iconAt.x-icon*.5f,iconAt.y-icon*.5f},{iconAt.x+icon*.5f,iconAt.y+icon*.5f});}
    if(!portrait&&page==Page::Profile){ImGui::GetWindowDrawList()->AddCircle(iconAt,icon*.48f,ImGui::GetColorU32(t.accent),0,t.X(1));PortraitArt::Draw(0,{iconAt.x-icon*.32f,iconAt.y-icon*.32f},{iconAt.x+icon*.32f,iconAt.y+icon*.32f},ImGui::GetColorU32(t.accent));portrait=true;}
    if(!portrait)HelpyImGuiTheme::DrawIcon(PageIcon(page),iconAt,icon,active?t.accentHover:t.icon);
    if(!compact)ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(a.x+t.X(36.0f),a.y+(h-textSize.y)*.5f),ImGui::GetColorU32(active?t.text:t.textDisabled),PageTitle(page),nullptr,textWidth);
    else if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s",PageTitle(page));
    NavigationOrder::Drag(HelpyNavigationOrder::SidebarSurface,page,sidebarDefaults,PageTitle(page));
    ImGui::PopStyleColor(3);ImGui::PopID();
    if(clicked){
        const auto target=page==Page::Items?LastPlayersPage:page==Page::Map?LastWorldPage:page==Page::Tools?LastToolsPage:page;
        CurrentPage=HelpyNavigation::Available(target,t.workspaces,HelpyEnvironment::Server())?target:page;
    }return clicked;
}

void RenderWorkspaceTabs(const HelpyImGuiTheme::Theme& t){
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::WorkspaceTabs.clear();
#endif
    const auto parent=HelpyNavigation::Parent(CurrentPage);
    if(parent!=Page::Items&&parent!=Page::Map&&parent!=Page::Tools&&parent!=Page::Profile)return;
    // Route-backed tabs change immediately and wrap on narrow windows. There
    // is no second, deferred ImGui selection that can dispatch an old editor.
    const float width=ImGui::GetContentRegionAvail().x,gap=ImGui::GetStyle().ItemSpacing.x;
    float used=0;ImGui::PushID(parent==Page::Profile?"profile-workflows":parent==Page::Items?"players-workflows":parent==Page::Tools?"tools-workflows":"world-workflows");
    const auto tab=[&](Page page){
        if(!HelpyNavigation::Available(page,t.workspaces,HelpyEnvironment::Server()))return;
        const auto label=HelpyNavigation::TabTitle(page);
        const float side=std::min(width,ImGui::CalcTextSize(label).x+ImGui::GetStyle().FramePadding.x*2+t.X(12));
        if(used>0&&used+gap+side<=width){ImGui::SameLine();used+=gap;}else used=0;
        const bool active=CurrentPage==page;
        ImGui::PushStyleColor(ImGuiCol_Header,active?t.navigationActive:t.panel);
        ImGui::PushStyleColor(ImGuiCol_Text,active?t.accentHover:t.textDisabled);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,{.5f,.5f});
        const bool clicked=ImGui::Selectable(label,active,ImGuiSelectableFlags_None,{side,ImGui::GetFrameHeight()});
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        if(active){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddLine({a.x,b.y},{b.x,b.y},ImGui::GetColorU32(t.accent),t.Y(2));}
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::WorkspaceTabs[label]=HelpyPreviewHooks::LastRect();
#endif
        if(parent==Page::Items)NavigationOrder::Drag("players-tabs",page,HelpyNavigation::PlayerTabs,label);
        else if(parent==Page::Map)NavigationOrder::Drag("world-tabs",page,HelpyNavigation::WorldTabs,label);
        else if(parent==Page::Tools)NavigationOrder::Drag("tools-tabs",page,HelpyNavigation::ToolTabs,label);
        if(clicked)CurrentPage=page;used+=side;
    };
    if(parent==Page::Items)for(auto page:NavigationOrder::Pages("players-tabs",HelpyNavigation::PlayerTabs))tab(page);
    else if(parent==Page::Map)for(auto page:NavigationOrder::Pages("world-tabs",HelpyNavigation::WorldTabs))tab(page);
    else if(parent==Page::Profile)for(auto page:HelpyNavigation::ProfileTabs)tab(page);
    else for(auto page:NavigationOrder::Pages("tools-tabs",HelpyNavigation::ToolTabs))tab(page);
    ImGui::PopID();ImGui::Separator();
    if(parent==Page::Items)LastPlayersPage=CurrentPage;else if(parent==Page::Map)LastWorldPage=CurrentPage;else if(parent==Page::Tools)LastToolsPage=CurrentPage;
}

bool ReadBrandPixels(const char* asset,UINT& width,UINT& height,std::vector<unsigned char>& pixels){
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
       FAILED(factory->CreateDecoderFromFilename(HelpyPaths::Asset(asset).c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||
       FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    if(FAILED(converter->GetSize(&width,&height))||!width||!height||width>4096||height>4096)return false;
    pixels.resize(width*height*4);
    return SUCCEEDED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data()));
}
ID3D11ShaderResourceView* UploadBrandPixels(ID3D11Device* device,UINT width,UINT height,const std::vector<unsigned char>& pixels){
    using Microsoft::WRL::ComPtr;
    if(pixels.size()!=static_cast<size_t>(width)*height*4)return nullptr;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};ComPtr<ID3D11Texture2D> texture;
    ID3D11ShaderResourceView* view=nullptr;
    if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)))device->CreateShaderResourceView(texture.Get(),nullptr,&view);
    return view;
}
ID3D11ShaderResourceView* LoadBrand(ID3D11Device* device,const char* asset="RuneSchema.png",bool monochrome=false,int logoLayer=0,bool darkForeground=false) {
    UINT width=0,height=0;std::vector<unsigned char> pixels;if(!ReadBrandPixels(asset,width,height,pixels))return nullptr;
    if(monochrome)for(size_t index=0;index<pixels.size();index+=4)HelpyBranding::MonochromeMask(pixels.data()+index,darkForeground);
    if(logoLayer)for(size_t index=0;index<pixels.size();index+=4)HelpyBranding::LogoLayer(pixels.data()+index,static_cast<unsigned>(index/4%width),width,logoLayer==2);
    return UploadBrandPixels(device,width,height,pixels);
}

void LoadBrandTextures(ID3D11Device* device) {
    HelpyBackgrounds::Decode=[](const std::filesystem::path& file){
        HelpyBackgrounds::Pixels result;
        const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        struct ComLifetime{bool Active;~ComLifetime(){if(Active)CoUninitialize();}} com{SUCCEEDED(initialized)};
        try{
            const auto fileAttributes=GetFileAttributesW(file.c_str()),folderAttributes=GetFileAttributesW(file.parent_path().c_str());
            if(fileAttributes==INVALID_FILE_ATTRIBUTES||folderAttributes==INVALID_FILE_ATTRIBUTES||(fileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||(folderAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||!std::filesystem::is_regular_file(file)||std::filesystem::file_size(file)>20*1024*1024)throw std::runtime_error("Background is missing, linked, or larger than 20 MiB.");
            using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
            if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||FAILED(converter->GetSize(&result.Width,&result.Height)))throw std::runtime_error("Background could not be decoded. Use PNG, JPG or BMP.");
            if(!result.Width||!result.Height||result.Width>4096||result.Height>4096)throw std::runtime_error("Background dimensions must be at most 4096 x 4096.");
            result.Data.resize(static_cast<size_t>(result.Width)*result.Height*4);
            if(FAILED(converter->CopyPixels(nullptr,result.Width*4,static_cast<UINT>(result.Data.size()),result.Data.data())))throw std::runtime_error("Background image pixels could not be read.");
        }catch(const std::exception& error){result.Data.clear();result.Error=error.what();}return result;
    };
    HelpyBackgrounds::Upload=[device](const HelpyBackgrounds::Pixels& pixels){return UploadBrandPixels(device,pixels.Width,pixels.Height,pixels.Data);};
    GameBrandTexture=LoadBrand(device,"dragonwilds.png");
    HomeBannerTexture=LoadBrand(device,"home-banner.png");
    const std::array detailAssets{"github-details.png","nexus-details.png","curseforge-details.png"};
    for(size_t i=0;i<detailAssets.size();++i){auto& image=HomeDetailImages[i];std::vector<unsigned char> pixels;
        if(ReadBrandPixels(detailAssets[i],image.Width,image.Height,pixels))image.Texture=UploadBrandPixels(device,image.Width,image.Height,pixels);
    }
    for(size_t i=0;i<HomeLinks.size();++i){
        if(i!=1){UINT width=0,height=0;std::vector<unsigned char> pixels;if(ReadBrandPixels(HomeLinks[i].Asset,width,height,pixels)){for(size_t p=0;p<pixels.size();p+=4)HelpyBranding::MonochromeMask(pixels.data()+p);HelpyBranding::NormalizeMark(width,height,pixels);HomeLinkTextures[i]=UploadBrandPixels(device,width,height,pixels);}continue;}
        UINT width=0,height=0;std::vector<unsigned char> pixels;
        if(ReadBrandPixels(HomeLinks[i].Asset,width,height,pixels)){
            // Normalize the combined silhouette once so the white and orange
            // layers remain aligned after padding is removed.
            for(size_t p=0;p<pixels.size();p+=4)if(std::max({pixels[p],pixels[p+1],pixels[p+2]})<12)pixels[p+3]=0;
            HelpyBranding::NormalizeMark(width,height,pixels);
            auto accent=pixels;for(size_t p=0;p<pixels.size();p+=4){HelpyBranding::CommunityLayer(pixels.data()+p,false);HelpyBranding::CommunityLayer(accent.data()+p,true);}
            HomeLinkTextures[i]=UploadBrandPixels(device,width,height,pixels);HomeNexusAccentTexture=UploadBrandPixels(device,width,height,accent);
        }
    }
    RuntimeDetailsPanel::GameLogo=ImTextureRef(GameBrandTexture);
    LoadingRingTexture=LoadBrand(device,"loading-ring.png",true);HelpyLoadingIndicator::Texture=ImTextureRef(LoadingRingTexture);
    BrandTexture=LoadBrand(device,"RuneSchema.png",false,1);
    BrandAccentTexture=LoadBrand(device,"RuneSchema.png",false,2);
    if(!BrandTexture||!BrandAccentTexture){
        if(BrandTexture){BrandTexture->Release();BrandTexture=nullptr;}
        if(BrandAccentTexture){BrandAccentTexture->Release();BrandAccentTexture=nullptr;}
        BrandTexture=LoadBrand(device);
    }
    UINT logoWidth=0,logoHeight=0;std::vector<unsigned char> logoPixels;
    if(ReadBrandPixels("RuneSchema.png",logoWidth,logoHeight,logoPixels)){
        auto parts=HelpySplash::SplitLogo(logoPixels,static_cast<int>(logoWidth),static_cast<int>(logoHeight));
        for(size_t i=0;i<parts.size();++i){auto& p=parts[i];if(!p.Width||!p.Height)continue;
            // Center keeps the neutral R edging; its gold uses the existing
            // theme-tinted accent texture. Brackets keep their original colors.
            if(i==1)for(size_t pixel=0;pixel<p.Pixels.size();pixel+=4)HelpyBranding::LogoLayer(p.Pixels.data()+pixel,p.X+static_cast<unsigned>(pixel/4%p.Width),logoWidth,false);
            SplashSprites[i].Texture=UploadBrandPixels(device,p.Width,p.Height,p.Pixels);
            SplashSprites[i].Bounds={float(p.X)/logoWidth,float(p.Y)/logoHeight,float(p.X+p.Width)/logoWidth,float(p.Y+p.Height)/logoHeight};
        }
    }
    SteamTexture=LoadBrand(device,"steam.png",true);
    // Render the supplied black-on-white mark as an accent-tinted upload
    // mask, not a white rectangle. The original PNG is kept byte-for-byte.
    EpicTexture=LoadBrand(device,"epic.png",true,0,true);
    XboxTexture=LoadBrand(device,"xbox.png",true);
    SaveEditorPanel::SteamIcon=ImTextureRef(SteamTexture);SaveEditorPanel::EpicIcon=ImTextureRef(EpicTexture);SaveEditorPanel::XboxIcon=ImTextureRef(XboxTexture);
    PartyMemberView::SteamIcon=ImTextureRef(SteamTexture);PartyMemberView::EpicIcon=ImTextureRef(EpicTexture);PartyMemberView::XboxIcon=ImTextureRef(XboxTexture);
    ItemGridPicker::ModLogo=ImTextureRef(BrandTexture);ItemGridPicker::ModLogoAccent=ImTextureRef(BrandAccentTexture);
}
void ReleaseBrandTextures(bool shutdownBackgrounds=true) {
    SourceBanners::Shutdown();
    PlayerPortraitPicker::ReleaseTextures();PartyMemberView::SteamIcon={};PartyMemberView::EpicIcon={};PartyMemberView::XboxIcon={};
    if(shutdownBackgrounds)HelpyBackgrounds::Shutdown();
    for(auto& image:HomeDetailImages){if(image.Texture)image.Texture->Release();image={};}
    for(auto*& texture:HomeLinkTextures)if(texture){texture->Release();texture=nullptr;}
    if(HomeNexusAccentTexture){HomeNexusAccentTexture->Release();HomeNexusAccentTexture=nullptr;}
    for(auto& part:SplashSprites){if(part.Texture)part.Texture->Release();part={};}
    ItemGridPicker::ModLogo={};ItemGridPicker::ModLogoAccent={};HelpyLoadingIndicator::Texture={};RuntimeDetailsPanel::GameLogo={};
    for(auto** view:{&BrandTexture,&BrandAccentTexture,&GameBrandTexture,&LoadingRingTexture,&HomeBannerTexture,&SteamTexture,&EpicTexture,&XboxTexture})if(*view){(*view)->Release();*view=nullptr;}
}
void DrawThemedLogo(ImDrawList* draw,ImVec2 a,ImVec2 b,const HelpyImGuiTheme::Theme& theme,float opacity=1){
    if(!BrandTexture){
        const char* text="{ R }";const float size=std::max(12.f,std::min((b.y-a.y)*.45f,(b.x-a.x)*.3f));
        const auto extent=ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,0,text);auto color=theme.accent;color.w=opacity;
        draw->AddText(ImGui::GetFont(),size,{(a.x+b.x-extent.x)*.5f,(a.y+b.y-extent.y)*.5f},ImGui::GetColorU32(color),text);return;
    }
    if(BrandTexture)draw->AddImage(ImTextureRef(BrandTexture),a,b,{0,0},{1,1},ImGui::GetColorU32(ImVec4(1,1,1,opacity)));
    if(BrandAccentTexture){auto tint=theme.accent;tint.w=opacity;draw->AddImage(ImTextureRef(BrandAccentTexture),a,b,{0,0},{1,1},ImGui::GetColorU32(tint));}
}
struct HomeDestination { Page Target; const char* Label; HelpyImGuiTheme::Icon Icon; const char* Description; };
void DrawRuneExit(ImDrawList* draw,ImVec2 a,ImVec2 b,const HelpyImGuiTheme::Theme& theme,const HelpySplash::ExitFrame& exit,float opacity=1){
    if(!SplashSprites[0].Texture||!SplashSprites[1].Texture||!SplashSprites[2].Texture){DrawThemedLogo(draw,a,b,theme,opacity*exit.Fade);return;}
    const float size=b.x-a.x;
    for(size_t i=0;i<SplashSprites.size();++i){const auto& sprite=SplashSprites[i];ImVec2 min{a.x+sprite.Bounds.x*size,a.y+sprite.Bounds.y*size},max{a.x+sprite.Bounds.z*size,a.y+sprite.Bounds.w*size};
        const float shift=i==0?-(max.x-MainSurfacePosition.x+theme.X(24))*exit.Travel:i==2?(MainSurfacePosition.x+MainSurfaceSize.x-min.x+theme.X(24))*exit.Travel:0;
        min.x+=shift;max.x+=shift;draw->AddImage(ImTextureRef(sprite.Texture),min,max,{0,0},{1,1},ImGui::GetColorU32(ImVec4(1,1,1,opacity*exit.Fade)));
    }
    if(BrandAccentTexture){auto tint=theme.accent;tint.w=opacity*exit.Fade;draw->AddImage(ImTextureRef(BrandAccentTexture),a,b,{0,0},{1,1},ImGui::GetColorU32(tint));}
}
constexpr std::array HomeDestinations{
    HomeDestination{Page::Mods,"Mods",HelpyImGuiTheme::Icon::Tools,HelpyNavigation::Description(Page::Mods)},
    HomeDestination{Page::Items,"Players",HelpyImGuiTheme::Icon::Item,"Inventory, gear, clones and character saves."},
    HomeDestination{Page::Map,"World",HelpyImGuiTheme::Icon::Map,"Ashenfall map, party, spawning and journal."},
    HomeDestination{Page::Settings,"Settings",HelpyImGuiTheme::Icon::Settings,"Appearance, item caches and RuneSchema settings."},
    HomeDestination{Page::Tools,"Tools",HelpyImGuiTheme::Icon::Tools,HelpyNavigation::Description(Page::Tools)},
    HomeDestination{Page::LiveLog,"Diagnostics",HelpyImGuiTheme::Icon::LiveLog,HelpyNavigation::Description(Page::LiveLog)},
    HomeDestination{Page::BugReport,"Report a bug",HelpyImGuiTheme::Icon::Info,HelpyNavigation::Description(Page::BugReport)},
    HomeDestination{Page::DedicatedServer,"Server / Co-op",HelpyImGuiTheme::Icon::Server,HelpyNavigation::Description(Page::DedicatedServer)}
};
static_assert(HomeDestinations.size()+1==HelpyNavigation::SidebarPages.size(),"Every top-level workspace needs a Home tile");
size_t HomeFocused=0;
double HomeLinkTouched=0;
ImVec2 HomeLinkAnchor{};
#if defined(HELPY_PREVIEW)
std::array<ImVec4,HomeDestinations.size()> PreviewHomeTiles{};
ImVec4 PreviewHomeQuick{};
ImVec4 PreviewHomeQuickPopup{};
ImVec4 PreviewHomeQuickTooltip{};
ImVec4 PreviewHomeTooltip{};
float PreviewHomeCenter=0;
std::array<ImVec4,HomeLinks.size()> PreviewHomeLinks{};
ImVec4 PreviewHomeLinkOpen{};
bool PreviewHomeHoverDetails=false;
bool PreviewSidebarVisible=false;
bool PreviewHomeFits=false;
ImVec4 PreviewHomeCard{};
std::array<ImVec4,HomeLinks.size()> PreviewHomeCards{};
bool PreviewHomeCardImage=false;
#endif
void CenterHomeLines(const char* text){
    HelpyImGuiTheme::DetailsTextUnformatted(text);
}
void RenderRuntimeBadge(const HelpyImGuiTheme::Theme& t,float logicalSize=HelpyBranding::RuntimeIconSize);
ImVec2 RuntimeBadgeSize(const HelpyImGuiTheme::Theme& t,float logicalSize=HelpyBranding::RuntimeIconSize) {
    const float size=std::min(t.Y(logicalSize),t.X(logicalSize));
    return { (HelpyEnvironment::SharedStoreBadge()?size*1.8f+t.X(3):size)+(RuntimeConnectionPanel::Read().Host?t.X(16):0),size };
}
void RenderHome(const HelpyImGuiTheme::Theme& theme){
    BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);
    if(HomeExit.Active&&HomeExit.At(ImGui::GetTime(),animate!=FALSE).Finished){CurrentPage=HomeExitTarget;HomeExit={};HomeLinkDetails=-1;return;}
    const float reveal=std::isfinite(HomeRevealStarted)?HelpySplash::Smooth(static_cast<float>((ImGui::GetTime()-HomeRevealStarted)/HelpySplash::RevealSeconds)):1;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*reveal);
    struct RevealStyle {~RevealStyle(){ImGui::PopStyleVar();}} revealStyle;
    std::vector<Page> homePages;for(const auto& destination:HomeDestinations)homePages.push_back(destination.Target);
    std::vector<size_t> enabled;
    for(auto page:NavigationOrder::Pages("home",homePages))for(size_t index=0;index<HomeDestinations.size();++index)
        if(HomeDestinations[index].Target==page&&HelpyNavigation::Available(page,theme.workspaces,HelpyEnvironment::Server()))enabled.push_back(index);
    if(std::find(enabled.begin(),enabled.end(),HomeFocused)==enabled.end())HomeFocused=enabled.front();
#if defined(HELPY_PREVIEW)
    PreviewHomeTiles.fill({});
    PreviewHomeHoverDetails=false;
#endif
    const bool compact=ImGui::GetContentRegionAvail().y<theme.Y(500);
    const auto at=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,height=theme.Y(compact?90:164);const ImVec2 center{at.x+width*.5f,at.y+height*.5f};
#if defined(HELPY_PREVIEW)
    PreviewHomeCenter=center.x;
#endif
    auto* draw=ImGui::GetWindowDrawList();
    // Home's chosen application background remains continuous behind the mark.
    // The supplied community banner belongs only to Discord's details card.
    if(BrandTexture){const float size=height-theme.Y(12);const ImVec2 a{center.x-size*.5f,center.y-size*.5f},b{a.x+size,a.y+size};if(HomeExit.Active)DrawRuneExit(draw,a,b,theme,HomeExit.At(ImGui::GetTime(),animate!=FALSE));else DrawThemedLogo(draw,a,b,theme);}
    ImGui::Dummy({width,height});
    ImGui::Spacing();
    const float gap=theme.X(12),minimum=theme.X(84);
    const int columns=std::min(int(enabled.size()),std::clamp(int((width+gap)/(minimum+gap)),1,6));
    const int rows=(int(enabled.size())+columns-1)/columns;
    const auto runtimeSize=RuntimeBadgeSize(theme);const float runtimeGap=theme.Y(12);
    const float heightBudget=std::max(theme.X(70),(ImGui::GetContentRegionAvail().y-theme.Y(156)-runtimeSize.y-runtimeGap-gap*(rows-1))/rows);
    const float tile=std::min({theme.X(138),(width-gap*(columns-1))/columns,heightBudget});
    for(size_t first=0;first<enabled.size();first+=columns){
        const int count=std::min(size_t(columns),enabled.size()-first);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(width-(tile*count+gap*(count-1)))*.5f));
        for(int col=0;col<count;++col){
            const auto index=enabled[first+col];const auto& destination=HomeDestinations[index];
            if(col)ImGui::SameLine(0,gap);
            ImGui::PushID(int(index));const auto origin=ImGui::GetCursorScreenPos();
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0,0,0));
            ImGui::BeginDisabled(HomeExit.Active);const bool activated=HelpyImGuiTheme::SoftButton("##home-tile",{tile,tile});ImGui::EndDisabled();ImGui::PopStyleColor(3);
            const auto homeRectMin=ImGui::GetItemRectMin(),homeRectMax=ImGui::GetItemRectMax();const bool homeHover=ImGui::IsItemHovered()||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible);
            if(!HomeExit.Active)NavigationOrder::Drag("home",destination.Target,homePages,destination.Label);
            if(homeHover){
                HomeFocused=index;
                PS::HelpyImGuiTheme::BeginDetailsTooltip(200);ImGui::PushTextWrapPos(-1);
                const auto iconAt=ImGui::GetCursorScreenPos();const float iconWidth=ImGui::GetContentRegionAvail().x;
                HelpyImGuiTheme::DrawIcon(destination.Icon,{iconAt.x+iconWidth*.5f,iconAt.y+theme.Y(16)},theme.Y(26),theme.accent);ImGui::Dummy({iconWidth,theme.Y(32)});
                ImGui::PushStyleColor(ImGuiCol_Text,theme.accent);CenterHomeLines(destination.Label);ImGui::PopStyleColor();
                ImGui::Separator();CenterHomeLines(destination.Description);ImGui::Dummy({0,0});
#if defined(HELPY_PREVIEW)
                PreviewHomeHoverDetails=true;
                PreviewHomeTooltip=HelpyPreviewHooks::WindowRect();
#endif
                ImGui::PopTextWrapPos();PS::HelpyImGuiTheme::EndDetailsTooltip();
            }
#if defined(HELPY_PREVIEW)
            PreviewHomeTiles[index]={homeRectMin.x,homeRectMin.y,homeRectMax.x,homeRectMax.y};
#endif
            const bool selected=HomeFocused==index;
            const auto tint=selected?theme.accent:theme.text;
            if(selected)draw->AddRect(origin,{origin.x+tile,origin.y+tile},ImGui::GetColorU32(theme.accent),theme.X(5));
            HelpyImGuiTheme::DrawIcon(destination.Icon,{origin.x+tile*.5f,origin.y+tile*.38f},theme.Y(33),tint);
            const std::string label=destination.Label;const auto split=label.find(' ');
            const auto line=[&](const std::string& text,float y){const auto size=ImGui::CalcTextSize(text.c_str());draw->AddText({origin.x+(tile-size.x)*.5f,y},ImGui::GetColorU32(tint),text.c_str());};
            const float labelY=origin.y+tile*.66f;
            if(split!=std::string::npos&&ImGui::CalcTextSize(label.c_str()).x>tile-theme.X(12)){
                line(label.substr(0,split),labelY);line(label.substr(split+1),labelY+ImGui::GetTextLineHeight());
            }else line(label,labelY);
            if(activated&&!HomeExit.Active){HomeExitTarget=destination.Target;HomeLinkDetails=-1;if(animate==FALSE)CurrentPage=destination.Target;else HomeExit.Begin(ImGui::GetTime(),1);}
            ImGui::PopID();
        }
        if(first+columns<enabled.size())ImGui::SetCursorPosY(ImGui::GetCursorPosY()+std::max(0.f,gap-ImGui::GetStyle().ItemSpacing.y));
    }
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
    // Secondary tools stay below the main choices; explanatory controls are on demand.
    const float buttonWidth=std::min(width,std::max(theme.X(128),ImGui::CalcTextSize("Quick settings").x+ImGui::GetStyle().FramePadding.x*2+theme.X(16)));
    const float profileWidth=theme.X(92),actionsWidth=buttonWidth+gap+profileWidth;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(width-actionsWidth)*.5f));
    if(HelpyImGuiTheme::SoftButton("Quick settings",{buttonWidth,theme.Y(28)}))ImGui::OpenPopup("Home quick settings");
#if defined(HELPY_PREVIEW)
    PreviewHomeQuick=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::IsItemHovered()){
        HelpyImGuiTheme::BeginDetailsTooltip();HelpyImGuiTheme::DetailsTextUnformatted("Common RuneSchema switches without leaving Home.");
#if defined(HELPY_PREVIEW)
        PreviewHomeQuickTooltip=HelpyPreviewHooks::WindowRect();
#endif
        HelpyImGuiTheme::EndDetailsTooltip();
    }
    ImGui::SameLine(0,gap);
    if(HelpyImGuiTheme::SoftButton("Profile",{profileWidth,theme.Y(28)}))CurrentPage=Page::Profile;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::Navigation["Profile"]=HelpyPreviewHooks::LastRect();
#endif
    if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("Your account, characters and loadouts");
    ImGui::SetNextWindowSize({std::min(theme.X(440),ImGui::GetIO().DisplaySize.x-theme.X(24)),0},ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({0,0},{ImGui::GetIO().DisplaySize.x-theme.X(24),ImGui::GetIO().DisplaySize.y-theme.Y(24)});
    HelpyImGuiTheme::CenterNextWindow();
    if(ImGui::BeginPopup("Home quick settings")){
        ImGui::SeparatorText("Quick settings");RuneSchemaSettingsPanel::QuickSettings();
#if defined(HELPY_PREVIEW)
        PreviewHomeQuickPopup=HelpyPreviewHooks::WindowRect();
#endif
        ImGui::EndPopup();
    }
    ImGui::Dummy({0,theme.Y(32)});
    ItemGridPicker::PopulationProgress(LazyCatalog::For(0),{},false,true);
    ImGui::Spacing();
    const float linkSize=theme.Y(40),linkGap=theme.X(16),linkWidth=linkSize*HomeLinks.size()+linkGap*(HomeLinks.size()-1);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY()+std::max(0.f,ImGui::GetContentRegionAvail().y-linkSize-runtimeSize.y-runtimeGap-theme.Y(8)));
    const float footerX=ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(footerX+std::max(0.f,(width-runtimeSize.x)*.5f));RenderRuntimeBadge(theme);
    ImGui::SetCursorPosY(ImGui::GetItemRectMax().y-ImGui::GetWindowPos().y+runtimeGap);ImGui::SetCursorPosX(footerX);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(width-linkWidth)*.5f));
    for(size_t i=0;i<HomeLinks.size();++i){
        if(i)ImGui::SameLine(0,linkGap);ImGui::PushID(static_cast<int>(i)+900);
        const auto start=ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
        const bool clicked=HelpyImGuiTheme::SoftButton("##community-link",{linkSize,linkSize});
        ImGui::PopStyleColor();
#if defined(HELPY_PREVIEW)
        PreviewHomeLinks[i]=HelpyPreviewHooks::LastRect();
#endif
        if(HomeLinkTextures[i]){const float inset=theme.Y(5);const ImVec2 a{start.x+inset,start.y+inset},b{start.x+linkSize-inset,start.y+linkSize-inset};
            const bool hovered=ImGui::IsItemHovered()||ImGui::IsItemFocused();
            draw->AddImage(ImTextureRef(HomeLinkTextures[i]),a,b,{0,0},{1,1},i==1?IM_COL32_WHITE:ImGui::GetColorU32(hovered?theme.accentHover:theme.accent));
            if(i==1&&HomeNexusAccentTexture)draw->AddImage(ImTextureRef(HomeNexusAccentTexture),a,b,{0,0},{1,1},ImGui::GetColorU32(hovered?theme.accentHover:theme.accent));
        }
        if(ImGui::IsItemHovered()||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible)){
            HomeLinkDetails=static_cast<int>(i);HomeLinkTouched=ImGui::GetTime();HomeLinkAnchor={start.x+linkSize*.5f,start.y-theme.Y(6)};
        }
        if(clicked)OpenHomeLink(i);ImGui::PopID();
    }
    const float footerBottom=ImGui::GetItemRectMax().y;
    // Unlike an ImGui tooltip, this hover card accepts clicks on its URL.
    // A short grace period lets the pointer cross the gap from the icon.
    if(HomeLinkDetails>=0&&ImGui::GetTime()-HomeLinkTouched<.35){
        const auto index=static_cast<size_t>(HomeLinkDetails);const auto& link=HomeLinks[index];
        const float cardWidth=std::min(theme.X(540),ImGui::GetIO().DisplaySize.x-theme.X(24));
        const float cardHeight=std::min(theme.Y(300),std::max(theme.Y(120),HomeLinkAnchor.y-theme.Y(12)));
        const float x=std::clamp(center.x,cardWidth*.5f+theme.X(8),ImGui::GetIO().DisplaySize.x-cardWidth*.5f-theme.X(8));
        ImGui::SetNextWindowSizeConstraints({cardWidth,0},{cardWidth,std::max(theme.Y(120),HomeLinkAnchor.y-theme.Y(12))});
        ImGui::SetNextWindowPos({x,HomeLinkAnchor.y},ImGuiCond_Always,{.5f,1});ImGui::SetNextWindowSize({cardWidth,cardHeight});
        if(ImGui::Begin("##home-destination-details",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)){
            if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))HomeLinkTouched=ImGui::GetTime();
            ImGui::PushStyleColor(ImGuiCol_Text,theme.accent);CenterHomeLines(link.Name);ImGui::PopStyleColor();
            const auto image=index<3?HomeDetailImages[index]:HomeDetailImage{HomeBannerTexture,3122,892};
            if(image.Texture&&image.Width&&image.Height){
                const float available=ImGui::GetContentRegionAvail().x;
                const float frameHeight=std::min(available*9.f/16.f,cardHeight*.48f),scale=std::min(available/image.Width,frameHeight/image.Height);
                const ImVec2 size{image.Width*scale,image.Height*scale},at=ImGui::GetCursorScreenPos();
                ImGui::Dummy({available,frameHeight});auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(at,{at.x+available,at.y+frameHeight},ImGui::GetColorU32(ImGuiCol_FrameBg));
                const ImVec2 a{at.x+(available-size.x)*.5f,at.y+(frameHeight-size.y)*.5f};draw->AddImage(ImTextureRef(image.Texture),a,{a.x+size.x,a.y+size.y});ImGui::Spacing();
            }
            if(index<3){ImGui::PushStyleColor(ImGuiCol_Text,theme.textDisabled);CenterHomeLines("Page preview (not live)");ImGui::PopStyleColor();}
            CenterHomeLines(link.Description);ImGui::Spacing();CenterHomeLines(link.Url);
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),cardHeight-ImGui::GetStyle().WindowPadding.y-ImGui::GetFrameHeight()));
            const float linkStart=ImGui::GetCursorPosX(),linkWidth=ImGui::CalcTextSize("Open this page").x;
            ImGui::SetCursorPosX(linkStart+std::max(0.f,(ImGui::GetContentRegionAvail().x-linkWidth)*.5f));
            if(ImGui::TextLink("Open this page"))OpenHomeLink(index);
#if defined(HELPY_PREVIEW)
            PreviewHomeLinkOpen=HelpyPreviewHooks::LastRect();
            PreviewHomeCard=HelpyPreviewHooks::WindowRect();PreviewHomeCardImage=image.Texture!=nullptr;
            PreviewHomeCards[index]=PreviewHomeCard;
#endif
        }ImGui::End();
    }
#if defined(HELPY_PREVIEW)
    const auto homeBadge=HelpyPreviewHooks::RuntimeBadge;
    PreviewHomeFits=footerBottom<=ImGui::GetWindowPos().y+ImGui::GetWindowSize().y-ImGui::GetStyle().WindowPadding.y&&
        std::abs((homeBadge.x+homeBadge.z)*.5f-PreviewHomeCenter)<1&&homeBadge.w+theme.Y(6)<=PreviewHomeLinks[0].y&&HelpyPreviewHooks::RuntimeBadgeDraws==1;
#endif
}
void RenderInformationBar(const HelpyImGuiTheme::Theme& t,bool runtimeBadge=true) {
    // Header badges are optional; Home keeps its status action above the social links.
    ImGui::BeginGroup();
    const auto start=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x;
    const auto badge=RuntimeBadgeSize(t);
    // Push an unscaled base size, then measure the final DPI-scaled height.
    ImGui::PushFont(nullptr,ImGui::GetStyle().FontSizeBase*1.65f);
    const float titleHeight=ImGui::GetFontSize();
    const float height=std::max(badge.y,titleHeight);
    if(CurrentPage!=Page::Home){
        ImGui::SetCursorScreenPos({start.x,start.y+(height-titleHeight)*.5f});
        ImGui::TextUnformatted(PageTitle(CurrentPage));
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::PageHeading=HelpyPreviewHooks::LastRect();
#endif
    }
    ImGui::PopFont();
    if(runtimeBadge){
        ImGui::SetCursorScreenPos({start.x+std::max(0.0f,width-badge.x),start.y+(height-badge.y)*.5f});
        RenderRuntimeBadge(t);
    }
    ImGui::SetCursorScreenPos({start.x,start.y+height});ImGui::Dummy({0,0});
    ImGui::EndGroup();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::InformationBar={start.x,start.y,start.x+width,start.y+height};
#endif
}
bool RuneSchemaUpdateAvailable(){const auto& state=HelpyUpdates::Current();return
    RuneSchemaUpdateInfo::Compare(HelpyEnvironment::RuneSchemaVersion,state.MainVersion)==RuneSchemaUpdateInfo::Comparison::Newer
    ||RuneSchemaUpdateInfo::Compare(HelpyEnvironment::RuneSchemaVersion,state.ReleaseVersion)==RuneSchemaUpdateInfo::Comparison::Newer;}
void RenderRuntimeBadge(const HelpyImGuiTheme::Theme& t,float logicalSize) {
    const float size=std::min(t.Y(logicalSize),t.X(logicalSize));
    const bool shared=HelpyEnvironment::SharedStoreBadge();
    const float marksWidth=shared?size*1.8f+t.X(3):size;
    const auto base=ImGui::GetCursorScreenPos();
    if(ImGui::InvisibleButton("##runtime-status",RuntimeBadgeSize(t,logicalSize),ImGuiButtonFlags_EnableNav))RuntimeDetailsPanel::Request();
    if(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible)ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),ImGui::GetColorU32(t.accent));
    const bool showDetails=!RuntimeDetailsPanel::Open&&(ImGui::IsItemHovered()||(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible));
    const ImVec2 start=base;
#if defined(HELPY_PREVIEW)
    ++HelpyPreviewHooks::RuntimeBadgeDraws;HelpyPreviewHooks::RuntimeBadge=HelpyPreviewHooks::LastRect();HelpyPreviewHooks::RuntimeDetailsVisible=showDetails;
#endif
    auto* texture=HelpyEnvironment::Current==HelpyEnvironment::Kind::Steam?SteamTexture:
        HelpyEnvironment::Current==HelpyEnvironment::Kind::Epic?EpicTexture:
        HelpyEnvironment::Current==HelpyEnvironment::Kind::GamePass?XboxTexture:nullptr;
    const ImVec4 gold(t.accent.x,t.accent.y,t.accent.z,0.78f);
    if(shared) {
        const float mark=size*.9f;
        for(int i=0;i<2;++i){auto* image=i?EpicTexture:SteamTexture;if(!image)continue;
            const ImVec2 point(start.x+i*(mark+t.X(3)),start.y+(size-mark)*.5f);
            ImGui::GetWindowDrawList()->AddImage(ImTextureRef(image),point,ImVec2(point.x+mark,point.y+mark),ImVec2(0,0),ImVec2(1,1),ImGui::GetColorU32(gold));}
    }
    else if(texture)ImGui::GetWindowDrawList()->AddImage(ImTextureRef(texture),start,ImVec2(start.x+size,start.y+size),ImVec2(0,0),ImVec2(1,1),ImGui::GetColorU32(gold));
    else HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Appearance,ImVec2(start.x+size*.5f,start.y+size*.5f),size*.75f,gold);
    const bool ready=SpawnToolRequests::IsAvailable();
    const auto network=RuntimeConnectionPanel::Read();
    if(network.Host){
        const ImVec2 host{start.x+marksWidth+t.X(6),start.y+size*.4f};
        ImGui::GetWindowDrawList()->AddCircleFilled(host,t.X(8),ImGui::GetColorU32(t.panel));
        HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Server,host,t.X(12),t.accent);
    }
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::RuntimeHostOverlay=network.Host;
#endif
    const auto status=ready?t.success:t.textDisabled;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(start.x+marksWidth-t.X(3),start.y+size-t.Y(3)),t.X(4),ImGui::GetColorU32(status));
    if(showDetails){
        PS::HelpyImGuiTheme::BeginDetailsTooltip();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+t.X(360));
        HelpyImGuiTheme::DetailsTextUnformatted(HelpyEnvironment::Label());ImGui::PushStyleColor(ImGuiCol_Text,status);HelpyImGuiTheme::DetailsText("%s",ready?"Runtime connected":"Runtime unavailable");ImGui::PopStyleColor();ImGui::Separator();
        HelpyImGuiTheme::DetailsText("Session: %s",network.Session.c_str());HelpyImGuiTheme::DetailsTextUnformatted(network.Health.c_str());
        HelpyImGuiTheme::DetailsTextUnformatted("Click for Runtime, Attributions and Legal");HelpyImGuiTheme::DetailsTextDisabled("%s",HelpySettings::HotkeyHint().c_str());
        ImGui::PopTextWrapPos();PS::HelpyImGuiTheme::EndDetailsTooltip();
    }
}

bool PendingWikiInitialAssets=false;
bool InitialAssetsBusy(){return HelpyDownloads::Busy()||HelpyMapCache::Busy()||HelpyWikiIcons::Busy()||!HelpyMapCache::Current().Ready;}
void DownloadInitialAssets(){
    if(InitialAssetsBusy())return;
    HelpyDownloads::StartArtwork();
    if(!HelpyMapCache::Current().Data)HelpyMapCache::StartInitial();
    else if(HelpyMapCache::RefreshEligible())HelpyMapCache::Refresh();
    PendingWikiInitialAssets=true;
}
void RenderSplash(const HelpyImGuiTheme::Theme& theme){
    RenderInformationBar(theme,false);
    const auto origin=ImGui::GetCursorScreenPos(),available=ImGui::GetContentRegionAvail();
    if(!std::isfinite(SplashStarted))SplashStarted=ImGui::GetTime();BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);
    const double now=ImGui::GetTime();double elapsed=now-SplashStarted;
#if defined(HELPY_PREVIEW)
    // Screenshot fixtures sample exact phases independently of compiler/GPU
    // contention. Shipping animation continues to use the real UI clock.
    if(HelpyPreviewHooks::SplashElapsed)elapsed=*HelpyPreviewHooks::SplashElapsed;
#endif
    const float blend=SplashExit.Active?SplashExit.InitialBlend:HelpySplash::Blend(elapsed,animate!=FALSE);
    const auto exit=SplashExit.At(now,animate!=FALSE);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SplashBlend=blend;HelpyPreviewHooks::SplashAnimated=animate!=FALSE;
    HelpyPreviewHooks::SplashExiting=SplashExit.Active;HelpyPreviewHooks::SplashExitProgress=SplashExit.Active?exit.Progress:0;
#endif
    const float artHeight=std::min(theme.Y(190),available.y*.38f),logoGap=std::min(theme.Y(26),available.y*.06f),buttonGap=std::min(theme.Y(16),available.y*.035f),buttonHeight=std::min(theme.Y(42),available.y*.12f);
    const auto& downloads=HelpyDownloads::Current();bool downloadHint=(downloads.NeedsArtwork||(HelpyMapCache::Current().Ready&&!HelpyMapCache::Current().Data))&&!downloads.HintDismissed;
#if defined(HELPY_PREVIEW)
    downloadHint=downloadHint||HelpyPreviewHooks::ForceArtworkHint;
#endif
    const float tipsHeight=ImGui::GetTextLineHeightWithSpacing()*2;
    const float groupHeight=artHeight+logoGap+ImGui::GetTextLineHeight()+buttonGap+buttonHeight+tipsHeight+(downloadHint?theme.Y(100):0);
    const ImVec2 center{origin.x+available.x*.5f,origin.y+std::max(0.f,(available.y-groupHeight)*.42f)+artHeight*.5f};
    auto* draw=ImGui::GetWindowDrawList();
    const float gameAlpha=SplashExit.Active?exit.GameOpacity:1-blend,runeAlpha=SplashExit.Active?exit.RuneOpacity:(!BrandTexture&&!GameBrandTexture?1.f:blend);
    if(GameBrandTexture&&gameAlpha>0){using G=HelpyBranding::GameWordmark;const float width=std::min(theme.X(540),available.x*.84f),height=width*G::Aspect;const ImVec2 a{center.x-width*.5f,center.y-height*.5f},b{center.x+width*.5f,center.y+height*.5f};
        const auto color=ImGui::GetColorU32(ImVec4(1,1,1,gameAlpha*(SplashExit.Active?exit.Fade:1)));
        if(SplashExit.Active&&SplashExit.Kind==HelpySplash::ExitKind::Dragonwilds){
            const float seam=a.y+height*(G::Split-G::Top)/(G::Bottom-G::Top);
            const float up=(seam-MainSurfacePosition.y+theme.Y(24))*exit.Travel,down=(MainSurfacePosition.y+MainSurfaceSize.y-seam+theme.Y(24))*exit.Travel;
            draw->AddImage(ImTextureRef(GameBrandTexture),{a.x,a.y-up},{b.x,seam-up},{G::Left,G::Top},{G::Right,G::Split},color);
            draw->AddImage(ImTextureRef(GameBrandTexture),{a.x,seam+down},{b.x,b.y+down},{G::Left,G::Split},{G::Right,G::Bottom},color);
        }else draw->AddImage(ImTextureRef(GameBrandTexture),a,b,{G::Left,G::Top},{G::Right,G::Bottom},color);
    }
    if((BrandTexture||!GameBrandTexture)&&runeAlpha>0){const float size=artHeight;const ImVec2 a{center.x-size*.5f,center.y-size*.5f},b{a.x+size,a.y+size};
        if(SplashExit.Active&&SplashExit.Kind==HelpySplash::ExitKind::RuneSchema&&SplashSprites[0].Texture&&SplashSprites[1].Texture&&SplashSprites[2].Texture){
            DrawRuneExit(draw,a,b,theme,exit,runeAlpha);
        }else DrawThemedLogo(draw,a,b,theme,runeAlpha*(SplashExit.Active?exit.Fade:1));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*(SplashExit.Active?exit.Fade:1));
    ImGui::SetCursorScreenPos({origin.x,center.y+artHeight*.5f+logoGap});
    RecipeStudioPanel::CenterText("Welcome to RuneSchema");ImGui::Dummy({1,buttonGap});
    const float buttonWidth=std::min(theme.X(190),available.x*.65f);ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(available.x-buttonWidth)*.5f);
    ImGui::BeginDisabled(SplashExit.Active);const bool entered=HelpyImGuiTheme::SoftButton("Enter",{buttonWidth,buttonHeight});ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SplashEnter=HelpyPreviewHooks::LastRect();
#endif
    ImGui::SetItemDefaultFocus();
    ImGui::Dummy({1,buttonGap});
    const auto tip=HelpySplash::Tips[HelpySplash::TipIndex(elapsed)];
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SplashTipIndex=HelpySplash::TipIndex(elapsed);
#endif
    // Center each wrapped line, not merely the fixed-width wrapping rectangle.
    const auto centeredText=[&](const char* text,bool recordTip=false,float minimumHeight=0.f){
        const float wrap=std::min(theme.X(600),available.x*.9f),fontSize=ImGui::GetFontSize();
        const auto start=ImGui::GetCursorScreenPos();const char* cursor=text;const char* end=text+std::strlen(text);float y=start.y;
        while(cursor<end){
            const char* lineEnd=ImGui::GetFont()->CalcWordWrapPosition(fontSize,cursor,end,wrap);
            if(lineEnd==cursor)lineEnd=cursor+1;
            const float width=ImGui::CalcTextSize(cursor,lineEnd).x;
            const ImVec2 at{origin.x+(available.x-width)*.5f,y};
            draw->AddText(ImGui::GetFont(),fontSize,at,ImGui::GetColorU32(theme.textDisabled),cursor,lineEnd);
#if defined(HELPY_PREVIEW)
            if(recordTip)HelpyPreviewHooks::SplashTipLines.push_back({at.x,at.y,at.x+width,at.y+fontSize});
#endif
            y+=ImGui::GetTextLineHeightWithSpacing();cursor=lineEnd;
            while(cursor<end&&(*cursor==' '||*cursor=='\t'||*cursor=='\n'||*cursor=='\r'))++cursor;
        }
        ImGui::SetCursorScreenPos({origin.x,start.y});ImGui::Dummy({available.x,std::max(minimumHeight,y-start.y)});
    };
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SplashTipLines.clear();HelpyPreviewHooks::SplashCenterX=center.x;HelpyPreviewHooks::ArtworkHintVisible=downloadHint;
#endif
    centeredText(tip,true,tipsHeight);
    if(downloadHint){
        centeredText("Download initial UI artwork and map filters (optional).");
        const auto centeredButton=[&](const char* label){const float width=ImGui::CalcTextSize(label).x+ImGui::GetStyle().FramePadding.x*2;ImGui::SetCursorScreenPos({center.x-width*.5f,ImGui::GetCursorScreenPos().y});return HelpyImGuiTheme::SoftButton(label,{width,0});};
        ImGui::BeginDisabled(InitialAssetsBusy());
        if(centeredButton("Download Initial Assets"))DownloadInitialAssets();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::ArtworkDownload=HelpyPreviewHooks::LastRect();
#endif
        ImGui::EndDisabled();
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))PS::HelpyImGuiTheme::Tooltip("Downloads UI PNGs from GitHub and licensed Wiki/Wildsforge map markers into RuneSchema AppData. Both splash logos are bundled. No game/player data is sent. Cached map filters refresh only after the runtime confirms an installed game build change; no timed online polls. Item icon caches are separate. Also in Settings > Updates & runtime > Initial assets.");
        if(centeredButton("Hide this prompt")){
            HelpyDownloads::DismissHint();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::ForceArtworkHint=false;
#endif
            PushToast("Prompt hidden. Downloads remain in Settings > Updates & runtime > Initial assets.",8.0);
        }
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::ArtworkHide=HelpyPreviewHooks::LastRect();
#endif
        if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("Hide this splash prompt. Downloads remain available in Settings > Updates & runtime > Initial assets.");
        if(HelpyDownloads::Busy()){const auto progress="Artwork: "+std::to_string(HelpyDownloads::Completed())+" / "+std::to_string(HelpyDownloads::Total())+" files";centeredText(progress.c_str());}
        else if(!downloads.Status.empty())centeredText(downloads.Status.c_str());
        if(HelpyMapCache::Busy())centeredText(HelpyMapCache::Current().Status.c_str());
    }
    const auto tipBottom=ImGui::GetCursorScreenPos().y;
    const float remaining=origin.y+available.y-tipBottom;
    if(remaining>=theme.Y(48)){
        const float logicalSize=std::min(64.f,(remaining-theme.Y(16))/std::max(.01f,theme.Scale()*theme.density));
        const auto badge=RuntimeBadgeSize(theme,logicalSize);
        ImGui::SetCursorScreenPos({center.x-badge.x*.5f,tipBottom+(remaining-badge.y)*.5f});RenderRuntimeBadge(theme,logicalSize);
    }
    ImGui::PopStyleVar();
    if(!SplashExit.Active&&(entered||(!RuntimeDetailsPanel::Open&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Enter,false))))SplashExit.Begin(now,blend);
    if(SplashExit.Active&&SplashExit.At(now,animate!=FALSE).Finished){SplashEntered=true;CurrentPage=Page::Home;HomeRevealStarted=animate?now:now-HelpySplash::RevealSeconds;}
}

void ApplyCaptionTheme(HWND window,const HelpyImGuiTheme::Theme& theme,bool force=false) {
    const auto rgb=[](const ImVec4& color){return RGB(static_cast<BYTE>(color.x*255),static_cast<BYTE>(color.y*255),static_cast<BYTE>(color.z*255));};
    const BOOL dark=(theme.window.x+theme.window.y+theme.window.z)<1.5f;
    const COLORREF caption=rgb(theme.window),text=rgb(theme.text),border=rgb(theme.border);
    struct Caption {COLORREF Color,Text,Border;BYTE Opacity;BOOL Dark;bool operator==(const Caption&)const=default;};
    static std::unordered_map<HWND,Caption> captions;
    const auto opacity=static_cast<BYTE>((HelpyBackgrounds::Opaque()?1.0f:std::clamp(theme.windowOpacity,0.60f,1.0f))*255);
    const Caption next{caption,text,border,opacity,dark};
    if(const auto found=captions.find(window);!force&&found!=captions.end()&&found->second==next)return;
    std::erase_if(captions,[](const auto& value){return !IsWindow(value.first);});captions.insert_or_assign(window,next);
    SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_LAYERED);
    SetLayeredWindowAttributes(window,0,opacity,LWA_ALPHA);
    DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    DwmSetWindowAttribute(window,35,&caption,sizeof(caption));
    DwmSetWindowAttribute(window,36,&text,sizeof(text));
    DwmSetWindowAttribute(window,34,&border,sizeof(border));
}

struct UserColorPreset { std::string Name; nlohmann::json Colors; };

void CopyThemeColors(HelpyImGuiTheme::Theme& dst,const HelpyImGuiTheme::Theme& src) {
#define COPY_COLOR(name) dst.name=src.name
    COPY_COLOR(window);COPY_COLOR(panel);COPY_COLOR(panelAlt);COPY_COLOR(navigation);COPY_COLOR(navigationHover);COPY_COLOR(navigationActive);
    COPY_COLOR(accent);COPY_COLOR(accentHover);COPY_COLOR(accentActive);COPY_COLOR(onAccent);COPY_COLOR(text);COPY_COLOR(textDisabled);
    COPY_COLOR(control);COPY_COLOR(controlHover);COPY_COLOR(controlActive);COPY_COLOR(button);COPY_COLOR(buttonHover);COPY_COLOR(buttonActive);
    COPY_COLOR(selection);COPY_COLOR(border);COPY_COLOR(separator);COPY_COLOR(scrollbarTrack);COPY_COLOR(scrollbar);COPY_COLOR(scrollbarHover);COPY_COLOR(scrollbarActive);
    COPY_COLOR(icon);COPY_COLOR(success);COPY_COLOR(warning);COPY_COLOR(error);COPY_COLOR(info);COPY_COLOR(scrollFade);
#undef COPY_COLOR
}
nlohmann::json ThemeColorsJson(const HelpyImGuiTheme::Theme& t) {
    using HelpyImGuiTheme::ColorJson;
    return {
        {"window",ColorJson(t.window)},{"panel",ColorJson(t.panel)},{"panelAlt",ColorJson(t.panelAlt)},
        {"navigation",ColorJson(t.navigation)},{"navigationHover",ColorJson(t.navigationHover)},{"navigationActive",ColorJson(t.navigationActive)},
        {"accent",ColorJson(t.accent)},{"accentHover",ColorJson(t.accentHover)},{"accentActive",ColorJson(t.accentActive)},{"onAccent",ColorJson(t.onAccent)},
        {"text",ColorJson(t.text)},{"textDisabled",ColorJson(t.textDisabled)},
        {"control",ColorJson(t.control)},{"controlHover",ColorJson(t.controlHover)},{"controlActive",ColorJson(t.controlActive)},
        {"button",ColorJson(t.button)},{"buttonHover",ColorJson(t.buttonHover)},{"buttonActive",ColorJson(t.buttonActive)},
        {"selection",ColorJson(t.selection)},{"border",ColorJson(t.border)},{"separator",ColorJson(t.separator)},
        {"scrollbarTrack",ColorJson(t.scrollbarTrack)},{"scrollbar",ColorJson(t.scrollbar)},{"scrollbarHover",ColorJson(t.scrollbarHover)},{"scrollbarActive",ColorJson(t.scrollbarActive)},
        {"icon",ColorJson(t.icon)},{"success",ColorJson(t.success)},{"warning",ColorJson(t.warning)},{"error",ColorJson(t.error)},{"info",ColorJson(t.info)},{"scrollFade",ColorJson(t.scrollFade)}
    };
}
void ApplyThemeColorsJson(HelpyImGuiTheme::Theme& t,const nlohmann::json& c) {
    if(!c.contains("onAccent"))t.onAccent=HelpyImGuiTheme::ReadColor(c,"text",t.text);
#define READ_COLOR(name) t.name=HelpyImGuiTheme::ReadColor(c,#name,t.name)
    READ_COLOR(window);READ_COLOR(panel);READ_COLOR(panelAlt);READ_COLOR(navigation);READ_COLOR(navigationHover);READ_COLOR(navigationActive);
    READ_COLOR(accent);READ_COLOR(accentHover);READ_COLOR(accentActive);READ_COLOR(onAccent);READ_COLOR(text);READ_COLOR(textDisabled);
    READ_COLOR(control);READ_COLOR(controlHover);READ_COLOR(controlActive);READ_COLOR(button);READ_COLOR(buttonHover);READ_COLOR(buttonActive);
    READ_COLOR(selection);READ_COLOR(border);READ_COLOR(separator);READ_COLOR(scrollbarTrack);READ_COLOR(scrollbar);READ_COLOR(scrollbarHover);READ_COLOR(scrollbarActive);
    READ_COLOR(icon);READ_COLOR(success);READ_COLOR(warning);READ_COLOR(error);READ_COLOR(info);READ_COLOR(scrollFade);
#undef READ_COLOR
}
std::filesystem::path ThemePresetPath() {
    return HelpyImGuiTheme::FilePath().parent_path()/"theme-presets.jsonc";
}
std::vector<UserColorPreset> LoadUserColorPresets() {
    std::vector<UserColorPreset> result;const auto file=ThemePresetPath();
    try {
        if(!std::filesystem::is_regular_file(file))return result;
        const auto doc=nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true);
        const auto entries=doc.value("presets",nlohmann::json::array());if(!entries.is_array())return result;
        for(const auto& entry:entries) {
            if(!entry.is_object())continue;const auto name=entry.value("name",std::string{});
            const auto colors=entry.value("colors",nlohmann::json::object());
            if(!name.empty()&&name.size()<=64&&colors.is_object())result.push_back({name,colors});
        }
    } catch(...) {}
    return result;
}
bool SaveUserColorPresets(const std::vector<UserColorPreset>& presets) {
    try {
        nlohmann::json entries=nlohmann::json::array();
        for(const auto& p:presets)entries.push_back({{"name",p.Name},{"colors",p.Colors}});
        ConfigFiles::Write(ThemePresetPath(),"// RSMK user color presets.\n"+nlohmann::json{{"version",1},{"presets",entries}}.dump(2)+"\n");
        return true;
    } catch(...) {return false;}
}
std::string TrimPresetName(std::string name) {
    const auto first=name.find_first_not_of(" \t\r\n");if(first==std::string::npos)return {};
    const auto last=name.find_last_not_of(" \t\r\n");name=name.substr(first,last-first+1);if(name.size()>48)name.resize(48);return name;
}
constexpr const char* BuiltinColorPresetNames[]={
    "OldSchool RuneScape","RuneSchema .7","Dark Mode","Light Mode",
    "Cyberpunk","Light Sky","Dark Sky","Mid Sky","Dragonwilds Sync"
};
HelpyImGuiTheme::Theme BuiltinColorPreset(int index) {
    using T=HelpyImGuiTheme::Theme;T t{};
    const auto C=[](float r,float g,float b,float a=1.0f){return ImVec4(r,g,b,a);};
    switch(index) {
    case 0:
        t.window=C(0x2E/255.0f,0x2C/255.0f,0x29/255.0f);t.panel=C(0x46/255.0f,0x43/255.0f,0x3A/255.0f);t.panelAlt=C(0x3E/255.0f,0x35/255.0f,0x30/255.0f);t.navigation=C(0x2A/255.0f,0x28/255.0f,0x24/255.0f);
        t.navigationHover=C(0x52/255.0f,0x49/255.0f,0x3F/255.0f);t.navigationActive=C(0x6B/255.0f,0x58/255.0f,0x40/255.0f);t.accent=C(0xD8/255.0f,0xA8/255.0f,0x1E/255.0f);t.accentHover=C(0xE8/255.0f,0xBE/255.0f,0x4A/255.0f);t.accentActive=C(0xB7/255.0f,0x86/255.0f,0x16/255.0f);
        t.text=C(0xF1/255.0f,0xCC/255.0f,0x4A/255.0f);t.textDisabled=C(0xA9/255.0f,0x93/255.0f,0x61/255.0f);t.control=C(0x3E/255.0f,0x35/255.0f,0x30/255.0f);t.controlHover=C(0x52/255.0f,0x49/255.0f,0x3F/255.0f);t.controlActive=C(0x6B/255.0f,0x58/255.0f,0x40/255.0f);
        t.button=C(0x3E/255.0f,0x35/255.0f,0x30/255.0f);t.buttonHover=C(0x6B/255.0f,0x58/255.0f,0x40/255.0f);t.buttonActive=C(0x7B/255.0f,0x67/255.0f,0x4B/255.0f);t.selection=C(0xC4/255.0f,0xB4/255.0f,0x8C/255.0f,0.28f);t.border=C(0x47/255.0f,0x47/255.0f,0x45/255.0f);t.separator=C(0x1C/255.0f,0x16/255.0f,0x12/255.0f);
        t.scrollbarTrack=C(0x2A/255.0f,0x28/255.0f,0x24/255.0f);t.scrollbar=C(0x47/255.0f,0x47/255.0f,0x45/255.0f);t.scrollbarHover=C(0x6B/255.0f,0x58/255.0f,0x40/255.0f);t.scrollbarActive=C(0xC4/255.0f,0xB4/255.0f,0x8C/255.0f);t.icon=C(0xE0/255.0f,0xB1/255.0f,0x2F/255.0f);
        t.success=C(0x5E/255.0f,0xA8/255.0f,0x64/255.0f);t.warning=C(0xE0/255.0f,0xB1/255.0f,0x2F/255.0f);t.error=C(0xBF/255.0f,0x55/255.0f,0x4C/255.0f);t.info=C(0x7E/255.0f,0xA8/255.0f,0xC9/255.0f);t.scrollFade=C(0x2E/255.0f,0x2C/255.0f,0x29/255.0f,.88f);break;
    case 1: ApplyThemeColorsJson(t,HelpyImGuiTheme::LegacyDefaultDocument().at("colors"));break;
    case 2:
        t.window=C(.025f,.027f,.029f);t.panel=C(.040f,.043f,.046f);t.panelAlt=C(.058f,.061f,.064f);t.navigation=C(.031f,.033f,.035f);
        t.navigationHover=C(.090f,.098f,.094f);t.navigationActive=C(.115f,.128f,.120f);t.accent=C(.330f,.470f,.390f);t.accentHover=C(.405f,.560f,.470f);t.accentActive=C(.260f,.390f,.320f);
        t.text=C(.790f,.800f,.770f);t.textDisabled=C(.455f,.465f,.445f);t.control=C(.075f,.080f,.082f);t.controlHover=C(.105f,.112f,.112f);t.controlActive=C(.135f,.145f,.142f);
        t.button=C(.085f,.092f,.092f);t.buttonHover=C(.210f,.300f,.250f);t.buttonActive=C(.175f,.250f,.210f);t.selection=C(.170f,.255f,.210f);t.border=C(.145f,.150f,.148f);t.separator=C(.110f,.115f,.113f);
        t.scrollbarTrack=C(.030f,.032f,.033f);t.scrollbar=C(.125f,.132f,.130f);t.scrollbarHover=C(.210f,.265f,.235f);t.scrollbarActive=C(.275f,.350f,.305f);t.icon=C(.610f,.665f,.625f);
        t.success=C(.350f,.620f,.410f);t.warning=C(.780f,.610f,.300f);t.error=C(.720f,.350f,.350f);t.info=C(.390f,.570f,.650f);t.scrollFade=C(.025f,.027f,.029f,.90f);break;
    case 3:
        t.window=C(.930f,.935f,.945f);t.panel=C(.975f,.978f,.984f);t.panelAlt=C(.895f,.905f,.920f);t.navigation=C(.865f,.875f,.895f);
        t.navigationHover=C(.775f,.800f,.850f);t.navigationActive=C(.675f,.715f,.805f);t.accent=C(.355f,.315f,.650f);t.accentHover=C(.430f,.390f,.760f);t.accentActive=C(.285f,.250f,.555f);
        t.text=C(.115f,.120f,.145f);t.textDisabled=C(.415f,.425f,.465f);t.control=C(.845f,.855f,.875f);t.controlHover=C(.795f,.810f,.840f);t.controlActive=C(.735f,.755f,.800f);
        t.button=C(.820f,.830f,.855f);t.buttonHover=C(.665f,.625f,.860f);t.buttonActive=C(.545f,.500f,.765f);t.selection=C(.700f,.675f,.875f,.75f);t.border=C(.655f,.670f,.710f);t.separator=C(.760f,.770f,.795f);
        t.scrollbarTrack=C(.885f,.892f,.905f);t.scrollbar=C(.650f,.665f,.700f);t.scrollbarHover=C(.500f,.485f,.675f);t.scrollbarActive=C(.405f,.380f,.610f);t.icon=C(.305f,.300f,.400f);
        t.success=C(.195f,.560f,.315f);t.warning=C(.735f,.500f,.110f);t.error=C(.745f,.245f,.270f);t.info=C(.190f,.475f,.735f);t.scrollFade=C(.930f,.935f,.945f,.88f);break;
    case 4:
        t.window=C(.018f,.020f,.045f);t.panel=C(.028f,.034f,.070f);t.panelAlt=C(.045f,.050f,.095f);t.navigation=C(.020f,.023f,.055f);
        t.navigationHover=C(.095f,.055f,.145f);t.navigationActive=C(.175f,.040f,.215f);t.accent=C(.000f,.880f,.950f);t.accentHover=C(.250f,.980f,1.000f);t.accentActive=C(.000f,.630f,.720f);
        t.text=C(.900f,.950f,.975f);t.textDisabled=C(.495f,.555f,.625f);t.control=C(.060f,.070f,.120f);t.controlHover=C(.095f,.110f,.180f);t.controlActive=C(.145f,.080f,.220f);
        t.button=C(.070f,.075f,.135f);t.buttonHover=C(.835f,.040f,.645f);t.buttonActive=C(.600f,.020f,.470f);t.selection=C(.220f,.060f,.350f,.90f);t.border=C(.120f,.520f,.610f);t.separator=C(.090f,.260f,.330f);
        t.scrollbarTrack=C(.020f,.025f,.055f);t.scrollbar=C(.080f,.300f,.370f);t.scrollbarHover=C(.000f,.650f,.740f);t.scrollbarActive=C(.850f,.055f,.660f);t.icon=C(.100f,.900f,.950f);
        t.success=C(.150f,.950f,.545f);t.warning=C(.970f,.820f,.120f);t.error=C(.980f,.145f,.390f);t.info=C(.050f,.780f,1.000f);t.scrollFade=C(.018f,.020f,.045f,.90f);break;
    case 5:
        t.window=C(.855f,.925f,.975f);t.panel=C(.925f,.960f,.988f);t.panelAlt=C(.790f,.885f,.950f);t.navigation=C(.755f,.860f,.935f);
        t.navigationHover=C(.640f,.795f,.910f);t.navigationActive=C(.505f,.700f,.865f);t.accent=C(.160f,.545f,.825f);t.accentHover=C(.245f,.650f,.920f);t.accentActive=C(.105f,.430f,.700f);
        t.text=C(.090f,.185f,.260f);t.textDisabled=C(.350f,.470f,.550f);t.control=C(.760f,.865f,.935f);t.controlHover=C(.690f,.825f,.915f);t.controlActive=C(.590f,.755f,.875f);
        t.button=C(.705f,.835f,.925f);t.buttonHover=C(.365f,.690f,.900f);t.buttonActive=C(.225f,.565f,.820f);t.selection=C(.440f,.745f,.925f,.72f);t.border=C(.455f,.675f,.805f);t.separator=C(.620f,.790f,.885f);
        t.scrollbarTrack=C(.825f,.900f,.950f);t.scrollbar=C(.505f,.705f,.830f);t.scrollbarHover=C(.300f,.610f,.820f);t.scrollbarActive=C(.185f,.505f,.750f);t.icon=C(.190f,.470f,.690f);
        t.success=C(.165f,.585f,.370f);t.warning=C(.800f,.570f,.130f);t.error=C(.780f,.275f,.310f);t.info=C(.105f,.510f,.825f);t.scrollFade=C(.855f,.925f,.975f,.88f);break;
    case 6:
        t.window=C(.025f,.055f,.090f);t.panel=C(.040f,.080f,.125f);t.panelAlt=C(.060f,.110f,.165f);t.navigation=C(.030f,.065f,.105f);
        t.navigationHover=C(.080f,.170f,.250f);t.navigationActive=C(.090f,.235f,.355f);t.accent=C(.190f,.610f,.900f);t.accentHover=C(.320f,.735f,.985f);t.accentActive=C(.120f,.480f,.760f);
        t.text=C(.880f,.935f,.970f);t.textDisabled=C(.480f,.590f,.675f);t.control=C(.070f,.125f,.180f);t.controlHover=C(.100f,.175f,.245f);t.controlActive=C(.115f,.220f,.320f);
        t.button=C(.075f,.145f,.210f);t.buttonHover=C(.175f,.525f,.790f);t.buttonActive=C(.115f,.415f,.670f);t.selection=C(.110f,.330f,.500f);t.border=C(.120f,.280f,.390f);t.separator=C(.080f,.205f,.295f);
        t.scrollbarTrack=C(.020f,.050f,.080f);t.scrollbar=C(.100f,.245f,.335f);t.scrollbarHover=C(.135f,.410f,.565f);t.scrollbarActive=C(.180f,.565f,.755f);t.icon=C(.480f,.770f,.930f);
        t.success=C(.300f,.730f,.500f);t.warning=C(.930f,.700f,.245f);t.error=C(.900f,.340f,.390f);t.info=C(.285f,.675f,.950f);t.scrollFade=C(.025f,.055f,.090f,.90f);break;
    case 7:
        t.window=C(.170f,.285f,.390f);t.panel=C(.205f,.335f,.450f);t.panelAlt=C(.245f,.385f,.505f);t.navigation=C(.145f,.250f,.345f);
        t.navigationHover=C(.260f,.440f,.575f);t.navigationActive=C(.285f,.510f,.675f);t.accent=C(.390f,.740f,.955f);t.accentHover=C(.535f,.825f,1.000f);t.accentActive=C(.285f,.620f,.875f);
        t.text=C(.925f,.960f,.980f);t.textDisabled=C(.650f,.735f,.790f);t.control=C(.255f,.400f,.520f);t.controlHover=C(.300f,.465f,.600f);t.controlActive=C(.340f,.525f,.670f);
        t.button=C(.270f,.425f,.550f);t.buttonHover=C(.350f,.650f,.845f);t.buttonActive=C(.270f,.545f,.760f);t.selection=C(.315f,.575f,.745f);t.border=C(.380f,.540f,.650f);t.separator=C(.310f,.455f,.565f);
        t.scrollbarTrack=C(.145f,.255f,.350f);t.scrollbar=C(.330f,.495f,.610f);t.scrollbarHover=C(.390f,.635f,.790f);t.scrollbarActive=C(.455f,.735f,.900f);t.icon=C(.650f,.845f,.950f);
        t.success=C(.440f,.790f,.570f);t.warning=C(.965f,.765f,.350f);t.error=C(.940f,.450f,.475f);t.info=C(.505f,.790f,.980f);t.scrollFade=C(.170f,.285f,.390f,.88f);break;
    default: break;
    }
    return t;
}

void RenderAuthorToolsDownload(){
    ImGui::SeparatorText("Authoring tools");
    ImGui::TextWrapped("Download the cache builders and instructions. Drop your mod ZIP onto Build-Helpy-Cache.cmd for a progress-guided Windows build, or use the Bash script. Keep the target game closed.");
    ImGui::BeginDisabled(HelpyDownloads::Busy());
    if(HelpyImGuiTheme::SoftButton("Download authoring tools ZIP"))HelpyDownloads::StartAuthorTools();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthorToolsDownload=HelpyPreviewHooks::LastRect();
#endif
    ImGui::EndDisabled();
    if(ImGui::GetContentRegionAvail().x>HelpyImGuiTheme::Current.X(420))ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Open authoring-tools folder")){
        const auto folder=HelpyDownloads::AuthorToolsFolder();std::error_code error;std::filesystem::create_directories(folder,error);
        if(!error)ShellExecuteW(nullptr,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        else PushToast("Could not open the authoring-tools folder: "+error.message(),4);
    }
    ImGui::TextLinkOpenURL("Usage walkthrough","https://gh0sted5456-us.github.io/RuneSchema/modders/offline-helpy-cache/");
    if(HelpyDownloads::Busy()){
        ImGui::TextDisabled("Downloads: %u / %u",HelpyDownloads::Completed(),HelpyDownloads::Total());ImGui::SameLine();
        if(ImGui::SmallButton("Cancel download"))HelpyDownloads::Cancel();
    }
    if(!HelpyDownloads::Current().Status.empty())ImGui::TextWrapped("%s",HelpyDownloads::Current().Status.c_str());
    if(!HelpyDownloads::Current().AuthorTools.empty())ImGui::TextDisabled("Saved: RuneSchema-Authoring-Tools.zip");
    ImGui::Spacing();
}
void RenderIconCacheSettings(bool advanced,bool expanded=false) {
    if(!expanded&&!ImGui::CollapsingHeader("Item cache"))return;
    CatalogProgressView::Render(SpawnToolsPanel::UiSnapshot);
    if(ImGui::Button("Open cache window"))HelpyIconPacks::Open=true;
    if(HelpyIconPacks::Advanced){
    static bool acknowledgeFullScan=false;
    ImGui::Checkbox("Allow full scan to load additional candidate assets",&acknowledgeFullScan);
    ImGui::BeginDisabled(!acknowledgeFullScan||HelpyIconPacks::Busy()||HelpyIconPacks::Building()||SpawnToolRequests::IsWaiting()||!SpawnToolRequests::IsAvailable()||SpawnToolsPanel::UiSnapshot.value("_CatalogIndexing",false));
    if(ImGui::Button("Full scan game catalogue")){if(SpawnToolRequests::TrySubmit({{"Action","IndexQuickCatalog"},{"_Source","HelpySettings"}}))acknowledgeFullScan=false;}
    ImGui::EndDisabled();
    ImGui::TextWrapped("Full scan discovers catalogue entries. Use Build & export game icons below for a portable, pre-mapped thumbnail pack, or import a shared pack to avoid recapturing its icons.");
    }
    bool persistent=HelpyRuntimePreferences::IconCacheLimitMB.load()>0;
    ImGui::BeginDisabled(HelpyIconPacks::Building()||HelpyIconPacks::Busy());
    if(ImGui::Checkbox("Keep icons between launches",&persistent)){HelpyRuntimePreferences::IconCacheLimitMB.store(persistent?200:0);HelpyImGuiTheme::SaveCurrent();}
    if(advanced&&persistent){int megabytes=HelpyRuntimePreferences::IconCacheLimitMB.load();
        if(ImGui::InputInt("Cache limit (MB)",&megabytes,16,128)){HelpyRuntimePreferences::IconCacheLimitMB.store(std::clamp(megabytes,1,16384));HelpyImGuiTheme::SaveCurrent();}}
    ImGui::EndDisabled();
    static ItemIconCache::DiskStats stats{};static PS::ProcessExitFuture<ItemIconCache::DiskStats> statsWorker;static bool measured=false;static std::filesystem::path measuredFolder;
    const auto folder=ItemIconCache::ActiveDiskFolder();if(folder!=measuredFolder){measured=false;stats={};}
    if(statsWorker.valid()&&statsWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{stats=statsWorker.get();measured=folder==measuredFolder;}catch(...){measured=false;}}
    if(measured)ImGui::Text("%.2f MB / %zu cached icons",static_cast<double>(stats.Bytes)/(1024.0*1024.0),stats.Files);else ImGui::TextDisabled(statsWorker.valid()?"Measuring cache usage…":"Cache usage not measured");
    ImGui::SameLine();ImGui::BeginDisabled(statsWorker.valid());
    if(HelpyImGuiTheme::SoftButton("Refresh usage")){measuredFolder=folder;statsWorker=std::async(std::launch::async,[folder]{return ItemIconCache::ScanDiskStats(folder);});}ImGui::EndDisabled();
    HelpyIconPacks::Render(SpawnToolsPanel::UiSnapshot.value("_CatalogIndexing",false));
    ImGui::BeginDisabled(HelpyIconPacks::Busy()||HelpyIconPacks::Building());
    if(HelpyImGuiTheme::SoftButton("Clean Icon Cache")){const bool ok=ItemIconCache::CleanDiskCache();stats={};measured=false;measuredFolder.clear();PushToast(ok?"Icon cache cleaned. Visible icons can be cached again.":"Some cache files could not be removed.",3.0);}
    ImGui::EndDisabled();
    ImGui::SameLine();if(ImGui::TextLink("Open cache folder")){
        const auto folder=HostServices::CacheDirectory();std::error_code error;std::filesystem::create_directories(folder,error);
        if(!error)ShellExecuteW(nullptr,L"explore",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        else PushToast("The cache folder could not be opened.",3.0);
    }
}
void RenderUpdatesAndRuntime(const HelpyImGuiTheme::Theme& t) {
    using namespace RuneSchemaUpdateInfo;
    const auto& state=HelpyUpdates::Current();auto& theme=HelpyImGuiTheme::Current;
    ImGui::SeparatorText("Initial assets");
    ImGui::BeginDisabled(InitialAssetsBusy());
    if(HelpyImGuiTheme::SoftButton("Download Initial Assets"))DownloadInitialAssets();
    ImGui::EndDisabled();
    if(HelpyDownloads::Busy()||HelpyMapCache::Busy()){ImGui::SameLine();if(ImGui::SmallButton("Cancel initial-assets download")){HelpyDownloads::Cancel();HelpyMapCache::Cancel();}}
    ImGui::TextWrapped("Optional UI artwork from GitHub and map filters from Wiki/Wildsforge, cached in RuneSchema AppData. Both splash logos are bundled. Item icon caches are separate. Existing map filters are retained unless a local installed game build change is confirmed.");
    auto& mapAssets=HelpyMapCache::Current();
    if(ImGui::Checkbox("Update map filters after confirmed game patches",&mapAssets.Filters.UpdateAfterGamePatch))HelpyMapCache::SaveFilters();
    if(!mapAssets.Status.empty())ImGui::TextWrapped("%s",mapAssets.Status.c_str());
    if(!HelpyDownloads::Current().Status.empty())ImGui::TextWrapped("%s",HelpyDownloads::Current().Status.c_str());
    ImGui::SeparatorText("RuneSchema updates");
    ImGui::Text("Installed: %s",HelpyEnvironment::RuneSchemaVersion.empty()?"Not reported":HelpyEnvironment::RuneSchemaVersion.c_str());
    if(ImGui::Checkbox("Check automatically once a day when Helpy opens",&theme.automaticUpdateChecks)){
        if(!theme.automaticUpdateChecks)HelpyUpdates::Cancel();HelpyImGuiTheme::SaveCurrent();
    }
    ImGui::TextWrapped("Public GitHub versions and Steam update announcements. Checks run in the background; no game/player data is sent and no update files are downloaded or installed.");
    const auto row=[&](const char* source,const std::string& version,const std::string& error){
        ImGui::TextUnformatted(source);
        if(!error.empty())ImGui::TextColored(t.warning,"Unable to verify: %s",error.c_str());
        else if(version.empty())ImGui::TextDisabled("Not checked yet");
        else {const auto comparison=Compare(HelpyEnvironment::RuneSchemaVersion,version);
            ImGui::TextColored(comparison==Comparison::Newer?t.warning:t.text,"%s · %s",version.c_str(),Label(comparison));}
    };
    row("GitHub main branch",state.MainVersion,state.MainError);row("Latest published stable release",state.ReleaseVersion,state.ReleaseError);
    if(HelpyUpdates::Busy())ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),{-1,0},HelpyUpdates::Stage()<2?"Checking main branch...":HelpyUpdates::Stage()<3?"Checking latest release...":"Checking Steam update news...");
    ImGui::BeginDisabled(HelpyUpdates::Busy()||HelpyUpdates::CooldownSeconds()>0||!HelpyEnvironment::UpdateChecksAvailable);
    if(HelpyImGuiTheme::SoftButton("Check now"))HelpyUpdates::Start(HelpyEnvironment::RuneSchemaVersion);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&HelpyUpdates::CooldownSeconds()>0)PS::HelpyImGuiTheme::Tooltip("Next manual check in %d seconds",HelpyUpdates::CooldownSeconds());
    const auto link=[&](const char* label,const wchar_t* url){if(HelpyImGuiTheme::SoftButton(label))ShellExecuteW(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);};
    ImGui::SameLine();link("GitHub releases",L"https://github.com/gh0sted5456-us/RuneSchema/releases/latest");
    ImGui::SameLine();link("Main branch",L"https://github.com/gh0sted5456-us/RuneSchema/tree/main");
    link("Nexus Mods",L"https://www.nexusmods.com/runescapedragonwilds/mods/570");
    ImGui::TextWrapped("Nexus version comparison opens the listing in your browser. Its login/anti-bot protections are not bypassed; automatic checks use GitHub.");
    if(state.CheckedAt){const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        ImGui::TextDisabled("Last check: %lld minute(s) ago · cached between launches",std::max<std::int64_t>(0,(now-state.CheckedAt)/60));}
    if(!state.CacheError.empty())ImGui::TextWrapped("%s",state.CacheError.c_str());
    ImGui::SeparatorText("Loaded UE4SS host");
    ImGui::TextWrapped("Build: %s",HelpyEnvironment::UE4SS.value("Build",std::string("Not reported")).c_str());
    ImGui::TextWrapped("Detection: %s",HelpyEnvironment::UE4SS.value("Source",std::string("Unavailable")).c_str());
    ImGui::TextWrapped("Capabilities: %s",HelpyEnvironment::UE4SS.value("Capabilities",std::string("Unavailable")).c_str());
    ImGui::TextWrapped("No build-number ceiling: newer hosts are not rejected by version. They must retain RuneSchema's required C++ exports and ABI. Missing optional capabilities use fallbacks; future breaking ABI changes may require a new RuneSchema build. UE4SS.dll is never replaced by this updater.");
}
void RenderDiagnosticsSettings(const HelpyImGuiTheme::Theme& t) {
    auto& theme=HelpyImGuiTheme::Current;
    ImGui::SeparatorText("Diagnostic notifications");
    bool changed=false;
    changed|=ImGui::Checkbox("Enable on-screen log toasts",&theme.diagnosticToastsEnabled);
    ImGui::TextDisabled("Includes emitted debug/diagnostic lines; does not enable advancedLogging.");
    ImGui::SetNextItemWidth(t.X(250));
    changed|=ImGui::SliderInt("Maximum visible toasts",&theme.diagnosticToastLimit,1,10);
    ImGui::SetNextItemWidth(t.X(250));
    changed|=ImGui::SliderInt("Toast duration (seconds)",&theme.diagnosticToastDuration,2,15);
    ImGui::TextWrapped("Repeated messages are grouped and overflowing notifications are summarized. Notifications while Helpy is closed remain click-through and do not steal game focus.");
    if(ImGui::SmallButton("Reset notification defaults")){
        const HelpyImGuiTheme::Theme defaults{};
        theme.diagnosticToastsEnabled=defaults.diagnosticToastsEnabled;
        theme.diagnosticToastLimit=defaults.diagnosticToastLimit;
        theme.diagnosticToastDuration=defaults.diagnosticToastDuration;
        changed=true;
    }
    ImGui::SeparatorText("Diagnostic presentation");
    changed|=ImGui::Checkbox("Open raw / DMP-style technical details by default",&theme.diagnosticDeveloperMode);
    ImGui::TextWrapped("Diagnostics panels use the shared Appearance spacing, colors and window/background opacity controls. Adjust padding or opacity in Settings > Appearance.");
    if(ImGui::SmallButton("Reset presentation defaults")){
        const HelpyImGuiTheme::Theme defaults{};
        theme.diagnosticDeveloperMode=defaults.diagnosticDeveloperMode;
        changed=true;
    }
    // Same keys in the same ui.jsonc file as M4. This is a relocation of the
    // controls, not a settings migration or a separate configuration source.
    static bool pendingSave=false;
    pendingSave|=changed;
    ImGui::SeparatorText("Diagnostic history");
    int days=theme.diagnosticHistoryDays,sessions=theme.diagnosticHistorySessions,mb=theme.diagnosticHistoryMB;
    ImGui::SetNextItemWidth(t.X(250));changed|=ImGui::SliderInt("Retain history (days)",&days,1,365);
    ImGui::SetNextItemWidth(t.X(250));changed|=ImGui::SliderInt("Saved sessions",&sessions,1,100);
    ImGui::SetNextItemWidth(t.X(250));changed|=ImGui::SliderInt("Maximum history storage (MB)",&mb,5,500);
    if(days!=theme.diagnosticHistoryDays||sessions!=theme.diagnosticHistorySessions||mb!=theme.diagnosticHistoryMB){
        theme.diagnosticHistoryDays=days;theme.diagnosticHistorySessions=sessions;theme.diagnosticHistoryMB=mb;
        DiagnosticHistory.SetRetention({days,sessions,mb});
        pendingSave=true;
    }
    ImGui::TextWrapped("The oldest saved sessions are pruned when any limit is reached. Original RuneSchema logs and mute rules are not deleted.");
    if(!DiagnosticHistory.Error().empty())ImGui::TextColored(t.warning,"%s",DiagnosticHistory.Error().c_str());
    if(ImGui::SmallButton("Clear saved sessions"))ImGui::OpenPopup("Delete diagnostic history?");
    if(HelpyImGuiTheme::BeginCenteredModal("Delete diagnostic history?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Delete previously saved diagnostic sessions? This does not delete RuneSchema.log or persistent mute rules.");
        if(ImGui::Button("Delete saved history")){DiagnosticHistory.ClearHistory();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SeparatorText("Muted notifications");
    const auto& rules=DiagnosticMutes.Rules();
    if(rules.empty())ImGui::TextDisabled("No muted diagnostic notifications.");
    for(std::size_t i=0;i<rules.size();++i){
        const auto& rule=rules[i];ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s - %s - %s | %llu suppressed",DiagnosticCenterMutes::Name(rule.Kind),
            LiveLog::Label(rule.Severity),DiagnosticCenterMutes::Name(rule.Lifetime),
            static_cast<unsigned long long>(rule.Suppressed));
        if(!rule.Label.empty())ImGui::TextWrapped("Matching: %s",rule.Label.c_str());
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Exact/category match and severity are preserved; logging is never silenced.");
        if(ImGui::SmallButton("Unmute")){DiagnosticMutes.Remove(i);ImGui::PopID();break;}
        ImGui::PopID();
    }
    if(!DiagnosticMutes.Error().empty())ImGui::TextColored(t.warning,"Mute settings: %s",DiagnosticMutes.Error().c_str());
    if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}
}
void RenderSettings(const HelpyImGuiTheme::Theme& t) {
    bool advanced=t.advancedMode;if(ImGui::Checkbox("Show advanced controls",&advanced)){HelpyImGuiTheme::Current.advancedMode=advanced;HelpyImGuiTheme::SaveCurrent();}
    ImGui::Separator();
    auto& theme=HelpyImGuiTheme::Current;static bool pendingSave=false;const HelpyImGuiTheme::Theme defaults{};
    const auto mix=[](const ImVec4& a,const ImVec4& b,float k){return ImVec4(a.x+(b.x-a.x)*k,a.y+(b.y-a.y)*k,a.z+(b.z-a.z)*k,a.w+(b.w-a.w)*k);};
    const auto lighter=[&](const ImVec4& c,float k){return mix(c,ImVec4(1,1,1,c.w),k);};
    const auto darker=[&](const ImVec4& c,float k){return ImVec4(c.x*(1.0f-k),c.y*(1.0f-k),c.z*(1.0f-k),c.w);};
    const auto changed=[&](){pendingSave=true;};
    const auto resetAllButton=[&](){ImGui::SameLine();if(ImGui::SmallButton("Reset all"))ImGui::OpenPopup("Reset RuneSchema UI?");};
    const auto sectionActions=[&](const char* id,const auto& resetSection){ImGui::PushID(id);if(ImGui::SmallButton("Reset section")){resetSection();changed();}resetAllButton();ImGui::PopID();};
    const auto optionReset=[&](const char* id,auto& value,const auto& def){ImGui::SameLine();ImGui::PushID(id);if(ImGui::SmallButton("Reset")){value=def;changed();}ImGui::PopID();};
    const auto slider=[&](const char* label,float& value,float lo,float hi,float def,const char* fmt="%.2f"){
        ImGui::PushID(label);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(t.X(220.0f));
        const float avail=ImGui::GetContentRegionAvail().x;ImGui::SetNextItemWidth(std::max(t.X(130.0f),avail-t.X(72.0f)));
        if(ImGui::SliderFloat("##value",&value,lo,hi,fmt,ImGuiSliderFlags_AlwaysClamp))changed();optionReset("value-reset",value,def);ImGui::PopID();
    };
    const auto sliderInt=[&](const char* label,int& value,int lo,int hi,int def){
        ImGui::PushID(label);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(t.X(220.0f));
        const float avail=ImGui::GetContentRegionAvail().x;ImGui::SetNextItemWidth(std::max(t.X(130.0f),avail-t.X(72.0f)));
        if(ImGui::SliderInt("##value",&value,lo,hi))changed();optionReset("value-reset",value,def);ImGui::PopID();
    };
    const auto checkbox=[&](const char* label,bool& value,bool def){ImGui::PushID(label);if(ImGui::Checkbox(label,&value))changed();optionReset("value-reset",value,def);ImGui::PopID();};
    const auto color=[&](const char* label,ImVec4& value,const ImVec4& def,bool* bold=nullptr,bool boldDef=false){
        ImGui::PushID(label);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(t.X(220.0f));
        float v[4]{value.x,value.y,value.z,value.w};ImGui::SetNextItemWidth(t.X(58.0f));
        constexpr ImGuiColorEditFlags compactColor=ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_AlphaPreviewHalf;
        if(ImGui::ColorEdit4("##value",v,compactColor)){value={v[0],v[1],v[2],v[3]};changed();}
        if(bold){ImGui::SameLine();if(ImGui::Checkbox("Bold",bold))changed();}
        optionReset("value-reset",value,def);
        if(bold){ImGui::SameLine();ImGui::PushID("bold-reset");if(ImGui::SmallButton("Reset bold")){*bold=boldDef;changed();}ImGui::PopID();}
        ImGui::PopID();
    };
    static std::string previewIconPath,previewItemName="Example item";static bool previewIconChosen=false;
    if(!previewIconChosen){const auto& rows=LazyCatalog::For(0).Rows;if(rows.is_array()&&!rows.empty()){std::vector<size_t> candidates;for(size_t i=0;i<rows.size();++i)if(!rows[i].value("Icon",std::string{}).empty())candidates.push_back(i);if(!candidates.empty()){static std::mt19937 rng{std::random_device{}()};const auto index=candidates[std::uniform_int_distribution<size_t>(0,candidates.size()-1)(rng)];previewIconPath=rows[index].value("Icon",std::string{});previewItemName=rows[index].value("Name",std::string("Example item"));previewIconChosen=true;}}}
    const auto drawPreviewIcon=[&](const ImVec2& a,const ImVec2& b){if(previewIconPath.empty()||!ItemIconCache::Draw(previewIconPath,a,b,true)){const auto c=ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f);HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Item,c,std::min(b.x-a.x,b.y-a.y)*0.72f,theme.icon);}};
    const auto sizingPreview=[&](){
        ImGui::PushID("sizing-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(168.0f)),true);ImGui::TextUnformatted("Sizing preview");ImGui::Separator();
        const float listIcon=t.Y(theme.listIconSize),rowH=std::max(listIcon,t.Y(32.0f));const auto row=ImGui::GetCursorScreenPos();const ImVec2 ia(row.x+t.X(6.0f),row.y+(rowH-listIcon)*.5f),ib(ia.x+listIcon,ia.y+listIcon);drawPreviewIcon(ia,ib);ImGui::SetCursorScreenPos(ImVec2(ib.x+t.X(8.0f),row.y+(rowH-ImGui::GetTextLineHeight())*.5f));ImGui::TextUnformatted(previewItemName.c_str());ImGui::SetCursorScreenPos(ImVec2(row.x,row.y+rowH+t.Y(8.0f)));
        const float cardW=std::min(t.X(theme.gridMinCardWidth),ImGui::GetContentRegionAvail().x),cardH=std::min(t.Y(theme.gridCardHeight),t.Y(88.0f));const auto a=ImGui::GetCursorScreenPos(),b=ImVec2(a.x+cardW,a.y+cardH);auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(a,b,ImGui::GetColorU32(theme.panelAlt),t.X(theme.childRounding));d->AddRect(a,b,ImGui::GetColorU32(theme.border),t.X(theme.childRounding));const float gridIcon=std::min(t.Y(theme.gridIconSize),cardH-t.Y(28.0f));const ImVec2 ga(a.x+(cardW-gridIcon)*.5f,a.y+t.Y(6.0f)),gb(ga.x+gridIcon,ga.y+gridIcon);drawPreviewIcon(ga,gb);const auto label=previewItemName.size()>22?previewItemName.substr(0,19)+"...":previewItemName;const auto ts=ImGui::CalcTextSize(label.c_str());d->AddText(ImVec2(a.x+(cardW-ts.x)*.5f,b.y-t.Y(22.0f)),ImGui::GetColorU32(theme.text),label.c_str());ImGui::Dummy(ImVec2(cardW,cardH));
        ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();
    };
    const auto surfacePreview=[&](){ImGui::PushID("surface-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(116.0f)),true);ImGui::TextUnformatted("Surface preview");ImGui::Separator();ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);ImGui::BeginChild("##alt",ImVec2(t.X(220.0f),t.Y(54.0f)),true);ImGui::TextUnformatted("Panel alternate / card");HelpyImGuiTheme::SoftButton("Control surface");ImGui::EndChild();ImGui::PopStyleColor();ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();};
    const auto accentPreview=[&](){ImGui::PushID("accent-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(105.0f)),true);ImGui::TextUnformatted("Interaction preview");ImGui::Separator();ImGui::PushStyleColor(ImGuiCol_Button,theme.accent);ImGui::PushStyleColor(ImGuiCol_ButtonHovered,theme.accentHover);ImGui::PushStyleColor(ImGuiCol_ButtonActive,theme.accentActive);HelpyImGuiTheme::SoftButton("Accent action");ImGui::PopStyleColor(3);ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Button,theme.selection);HelpyImGuiTheme::SoftButton("Selected");ImGui::PopStyleColor();ImGui::SameLine();HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Item,ImVec2(ImGui::GetCursorScreenPos().x+t.X(12.0f),ImGui::GetCursorScreenPos().y+ImGui::GetFrameHeight()*.5f),t.Y(16.0f),theme.icon);ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();};
    const auto typographyPreview=[&](){ImGui::PushID("type-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(132.0f)),true);ImGui::TextUnformatted("Typography & status preview");ImGui::Separator();ImGui::TextUnformatted("Primary text");ImGui::TextDisabled("Disabled / secondary text");ImGui::PushStyleColor(ImGuiCol_Text,theme.success);ImGui::TextUnformatted("Success");ImGui::PopStyleColor();ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Text,theme.warning);ImGui::TextUnformatted("Warning");ImGui::PopStyleColor();ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Text,theme.error);ImGui::TextUnformatted("Error");ImGui::PopStyleColor();ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Text,theme.info);ImGui::TextUnformatted("Info");ImGui::PopStyleColor();ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();};
    const auto layoutPreview=[&](){ImGui::PushID("layout-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(115.0f)),true);ImGui::TextUnformatted("Spacing & rounding preview");ImGui::Separator();const auto a=ImGui::GetCursorScreenPos();const float w=std::min(t.X(260.0f),ImGui::GetContentRegionAvail().x),h=t.Y(54.0f);auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(a,ImVec2(a.x+w,a.y+h),ImGui::GetColorU32(theme.panelAlt),t.X(theme.childRounding));d->AddRect(a,ImVec2(a.x+w,a.y+h),ImGui::GetColorU32(theme.border),t.X(theme.childRounding));d->AddText(ImVec2(a.x+t.X(theme.contentPaddingX),a.y+t.Y(theme.contentPaddingY)),ImGui::GetColorU32(theme.text),"Content padding");ImGui::Dummy(ImVec2(w,h));ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();};


    ImGui::SeparatorText("Settings storage");
    int settingsSource=HelpyImGuiTheme::UseUserAppData?1:0;
    ImGui::SetNextItemWidth(t.X(240.0f));
    if(ImGui::Combo("Source##theme-source",&settingsSource,"Default (Root)\0User (AppData)\0")){
        HelpyImGuiTheme::SelectSettingsSource(settingsSource==1);
        HelpyImGuiTheme::Reload(true);
        theme=HelpyImGuiTheme::Current;
    }
    ImGui::TextDisabled("Active: %s",HelpyImGuiTheme::FilePath().string().c_str());
    ImGui::SeparatorText("Appearance");
    ImGui::TextWrapped("Settings here and ui.jsonc edit the same live theme. Every section and option can return to the shipped RuneSchema defaults.");
    ImGui::Text("Effective font scale: %.2fx",t.Scale()*theme.fontScale);ImGui::SameLine();ImGui::TextDisabled("DPI %.2fx  density %.2fx",t.Dpi(),theme.density);

    static std::vector<UserColorPreset> userPresets=LoadUserColorPresets();
    static int selectedPreset=1;
    static char presetName[64]{};
    const int builtinCount=static_cast<int>(sizeof(BuiltinColorPresetNames)/sizeof(BuiltinColorPresetNames[0]));
    const int presetCount=builtinCount+static_cast<int>(userPresets.size());
    const auto presetLabel=[&](int index)->std::string {
        if(index<builtinCount)return BuiltinColorPresetNames[index];
        const auto custom=static_cast<size_t>(index-builtinCount);
        return custom<userPresets.size()?("[Custom] "+userPresets[custom].Name):std::string("Select preset");
    };
    if(selectedPreset<0||selectedPreset>=presetCount)selectedPreset=std::min(1,std::max(0,presetCount-1));

    if(theme.advancedMode&&ImGui::CollapsingHeader("Color theme presets",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Presets change colors only. Your sizing, layout, cache, and browser preferences stay untouched.");
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Preset");ImGui::SameLine(t.X(220.0f));ImGui::SetNextItemWidth(std::max(t.X(220.0f),ImGui::GetContentRegionAvail().x-t.X(162.0f)));
        const auto preview=presetLabel(selectedPreset);
        if(ImGui::BeginCombo("##color-preset",preview.c_str())) {
            for(int i=0;i<presetCount;++i) {
                const bool selected=i==selectedPreset;const auto label=presetLabel(i);
                if(ImGui::Selectable(label.c_str(),selected))selectedPreset=i;
                if(selected)ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Apply")) {
            if(selectedPreset<builtinCount) {
                const auto preset=BuiltinColorPreset(selectedPreset);CopyThemeColors(theme,preset);changed();
                PushToast(std::string("Applied color preset: ")+BuiltinColorPresetNames[selectedPreset],2.5);
            } else {
                const auto custom=static_cast<size_t>(selectedPreset-builtinCount);
                if(custom<userPresets.size()){ApplyThemeColorsJson(theme,userPresets[custom].Colors);changed();PushToast("Applied custom color preset: "+userPresets[custom].Name,2.5);}
            }
        }
        if(selectedPreset>=builtinCount) {
            ImGui::SameLine();
            if(HelpyImGuiTheme::SoftButton("Delete"))ImGui::OpenPopup("Delete color preset?");
        }
        if(HelpyImGuiTheme::BeginCenteredModal("Delete color preset?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto custom=selectedPreset>=builtinCount?static_cast<size_t>(selectedPreset-builtinCount):userPresets.size();
            if(custom<userPresets.size())ImGui::TextWrapped("Delete custom color preset \"%s\"?",userPresets[custom].Name.c_str());
            if(HelpyImGuiTheme::SoftButton("Delete preset")&&custom<userPresets.size()) {
                const auto deleted=userPresets[custom].Name;userPresets.erase(userPresets.begin()+custom);
                if(SaveUserColorPresets(userPresets)){selectedPreset=std::min(1,builtinCount-1);PushToast("Deleted custom color preset: "+deleted,2.5);}
                else PushToast("Could not save theme-presets.jsonc after deleting preset.",3.5);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(1,t.Y(3.0f)));
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Save current colors");ImGui::SameLine(t.X(220.0f));ImGui::SetNextItemWidth(std::max(t.X(180.0f),ImGui::GetContentRegionAvail().x-t.X(150.0f)));
        ImGui::InputTextWithHint("##preset-name","Preset name",presetName,sizeof(presetName));ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Save preset")) {
            const auto name=TrimPresetName(presetName);
            if(name.empty())PushToast("Enter a name for the custom color preset.",3.0);
            else {
                bool reserved=false;for(const auto* builtIn:BuiltinColorPresetNames)if(name==builtIn){reserved=true;break;}
                if(reserved)PushToast("That name is reserved by a built-in color preset.",3.0);
                else {
                    auto found=std::find_if(userPresets.begin(),userPresets.end(),[&](const UserColorPreset& p){return p.Name==name;});
                    if(found==userPresets.end()){userPresets.push_back({name,ThemeColorsJson(theme)});selectedPreset=builtinCount+static_cast<int>(userPresets.size())-1;}
                    else {found->Colors=ThemeColorsJson(theme);selectedPreset=builtinCount+static_cast<int>(found-userPresets.begin());}
                    if(SaveUserColorPresets(userPresets)){presetName[0]='\0';PushToast("Saved custom color preset: "+name,2.8);}
                    else PushToast("Could not write theme-presets.jsonc.",3.5);
                }
            }
        }
        ImGui::TextDisabled("Saving an existing custom name updates that preset in place.");
    }

    if(HelpyImGuiTheme::SoftButton("Use Dragonwilds Sync colors")){HelpyImGuiTheme::UseSyncColors();changed();}
    if(!theme.advancedMode) {
        ImGui::SeparatorText("Size");
        if(ImGui::SliderFloat("Window scale",&theme.userScale,0.75f,2.0f,"%.2fx"))changed();
        if(ImGui::SliderFloat("Text scale",&theme.fontScale,0.8f,2.0f,"%.2fx"))changed();
        LiveLogPanel::RenderColors();
        ImGui::TextDisabled("Advanced mode adds detailed layout and color controls. Authoring has its own workspace.");
        if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}return;
    }
    if(ImGui::CollapsingHeader("Overall style",ImGuiTreeNodeFlags_DefaultOpen)) {
        sectionActions("overall-style",[&](){
            theme.accent=defaults.accent;theme.accentHover=defaults.accentHover;theme.accentActive=defaults.accentActive;theme.buttonHover=defaults.buttonHover;theme.buttonActive=defaults.buttonActive;theme.selection=defaults.selection;theme.navigationHover=defaults.navigationHover;theme.navigationActive=defaults.navigationActive;theme.scrollbarHover=defaults.scrollbarHover;theme.scrollbarActive=defaults.scrollbarActive;theme.icon=defaults.icon;
            theme.window=defaults.window;theme.panel=defaults.panel;theme.panelAlt=defaults.panelAlt;theme.navigation=defaults.navigation;theme.control=defaults.control;theme.controlHover=defaults.controlHover;theme.controlActive=defaults.controlActive;theme.button=defaults.button;theme.scrollbarTrack=defaults.scrollbarTrack;theme.scrollbar=defaults.scrollbar;theme.border=defaults.border;theme.separator=defaults.separator;theme.scrollFade=defaults.scrollFade;
        });
        ImVec4 overallAccent=theme.accent;
        ImGui::PushID("overall-accent");ImGui::TextUnformatted("Overall accent");ImGui::SameLine(t.X(220.0f));float av[4]{overallAccent.x,overallAccent.y,overallAccent.z,overallAccent.w};
        ImGui::SetNextItemWidth(t.X(58.0f));
        if(ImGui::ColorEdit4("##value",av,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_AlphaPreviewHalf)) {overallAccent={av[0],av[1],av[2],av[3]};theme.accent=overallAccent;theme.accentHover=lighter(overallAccent,0.20f);theme.accentActive=darker(overallAccent,0.18f);theme.buttonHover=theme.accentHover;theme.buttonActive=theme.accentActive;theme.selection=darker(overallAccent,0.28f);theme.navigationHover=darker(overallAccent,0.58f);theme.navigationActive=darker(overallAccent,0.38f);theme.scrollbarHover=darker(overallAccent,0.28f);theme.scrollbarActive=lighter(overallAccent,0.08f);theme.icon=lighter(overallAccent,0.38f);changed();}
        ImGui::SameLine();if(ImGui::SmallButton("Reset")){theme.accent=defaults.accent;theme.accentHover=defaults.accentHover;theme.accentActive=defaults.accentActive;theme.buttonHover=defaults.buttonHover;theme.buttonActive=defaults.buttonActive;theme.selection=defaults.selection;theme.navigationHover=defaults.navigationHover;theme.navigationActive=defaults.navigationActive;theme.scrollbarHover=defaults.scrollbarHover;theme.scrollbarActive=defaults.scrollbarActive;theme.icon=defaults.icon;changed();}ImGui::PopID();

        ImVec4 overallBackground=theme.window;
        ImGui::PushID("overall-background");ImGui::TextUnformatted("Overall background");ImGui::SameLine(t.X(220.0f));float bv[4]{overallBackground.x,overallBackground.y,overallBackground.z,overallBackground.w};
        ImGui::SetNextItemWidth(t.X(58.0f));
        if(ImGui::ColorEdit4("##value",bv,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_AlphaPreviewHalf)) {overallBackground={bv[0],bv[1],bv[2],bv[3]};theme.window=overallBackground;theme.panel=lighter(overallBackground,0.10f);theme.panelAlt=lighter(overallBackground,0.18f);theme.navigation=lighter(overallBackground,0.045f);theme.control=lighter(overallBackground,0.20f);theme.controlHover=lighter(overallBackground,0.29f);theme.controlActive=lighter(overallBackground,0.36f);theme.button=lighter(overallBackground,0.23f);theme.scrollbarTrack=darker(overallBackground,0.05f);theme.scrollbar=lighter(overallBackground,0.28f);theme.border=lighter(overallBackground,0.34f);theme.separator=lighter(overallBackground,0.27f);theme.scrollFade=overallBackground;theme.scrollFade.w=0.86f;changed();}
        ImGui::SameLine();if(ImGui::SmallButton("Reset")){theme.window=defaults.window;theme.panel=defaults.panel;theme.panelAlt=defaults.panelAlt;theme.navigation=defaults.navigation;theme.control=defaults.control;theme.controlHover=defaults.controlHover;theme.controlActive=defaults.controlActive;theme.button=defaults.button;theme.scrollbarTrack=defaults.scrollbarTrack;theme.scrollbar=defaults.scrollbar;theme.border=defaults.border;theme.separator=defaults.separator;theme.scrollFade=defaults.scrollFade;changed();}ImGui::PopID();
    }

    if(ImGui::CollapsingHeader("Display & sizing",ImGuiTreeNodeFlags_DefaultOpen)) {
        sectionActions("display-sizing",[&](){
            theme.itemPageSize=defaults.itemPageSize;
            theme.itemBayBrowseRatio=defaults.itemBayBrowseRatio;theme.dockedBayHeight=defaults.dockedBayHeight;theme.itemCartWidth=defaults.itemCartWidth;
            theme.windowOpacity=defaults.windowOpacity;theme.dpiAware=defaults.dpiAware;theme.autoReload=defaults.autoReload;theme.verboseTooltips=defaults.verboseTooltips;theme.tooltipDelay=defaults.tooltipDelay;theme.userScale=defaults.userScale;theme.fontScale=defaults.fontScale;theme.density=defaults.density;theme.resultListHeight=defaults.resultListHeight;theme.itemResultHeight=defaults.itemResultHeight;theme.aiResultHeight=defaults.aiResultHeight;theme.itemGridView=defaults.itemGridView;theme.itemGridColumns=defaults.itemGridColumns;theme.itemGridRows=defaults.itemGridRows;theme.itemGridFitRows=defaults.itemGridFitRows;theme.aiGridView=defaults.aiGridView;theme.itemCategory=defaults.itemCategory;theme.listIconSize=defaults.listIconSize;theme.gridIconSize=defaults.gridIconSize;theme.gridCardHeight=defaults.gridCardHeight;theme.gridMinCardWidth=defaults.gridMinCardWidth;theme.navigationItemHeight=defaults.navigationItemHeight;theme.navigationIconSize=defaults.navigationIconSize;theme.loadingSpinnerRadius=defaults.loadingSpinnerRadius;theme.loadingSpinnerThickness=defaults.loadingSpinnerThickness;theme.overscanRows=defaults.overscanRows;});
        checkbox("DPI aware",theme.dpiAware,defaults.dpiAware);checkbox("Auto reload ui.jsonc",theme.autoReload,defaults.autoReload);checkbox("Verbose tooltips",theme.verboseTooltips,defaults.verboseTooltips);ImGui::TextDisabled("Adds internal paths, IDs, classes and other available metadata to hover tooltips without triggering extra runtime queries.");slider("Tooltip delay",theme.tooltipDelay,0.0f,2.0f,defaults.tooltipDelay,"%.2f s");
        slider("Window opacity",theme.windowOpacity,0.60f,1.0f,defaults.windowOpacity,"%.2f");
        slider("User scale",theme.userScale,0.60f,2.50f,defaults.userScale);slider("Font scale",theme.fontScale,0.70f,1.75f,defaults.fontScale);slider("Density",theme.density,0.65f,1.50f,defaults.density);
        sliderInt("Items per page",theme.itemPageSize,1,ItemPaging::MaximumSize,defaults.itemPageSize);
        sliderInt("Item bay columns",theme.itemGridColumns,1,ItemBayLayout::MaxColumns,defaults.itemGridColumns);sliderInt("Item bay rows",theme.itemGridRows,1,ItemBayLayout::MaxRows,defaults.itemGridRows);checkbox("Size item grid by rows",theme.itemGridFitRows,defaults.itemGridFitRows);
        ImGui::TextDisabled("Default: 20 items per page in a 5 by 4 grid. View controls are also above each item bay.");
        slider("List icon size",theme.listIconSize,16.0f,96.0f,defaults.listIconSize,"%.0f");slider("Grid icon size",theme.gridIconSize,28.0f,160.0f,defaults.gridIconSize,"%.0f");
        slider("Grid card height",theme.gridCardHeight,72.0f,260.0f,defaults.gridCardHeight,"%.0f");slider("Grid minimum card width",theme.gridMinCardWidth,92.0f,320.0f,defaults.gridMinCardWidth,"%.0f");slider("Item results pane height",theme.itemResultHeight,180.0f,900.0f,defaults.itemResultHeight,"%.0f");slider("AI results pane height",theme.aiResultHeight,180.0f,900.0f,defaults.aiResultHeight,"%.0f");slider("Navigation item height",theme.navigationItemHeight,24.0f,72.0f,defaults.navigationItemHeight,"%.0f");slider("Navigation icon size",theme.navigationIconSize,10.0f,40.0f,defaults.navigationIconSize,"%.0f");slider("Loading spinner radius",theme.loadingSpinnerRadius,6.0f,40.0f,defaults.loadingSpinnerRadius,"%.1f");slider("Loading spinner thickness",theme.loadingSpinnerThickness,1.0f,8.0f,defaults.loadingSpinnerThickness,"%.1f");sliderInt("Icon overscan rows",theme.overscanRows,0,16,defaults.overscanRows);
        sizingPreview();
    }

    if(ImGui::CollapsingHeader("Backgrounds & surfaces")) {
        sectionActions("background-colors",[&](){theme.window=defaults.window;theme.panel=defaults.panel;theme.panelAlt=defaults.panelAlt;theme.navigation=defaults.navigation;theme.control=defaults.control;theme.controlHover=defaults.controlHover;theme.controlActive=defaults.controlActive;theme.button=defaults.button;theme.scrollbarTrack=defaults.scrollbarTrack;theme.scrollbar=defaults.scrollbar;theme.border=defaults.border;theme.separator=defaults.separator;theme.scrollFade=defaults.scrollFade;});
        color("Window",theme.window,defaults.window);color("Panel",theme.panel,defaults.panel);color("Panel alternate / cards",theme.panelAlt,defaults.panelAlt);color("Navigation",theme.navigation,defaults.navigation);color("Control",theme.control,defaults.control);color("Control hover",theme.controlHover,defaults.controlHover);color("Control active",theme.controlActive,defaults.controlActive);color("Button",theme.button,defaults.button);color("Scrollbar track",theme.scrollbarTrack,defaults.scrollbarTrack);color("Scrollbar",theme.scrollbar,defaults.scrollbar);color("Border",theme.border,defaults.border);color("Separator",theme.separator,defaults.separator);color("Scroll fade",theme.scrollFade,defaults.scrollFade);
        surfacePreview();
    }

    if(ImGui::CollapsingHeader("Accents & interaction")) {
        sectionActions("accent-colors",[&](){theme.accent=defaults.accent;theme.accentHover=defaults.accentHover;theme.accentActive=defaults.accentActive;theme.navigationHover=defaults.navigationHover;theme.navigationActive=defaults.navigationActive;theme.buttonHover=defaults.buttonHover;theme.buttonActive=defaults.buttonActive;theme.selection=defaults.selection;theme.scrollbarHover=defaults.scrollbarHover;theme.scrollbarActive=defaults.scrollbarActive;theme.icon=defaults.icon;});
        color("Accent",theme.accent,defaults.accent);color("Accent hover",theme.accentHover,defaults.accentHover);color("Accent active",theme.accentActive,defaults.accentActive);color("Navigation hover",theme.navigationHover,defaults.navigationHover);color("Navigation active",theme.navigationActive,defaults.navigationActive);color("Button hover",theme.buttonHover,defaults.buttonHover);color("Button active",theme.buttonActive,defaults.buttonActive);color("Selection",theme.selection,defaults.selection);color("Scrollbar hover",theme.scrollbarHover,defaults.scrollbarHover);color("Scrollbar active",theme.scrollbarActive,defaults.scrollbarActive);color("Non-game icon tint",theme.icon,defaults.icon);
        accentPreview();
    }

    if(ImGui::CollapsingHeader("Typography & status")) {
        sectionActions("typography",[&](){theme.fontFamily=defaults.fontFamily;theme.text=defaults.text;theme.textDisabled=defaults.textDisabled;theme.textBold=defaults.textBold;theme.success=defaults.success;theme.warning=defaults.warning;theme.error=defaults.error;theme.info=defaults.info;});
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Font family");ImGui::SameLine(t.X(220.0f));ImGui::SetNextItemWidth(std::max(t.X(180.0f),ImGui::GetContentRegionAvail().x-t.X(72.0f)));
        if(ImGui::BeginCombo("##font-family",theme.fontFamily.c_str())) {
            for(const auto& f:UiFontFamilies){const bool selected=theme.fontFamily==f.Name;std::string label=f.Name;if(!f.Available)label+="  [unavailable]";if(ImGui::Selectable(label.c_str(),selected)){theme.fontFamily=f.Name;changed();}if(selected)ImGui::SetItemDefaultFocus();}
            ImGui::EndCombo();
        }
        ImGui::SameLine();if(ImGui::SmallButton("Reset##font-family")){theme.fontFamily=defaults.fontFamily;changed();}
        if(const auto* chosen=FindUiFontFamily(theme.fontFamily);chosen&&chosen->Available)ImGui::TextDisabled("Font source: %s",chosen->Source.c_str());
        else if(theme.fontFamily=="RuneScape")ImGui::TextDisabled("RuneScape font not loaded. RSMK scans assets/fonts for any RuneScape TTF/OTF.");
        else ImGui::TextDisabled("Selected font is unavailable on this Windows install; Segoe UI fallback is active.");
        if(ImGui::SmallButton("Rescan font files"))FontRescanRequested=true;
        color("Text",theme.text,defaults.text,&theme.textBold,defaults.textBold);color("Disabled text",theme.textDisabled,defaults.textDisabled);color("Success",theme.success,defaults.success);color("Warning",theme.warning,defaults.warning);color("Error",theme.error,defaults.error);color("Info",theme.info,defaults.info);
        color("Primary action text",theme.onAccent,defaults.onAccent);
        typographyPreview();
    }

    if(ImGui::CollapsingHeader("Granular layout")) {
        sectionActions("layout",[&](){theme.frameRounding=defaults.frameRounding;theme.childRounding=defaults.childRounding;theme.scrollbarSize=defaults.scrollbarSize;theme.framePaddingX=defaults.framePaddingX;theme.framePaddingY=defaults.framePaddingY;theme.itemSpacingX=defaults.itemSpacingX;theme.itemSpacingY=defaults.itemSpacingY;theme.sectionSpacing=defaults.sectionSpacing;theme.scrollFadeHeight=defaults.scrollFadeHeight;theme.navigationWidth=defaults.navigationWidth;theme.navigationMinWidth=defaults.navigationMinWidth;theme.navigationMaxWidth=defaults.navigationMaxWidth;theme.contentGutter=defaults.contentGutter;theme.contentPaddingX=defaults.contentPaddingX;theme.contentPaddingY=defaults.contentPaddingY;theme.splitterWidth=defaults.splitterWidth;theme.minWindowWidth=defaults.minWindowWidth;theme.minWindowHeight=defaults.minWindowHeight;});
        slider("Frame rounding",theme.frameRounding,0.0f,20.0f,defaults.frameRounding);slider("Child/card rounding",theme.childRounding,0.0f,20.0f,defaults.childRounding);slider("Scrollbar size",theme.scrollbarSize,6.0f,28.0f,defaults.scrollbarSize);
        slider("Frame padding X",theme.framePaddingX,0.0f,24.0f,defaults.framePaddingX);slider("Frame padding Y",theme.framePaddingY,0.0f,24.0f,defaults.framePaddingY);slider("Item spacing X",theme.itemSpacingX,0.0f,30.0f,defaults.itemSpacingX);slider("Item spacing Y",theme.itemSpacingY,0.0f,30.0f,defaults.itemSpacingY);
        slider("Section spacing",theme.sectionSpacing,0.0f,40.0f,defaults.sectionSpacing);slider("Scroll fade height",theme.scrollFadeHeight,0.0f,64.0f,defaults.scrollFadeHeight);slider("Navigation width",theme.navigationWidth,112.0f,360.0f,defaults.navigationWidth);slider("Navigation minimum width",theme.navigationMinWidth,96.0f,280.0f,defaults.navigationMinWidth);slider("Navigation maximum width",theme.navigationMaxWidth,126.0f,480.0f,defaults.navigationMaxWidth);
        slider("Content gutter",theme.contentGutter,0.0f,40.0f,defaults.contentGutter);slider("Content padding X",theme.contentPaddingX,0.0f,40.0f,defaults.contentPaddingX);slider("Content padding Y",theme.contentPaddingY,0.0f,24.0f,defaults.contentPaddingY);slider("Splitter width",theme.splitterWidth,3.0f,16.0f,defaults.splitterWidth);slider("Minimum window width",theme.minWindowWidth,560.0f,1600.0f,defaults.minWindowWidth,"%.0f");slider("Minimum window height",theme.minWindowHeight,420.0f,1200.0f,defaults.minWindowHeight,"%.0f");
        layoutPreview();
    }

    LiveLogPanel::RenderColors();

    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::SeparatorText("Theme file & reset");
    if(HelpyImGuiTheme::SoftButton("Reload ui.jsonc")){HelpyImGuiTheme::RequestReload();pendingSave=false;}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Open ui.jsonc")){const auto path=HelpyImGuiTheme::FilePath();ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,path.parent_path().c_str(),SW_SHOWNORMAL);}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Reset to RuneSchema defaults"))ImGui::OpenPopup("Reset RuneSchema UI?");
    if(HelpyImGuiTheme::BeginCenteredModal("Reset RuneSchema UI?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {ImGui::TextWrapped("Restore all RSMK UI colors, typography, scaling, spacing, icon/card sizing, and icon cache limit to the shipped RuneSchema defaults?");if(HelpyImGuiTheme::SoftButton("Reset defaults")){HelpyImGuiTheme::ResetDefaults();ItemGridPicker::ResetViewPreference();SidebarWidthLogical=-1.0f;pendingSave=false;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
    ImGui::TextWrapped("%s",HelpyImGuiTheme::Status.c_str());if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}

    ImGui::Dummy(ImVec2(1,t.Y(12.0f)));ImGui::SeparatorText("Hotkeys");ImGui::TextUnformatted(HelpySettings::HotkeyHint().c_str());ImGui::TextDisabled("Change it in Home > Quick settings.");
}
void RenderWorkspace(const HelpyImGuiTheme::Theme& t) {
    if(!HelpyNavigation::Available(CurrentPage,t.workspaces,HelpyEnvironment::Server())){
        const auto parent=HelpyNavigation::Parent(CurrentPage);
        CurrentPage=parent!=CurrentPage?parent:Page::Settings;
    }
    if(HelpyNavigation::Parent(CurrentPage)==Page::Items)LastPlayersPage=CurrentPage;
    if(HelpyNavigation::Parent(CurrentPage)==Page::Map)LastWorldPage=CurrentPage;
    if(HelpyNavigation::Parent(CurrentPage)==Page::Tools)LastToolsPage=CurrentPage;
    const bool showSidebar=HelpyNavigation::ShowSidebar(CurrentPage);
#if defined(HELPY_PREVIEW)
    PreviewSidebarVisible=showSidebar;
#endif
    try{SpawnToolsPanel::RefreshSnapshot(SpawnToolsPanel::UiCategory==-99?0:SpawnToolsPanel::UiCategory);
        if(const auto* local=PlayerStatus::LocalCharacter(SpawnToolsPanel::UiSnapshot))RuneSchemaProfile::Observe(*local);}
    catch(const std::exception& error){
        PageErrors[static_cast<size_t>(CurrentPage)]=error.what();
        SpawnToolsPanel::UiSnapshot.erase("Players");SpawnToolsPanel::ReconcileCartRecipients(SpawnToolsPanel::UiSnapshot);
    }catch(...){
        PageErrors[static_cast<size_t>(CurrentPage)]="The player snapshot could not be refreshed.";
        SpawnToolsPanel::UiSnapshot.erase("Players");SpawnToolsPanel::ReconcileCartRecipients(SpawnToolsPanel::UiSnapshot);
    }
    SpawnToolsPanel::PollCartDelivery();
    if(MapPanel::Data.PartyDetached)MapPanel::Poll();
    PlayerInventoryPanel::Poll();ItemDetails::Poll();
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::SplashVisible=!SplashEntered;
#endif
    if(!SplashEntered){
#if defined(HELPY_PREVIEW)
        PreviewSidebarVisible=false;
#endif
        RenderSplash(t);return;
    }
    ItemGridPicker::ItemAction=[](const ItemGridPicker::Result& action){
        SpawnToolsPanel::RefreshSnapshot(SpawnToolsPanel::UiCategory);
        if(SpawnToolsPanel::SelectedPlayer.empty()||!SpawnToolsPanel::CurrentCartPlayer(SpawnToolsPanel::UiSnapshot,SpawnToolsPanel::SelectedPlayer)){ItemGridPicker::CartStatus="Choose a current player before giving items.";SharedCartVisible=true;return;}
        if(SpawnToolRequests::IsWaiting()){ItemGridPicker::CartStatus="Wait for the current operation before giving items.";return;}
        auto items=action.ActionItems;if(items.empty())items=nlohmann::json::array({{{"Item",action.ActionPath},{"Count",action.ActionCount}}});
        if(!SpawnToolRequests::TrySubmit({{"Action","GiveItems"},{"_Source","HelpySharedItemBay"},{"Player",SpawnToolsPanel::SelectedPlayer},{"Items",std::move(items)}}))ItemGridPicker::CartStatus="The runtime did not accept the item request.";
    };
    if(showSidebar){
    if(SidebarWidthLogical<0.0f)SidebarWidthLogical=t.navigationWidth;
    const float totalLogical=ImGui::GetContentRegionAvail().x/std::max(0.01f,t.Scale());
    const float minimumContent=360.0f;
    const float dynamicMax=std::max(t.navigationMinWidth,totalLogical-minimumContent-t.splitterWidth-t.contentGutter);
    SidebarWidthLogical=std::clamp(SidebarWidthLogical,t.navigationMinWidth,std::min(t.navigationMaxWidth,dynamicMax));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,t.navigation);
    ImGui::BeginChild("##helpy-nav",ImVec2(t.X(SidebarWidthLogical),0),false);
#if defined(HELPY_PREVIEW)
    const auto railAt=ImGui::GetWindowPos(),railSize=ImGui::GetWindowSize();HelpyPreviewHooks::NavigationRail={railAt.x,railAt.y,railAt.x+railSize.x,railAt.y+railSize.y};
#endif
    ImGui::Dummy(ImVec2(1,t.Y(10.0f)));
    if(BrandTexture){
        const float size=std::min(t.X(62.0f),ImGui::GetContentRegionAvail().x);ImGui::SetCursorPosX((ImGui::GetWindowWidth()-size)*0.5f);
        const auto at=ImGui::GetCursorScreenPos();ImGui::Dummy({size,size});DrawThemedLogo(ImGui::GetWindowDrawList(),at,{at.x+size,at.y+size},t);
    }
    const auto centered=[](const char* text){ImGui::SetCursorPosX(std::max(0.0f,(ImGui::GetWindowWidth()-ImGui::CalcTextSize(text).x)*0.5f));ImGui::TextUnformatted(text);};
    centered("RuneSchema");
    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::Separator();ImGui::Dummy(ImVec2(1,t.Y(5.0f)));
    const auto sidebarDefaults=HelpyNavigationOrder::SidebarDefaults();
    std::vector<Page> routes,lowerRoutes;bool lower=false;
    for(int id:HelpyNavigationOrder::Ordered(HelpyNavigationOrder::SidebarSurface,sidebarDefaults)){
        if(id==HelpyNavigationOrder::SidebarDivider){lower=true;continue;}
        const auto page=static_cast<Page>(id);
        if(HelpyNavigation::Available(page,t.workspaces,HelpyEnvironment::Server()))(lower?lowerRoutes:routes).push_back(page);
    }
    const float spacing=ImGui::GetStyle().ItemSpacing.y;
    const auto rowHeight=[&](Page page){
        const auto text=ImGui::CalcTextSize(PageTitle(page),nullptr,false,std::max(1.f,ImGui::GetContentRegionAvail().x-t.X(46)));
        return std::max(t.Y(t.navigationItemHeight),text.y+t.Y(18))+spacing;
    };
    float routeHeight=0,lowerHeight=0;for(auto page:routes)routeHeight+=rowHeight(page);for(auto page:lowerRoutes)lowerHeight+=rowHeight(page);
    const float budget=std::max(1.f,ImGui::GetContentRegionAvail().y-t.Y(84)-t.Y(8)-spacing*3);
    lowerHeight=std::min(lowerHeight,budget*(routes.empty()?1.f:.5f));
    const float routeLimit=std::max(1.f,budget-lowerHeight);
    if(ImGui::BeginChild("##helpy-destinations",ImVec2(0,std::max(1.f,std::min(routeHeight,routeLimit))),false))
        for(auto page:routes)NavItem(page,t,sidebarDefaults);
    ImGui::EndChild();
    const auto divider=ImGui::GetCursorScreenPos();const float dividerWidth=std::max(1.f,ImGui::GetContentRegionAvail().x),dividerHeight=t.Y(8);
    ImGui::InvisibleButton("##sidebar-divider",{dividerWidth,dividerHeight});
    ImGui::GetWindowDrawList()->AddLine({divider.x,divider.y+dividerHeight*.5f},{divider.x+dividerWidth,divider.y+dividerHeight*.5f},ImGui::GetColorU32(t.separator));
    NavigationOrder::Drag(HelpyNavigationOrder::SidebarSurface,static_cast<Page>(HelpyNavigationOrder::SidebarDivider),sidebarDefaults,"Navigation divider");
    if(ImGui::BeginChild("##helpy-lower-destinations",{0,std::max(1.f,lowerHeight)},false))for(auto page:lowerRoutes)NavItem(page,t,sidebarDefaults);
    ImGui::EndChild();
    const auto runtimeSize=RuntimeBadgeSize(t);
    const float runtimeY=ImGui::GetCursorPosY()+std::max(0.f,ImGui::GetContentRegionAvail().y-t.Y(84))+(t.Y(84)-runtimeSize.y)*.5f;
    ImGui::SetCursorPos({std::max(0.f,(ImGui::GetWindowWidth()-runtimeSize.x)*.5f),runtimeY});
    RenderRuntimeBadge(t);
    ImGui::EndChild();ImGui::PopStyleColor();

    ImGui::SameLine(0,0);ImGui::PushID("helpy-nav-splitter");
    const float splitHeight=ImGui::GetContentRegionAvail().y;
    ImGui::InvisibleButton("##split",ImVec2(t.X(t.splitterWidth),std::max(1.0f,splitHeight)));
    const bool splitHover=ImGui::IsItemHovered(),splitActive=ImGui::IsItemActive();
    if(splitHover||splitActive)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if(splitActive&&ImGui::GetIO().MouseDelta.x!=0.0f) {
        SidebarWidthLogical+=ImGui::GetIO().MouseDelta.x/std::max(0.01f,t.Scale());
        SidebarWidthLogical=std::clamp(SidebarWidthLogical,t.navigationMinWidth,std::min(t.navigationMaxWidth,dynamicMax));HelpyImGuiTheme::Current.navigationWidth=SidebarWidthLogical;
    }
    static bool navDirty=false;if(splitActive)navDirty=true;if(navDirty&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)){HelpyImGuiTheme::SaveCurrent();navDirty=false;}
    ImGui::PopID();

    ImGui::SameLine(0,t.X(t.contentGutter));
    }
    ImGui::BeginChild("##helpy-content",ImVec2(0,0),false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    RenderInformationBar(t,false);
    RenderWorkspaceTabs(t);
    if(CurrentPage==Page::Map||CurrentPage==Page::Recipes){
        const size_t index=CurrentPage==Page::Map?0:1;
        if(HelpyImGuiTheme::SoftButton(DetachedWorkspaces[index]?"Dock back":index==0?"Detach Map":"Detach Recipes"))DetachedWorkspaces[index]=!DetachedWorkspaces[index];
    }
    const bool cartAllowed=CurrentPage==Page::Creation||CurrentPage==Page::Recipes||CurrentPage==Page::Spawns||CurrentPage==Page::Map;
    if(cartAllowed){
        const auto caption="Cart ("+std::to_string(ItemGridPicker::SharedCart.Entries().size())+")";
        if((CurrentPage==Page::Map||CurrentPage==Page::Recipes)&&ImGui::GetContentRegionAvail().x>t.X(130))ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton(caption.c_str()))SharedCartVisible=!SharedCartVisible;
    }
    if(CurrentPage!=Page::Home){HomeLinkDetails=-1;ImGui::PushStyleColor(ImGuiCol_Text,t.textDisabled);ImGui::TextWrapped("%s",HelpyNavigation::Description(CurrentPage));ImGui::PopStyleColor();ImGui::Separator();}
    const std::string scrollId="##helpy-scroll-"+std::to_string(static_cast<int>(CurrentPage));
    const bool hasBayDock=CurrentPage==Page::Creation||CurrentPage==Page::Recipes||(CurrentPage==Page::Spawns&&SpawnToolsPanel::ExtraLootBay);
    const bool dockBay=hasBayDock&&!ItemGridPicker::DetachedBay;
    ItemGridPicker::DockedBay=hasBayDock;
    const float bayHeight=ItemBayLayout::PaneHeight(t.Y(t.dockedBayHeight),std::max(1.0f,ImGui::GetContentRegionAvail().y-t.Y(150)),t.Y(160));
    const float editorHeight=dockBay?(ItemGridPicker::Bay.Expanded?1.f:std::max(t.Y(110),ImGui::GetContentRegionAvail().y-bayHeight-ImGui::GetFrameHeightWithSpacing())):0;
    const bool scrollVisible=HelpyImGuiTheme::BeginScroll(scrollId.c_str(),ImVec2(0,editorHeight),false);
    if(scrollVisible&&!(dockBay&&ItemGridPicker::Bay.Expanded)) {
        ImGui::Dummy(ImVec2(1,t.Y(4.0f)));
        auto& pageError=PageErrors[static_cast<size_t>(CurrentPage)];
        if(!pageError.empty()) {
            ImGui::TextColored(t.error,"This page could not finish loading.");ImGui::TextWrapped("%s",pageError.c_str());
            if(HelpyImGuiTheme::SoftButton("Retry page"))pageError.clear();
        } else {
            ImGuiErrorRecoveryState recovery;ImGui::ErrorRecoveryStoreState(&recovery);
            try {
                switch(CurrentPage) {
                case Page::Home:RenderHome(t);break;
                case Page::Profile:if(ProfilePanel::Render(SpawnToolsPanel::UiSnapshot))CurrentPage=Page::SaveEditor;break;
                case Page::Items:SpawnToolsPanel::Render(SpawnToolsPanel::View::Items);break;
                case Page::Creation:SpawnToolsPanel::Render(SpawnToolsPanel::View::ItemLab);break;
                case Page::Recipes:if(!DetachedWorkspaces[1])SpawnToolsPanel::Render(SpawnToolsPanel::View::Recipes);else ImGui::TextWrapped("Recipes is open in a separate window. The shared Item Bay remains available below or detached.");break;
                case Page::Map:if(!DetachedWorkspaces[0])MapPanel::Render();else ImGui::TextWrapped("Map is open in a separate window. Minimized windows can be restored from Helpy's taskbar.");break;
                case Page::LiveLog:
                    DiagnosticsPanel::Render(DiagnosticStore,&DiagnosticHistory,&DiagnosticMutes);break;
                case Page::Mods:ModsPanel::Render();break;
                case Page::BugReport:BugReportPanel::Render();break;
                case Page::SaveEditor:SpawnToolsPanel::RefreshSnapshot(0);SaveEditorPanel::Render(SpawnToolsPanel::UiSnapshot,SpawnToolsPanel::UiRevision);break;
                case Page::Journal:JournalPanel::Render();break;
                case Page::DedicatedServer:
                    ImGui::SeparatorText("Session tools");
                    ImGui::TextWrapped("Co-op hosts can use the current session's player delivery, map and spawn tools. Actions still require the native server's authorization; enabling this workspace grants no rights.");
                    if(HelpyImGuiTheme::SoftButton("Players & item delivery"))CurrentPage=Page::Items;
                    if(HelpyImGuiTheme::SoftButton("Map & teleportation"))CurrentPage=Page::Map;
                    if(HelpyImGuiTheme::SoftButton("Spawns & resources"))CurrentPage=Page::Spawns;
                    ImGui::SeparatorText("Dedicated server configuration");
                    if(HelpyEnvironment::Server())DedicatedServerPanel::Render();
                    else ImGui::TextWrapped("A co-op world does not use DedicatedServer.ini. Its settings and permissions remain controlled by the host game. Run Helpy on the dedicated server itself to review and stage that server's INI; a connected client cannot edit it.");
                    break;
                case Page::Settings:
                    if(ImGui::BeginTabBar("Configuration sections",ImGuiTabBarFlags_Reorderable)){
                        if(ImGui::BeginTabItem("Workspaces")){
                            auto& options=HelpyImGuiTheme::Current.workspaces;bool changed=false;
                            ImGui::TextWrapped("Character lives in Players, Journal in World, and Authoring in Tools. Hidden tabs keep their drafts.");
                            changed|=ImGui::Checkbox("Authoring (inside Tools)",&options.Authoring);changed|=ImGui::Checkbox("Session tools (inside Tools)",&options.Tools);
                            changed|=ImGui::Checkbox("Diagnostics",&options.LiveLog);
                            changed|=ImGui::Checkbox("Server / Co-op",&options.ServerTools);
                            changed|=ImGui::Checkbox("Journal",&options.Journal);
                            if(changed)HelpyImGuiTheme::SaveCurrent();ImGui::EndTabItem();
                        }
                        if(ImGui::BeginTabItem("RuneSchema")){RuneSchemaSettingsPanel::Render();ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("Appearance")){RenderSettings(t);ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("Diagnostics")){RenderDiagnosticsSettings(t);ImGui::EndTabItem();}
                        ImGuiTabItemFlags backgroundFlags=0;
#if defined(HELPY_PREVIEW)
                        if(PreviewBackgroundsTab)backgroundFlags|=ImGuiTabItemFlags_SetSelected;
#endif
                        if(ImGui::BeginTabItem("Backgrounds",nullptr,backgroundFlags)){HelpyBackgrounds::Render();ImGui::EndTabItem();}
                        ImGuiTabItemFlags updateFlags=0;
#if defined(HELPY_PREVIEW)
                        if(PreviewUpdatesTab)updateFlags|=ImGuiTabItemFlags_SetSelected;
#endif
                        if(ImGui::BeginTabItem("Updates & runtime",nullptr,updateFlags)){RenderUpdatesAndRuntime(t);ImGui::EndTabItem();}
                        ImGui::EndTabBar();
                    }break;
                case Page::Authoring:AuthoringPanel::SessionTools=[] {SpawnToolsPanel::RenderSessionCleanup();};AuthoringPanel::OpenSpawns=[] {CurrentPage=Page::Spawns;};AuthoringPanel::Render();break;
                case Page::Spawns:
                    {auto selectedSection=CurrentSpawn;
#if defined(HELPY_PREVIEW)
                    if(PreviewSelectSections)selectedSection=PreviewWantedSpawn;
#endif
                    if(ImGui::BeginTabBar("Spawn sections",ImGuiTabBarFlags_Reorderable)) {
                        if(ImGui::BeginTabItem("Resources",nullptr,SectionFlags(selectedSection==HelpyNavigation::SpawnSection::Resources))){CurrentSpawn=HelpyNavigation::SpawnSection::Resources;SpawnToolsPanel::ForcedActorMode=-1;SpawnToolsPanel::Render(SpawnToolsPanel::View::Resources);ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("NPCs",nullptr,SectionFlags(selectedSection==HelpyNavigation::SpawnSection::Npcs))){CurrentSpawn=HelpyNavigation::SpawnSection::Npcs;SpawnToolsPanel::ForcedActorMode=0;SpawnToolsPanel::Render(SpawnToolsPanel::View::Ai);ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("AI / Enemies",nullptr,SectionFlags(selectedSection==HelpyNavigation::SpawnSection::Ai))){CurrentSpawn=HelpyNavigation::SpawnSection::Ai;SpawnToolsPanel::ForcedActorMode=1;SpawnToolsPanel::Render(SpawnToolsPanel::View::Ai);ImGui::EndTabItem();}
                        ImGui::EndTabBar();
                    }}break;
                case Page::Tools:
                    RenderAuthorToolsDownload();
                    {auto selectedSection=CurrentTool;
#if defined(HELPY_PREVIEW)
                    if(PreviewSelectSections)selectedSection=PreviewWantedTool;
#endif
                    if(ImGui::BeginTabBar("Tools sections",ImGuiTabBarFlags_Reorderable)) {
                        if(ImGui::BeginTabItem("Item Cache",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::ItemCache))){CurrentTool=HelpyNavigation::ToolSection::ItemCache;RenderIconCacheSettings(true,true);ImGui::EndTabItem();}
                        if(t.workspaces.Tools&&ImGui::BeginTabItem("Maintenance",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::Maintenance))) {
                            CurrentTool=HelpyNavigation::ToolSection::Maintenance;
                            ImGui::TextWrapped("Refresh catalogs and players when the installed content changes.");
                            if(HelpyImGuiTheme::SoftButton("Refresh players"))QueuePlayerRefresh(false);
                            static bool acknowledged=false;
                            ImGui::Checkbox("Allow a full scan to load additional candidate assets",&acknowledged);
                            ImGui::BeginDisabled(!acknowledged||SpawnToolRequests::IsWaiting());
                            if(HelpyImGuiTheme::SoftButton("Rebuild live catalog")){SpawnToolRequests::Submit({{"Action","IndexQuickCatalog"}});acknowledged=false;}
                            ImGui::EndDisabled();
                            ImGui::TextWrapped("%s",SpawnToolsPanel::UiSnapshot.value("Status",std::string{}).c_str());
                            ImGui::EndTabItem();
                        }
                        if(t.workspaces.Tools&&ImGui::BeginTabItem("Session cleanup",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::Cleanup))){CurrentTool=HelpyNavigation::ToolSection::Cleanup;SpawnToolsPanel::RenderSessionCleanup();ImGui::EndTabItem();}
                        if(t.workspaces.Tools&&t.advancedMode&&ImGui::BeginTabItem("Building tools",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::Buildings))){CurrentTool=HelpyNavigation::ToolSection::Buildings;SpawnToolsPanel::Render(SpawnToolsPanel::View::Buildings);ImGui::EndTabItem();}
                        ImGui::EndTabBar();
                    }}break;
                }
            }catch(const std::exception& error){pageError=error.what();}
            catch(...){pageError="An unexpected interface error occurred.";}
            if(!pageError.empty()) {
                auto& io=ImGui::GetIO();const bool previousAssert=io.ConfigErrorRecoveryEnableAssert;io.ConfigErrorRecoveryEnableAssert=false;
                ImGui::ErrorRecoveryTryToRecoverState(&recovery);io.ConfigErrorRecoveryEnableAssert=previousAssert;
            }
        }
    }
    HelpyImGuiTheme::EndScroll(t);
    if(dockBay){
        ItemGridPicker::ResizeDivider("docked-item-bay-height",true,HelpyImGuiTheme::Current.dockedBayHeight,160,900,-1.0f/std::max(.01f,t.Scale()*t.density));
        ImGui::SeparatorText("Item Bay");
        if(ImGui::BeginChild("##shared-bay-dock",ImVec2(0,0),false)){
            ItemGridPicker::DockedBay=false;ItemGridPicker::RenderShared(SpawnToolsPanel::UiSnapshot,SpawnToolsPanel::UiRevision,true);
        }ImGui::EndChild();
    }
    ItemGridPicker::DockedBay=false;ImGui::EndChild();
    if(SharedCartVisible&&cartAllowed){
        const float cartPad=t.X(8);const ImVec2 cartMin{MainSurfacePosition.x+cartPad,MainSurfacePosition.y+cartPad};
        const ImVec2 cartMax{MainSurfacePosition.x+MainSurfaceSize.x-cartPad,MainSurfacePosition.y+MainSurfaceSize.y-cartPad};
        ImGui::SetNextWindowSizeConstraints({std::min(t.X(240),cartMax.x-cartMin.x),std::min(t.Y(220),cartMax.y-cartMin.y)},
            {std::max(1.f,cartMax.x-cartMin.x),std::max(1.f,cartMax.y-cartMin.y)});
        ImGui::SetNextWindowSize(ImVec2(t.X(480),t.Y(540)),ImGuiCond_FirstUseEver);
        HelpyImGuiTheme::CenterNextWindow();
        if(ImGui::Begin("Your cart",&SharedCartVisible,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)){
            const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
            const ImVec2 fitted{std::clamp(position.x,cartMin.x,std::max(cartMin.x,cartMax.x-size.x)),std::clamp(position.y,cartMin.y,std::max(cartMin.y,cartMax.y-size.y))};
            if(fitted.x!=position.x||fitted.y!=position.y)ImGui::SetWindowPos(fitted);
            if(CurrentPage==Page::Tools)SpawnToolsPanel::RefreshSnapshot(-1);
            SpawnToolsPanel::RenderCartDock(ImVec2(0,0),[]{SpawnToolsPanel::RenderCartRecipient(SpawnToolsPanel::UiSnapshot);});
        }ImGui::End();
    }
}
}

bool HelpyStandaloneWindow::Start() {
    if(m_thread.joinable())return m_ready.load();m_stop=false;m_started=false;m_thread=std::thread([this]{
        try{ThreadMain();}
        catch(const std::exception& error){SetError(error.what());m_ready=false;m_started=true;m_visible=false;if(auto* host=PluginRuntimeServices::Host;host&&host->Log)host->Log(RS_LOG_ERROR,"RuneSchema.Helpy",error.what());}
        catch(...){SetError("Unexpected RSMK window failure");m_ready=false;m_started=true;m_visible=false;if(auto* host=PluginRuntimeServices::Host;host&&host->Log)host->Log(RS_LOG_ERROR,"RuneSchema.Helpy","Unexpected RSMK window failure");}
    });
    for(int i=0;i<500&&!m_started.load();++i)Sleep(2);return m_ready.load();
}
void HelpyStandaloneWindow::Toggle(){if(m_threadId)PostThreadMessageW(m_threadId,ToggleMessage,0,0);}
void HelpyStandaloneWindow::PollHotkey(bool down){
    const auto foreground=GetForegroundWindow();
    if(!Hotkey.Poll(down))return;
    const int key=HelpySettings::Hotkey.load();
    const bool editing=ImGui::GetCurrentContext()&&ImGui::GetIO().WantTextInput&&(key<VK_F1||key>VK_F24);
    const bool modified=(GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_MENU)|GetAsyncKeyState(VK_SHIFT)|GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000;
    if(HelpyPopupInput::CanToggle(GameWindow(foreground),OwnsWindow(foreground),editing,modified))Toggle();
}
void HelpyStandaloneWindow::ReloadTheme(){if(m_threadId)PostThreadMessageW(m_threadId,ReloadMessage,0,0);}
void HelpyStandaloneWindow::Shutdown(){if(!m_thread.joinable())return;m_stop=true;HelpyIconPacks::Progress.Cancel=true;if(m_threadId)PostThreadMessageW(m_threadId,StopMessage,0,0);m_thread.join();ModsPanel::Shutdown();HelpyIconPacks::Shutdown();LiveLogPanel::Shutdown();m_threadId=0;m_ready=false;}
std::string HelpyStandaloneWindow::Error() const{std::scoped_lock lock(m_errorMutex);return m_error;}
void HelpyStandaloneWindow::SetError(std::string value){std::scoped_lock lock(m_errorMutex);m_error=std::move(value);}
bool HelpyStandaloneWindow::OpenPopup() {
    const auto game=GetForegroundWindow();if(!HelpyEnvironment::Server()&&!GameWindow(game))return false;
    m_gameWindow=HelpyEnvironment::Server()?nullptr:game;
    ShowWindow(m_hwnd,IsIconic(m_hwnd)?SW_RESTORE:SW_SHOW);m_visible=true;
    if(m_bayWindow&&ItemGridPicker::DetachedBay&&!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::ItemBay))ShowWindow(m_bayWindow,SW_SHOWNOACTIVATE);
    if(m_cacheWindow&&HelpyIconPacks::Detached&&!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::IconCache))ShowWindow(m_cacheWindow,SW_SHOWNOACTIVATE);
    for(size_t i=0;i<m_workspaces.size();++i)if(m_workspaces[i].Window&&WorkspaceDetached(i)&&!HelpyTaskbar::Current.Contains(WorkspaceTask(i)))ShowWindow(m_workspaces[i].Window,SW_SHOWNOACTIVATE);
    SetForegroundWindow(m_hwnd);SetFocus(m_hwnd);
#if !defined(HELPY_PREVIEW)
    if(!LazyCatalog::LocalSubmit)HelpyCachedCatalog::Configure();
#endif
    LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());QueuePlayerRefresh(true);return true;
}
void HelpyStandaloneWindow::ClosePopup(){
    // Closing Helpy must not steal focus back from an unrelated application.
    const auto foreground=GetForegroundWindow();const bool returnToGame=HelpyPopupInput::ReturnToGame(OwnsWindow(foreground),GameWindow(m_gameWindow));
    m_visible=false;ShowWindow(m_hwnd,HelpyEnvironment::Server()?SW_MINIMIZE:SW_HIDE);
    if(m_bayWindow)ShowWindow(m_bayWindow,SW_HIDE);
    if(m_cacheWindow)ShowWindow(m_cacheWindow,SW_HIDE);
    for(auto& panel:m_workspaces)if(panel.Window)ShowWindow(panel.Window,SW_HIDE);
    if(OwnsWindow(GetCapture())){ReleaseCapture();if(ImGui::GetCurrentContext())ImGui::GetIO().AddMouseButtonEvent(0,false);}
    if(returnToGame)SetForegroundWindow(m_gameWindow);
}

void HelpyStandaloneWindow::ThreadMain() {
    m_threadId=GetCurrentThreadId();
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,WndProc,0,0,m_module,nullptr,LoadCursorW(nullptr,IDC_ARROW),nullptr,nullptr,L"RuneSchema.Helpy.Window",nullptr};
    wc.hIcon=static_cast<HICON>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED));
    wc.hIconSm=static_cast<HICON>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED));
    const auto cleanup=[&](void*){
        m_publishedWindow=nullptr;ClosePopup();
        LiveLogPanel::DrainPending();StructuredLogFeed::DrainPending();
        DiagnosticHistory.Queue(DiagnosticStore,true);DiagnosticHistory.Close();
        DiagnosticOverlay.Close();StructuredLogFeed::Shutdown();DiagnosticsPanel::Shutdown();
        LiveLogPanel::OnEntry={};LiveLogPanel::OnBackfillEntry={};
        if(ImGui::GetCurrentContext()){
            CharacterPreviewPanel::Shutdown();
            if(ImGui::GetIO().BackendRendererUserData)ImGui_ImplDX11_Shutdown();
            CharacterPreviewPanel::ReleaseTexture();
            if(ImGui::GetIO().BackendPlatformUserData)ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        DestroyItemBay();DestroyIconCache();for(size_t i=0;i<m_workspaces.size();++i)DestroyWorkspacePanel(i);ReleaseBrandTextures();DestroyDevice();
        if(m_hwnd)DestroyWindow(m_hwnd);m_hwnd=nullptr;
        UnregisterClassW(wc.lpszClassName,m_module);m_ready=false;
        if(SUCCEEDED(com))CoUninitialize();
    };
    std::unique_ptr<void,decltype(cleanup)> resources(reinterpret_cast<void*>(1),cleanup);
    RegisterClassExW(&wc);
    m_hwnd=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,HelpyBranding::WindowTitle,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,900,nullptr,nullptr,m_module,this);
    if(!m_hwnd){SetError("CreateWindowExW failed");m_started=true;return;}
    const UINT initialDpi=GetDpiForWindow(m_hwnd);HelpyImGuiTheme::SetRuntimeDpi(initialDpi?float(initialDpi)/96.f:1.f);
    DragAcceptFiles(m_hwnd,TRUE);
    SendMessageW(m_hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(wc.hIcon));
    SendMessageW(m_hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(wc.hIconSm));
    ApplyCaptionTheme(m_hwnd,HelpyImGuiTheme::Current,true);
    if(!CreateDevice()){SetError("Direct3D 11 initialization failed");m_started=true;return;}
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
    HelpyImGuiTheme::Poll();
    LoadUiFontFamilies(io);
    LoadBrandTextures(m_device);
    ImGui::StyleColorsDark();ImGui_ImplWin32_Init(m_hwnd);ImGui_ImplDX11_Init(m_device,m_context);ShowWindow(m_hwnd,SW_HIDE);UpdateWindow(m_hwnd);
    m_publishedWindow=m_hwnd;
    // A dedicated host has no UnrealWindow/shortcut entry point. Expose a normal
    // taskbar window without taking focus from its launcher or console.
    if(HelpyEnvironment::Server()){
        CurrentPage=Page::DedicatedServer;
        ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);m_visible=true;
        LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());QueuePlayerRefresh(true);
    }
    const auto kind=HelpyEnvironment::Current;
    const auto lane=kind==HelpyEnvironment::Kind::Steam?"steam":kind==HelpyEnvironment::Kind::Epic?"epic":kind==HelpyEnvironment::Kind::GamePass?"gamepass":HelpyEnvironment::Server()?"server":"unknown";
    const auto diagnosticRoot=RuneSchemaData::PlatformDirectory(lane)/"diagnostics";
    const auto& diagnosticTheme=HelpyImGuiTheme::Current;
    DiagnosticHistory.Open(diagnosticRoot,{diagnosticTheme.diagnosticHistoryDays,diagnosticTheme.diagnosticHistorySessions,diagnosticTheme.diagnosticHistoryMB});
    DiagnosticMutes.Open(diagnosticRoot);
    StructuredLogFeed::OnRecord=[](const StructuredDiagnostic::Record& record){
        const auto id=DiagnosticStore.AcceptStructured(record);if(!id)return;
        const auto* group=DiagnosticStore.FindGroup(id);if(!group||DiagnosticMutes.SuppressStructured(*group))return;
        auto toast=LiveLog::Parse(std::string("[RuneSchema] [")+(record.Severity=="Error"?"error":record.Severity=="Warning"?"warning":"info")+"] ["+record.Code+"] "+record.Title+": "+record.Message);
        toast.Scope=record.Source;DiagnosticFeed.Accept(toast,DiagnosticOptions());
    };
    LiveLogPanel::OnBackfillEntry=[](const LiveLog::Entry& entry){DiagnosticStore.Accept(entry);};
    LiveLogPanel::OnEntry=[](const LiveLog::Entry& entry){
        DiagnosticStore.Accept(entry);if(DiagnosticLegacyIntake::Eligible(entry)&&DiagnosticLegacyIntake::MirrorReference(entry).empty()&&!DiagnosticMutes.Suppress(entry))DiagnosticFeed.Accept(entry,DiagnosticOptions());
    };
    m_ready=true;m_started=true;
    // Opt-in only, once after the renderer is ready. No recurring reopen or
    // game-thread polling; dedicated servers retain their existing entry page.
    if(!HelpyEnvironment::Server()&&HelpySettings::OpenOnGameStart()){
        ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);m_visible=true;
        LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());
    }
    MSG msg{};bool done=false;int boundKey=HelpySettings::Hotkey.load();HelpyFramePacing pacing;
    while(!done&&!m_stop.load()) {
        // Hotkeys belong to Helpy's window thread. No engine tick callback,
        // viewport input hook, or UE4SS C++ ABI is needed by this plugin.
        const int key=HelpySettings::Hotkey.load();const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
        // A key held while rebinding must be released before it can toggle.
        if(key!=boundKey){Hotkey.Poll(down);boundKey=key;}else PollHotkey(down);
        const HWND foreground=GetForegroundWindow();
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT){done=true;break;}
            if(msg.hwnd==nullptr&&msg.message==ToggleMessage) {
                if(m_visible.load())ClosePopup();else OpenPopup();
                continue;
            }
            if(msg.hwnd==nullptr&&msg.message==ReloadMessage){HelpyImGuiTheme::RequestReload();continue;}
            if(msg.hwnd==nullptr&&msg.message==StopMessage){done=true;break;}
            const bool editing=io.WantTextInput&&(boundKey<VK_F1||boundKey>VK_F24);
            if(OwnsWindow(msg.hwnd)&&msg.wParam==boundKey&&!editing&&(msg.message==WM_KEYDOWN||msg.message==WM_KEYUP)) {
                PollHotkey(msg.message==WM_KEYDOWN);
                continue;
            }
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(done)break;
        const bool diagnosticBackground=!m_visible.load()||IsIconic(m_hwnd)||CurrentPage!=Page::LiveLog;
        if(!m_visible.load())HelpyImGuiTheme::Poll();
        LiveLogPanel::Poll(0,diagnosticBackground);StructuredLogFeed::Poll(diagnosticBackground);
        DiagnosticFeed.Tick(DiagnosticOptions());DiagnosticHistory.Queue(DiagnosticStore);
        DiagnosticOverlay.Update(m_module,GameWindow(foreground)?foreground:nullptr,DiagnosticFeed,HelpyImGuiTheme::Current,!m_visible.load()&&!HelpyEnvironment::Server());
        if(m_visible.load()&&std::chrono::steady_clock::now()>=NextPlayerRefresh)QueuePlayerRefresh(true);
        if(pacing.Due(std::chrono::steady_clock::now(),m_visible.load()&&!IsIconic(m_hwnd),OwnsWindow(GetForegroundWindow())))RenderFrame();
        MsgWaitForMultipleObjectsEx(0,nullptr,8,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
}

#if defined(HELPY_PREVIEW)
namespace {
void CapturePreview(ID3D11Device* device,ID3D11DeviceContext* context,IDXGISwapChain* swap,const std::filesystem::path& path) {
    using Microsoft::WRL::ComPtr;
    const auto check=[&](HRESULT result){if(FAILED(result))throw std::runtime_error(std::format("Preview capture {} failed: HRESULT 0x{:08X}",path.filename().string(),static_cast<uint32_t>(result)));};
    ComPtr<ID3D11Texture2D> back,staging;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)));
    D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    check(device->CreateTexture2D(&desc,nullptr,&staging));context->CopyResource(staging.Get(),back.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    std::vector<unsigned char> pixels(desc.Width*desc.Height*4);
    for(UINT y=0;y<desc.Height;++y)std::memcpy(pixels.data()+y*desc.Width*4,static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,desc.Width*4);
    context->Unmap(staging.Get(),0);
    std::filesystem::create_directories(path.parent_path());
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;
    check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    check(encoder->CreateNewFrame(&frame,&properties));check(frame->Initialize(properties.Get()));check(frame->SetSize(desc.Width,desc.Height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;check(frame->SetPixelFormat(&format));
    // The Windows PNG encoder negotiates BGRA on some Windows builds.
    if(IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    else if(!IsEqualGUID(format,GUID_WICPixelFormat32bppRGBA))throw std::runtime_error("Unsupported preview PNG pixel format");
    check(frame->WritePixels(desc.Height,desc.Width*4,static_cast<UINT>(pixels.size()),pixels.data()));check(frame->Commit());check(encoder->Commit());
}
}

int HelpyStandaloneWindow::Preview(const std::filesystem::path& output,bool capture,int width,int height) {
    AuthoringPreviewFixture authoringFixture;
    // Synthetic metadata only. These callbacks refuse every gameplay mutation.
    const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    m_threadId=GetCurrentThreadId();
    WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,WndProc,0,0,m_module,nullptr,LoadCursorW(nullptr,IDC_ARROW),nullptr,nullptr,L"Helpy.Preview",nullptr};
    wc.hIcon=static_cast<HICON>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED));
    wc.hIconSm=static_cast<HICON>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED));
    RegisterClassExW(&wc);RECT bounds{0,0,width,height};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
    m_hwnd=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"RSMK preview - sample data, game actions disabled",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,m_module,this);
    if(!m_hwnd||!CreateDevice())return 3;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
    LoadUiFontFamilies(io);ImGui_ImplWin32_Init(m_hwnd);ImGui_ImplDX11_Init(m_device,m_context);LoadBrandTextures(m_device);
    HelpyEnvironment::Configure({{"Storefront","Game Pass/WinGDK"},{"RuneSchemaVersion","0.8.0.0"},{"GameVersion","Preview build"}});
    HelpySourceCache::Current.Key="preview-fixture";HelpyImGuiTheme::Loaded=true;HelpyImGuiTheme::Current=HelpyImGuiTheme::Theme{};HelpyImGuiTheme::Current.autoReload=false;
    HelpyImGuiTheme::Current.workspaces={true,true,true,true};
    HelpyImGuiTheme::ForceReload=false; // Keep this isolated fixture's explicit options.
    HelpyImGuiTheme::NextPoll=std::chrono::steady_clock::now()+std::chrono::hours(24);
    // Exercise lazy font loading through the production path. Do not restore
    // the former eager loading of every settings choice at application start.
    if(io.Fonts->Fonts.Size>2)throw std::runtime_error("Startup loaded unselected font families");
    const auto initialFont=UiRegularFont;
    if(const auto* arial=FindUiFontFamily("Arial");arial&&arial->Available){
        SelectUiFontFamily("Arial");const auto count=io.Fonts->Fonts.Size;const auto selected=UiRegularFont;
        SelectUiFontFamily("Arial");if(UiRegularFont!=selected||io.Fonts->Fonts.Size!=count)throw std::runtime_error("Repeated font selection reloaded a font");
    }
    SelectUiFontFamily("not-installed-font");if(UiRegularFont!=initialFont)throw std::runtime_error("Missing font did not use the startup fallback");
    nlohmann::json items=nlohmann::json::array(),definitions=nlohmann::json::array();
    constexpr const char* names[]{"Bronze sword","Iron pickaxe","Oak logs","Copper ore","Iron platebody","Leather boots","Healing potion","Wild berries","Steel axe","Rune staff","Ashwood bow","Bronze arrows","Campfire kit","Stone blocks","Wool cloth","Copper bar","Iron shield","Traveler's cape","Map fragment","Carved charm"};
    for(int i=0;i<40;++i){const auto id=std::to_string(i);items.push_back({{"Path","/Preview/Items/Item"+id+".Item"+id},{"Name",std::string(names[i%20])+(i>=20?" II":"")},{"Available",true},{"Cooked",true},{"PowerLevel",i*3},{"MaxStackSize",100},{"Category","Sample"}});}
    items[0]["Masterwork"]=true;items[0]["Name"]="A sample stone";items[0]["Icon"]="/Game/Art/UI/Icons/Resources_ConceptArt/T_Icon_Resource_Stone.T_Icon_Resource_Stone";
    for(size_t i=0;i<items.size();++i){items[i]["SourceKind"]=i%3==0?"vanilla":i%3==1?"expansion":"modded";items[i]["SourceName"]=i%3==0?"Dragonwilds":i%3==1?"Agility":"Example mod";}
    const char* familyNames[]{"Fallen Hoplite's Helm (Dyed Chrome)","Fallen Hoplite's Chestplate (Dyed Chrome)","Fallen Hoplite's Greaves (Dyed Chrome)","Fallen Hoplite's Helm (Dyed Gold)"};
    for(int i=1;i<=4;++i){items[i]["Name"]=familyNames[i-1];items[i]["SourceKind"]="modded";items[i]["SourceName"]="Dye";}
    for(const auto* type:{"NPC","AI","Resource"})for(int i=0;i<8;++i){const auto id=std::string(type)+std::to_string(i);definitions.push_back({{"Key",id},{"Name",std::string(type==std::string_view("Resource")?"Oak tree":type==std::string_view("NPC")?"Village trader":"Wild enemy")+" "+std::to_string(i+1)},{"Class","/Preview/Actors/"+id+"."+id+"_C"},{"Type",type},{"ResourceFamily","Tree"},{"Available",true},{"Loaded",true},{"TemporaryAllowed",false},{"PermanentAllowed",false},{"Reason","Preview data; gameplay actions disabled"}});}
    for(auto& definition:definitions){definition["SourceName"]="Fixture source";if(definition["Type"]=="AI")definition["SpawnableCharacter"]=true;}
    MapPanel::Data.PartyDetached=false; // Exercise the default party rows beneath the map.
    nlohmann::json snapshot={{"Status","Sample data preview - game actions disabled."},{"Items",items},{"Definitions",definitions},{"Players",nlohmann::json::array({{{"Path","preview-player"},{"Name","Preview player"}}})},{"CatalogReady",true},{"CatalogStatus","Sample catalog for layout review"}};
    snapshot["Players"][0]["RoleStatus"]=PlayerStatus::Describe(14,{1,2,4,8,128},true);
    std::deque<nlohmann::json> queries;uint64_t revision=10;bool previewLoading=false;
    std::map<uint64_t,nlohmann::json> detailReceipts;
    nlohmann::json mapPlacement,authoringRequest;
    size_t mapProbeCount=0;bool previewTravelWaiting=false;
    const auto mapFixture=HostServices::CacheDirectory()/"map-fixture.rsicon";std::filesystem::create_directories(mapFixture.parent_path());
    {std::ofstream file(mapFixture,std::ios::binary);file.write("RSI3",4);const uint32_t width=8,height=8,bytes=256;file.write(reinterpret_cast<const char*>(&width),4);file.write(reinterpret_cast<const char*>(&height),4);file.write(reinterpret_cast<const char*>(&bytes),4);
        for(int i=0;i<64;++i){const unsigned char pixel[4]{static_cast<unsigned char>((i+i/8)%2?37:44),50,65,255};file.write(reinterpret_cast<const char*>(pixel),4);}}
    SpawnToolRequests::RemoteSubmit=[&](nlohmann::json request){
        const auto action=request.value("Action",std::string{});const auto id=request.value("_RequestId",uint64_t{});
        if(action=="MapSnapshot"){
            MapProjection::Projection projection{{198000,91450},{0,1},{1,0},210000,210000,-90000,90000};
            detailReceipts[id]={{"MapSnapshot",{{"BridgeVersion",1},{"CanBuildAndTeleport",true},{"PartyTravelVersion",2},{"World","/Preview/World"},{"Projection",projection.ToJson()},{"Image",{{"File",mapFixture.string()},{"Status","Ready"}}},
                {"Markers",nlohmann::json::array({{{"Name","Test lodestone"},{"Path","preview-lodestone"},{"Kind","Lodestone"},{"Managed",false},{"Location",{230000,125000,100}}},{{"Name","Placed iron node"},{"Path","preview-node"},{"Kind","Resource"},{"Managed",true},{"Location",{155000,125000,100}}}})},
                {"Players",nlohmann::json::array({{{"Name","Preview player"},{"Path","preview-player"},{"Guid","host-guid"},{"IsLocal",true},{"Location",{198000,91450,100}}},{{"Name","Party member"},{"Path","preview-other"},{"Guid","guest-guid"},{"IsLocal",false},{"Location",{210000,100000,100}}}})}}}};return true;
        }
        if(action=="InspectClone"&&!request.value("InspectDetails",false)){
            detailReceipts[id]={{"CloneSource",{{"Source",request.at("Source")},{"SoftDeleteField","bSoftDelete"},{"Fields",nlohmann::json::array({{{"Name","PowerLevel"},{"NativeName","PowerLevel"},{"Editable",true},{"HasValue",true},{"Value",5}},{{"Name","@row/WeaponStats/DamageMultiplier"},{"NativeName","DamageMultiplier"},{"Editable",true},{"HasValue",true},{"Value",1.2}}})}}}};return true;
        }
        if(action=="PrepareClone"){
            authoringRequest=request;nlohmann::json prepared={{"State","Prepared"},{"InternalName",ClonePresentation::InternalName(request.at("PersistenceID").get<std::string>())}};
            prepared["Documents"]["assets"]["/Preview/Clones/Result.Result"]={{"$Clone",request.at("Source")},{"PersistenceID",request.at("PersistenceID")}};
            prepared["Documents"]["raw"]={{"/Preview/Stats.Table",{{request.at("PersistenceID").get<std::string>(),{{"DamageMultiplier",3.5}}}}}};
            detailReceipts[id]={{"CloneResult",prepared},{"Status","Synthetic export prepared; no game actions called."}};return true;
        }
        if(action=="MapProbe"){++mapProbeCount;const auto& point=request.at("TargetLocation");detailReceipts[id]={{"MapPoint",{{"GroundAvailable",true},{"Location",{point["X"],point["Y"],100}}}}};return true;}
        if(action=="MapBuildings"){detailReceipts[id]={{"MapBuildings",{{"Rows",nlohmann::json::array({
            {{"Path","/Preview/Buildings/TeleportStone.TeleportStone"},{"Name","Teleport stone"},{"Type","Building"},{"CategoryId",0},{"Category","Utility"},{"Group","Travel"},{"Icon",items[0]["Icon"]}},
            {{"Path","/Preview/Buildings/IronChest.IronChest"},{"Name","Iron Chest"},{"Type","Building"},{"CategoryId",1},{"Category","Furniture"},{"Group","Storage"},{"Icon",items[0]["Icon"]}}})},
            {"Categories",nlohmann::json::array({{{"Id",0},{"Name","Utility"}},{{"Id",1},{"Name","Furniture"}}})},{"NextOffset",2},{"HasMore",false}}}};return true;}
        if(action=="MapTeleport"&&request.value("_Source",std::string{})=="HelpyMapParty"){mapPlacement=request;detailReceipts[id]={{"Status","Synthetic party travel checked; no game action called."}};return true;}
        if((action=="Spawn"||action=="SpawnBuilding"||action=="ImportBuildings"||action=="MapTeleport")&&request.value("_Source",std::string{})=="HelpyMap"){
            mapPlacement=request;detailReceipts[id]={{"Status","Synthetic map action checked; no game action called."}};
            if(action=="SpawnBuilding")detailReceipts[id]["SpawnResult"]={{"Active",true},{"Permanent",request.value("Permanent",false)},{"Building",request.at("Building")},{"Location",{request["TargetLocation"]["X"],request["TargetLocation"]["Y"],100}}};
            if(action=="ImportBuildings")detailReceipts[id]["SpawnResult"]={{"Active",true},{"Permanent",true},{"Created",request["Document"]["pieces"].size()},{"Location",{request["TargetLocation"]["X"],request["TargetLocation"]["Y"],100}}};
            return true;}
        if(action=="RecipeWorkspaceCatalog"){
            detailReceipts[id]={{"RecipeWorkspaceCatalog",{{"Stations",nlohmann::json::array({{{"Id","preview-station"},{"DisplayName","Crafting station"},{"Icon",QuickDecorations::CraftingStationIcon},{"Path","/Preview/Station"}}})},{"Recipes",nlohmann::json::array()}}}};return true;
        }
        if(action=="InspectPlayerInventory"||action=="InspectContainerInventory"){
            const bool storage=action=="InspectContainerInventory"||request.value("Personal",false);const int offset=request.value("Offset",0);
            auto rows=nlohmann::json::array();if(offset==0){rows.push_back({{"Name","Ash Logs"},{"Path","/Preview/Items/Ash.Ash"},{"Icon",items[0]["Icon"]},{"Slot",0},{"Count",12},{"Group","Quick slots"}});rows.push_back({{"Name","Copper ore"},{"Path",items[3]["Path"]},{"Icon",items[0]["Icon"]},{"Slot",8},{"Count",5},{"Group","Inventory"}});}
            else{rows.push_back({{"Name","Fire Rune"},{"Path",items[9]["Path"]},{"Icon",items[0]["Icon"]},{"Slot",40},{"Count",6},{"Group","Runes"}});rows.push_back({{"Name","Wooden Arrow"},{"Path",items[11]["Path"]},{"Icon",items[0]["Icon"]},{"Slot",48},{"Count",20},{"Group","Ammo"}});}
            detailReceipts[id]={{"InventorySnapshot",{{"Items",rows},{"Loadout",nlohmann::json::array()},{"Offset",offset},{"NextOffset",offset+32},{"Player",request.value("Player",std::string{})},{"PlayerGuid",request.value("PlayerGuid",std::string{})},{"Generation",uint64_t(1)},{"Container",action=="InspectContainerInventory"},{"LockState","Unknown"},{"CanDeposit",false},{"CanOverrideUnlock",false},{"Personal",request.value("Personal",false)},{"ObservedUnixMs",PartyMemberView::Now()},{"HasMore",!storage&&offset==0},{"Slots",storage?32:64}}},{"Status","Synthetic native inventory; no game actions called."}};return true;
        }
        if(request.value("Action",std::string{})!="InspectClone"||!request.value("InspectDetails",false))return false;
        const auto source=request.value("Source",std::string{});
        detailReceipts[request.at("_RequestId").get<uint64_t>()]={{"ItemDetails",{{"Source",source},{"Name","Stone"},{"PowerLevel",0},
            {"Fields",nlohmann::json::array({{{"NativeName","Description"},{"Value","Sample item description for hover-card layout review."},{"HasValue",true}},{{"NativeName","MaxStackSize"},{"Value",100},{"HasValue",true}},{{"NativeName","WindUpMaxMovementSpeedOverride"},{"Value",-1},{"HasValue",true}},{{"NativeName","PreferredLockOnCameraAimMode"},{"Value","ELockOnCameraAimMode::Default"},{"HasValue",true}},{{"NativeName","AnimationPoseType"},{"Value","EAnimationPoseType::Default"},{"HasValue",true}}})}}}};return true;
    };SpawnToolRequests::RemoteAvailable=[] {return true;};
    SpawnToolRequests::RemoteCompleted=[&](uint64_t id,nlohmann::json& receipt){const auto found=detailReceipts.find(id);if(found==detailReceipts.end())return false;receipt=std::move(found->second);detailReceipts.erase(found);return true;};
    SpawnToolRequests::RemoteWaiting=[&] {return previewTravelWaiting;};SpawnToolRequests::RemoteGeneration=[] {return uint64_t{1};};
    SpawnToolRequests::RemoteMapGroundSubmit=[&](nlohmann::json request){const auto& point=request.at("TargetLocation");
        snapshot["MapGround"]={{"RequestId",request.at("RequestId")},{"Generation",uint64_t(1)},{"GroundAvailable",true},{"Location",{point["X"],point["Y"],1234}}};return true;};
    SpawnToolRequests::RemoteReadQuick=[&](uint64_t& seen,int,nlohmann::json& out){out=snapshot;out["PlayerRoles"]=PlayerStatus::Snapshot(snapshot["Players"],1);
        if(previewLoading){out["Items"]=nlohmann::json::array();seen=++revision;return true;}
        if(!queries.empty()){out["_QueryResults"]=nlohmann::json::array();while(!queries.empty()){
            auto request=std::move(queries.front());queries.pop_front();const int category=request.value("Category",0);auto window=request;
            auto rows=category==0?items:definitions;
            if(category==0){auto query=request.value("Query",std::string{});const auto source=ItemSource::TakeFilter(query);
                if(query.starts_with("__helpycat:")){const auto split=query.find(' ');query=split==query.npos?std::string{}:query.substr(split+1);}
                const auto family=ItemFamily::TakeFilter(query);auto filtered=nlohmann::json::array();
                for(const auto& row:rows)if(ItemSource::Matches(source,ItemSource::Row(row))&&ItemFamily::Matches(family,row.value("Name",std::string{})))filtered.push_back(row);
                rows=std::move(filtered);
            }
            window["ClientGeneration"]=1;window["HasMore"]=false;window["Reset"]=true;window["Rows"]=std::move(rows);window["Coverage"]="Sample metadata";
            out["_QueryResults"].push_back({{"CatalogWindow",window}});
        }}seen=++revision;return true;};
    LazyCatalog::ConfigureRemote([&](nlohmann::json request){queries.push_back(std::move(request));return true;},[](int,uint64_t){});
    RuneSchemaSettingsPanel::Remote=[](const nlohmann::json& request,nlohmann::json& reply){
        if(request.value("Action",std::string{})=="CrashDumpRead"||request.value("Action",std::string{})=="CrashDumpSave"){
            static bool full=false;if(request.at("Action")=="CrashDumpSave")full=request.at("FullMemoryDump").get<bool>();
            reply={{"Ok",true},{"ExpectedContents","preview-crash-settings"},{"FullMemoryDump",full},{"EnableDumping",true},{"RestartRequired",true},{"Status","Preview only; no UE4SS configuration was changed."}};return true;
        }
        if(request.value("Action",std::string{})=="AdditionalLoggingRead"){
            reply={{"Ok",true},{"ExpectedContents","preview"},{"Active",false},{"Document",{{"enabled",false},{"filePaths",false},{"region",false},{"networkRates",false},{"savePaths",false},{"averageLatency",false}}}};return true;
        }
        if(request.value("Action",std::string{})=="QuickLoadRead"){
            reply={{"Ok",true},{"ExpectedContents",""},{"Document",{{"Mode","vanilla"},{"CharacterName","BREWER"},{"WorldName","Preview world"}}},{"DedicatedServer",HelpyEnvironment::Server()},{"Characters",nlohmann::json::array({{{"Name","BREWER"}}})},{"Worlds",nlohmann::json::array({{{"Name","Preview world"}}})},{"Status","Synthetic save choices; no native load or file write."}};return true;
        }
        if(request.value("Action",std::string{})=="NetworkStatus"){
            reply={{"Generation",SpawnToolRequests::CurrentGeneration()},{"Available",true},{"Waiting",false},{"Fresh",true},{"AgeMs",0},
                {"Role",HelpyEnvironment::Server()?"dedicated-server":RuntimeConnectionPanel::PreviewRole},{"KnownPlayers",2}};return true;
        }
        if(request.value("Action",std::string{})=="StartupStats"){
            reply={{"Available",true},{"Complete",true},{"Dropped",0},{"Checkpoints",nlohmann::json::array({
                {{"Stage","construct loaders begin"},{"ElapsedMs",0}},{{"Stage","InitCore complete"},{"ElapsedMs",260}},
                {{"Stage","GameInstanceInit loaders complete (preview fixture)"},{"ElapsedMs",640}}})}};return true;
        }
        if(request.value("Action",std::string{})!="SettingsRead")return false;
        reply={{"Ok",true},{"ExpectedContents","preview"},{"Path","Preview only / settings/settings.ini"},
            {"Document",{{"helpyHotkey","F2"},{"advancedLogging",false},{"archiveLogs",false},{"autoUpdateMappings",true},{"authoringTools",true},{"persistence",{{"allowCorruptCharacterEntry",false},{"quests",true}}}}},
            {"Editable",{{"/helpyHotkey","F2"},{"/advancedLogging",false},{"/archiveLogs",false},{"/autoUpdateMappings",true},{"/authoringTools",true},{"/persistence/allowCorruptCharacterEntry",false},{"/persistence/quests",true}}},
            {"BypassStatus","Disabled for this preview session"}};return true;
    };
    SpawnToolsPanel::SyncCartWorld();SpawnToolsPanel::SelectedPlayer="preview-player";
    int result=0;
    try {
        if(capture){
            if(!BrandTexture||!GameBrandTexture||!SteamTexture||!EpicTexture||!XboxTexture||!LoadingRingTexture||!SplashSprites[0].Texture||!SplashSprites[1].Texture||!SplashSprites[2].Texture)throw std::runtime_error("Packaged application, split logo, loading ring or platform logo failed to load");
            UINT wordmarkWidth=0,wordmarkHeight=0;std::vector<unsigned char> wordmarkPixels;
            if(!ReadBrandPixels("dragonwilds.png",wordmarkWidth,wordmarkHeight,wordmarkPixels)||wordmarkWidth!=HelpyBranding::GameWordmark::Width||wordmarkHeight!=HelpyBranding::GameWordmark::Height)throw std::runtime_error("Dragonwilds wordmark must use the full-resolution splash artwork, not a thumbnail");
            std::filesystem::create_directories(output);std::error_code ignored;
            std::filesystem::remove(output/"render-error.txt",ignored);std::filesystem::remove(output/"render-result.txt",ignored);PreviewSelectSections=true;
            LiveLogPanel::Preview=true;LiveLogPanel::History.Clear();
            LiveLogPanel::History.Append("[2026-10-16T12:30:00.000Z thread=14] [RuneSchema] [summary] recipes loaded=46515 errors=0");
            LiveLogPanel::History.Append("[2026-10-16T12:30:01.000Z thread=14] [RuneSchema] [warning] [PERFORMANCE][SLOW-WORK] phase=npcs.spawn:ExampleVendor elapsed_ms=185");
            LiveLogPanel::History.Append("[2026-10-16T12:30:02.000Z thread=14] [RuneSchema] [error] [PERSISTENCE-PRUNER] Duplicate quest owner marker. ExampleMod and OldExampleMod declare the same quest.");
            LiveLogPanel::History.Append("[2026-10-18T12:30:03.000Z thread=14] [RuneSchema] [warning] [RS-CACHE-001] Source changed: ExampleMod. Previous presentation retained.");
            LiveLogPanel::History.Append("[2026-10-18T12:30:04.000Z thread=14] [RuneSchema] [warning] [RS-CACHE-001] Source changed: ExampleMod. Previous presentation retained.");
            LiveLogPanel::History.Append("[2026-10-18T12:30:05.000Z thread=14] [RuneSchema] [warning] [RS-CACHE-001] Source changed: ExampleMod. Previous presentation retained.");
            LiveLogPanel::Selected=LiveLogPanel::History.Rows.back().Id;
            LiveLogPanel::Streams[1].History.Clear();
            LiveLogPanel::Streams[1].History.Append("[2026-10-17 12:30:00] UE4SS - preview fixture (not an installed build)");
            LiveLogPanel::Streams[1].History.Append("[2026-10-17 12:30:01] [Lua] [ExampleMod] Fixture addon initialized");
            LiveLogPanel::Streams[1].History.Append("[2026-10-17 12:30:02] [RegisterHook] Fixture callback registered");
            LiveLogPanel::Streams[2].History.Clear();LiveLogPanel::Streams[2].History.Append("[2026.10.18-12.30.01:123][0]LogInit: Display: Preview game log fixture");
            LiveLogPanel::Streams[2].History.Append("[2026.10.18-12.30.02:123][0]LogEOSSDK: Warning: Preview connection delayed");
            LiveLogPanel::Streams[3].History.Clear();LiveLogPanel::Streams[3].History.Append("[2026.10.18-12.30.02:123][0]LogEOSSDK: Warning: Preview connection delayed");
            {
                SplashEntered=false;SplashExit={};SplashStarted=ImGui::GetTime();
                for(int i=0;i<3;++i)RenderFrame();
                RECT clientBounds{};GetClientRect(m_hwnd,&clientBounds);width=clientBounds.right;height=clientBounds.bottom;
                constexpr const char* controls[]{"main/minimize","main/maximize","main/close"};
                constexpr int hits[]{HTMINBUTTON,HTMAXBUTTON,HTCLOSE};
                float previousRight=0;
                for(int column=0;column<3;++column){const auto rect=HelpyPreviewHooks::WindowControls.at(controls[column]);
                    if(rect.x<previousRight||rect.z>width||rect.y<0||rect.w>NativeCaption.Height||rect.z<=rect.x||rect.w<=rect.y||
                        NativeCaption.HitTest(int((rect.x+rect.z)*.5f),int((rect.y+rect.w)*.5f),width,false)!=hits[column])throw std::runtime_error("Visible caption controls must match native hit targets in the top-right caption");
                    previousRight=rect.z;
                }
                PreviewCapture=output/"native-window-controls.png";RenderFrame();PreviewCapture.clear();
                if(!HelpyPreviewHooks::SplashVisible||PreviewSidebarVisible||LazyCatalog::For(0).Rows.empty())throw std::runtime_error("Splash must hide navigation without blocking cache-first catalogue loading");
                for(const auto& [time,name]:std::array{std::pair{.5,"splash-dragonwilds"},std::pair{6.0,"splash-blend"},std::pair{10.0,"splash-runeschema"},std::pair{14.0,"splash-blend-back"}}){
                    HelpyPreviewHooks::SplashElapsed=time;
                    SplashStarted=ImGui::GetTime()-time;RenderFrame();PreviewCapture=output/(std::string(name)+".png");RenderFrame();PreviewCapture.clear();
                    if(std::abs(HelpyPreviewHooks::SplashBlend-HelpySplash::Blend(time,HelpyPreviewHooks::SplashAnimated))>.12f)throw std::runtime_error("Splash rendered the wrong hold/blend phase");
                    const auto badge=HelpyPreviewHooks::RuntimeBadge;
                    if(badge.z>badge.x&&badge.w>badge.y){
                        if(std::abs((badge.x+badge.z)*.5f-HelpyPreviewHooks::SplashCenterX)>1.f||badge.w>height||
                            (!HelpyPreviewHooks::SplashTipLines.empty()&&badge.y<HelpyPreviewHooks::SplashTipLines.back().w))
                            throw std::runtime_error("Splash platform badge must be centered below the tips inside the viewport: "+nlohmann::json{{"Width",width},{"Height",height},{"Badge",{badge.x,badge.y,badge.z,badge.w}},{"Center",HelpyPreviewHooks::SplashCenterX}}.dump());
                    }
                }
                const auto accent=HelpyImGuiTheme::Current.accent;HelpyImGuiTheme::Current.accent={.12f,.5f,1.f,.3f};SplashStarted=ImGui::GetTime()-6.0;
                HelpyPreviewHooks::SplashElapsed=6.0;
                PreviewCapture=output/"splash-blend-blue.png";RenderFrame();PreviewCapture.clear();HelpyImGuiTheme::Current.accent=accent;
                for(const auto& [time,name]:std::array{std::pair{10.0,"runeschema"},std::pair{.5,"dragonwilds"},std::pair{6.2,"blend"}}){
                    HelpyPreviewHooks::SplashElapsed=time;
                    SplashEntered=false;SplashExit={};SplashStarted=ImGui::GetTime()-time;RenderFrame();RenderFrame();
                    const auto enter=HelpyPreviewHooks::SplashEnter;
                    if(enter.x<0||enter.y<0||enter.z>width||enter.w>height||enter.z<=enter.x||enter.w<=enter.y)throw std::runtime_error("Splash Enter button escaped the visible surface");
                    HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={(enter.x+enter.z)*.5f,(enter.y+enter.w)*.5f};HelpyPreviewHooks::MouseDown=false;RenderFrame();
                    HelpyPreviewHooks::MouseDown=true;RenderFrame();HelpyPreviewHooks::MouseDown=false;RenderFrame();
                    if(HelpyPreviewHooks::SplashAnimated){
                        if(!SplashExit.Active||SplashEntered)throw std::runtime_error("Enter must start a bounded exit rather than jump directly to Home");
                        const auto kind=SplashExit.Kind;const auto started=SplashExit.Started;
                        if(SplashExit.Begin(ImGui::GetTime(),1-SplashExit.InitialBlend)||SplashExit.Started!=started)throw std::runtime_error("Repeated Enter restarted the exit animation");
                        if(std::string_view(name)=="runeschema"&&kind!=HelpySplash::ExitKind::RuneSchema)throw std::runtime_error("RuneSchema must use its bracket exit");
                        if(std::string_view(name)=="dragonwilds"&&kind!=HelpySplash::ExitKind::Dragonwilds)throw std::runtime_error("Dragonwilds must use its vertical wordmark exit");
                        SplashExit.Started=ImGui::GetTime()-HelpySplash::ExitSeconds*.25;PreviewCapture=output/(std::string("splash-exit-")+name+".png");RenderFrame();PreviewCapture.clear();
                        SplashExit.Started=ImGui::GetTime()-HelpySplash::ExitSeconds-1;RenderFrame();
                    }
                    HomeRevealStarted=ImGui::GetTime()-HelpySplash::RevealSeconds;RenderFrame();
                    if(!SplashEntered||CurrentPage!=Page::Home||HelpyPreviewHooks::SplashVisible||PreviewSidebarVisible)throw std::runtime_error("Completed exit must reveal Home without reopening the splash or navigation rail");
                }
                HelpyPreviewHooks::MouseEnabled=false;HelpyPreviewHooks::Mouse={20,20};
                HelpyPreviewHooks::SplashElapsed.reset();
            }
            {
                // Lean-package first launch: no bitmap logo is necessary to
                // enter Helpy or read the optional download hint and tips.
                auto* savedBrand=BrandTexture;auto* savedAccent=BrandAccentTexture;auto* savedGame=GameBrandTexture;
                BrandTexture=BrandAccentTexture=GameBrandTexture=nullptr;
                SplashEntered=false;SplashExit={};HelpyPreviewHooks::ForceArtworkHint=true;HelpyPreviewHooks::SplashElapsed=8.0;
                RenderFrame();PreviewCapture=output/"splash-first-run-artwork.png";RenderFrame();PreviewCapture.clear();
                for(const auto& rect:{HelpyPreviewHooks::ArtworkDownload,HelpyPreviewHooks::ArtworkHide,HelpyPreviewHooks::SplashEnter}){
                    if(rect.x<0||rect.y<0||rect.z>width||rect.w>height||rect.z<=rect.x||rect.w<=rect.y)throw std::runtime_error("Offline first-run controls escaped the viewport");
                    if(std::abs((rect.x+rect.z)*.5f-HelpyPreviewHooks::SplashCenterX)>1.f)throw std::runtime_error("Splash button is not centered");
                }
                if(HelpyPreviewHooks::SplashTipIndex!=1)throw std::runtime_error("Splash tip did not rotate independently of its logo");
                for(size_t tip=0;tip<HelpySplash::Tips.size();++tip){
                    HelpyPreviewHooks::SplashElapsed=tip*7.0+1;RenderFrame();
                    if(HelpyPreviewHooks::SplashTipLines.empty())throw std::runtime_error("Splash tip missing");
                    for(const auto& line:HelpyPreviewHooks::SplashTipLines)
                        if(std::abs((line.x+line.z)*.5f-HelpyPreviewHooks::SplashCenterX)>1.f||line.x<0||line.z>width)throw std::runtime_error("Wrapped splash tip line is not centered");
                }
                const auto hide=HelpyPreviewHooks::ArtworkHide;
                HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={(hide.x+hide.z)*.5f,(hide.y+hide.w)*.5f};
                HelpyPreviewHooks::MouseDown=false;RenderFrame();HelpyPreviewHooks::MouseDown=true;RenderFrame();HelpyPreviewHooks::MouseDown=false;RenderFrame();RenderFrame();
                if(HelpyPreviewHooks::ArtworkHintVisible||!HelpyDownloads::Current().HintDismissed||Toasts.empty()||Toasts.back().Text.find("Settings > Updates & runtime > Initial assets")==std::string::npos)throw std::runtime_error("Hiding initial-assets prompt must persist and explain Settings access");
                PreviewCapture=output/"splash-artwork-hidden.png";RenderFrame();PreviewCapture.clear();
                Toasts.clear();HelpyPreviewHooks::MouseEnabled=false;
                BrandTexture=savedBrand;BrandAccentTexture=savedAccent;GameBrandTexture=savedGame;
                HelpyPreviewHooks::ForceArtworkHint=false;HelpyPreviewHooks::SplashElapsed.reset();SplashEntered=true;SplashExit={};CurrentPage=Page::Home;
            }
            {
                CurrentPage=Page::Home;RenderFrame();RenderFrame();previewLoading=true;
                auto& loading=LazyCatalog::For(0);const auto saved=loading;loading.Started=true;loading.Pending=true;loading.RequestedAt=std::chrono::steady_clock::now();loading.Rows=nlohmann::json::array();loading.HasMore=true;loading.PreparingAlphabetical=true;loading.PopulationStage="discovering-items";loading.PopulationDone=0;
                const auto homeRings=HelpyPreviewHooks::LoadingRings;
                CurrentPage=Page::Home;RenderFrame();PreviewCapture=output/"home-loading.png";RenderFrame();PreviewCapture.clear();
                if(HelpyPreviewHooks::LoadingRings==homeRings)throw std::runtime_error("Home loading fixture failed to draw its centered ring");
                const auto profile=HelpyPreviewHooks::Navigation.at("Profile"),orb=HelpyPreviewHooks::LoadingRect;
                if(std::abs(profile.y-PreviewHomeQuick.y)>1||profile.x<PreviewHomeQuick.z||std::abs((PreviewHomeQuick.x+profile.z)*.5f-PreviewHomeCenter)>2)throw std::runtime_error("Home Profile and Quick settings must share a centered row");
                if(orb.y<std::max(profile.w,PreviewHomeQuick.w)+HelpyImGuiTheme::Current.Y(24))throw std::runtime_error("Home loading orb must sit below the action row with breathing room");
                HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={(orb.x+orb.z)*.5f,(orb.y+orb.w)*.5f};for(int i=0;i<4;++i)RenderFrame();
                const auto homeDetails=HelpyPreviewHooks::LoadingDetailsRect;
                if(std::abs((homeDetails.x+homeDetails.z-orb.x-orb.z)*.5f)>2)throw std::runtime_error("Home loading tooltip must be anchored to its ring, not previous navigation");
                PreviewCapture=output/"home-loading-hover.png";RenderFrame();PreviewCapture.clear();HelpyPreviewHooks::MouseEnabled=false;
                const auto bayRings=HelpyPreviewHooks::LoadingRings;
                CurrentPage=Page::Items;RenderFrame();PreviewCapture=output/"item-service-loading.png";RenderFrame();PreviewCapture.clear();
                if(HelpyPreviewHooks::LoadingRings==bayRings)throw std::runtime_error("Empty Item Bay failed to draw its centered loading ring");
                if(HelpyPreviewHooks::LoadingRings-bayRings!=2)throw std::runtime_error("Empty Item Bay must show only one loading wheel per frame");
                const auto ring=HelpyPreviewHooks::LoadingRect;const auto detailCount=HelpyPreviewHooks::LoadingDetailsShown;
                HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={(ring.x+ring.z)*.5f,(ring.y+ring.w)*.5f};RenderFrame();RenderFrame();
                if(HelpyPreviewHooks::LoadingDetailsShown==detailCount||HelpyPreviewHooks::LoadingDetailsText.find("Stage:")==std::string::npos)throw std::runtime_error("Loading wheel hover must expose stage and work details");
                RenderFrame();RenderFrame();const auto details=HelpyPreviewHooks::LoadingDetailsRect;
                if(std::abs((details.x+details.z-ring.x-ring.z)*.5f)>2)throw std::runtime_error("Item loading tooltip must be anchored to its own ring");
                PreviewCapture=output/"item-service-loading-hover.png";RenderFrame();PreviewCapture.clear();
                HelpyPreviewHooks::MouseEnabled=false;HelpyPreviewHooks::Mouse={-100,-100};
                loading=saved;previewLoading=false;
            }
            const std::array pairs{std::pair{Page::Home,"home"},std::pair{Page::Items,"item-service"},std::pair{Page::Creation,"transmog-creation"},std::pair{Page::Recipes,"recipes"},std::pair{Page::Spawns,"spawns"},std::pair{Page::Map,"map"},std::pair{Page::Tools,"tools"},std::pair{Page::Authoring,"authoring-inactive"},std::pair{Page::Settings,"settings"},std::pair{Page::LiveLog,"live-log"}};
            {
                CurrentPage=Page::Settings;PreviewBackgroundsTab=true;HelpyBackgrounds::DraftReady=true;HelpyBackgrounds::Selection="RuneSchema.png";HelpyBackgrounds::SettingsVisible=true;
                for(int i=0;i<120&&!HelpyBackgrounds::Find("RuneSchema.png");++i){RenderFrame();Sleep(5);}
                if(!HelpyBackgrounds::Find("RuneSchema.png"))throw std::runtime_error("Bundled background preview did not decode asynchronously");
                if(std::find(HelpyBackgrounds::Files.begin(),HelpyBackgrounds::Files.end(),"Dragonwilds.png")==HelpyBackgrounds::Files.end())throw std::runtime_error("Background choices did not include both bundled pictures");
                RenderFrame();const auto picture=HelpyPreviewHooks::BackgroundPicture;
                if(picture.z<=picture.x||picture.w<=picture.y)throw std::runtime_error("Background settings did not show the actual picture");
                const auto uploads=HelpyBackgrounds::Uploads;for(int i=0;i<4;++i)RenderFrame();
                if(uploads!=HelpyBackgrounds::Uploads)throw std::runtime_error("Unchanged backgrounds must not upload repeatedly");
                HelpyBackgrounds::Preview=true;HelpyBackgrounds::DraftStrength=.2f;
                PreviewCapture=output/"background-settings.png";RenderFrame();PreviewCapture.clear();
                BYTE pictureOpacity=0;DWORD pictureFlags=0;COLORREF pictureKey=0;
                if(!GetLayeredWindowAttributes(m_hwnd,&pictureKey,&pictureOpacity,&pictureFlags)||pictureOpacity!=255||!HelpyBackgrounds::Opaque())throw std::runtime_error("A picture background must block underlying native windows");
                if(!HelpyPreviewHooks::BackgroundDraws||!HelpyImGuiTheme::Current.backgroundFile.empty())throw std::runtime_error("Background preview must draw without saving the draft");
                CurrentPage=Page::Home;PreviewCapture=output/"background-home.png";RenderFrame();PreviewCapture.clear();
                if(!HelpyPreviewHooks::BackgroundDraws)throw std::runtime_error("Home must use the same preview background");
                HelpyBackgrounds::Preview=false;HelpyBackgrounds::DraftStrength=.12f;PreviewBackgroundsTab=false;RenderFrame();
                BYTE glassOpacity=0;DWORD glassFlags=0;COLORREF glassKey=0;
                if(!GetLayeredWindowAttributes(m_hwnd,&glassKey,&glassOpacity,&glassFlags)||glassOpacity!=static_cast<BYTE>(std::clamp(HelpyImGuiTheme::Current.windowOpacity,.6f,1.f)*255)||HelpyBackgrounds::Opaque())throw std::runtime_error("Clear glass must restore configured native transparency");
            }
            for(const auto& [page,name]:pairs){CurrentPage=page;for(int i=0;i<4;++i)RenderFrame();
                if(page==Page::Tools){const auto button=HelpyPreviewHooks::AuthorToolsDownload;
                    if(button.z<=button.x||button.x<0||button.y<0||button.z>width||button.w>height)throw std::runtime_error("Authoring-tools download control is missing or clipped");}
                PreviewCapture=output/(std::string(name)+".png");RenderFrame();PreviewCapture.clear();}
            {
                StructuredDiagnostic::Record record;
                record.EventId="preview-e1";record.SessionId="preview";record.Timestamp="2026-10-23T12:30:00.000Z";
                record.Code="RS-ASSET-001";record.Source="AssetLoader";record.Severity="Error";record.Outcome="Failed";
                record.Title="Fixture asset unavailable";record.Message="Synthetic diagnostic; no game action was attempted.";
                record.Context={{"ModId","ExampleMod"},{"File","assets/example.jsonc"},{"Target","/Game/Mods/Example/Item"},{"Phase","Resolution"}};
                record.Cause={{"Status","Unknown"}};
                const auto id=DiagnosticStore.AcceptStructured(record);
                if(!id)throw std::runtime_error("Structured preview diagnostic was rejected");
                DiagnosticStore.Accept(LiveLog::Parse("[RuneSchema] [warning] [WEARABLE-BRIDGE][NOT-ENROLLED] Example cape"));
                CurrentPage=Page::LiveLog;
                DiagnosticsPanel::Current=DiagnosticsPanel::View::Overview;DiagnosticsPanel::RequestedTab=0;
                for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"diagnostics-overview.png";RenderFrame();PreviewCapture.clear();
                {
                    ImGuiWindow* metric=nullptr;
                    for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::string_view(window->Name).find("##metric")!=std::string_view::npos){metric=window;break;}
                    if(!metric)throw std::runtime_error("Diagnostics metric card missing");
                    const auto oldMouse=HelpyPreviewHooks::Mouse;const bool oldEnabled=HelpyPreviewHooks::MouseEnabled;
                    HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={metric->Pos.x+metric->Size.x*.5f,metric->Pos.y+metric->Size.y*.5f};
                    for(int i=0;i<3;++i)RenderFrame();
                    ImGuiWindow* tooltip=nullptr;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&(window->Flags&ImGuiWindowFlags_Tooltip))tooltip=window;
                    if(!tooltip||tooltip->Pos.y<metric->Pos.y+metric->Size.y+HelpyImGuiTheme::Current.Y(4.0f)-1.f)throw std::runtime_error("Metric tooltip must sit below the complete card with a gap");
                    PreviewCapture=output/"diagnostics-metric-hover.png";RenderFrame();PreviewCapture.clear();
                    HelpyPreviewHooks::Mouse=oldMouse;HelpyPreviewHooks::MouseEnabled=oldEnabled;
                }
                DiagnosticsPanel::Current=DiagnosticsPanel::View::Issues;DiagnosticsPanel::RequestedTab=1;DiagnosticsPanel::SelectedGroup=id;
                for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"diagnostics-issue.png";RenderFrame();PreviewCapture.clear();
                DiagnosticsPanel::RequestReport(DiagnosticStore,DiagnosticsPanel::ExportKind::Issue,id);
                for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"diagnostics-report-review.png";RenderFrame();PreviewCapture.clear();
                ImGui::ClosePopupsOverWindow(nullptr,false);DiagnosticsPanel::Current=DiagnosticsPanel::View::LiveLog;DiagnosticsPanel::RequestedTab=2;
                for(int i=0;i<3;++i)RenderFrame();
                LiveLogPanel::PreviewOptions=true;for(int i=0;i<3;++i)RenderFrame();
                PreviewCapture=output/"diagnostics-crash-capture.png";RenderFrame();PreviewCapture.clear();
                LiveLogPanel::PreviewOptions=false;ImGui::ClosePopupsOverWindow(nullptr,false);
            }
            {
                using namespace ModsPanel;const auto oldSnapshot=Snapshot,oldRows=Rows;const auto oldStatus=Status;const auto oldHealth=Health;const auto oldRevision=HealthRevision;
                Rows=nlohmann::json::array();for(const auto* owner:{"Dye_Quests","Flintlocks","RSDWSpears"})Rows.push_back({{"Name",owner},{"Enabled",true},{"Present",true},{"Metadata",{{"version","1.0"},{"description","Synthetic mod-manager layout fixture"}}},{"Loaders",nlohmann::json::array({"RuneSchema","Unreal PAK"})},{"HasCache",true}});
                Snapshot={{"Ok",true},{"OrderEnabled",true},{"ExpectedExists",false},{"ExpectedContents",""},{"Rows",Rows}};Status.clear();PreviewChecked=true;
                Health.clear();Health["Flintlocks"].Warnings=1;Health["RSDWSpears"].Errors=1;HealthRevision=LiveLogPanel::Streams[0].History.Revision+LiveLogPanel::Streams[2].History.Revision;
                CurrentPage=Page::Mods;for(int i=0;i<400;++i){RenderFrame();Sleep(5);if(PreviewNativeBadges==3&&PreviewArtCards==3)break;}
                if(PreviewPlacards!=3||PreviewNativeBadges!=3||PreviewArtCards!=3)throw std::runtime_error("Mod placards must show their three native health glyphs and mod artwork, not just fallback shapes");
                PreviewCapture=output/"mods-placards.png";RenderFrame();PreviewCapture.clear();
                if(PreviewBadgeOrders!=std::vector<size_t>{1,2,3})throw std::runtime_error("Enabled mods must have consecutive load-order numbers");
                Rows[1]["Enabled"]=false;for(int i=0;i<3;++i)RenderFrame();
                if(PreviewBadgeOrders!=std::vector<size_t>{1,0,2}||PreviewBadgeTints[1]!=IM_COL32(0,0,0,255))throw std::runtime_error("Disabled mods must have a black unnumbered badge and not consume load order");
                PreviewCapture=output/"mods-disabled.png";RenderFrame();PreviewCapture.clear();
                std::strcpy(Search,"RSDWSpears");RenderFrame();
                if(PreviewBadgeOrders!=std::vector<size_t>{2})throw std::runtime_error("Filtering mods must preserve their full-list active load-order number");
                Search[0]=0;auto inactive=Rows[1];Rows.erase(Rows.begin()+1);Rows.insert(Rows.begin(),std::move(inactive));RenderFrame();
                if(PreviewBadgeOrders!=std::vector<size_t>{0,1,2})throw std::runtime_error("Reordering a disabled mod must not renumber enabled entries");
                Rows[0]["Enabled"]=true;RenderFrame();
                if(PreviewBadgeOrders!=std::vector<size_t>{1,2,3}||PreviewBadgeTints[0]!=IM_COL32_WHITE)throw std::runtime_error("Re-enabling a mod must restore its numbered health badge");
                Rows[0]["Present"]=false;RenderFrame();
                if(PreviewBadgeOrders!=std::vector<size_t>{0,1,2})throw std::runtime_error("Missing mod folders must not consume load-order numbers");
                for(auto& row:Rows)row["Enabled"]=false;RenderFrame();
                if(PreviewBadgeOrders!=std::vector<size_t>{0,0,0})throw std::runtime_error("An entirely disabled list must not display load-order numbers");
                Snapshot=oldSnapshot;Rows=oldRows;Status=oldStatus;Health=oldHealth;HealthRevision=oldRevision;PreviewChecked=false;
            }
            {
                CurrentPage=Page::Items;auto& state=LazyCatalog::For(0);const auto saved=state;
                if(state.Rows.empty())state.Rows=nlohmann::json::array({{{"Path","/Preview/Items/IronOre.IronOre"},{"Name","Iron ore (sample)"},{"Category","Resources"}}});
                state.Started=true;state.Pending=true;state.RequestedAt=std::chrono::steady_clock::now();state.PopulationStage="items-ready";state.CheckingChanges=false;
                for(int i=0;i<3;++i)RenderFrame();const auto steady=HelpyPreviewHooks::Browser;const auto page=ItemGridPicker::Bay.Picker.PageIndex;
                state.PopulationStage="cached-items-verifying";state.CheckingChanges=true;
                for(int i=0;i<3;++i)RenderFrame();const auto busy=HelpyPreviewHooks::Browser;
                if(std::abs(steady.x-busy.x)>1||std::abs(steady.y-busy.y)>1||std::abs(steady.z-busy.z)>1||std::abs(steady.w-busy.w)>1||page!=ItemGridPicker::Bay.Picker.PageIndex)
                    throw std::runtime_error("Background item revalidation moved or resized the grid/page");
                PreviewCapture=output/"item-service-background-check.png";RenderFrame();PreviewCapture.clear();
                const auto modifiedBefore=HelpyPreviewHooks::ModifiedItemBadges;
                state.Rows=nlohmann::json::array({{{"Path","/Game/Preview/PatchedTea.PatchedTea"},{"Name","Tea Leaves (patched native sample)"},{"Category","Resources"},{"SourceKind","vanilla"},{"ModifiedByRuneSchema",true},{"ModifiedBy","PreviewTeaTweaks"}}});
                ItemGridPicker::Bay.Picker.CatalogRevision=0;
                for(int i=0;i<3;++i)RenderFrame();
                if(HelpyPreviewHooks::ModifiedItemBadges==modifiedBefore)throw std::runtime_error("Patched native item did not show its separate bottom-left edit indicator");
                PreviewCapture=output/"item-service-patched-native.png";RenderFrame();PreviewCapture.clear();state=saved;ItemGridPicker::Bay.Picker.CatalogRevision=0;
            }
            {const auto accent=HelpyImGuiTheme::Current.accent;CurrentPage=Page::Home;HelpyImGuiTheme::Current.accent={.12f,.5f,1.f,.3f};
                for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"home-blue.png";RenderFrame();PreviewCapture.clear();
                HelpyImGuiTheme::Current.accent={1.f,.35f,.25f,1.f};PreviewCapture=output/"home-coral.png";RenderFrame();PreviewCapture.clear();
                HelpyImGuiTheme::Current.accent=accent;CurrentPage=Page::LiveLog;}
            LiveLogPanel::PreviewUE4SS=true;for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"live-log-ue4ss.png";RenderFrame();PreviewCapture.clear();LiveLogPanel::PreviewUE4SS=false;
            for(const int stream:{2,3}){LiveLogPanel::PreviewStream=stream;for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/(stream==2?"live-log-game.png":"live-log-eos.png");RenderFrame();PreviewCapture.clear();}LiveLogPanel::PreviewStream=-1;
            const auto previousEnvironment=HelpyEnvironment::Current;HelpyEnvironment::Current=HelpyEnvironment::Kind::Dedicated;
            DedicatedServerPanel::Preview=true;DedicatedServerPanel::Initialized=true;DedicatedServerPanel::File=L"Preview only/RSDragonwilds/Saved/Config/WindowsServer/DedicatedServer.ini";
            DedicatedServerPanel::Draft.emplace("[/Script/Dominion.DedicatedServerSettings]\r\nOwnerId=example-account-id\r\nServerName=Example RuneSchema server\r\nAdminPassword=preview-not-a-real-password\r\nDefaultWorldName=ExampleWorld\r\nWorldPassword=\r\nKnownPlayerList=(UserId=fixture,LastAdminPassword=must-stay-redacted)\r\n");
            CurrentPage=Page::DedicatedServer;for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"dedicated-server.png";RenderFrame();PreviewCapture.clear();HelpyEnvironment::Current=previousEnvironment;
            CurrentPage=Page::Settings;PreviewUpdatesTab=true;for(int i=0;i<4;++i)RenderFrame();
            PreviewCapture=output/"updates-runtime.png";RenderFrame();PreviewCapture.clear();PreviewUpdatesTab=false;
            CurrentPage=Page::Items;
            for(const auto kind:{HelpyEnvironment::Kind::Steam,HelpyEnvironment::Kind::Epic,HelpyEnvironment::Kind::SteamEpic,HelpyEnvironment::Kind::GamePass,HelpyEnvironment::Kind::Dedicated,HelpyEnvironment::Kind::Unknown}) {
                HelpyEnvironment::Current=kind;
                RuntimeConnectionPanel::Next={};
                for(int i=0;i<3;++i)RenderFrame();
                const auto badge=HelpyPreviewHooks::RuntimeBadge;
                const auto bar=HelpyPreviewHooks::InformationBar,heading=HelpyPreviewHooks::PageHeading;
                if(badge.x<MainSurfacePosition.x||badge.y<HelpyPreviewHooks::Navigation.at("Home").w||badge.z>bar.x||badge.w>MainSurfacePosition.y+MainSurfaceSize.y
                    ||badge.z-badge.x>HelpyImGuiTheme::Current.X(80)||badge.w-badge.y<HelpyImGuiTheme::Current.Y(32)
                    ||std::abs((badge.x+badge.z-HelpyPreviewHooks::NavigationRail.x-HelpyPreviewHooks::NavigationRail.z)*.5f)>HelpyImGuiTheme::Current.X(2)
                    ||MainSurfacePosition.y+MainSurfaceSize.y-badge.w>HelpyImGuiTheme::Current.Y(48))
                    throw std::runtime_error("Runtime icon must be centered at the bottom of the navigation rail: badge="+std::to_string(badge.x)+","+std::to_string(badge.y)+","+std::to_string(badge.z)+","+std::to_string(badge.w)
                        +" header="+std::to_string(bar.x)+","+std::to_string(bar.y)+","+std::to_string(bar.z)+","+std::to_string(bar.w)
                        +" title="+std::to_string(heading.x)+","+std::to_string(heading.y)+","+std::to_string(heading.z)+","+std::to_string(heading.w));
                const auto source=HelpyPreviewHooks::SourceFilter;
                if(source.x<0||source.z>ImGui::GetIO().DisplaySize.x||source.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Source filter escaped the viewport");
                const char* name=kind==HelpyEnvironment::Kind::Steam?"runtime-steam":kind==HelpyEnvironment::Kind::Epic?"runtime-epic":kind==HelpyEnvironment::Kind::SteamEpic?"runtime-steam-epic":kind==HelpyEnvironment::Kind::GamePass?"runtime-gamepass":kind==HelpyEnvironment::Kind::Dedicated?"runtime-server":"runtime-unknown";
                PreviewCapture=output/(std::string(name)+".png");RenderFrame();PreviewCapture.clear();
                HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={(badge.x+badge.z)*.5f,(badge.y+badge.w)*.5f};
                for(int i=0;i<3;++i)RenderFrame();
                if(!HelpyPreviewHooks::RuntimeDetailsVisible)throw std::runtime_error("Hovering the runtime icon did not expose its details");
                PreviewCapture=output/(std::string(name)+"-details.png");RenderFrame();PreviewCapture.clear();
                if(kind==HelpyEnvironment::Kind::Steam){
                    HelpyPreviewHooks::MouseDown=true;RenderFrame();HelpyPreviewHooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();
                    if(!RuntimeDetailsPanel::Open||RuntimeDetailsPanel::Selected!=RuntimeDetailsPanel::Tab::Runtime)throw std::runtime_error("Runtime icon did not open Runtime details");
                    RuntimeDetailsPanel::Open=false;RenderFrame();RenderFrame();
                }
                HelpyPreviewHooks::MouseEnabled=false;HelpyPreviewHooks::Mouse={20,20};for(int i=0;i<2;++i)RenderFrame();
                CurrentPage=Page::Home;for(int i=0;i<3;++i)RenderFrame();
                if(!PreviewHomeFits)throw std::runtime_error("Home runtime badge must appear once, centered above social links for each platform");
                PreviewCapture=output/(std::string("home-")+name+".png");RenderFrame();PreviewCapture.clear();
                if(kind==HelpyEnvironment::Kind::Steam){
                    const auto homeBadge=HelpyPreviewHooks::RuntimeBadge;
                    HelpyPreviewHooks::MouseEnabled=true;HelpyPreviewHooks::Mouse={(homeBadge.x+homeBadge.z)*.5f,(homeBadge.y+homeBadge.w)*.5f};
                    HelpyPreviewHooks::MouseDown=false;RenderFrame();HelpyPreviewHooks::MouseDown=true;RenderFrame();HelpyPreviewHooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();
                    if(!RuntimeDetailsPanel::Open)throw std::runtime_error("Home runtime badge must still open runtime details");
                    RuntimeDetailsPanel::Open=false;HelpyPreviewHooks::MouseEnabled=false;
                }
                CurrentPage=Page::Items;for(int i=0;i<2;++i)RenderFrame();
            }
            for(const auto kind:{HelpyEnvironment::Kind::Steam,HelpyEnvironment::Kind::GamePass}){
                HelpyEnvironment::Current=kind;RuntimeConnectionPanel::PreviewRole="listen-server";RuntimeConnectionPanel::Next={};for(int i=0;i<3;++i)RenderFrame();
                if(!HelpyPreviewHooks::RuntimeHostOverlay||RuntimeConnectionPanel::Read().Session!="Co-op host")throw std::runtime_error("Co-op host hybrid badge did not follow native network mode");
                PreviewCapture=output/(kind==HelpyEnvironment::Kind::Steam?"runtime-steam-coop-host.png":"runtime-gamepass-coop-host.png");RenderFrame();PreviewCapture.clear();
            }
            RuntimeConnectionPanel::PreviewRole="client";RuntimeConnectionPanel::Next={};for(int i=0;i<3;++i)RenderFrame();if(HelpyPreviewHooks::RuntimeHostOverlay)throw std::runtime_error("Client must not retain the host overlay");
            RuntimeConnectionPanel::PreviewRole="standalone";RuntimeConnectionPanel::Next={};
            HelpyEnvironment::Current=HelpyEnvironment::Kind::GamePass;
            for(const auto tab:{RuntimeDetailsPanel::Tab::Runtime,RuntimeDetailsPanel::Tab::Attributions,RuntimeDetailsPanel::Tab::Legal}) {
                HelpyPreviewHooks::RuntimeGameLogo={};
                RuntimeDetailsPanel::Request(tab);for(int i=0;i<4;++i)RenderFrame();
                if(RuntimeDetailsPanel::Selected!=tab)throw std::runtime_error("Runtime details tab selection was lost");
                if(tab==RuntimeDetailsPanel::Tab::Runtime&&HelpyPreviewHooks::RuntimeGameLogo.z<=HelpyPreviewHooks::RuntimeGameLogo.x)throw std::runtime_error("Runtime tab did not render its shared Dragonwilds logo");
                PreviewCapture=output/(tab==RuntimeDetailsPanel::Tab::Attributions?"attributions.png":tab==RuntimeDetailsPanel::Tab::Legal?"legal.png":"info.png");RenderFrame();PreviewCapture.clear();
                RuntimeDetailsPanel::Open=false;RenderFrame();RenderFrame();
            }
            {
                namespace Hooks=HelpyPreviewHooks;
                CurrentPage=Page::BugReport;for(int frame=0;frame<3;++frame)RenderFrame();
                if(!Hooks::Navigation.contains("Report a bug")||!PageErrors[static_cast<size_t>(Page::BugReport)].empty())throw std::runtime_error("Bug-report form is not reachable by default");
                std::snprintf(BugReportPanel::Title,sizeof(BugReportPanel::Title),"Item icons disappear after menu return");
                std::snprintf(BugReportPanel::Details,sizeof(BugReportPanel::Details),"Pictures stop appearing in the item browser after returning to the menu.");
                std::snprintf(BugReportPanel::Steps,sizeof(BugReportPanel::Steps),"Open Players / Items, return to the game menu, then reopen Helpy.");
                std::snprintf(BugReportPanel::Expected,sizeof(BugReportPanel::Expected),"Pictures and power badges remain visible.");
                BugReportPanel::Category=6;for(int frame=0;frame<3;++frame)RenderFrame();
                PreviewCapture=output/"bug-report-form.png";RenderFrame();PreviewCapture.clear();
                const auto titleRect=Hooks::BugFields.at("Title");if(titleRect.x<0||titleRect.z>width||titleRect.y<0||titleRect.w>height)throw std::runtime_error("Bug-report title field escaped the viewport");
                BugReportPanel::ScrollToSubmit=true;for(int frame=0;frame<4;++frame)RenderFrame();BugReportPanel::ScrollToSubmit=false;
                const auto submit=Hooks::BugSubmit;if(submit.y<0||submit.w>height||submit.z<=submit.x)throw std::runtime_error("Bug-report submit control is not scroll-reachable");
                Hooks::MouseEnabled=true;Hooks::Mouse={(submit.x+submit.z)*.5f,(submit.y+submit.w)*.5f};Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;RenderFrame();RenderFrame();
                if(!BugReportPanel::OpenedUrl.starts_with(std::string(BugReport::NewIssue)+"?title=")||BugReportPanel::OpenedUrl.find("Cache%20%2F%20icons")==std::string::npos||BugReportPanel::Status.find("has not submitted")==std::string::npos)throw std::runtime_error("Bug-report action must prepare a categorized GitHub draft, not claim submission");
                PreviewCapture=output/"bug-report-github-review.png";RenderFrame();PreviewCapture.clear();
                Hooks::MouseEnabled=false;CurrentPage=Page::Items;RenderFrame();CurrentPage=Page::BugReport;RenderFrame();
                if(std::string(BugReportPanel::Title)!="Item icons disappear after menu return")throw std::runtime_error("Navigation discarded the report draft");
                BugReportPanel::Title[0]=BugReportPanel::Details[0]=BugReportPanel::Steps[0]=BugReportPanel::Expected[0]=BugReportPanel::Additional[0]=0;BugReportPanel::Status.clear();BugReportPanel::OpenedUrl.clear();BugReportPanel::CopiedBody.clear();BugReportPanel::Category=0;
                CurrentPage=Page::Home;
            }
            // The expanded browser must remain usable at the minimum desktop
            // size, not just in the full-size interactive fixture below.
            {
                namespace Hooks=HelpyPreviewHooks;
                const auto center=[](const ImVec4& r){return ImVec2((r.x+r.z)*.5f,(r.y+r.w)*.5f);};
                const auto click=[&](const ImVec4& rect){Hooks::Mouse=center(rect);Hooks::MouseDown=true;for(int i=0;i<2;++i)RenderFrame();Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();};
                Hooks::MouseEnabled=true;
                const auto enabledWorkspaces=HelpyImGuiTheme::Current.workspaces;
                for(int mask=0;mask<32;++mask){
                    HelpyImGuiTheme::Current.workspaces={bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8),bool(mask&16)};
                    CurrentPage=Page::Home;for(int i=0;i<2;++i)RenderFrame();
                    if(PreviewSidebarVisible)throw std::runtime_error("Home did not collapse its sidebar");
                    if(!PreviewHomeFits){PreviewCapture=output/"home-layout-failed.png";RenderFrame();PreviewCapture.clear();throw std::runtime_error("Home controls or cache progress escaped the visible area, workspace mask="+std::to_string(mask));}
                    for(auto page:HelpyNavigation::SidebarPages)if(page!=Page::Home){
                        size_t count=0;
                        for(size_t index=0;index<HomeDestinations.size();++index)if(HomeDestinations[index].Target==page&&PreviewHomeTiles[index].z>PreviewHomeTiles[index].x)++count;
                        if(count!=(HelpyNavigation::Available(page,HelpyImGuiTheme::Current.workspaces,HelpyEnvironment::Server())?1:0))
                            throw std::runtime_error("Home tiles do not match enabled workspaces");
                    }
                    if(mask==0||mask==5){PreviewCapture=output/(mask==0?"home-default.png":"home-mixed.png");RenderFrame();PreviewCapture.clear();}
                }
                HelpyImGuiTheme::Current.workspaces=enabledWorkspaces;
                for(size_t index=0;index<HomeDestinations.size();++index){
                    if(!HelpyNavigation::Available(HomeDestinations[index].Target,enabledWorkspaces,HelpyEnvironment::Server()))continue;
                    CurrentPage=Page::Home;for(int i=0;i<3;++i)RenderFrame();const auto rect=PreviewHomeTiles[index];
                    if(rect.x<0||rect.z>ImGui::GetIO().DisplaySize.x||rect.y<0||rect.w>ImGui::GetIO().DisplaySize.y||std::abs((rect.z-rect.x)-(rect.w-rect.y))>1)
                        throw std::runtime_error("Home destination tile is clipped or not square");
                    Hooks::Mouse=center(rect);for(int i=0;i<2;++i)RenderFrame();
                    if(HomeFocused!=index||!PreviewHomeHoverDetails)throw std::runtime_error("Home hover details do not follow the hovered tile");
                    const auto tip=PreviewHomeTooltip;const float tipWidth=tip.z-tip.x,tipCenter=(tip.x+tip.z)*.5f;
                    if(tipWidth>HelpyImGuiTheme::Current.X(200)+1||tip.x<0||tip.y<0||tip.z>ImGui::GetIO().DisplaySize.x||tip.w>ImGui::GetIO().DisplaySize.y){
                        PreviewCapture=output/"home-tooltip-layout-failed.png";RenderFrame();PreviewCapture.clear();
                        throw std::runtime_error("Home hover details must stay narrow and within the viewport: "+std::to_string(tip.x)+","+std::to_string(tip.y)+","+std::to_string(tip.z)+","+std::to_string(tip.w));}
                    const float expectedCenter=std::clamp(center(rect).x,tipWidth*.5f+HelpyImGuiTheme::Current.X(8),ImGui::GetIO().DisplaySize.x-tipWidth*.5f-HelpyImGuiTheme::Current.X(8));
                    if(std::abs(tipCenter-expectedCenter)>2)throw std::runtime_error("Home hover details must center on their tile, not the pointer");
                    if(index==1){PreviewCapture=output/"home-map-details.png";RenderFrame();PreviewCapture.clear();}
                    click(rect);
                    if(HomeExit.Active){if(CurrentPage!=Page::Home||HomeExitTarget!=HomeDestinations[index].Target||HomeExit.Kind!=HelpySplash::ExitKind::RuneSchema)throw std::runtime_error("Home selection failed to freeze its bracket-exit destination");
                        HomeExit.Started=ImGui::GetTime()-HelpySplash::ExitSeconds*.35;PreviewCapture=output/"home-brackets-exit.png";RenderFrame();PreviewCapture.clear();
                        HomeExit.Started=ImGui::GetTime()-HelpySplash::ExitSeconds;for(int i=0;i<3;++i)RenderFrame();}
                    if(CurrentPage!=HomeDestinations[index].Target)throw std::runtime_error("Home destination opened the wrong tool");
                    if(!PreviewSidebarVisible)throw std::runtime_error("Opening a Home tile did not restore navigation");
                }
                // Exercise real tab and sidebar input, not only direct routes.
                const auto cartBeforeGrouping=ItemGridPicker::SharedCart.Payload();
                CurrentPage=Page::Items;for(int i=0;i<4;++i)RenderFrame();
                for(auto page:HelpyNavigation::PlayerTabs){
                    if(!HelpyNavigation::Available(page,enabledWorkspaces,HelpyEnvironment::Server()))continue;
                    const auto label=HelpyNavigation::TabTitle(page);click(Hooks::WorkspaceTabs.at(label));
                    if(CurrentPage!=page)throw std::runtime_error(std::string("Players tab did not open: ")+label);
                }
                const auto lastPlayerTab=CurrentPage;
                PreviewCapture=output/"players-grouped.png";RenderFrame();PreviewCapture.clear();
                CurrentPage=Page::Map;for(int i=0;i<4;++i)RenderFrame();
                for(auto page:HelpyNavigation::WorldTabs){
                    if(!HelpyNavigation::Available(page,enabledWorkspaces,HelpyEnvironment::Server()))continue;
                    const auto label=HelpyNavigation::TabTitle(page);click(Hooks::WorkspaceTabs.at(label));
                    if(CurrentPage!=page)throw std::runtime_error(std::string("World tab did not open: ")+label);
                }
                const auto lastWorldTab=CurrentPage;
                PreviewCapture=output/"world-grouped.png";RenderFrame();PreviewCapture.clear();
                click(Hooks::Navigation.at("Players"));
                if(CurrentPage!=lastPlayerTab)throw std::runtime_error("Players did not remember its previous workflow");
                click(Hooks::Navigation.at("World"));
                if(CurrentPage!=lastWorldTab)throw std::runtime_error("World did not remember its previous workflow");
                if(ItemGridPicker::SharedCart.Payload()!=cartBeforeGrouping)throw std::runtime_error("Grouped navigation changed the delivery cart");
                HelpyImGuiTheme::Current.workspaces.SaveEditor=false;CurrentPage=Page::SaveEditor;for(int i=0;i<4;++i)RenderFrame();
                if(CurrentPage!=Page::SaveEditor||!Hooks::WorkspaceTabs.contains("Character")||!Hooks::WorkspaceTabs.contains("Profile"))throw std::runtime_error("Character must remain available within Profile");
                click(Hooks::WorkspaceTabs.at("Profile"));if(CurrentPage!=Page::Profile)throw std::runtime_error("Profile tab did not open");
                for(int frame=0;frame<3;++frame)RenderFrame();
                if(!Hooks::AuthoringButtons.contains("Profile portrait"))throw std::runtime_error("Profile route did not render its account controls");
                const auto homeFooter=Hooks::Navigation.at("Home"),profileFooter=Hooks::Navigation.at("Profile");
                if(std::abs(profileFooter.x-homeFooter.x)>2||std::abs(profileFooter.z-homeFooter.z)>2||profileFooter.y<homeFooter.w)throw std::runtime_error("Profile must be a matching navigation row below Home");
                {
                    const auto originalProfile=RuneSchemaProfile::Read();auto sample=originalProfile;
                    {
                        namespace B=SpawnBrowserSettings;const auto folder=output/"preset-tests"/std::to_string(GetTickCount64());const auto legacy=folder/"legacy.json",central=folder/"profile/spawn-browser.json";
                        const std::string old=R"({"Version":1,"Hidden":["Resource|/Game/Tree.Tree_C"]})";ConfigFiles::Write(legacy,old);
                        const auto stored=B::ReadStored(central,legacy);if(stored.Choices.Hidden.size()!=1||ConfigFiles::Read(legacy)!=old||ConfigFiles::Read(central)!=old||B::File()!=RuneSchemaProfile::Data("spawn-browser.json"))throw std::runtime_error("Browser choices did not migrate into profile without changing the legacy source");
                        auto preset=stored.Choices;preset.SetFilter("Resources",{"yew",true,0});const auto text=preset.Json().dump();B::WriteStored(central,text,stored.Baseline);
                        if(B::ReadPreset(central).Filter("Resources").Query!="yew"||text.find("Scale")!=text.npos||text.find("ProfileId")!=text.npos)throw std::runtime_error("Portable browser preset leaked unrelated settings or lost filters");
                        ConfigFiles::Write(central,text+" ");bool stopped=false;try{B::WriteStored(central,text,text);}catch(...){stopped=true;}
                        if(!stopped||ConfigFiles::Read(central)!=text+" ")throw std::runtime_error("Concurrent browser preference changes were overwritten");
                        ConfigFiles::Write(folder/"empty.json","");stopped=false;try{(void)B::ReadStored(folder/"empty.json",{});}catch(...){stopped=true;}if(!stopped)throw std::runtime_error("An invalid empty browser file must not become a writable blank preset");
                    }
                    sample["ProfileName"]="QA Profile";sample["Social"]={{"Steam","https://steamcommunity.com/id/runeschema-qa"},{"Nexus","https://www.nexusmods.com/users/123"},{"Discord","qa.profile"}};
                    sample["Platforms"]={"Steam","Game Pass","Epic Games"};sample["Characters"]={
                        {"1234567890abcdef1234567890abcdef",{{"Name","Xbox character"},{"Platforms",{"Game Pass"}},{"LastSeen",uint64_t(0)}}},
                        {"aaaaaaaa222222223333333344444444",{{"Name","Steam character"},{"Platforms",{"Steam"}},{"LastSeen",uint64_t(0)}}},
                        {"bbbbbbbb222222223333333344444444",{{"Name","Shared character"},{"Platforms",{"Steam","Game Pass"}},{"LastSeen",uint64_t(0)}}}};
                    {std::lock_guard lock(RuneSchemaProfile::Gate);RuneSchemaProfile::Document=RuneSchemaProfile::Validate(sample);++RuneSchemaProfile::Revision;}
                    ProfilePanel::Dirty=false;ProfilePanel::PlatformFilter.clear();ProfilePanel::CharacterSearch[0]=0;for(int i=0;i<3;++i)RenderFrame();
                    if(ProfilePanel::PlatformLogos!=3||ProfilePanel::CharacterRows.size()!=3||PartyMemberView::PlatformIcon("Epic Games").GetTexID()!=PartyMemberView::EpicIcon.GetTexID()||PartyMemberView::PlatformIcon("Xbox").GetTexID()!=PartyMemberView::XboxIcon.GetTexID())throw std::runtime_error("Profile must render associated Steam, Xbox and Epic logos and linked characters");
                    for(const auto* key:{"Profile platform Steam","Profile platform Game Pass","Profile platform Epic"}){const auto rect=Hooks::AuthoringButtons.at(key);if(rect.z>ImGui::GetIO().DisplaySize.x||rect.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Profile platform buttons escaped the viewport");}
                    PreviewCapture=output/"profile.png";RenderFrame();PreviewCapture.clear();click(Hooks::AuthoringButtons.at("Profile platform Game Pass"));
                    if(ProfilePanel::PlatformFilter!="Game Pass"||ProfilePanel::CharacterRows.size()!=2||ProfilePanel::CharacterRows.contains("aaaaaaaa222222223333333344444444"))throw std::runtime_error("Profile platform filter showed an unrelated character");
                    PreviewCapture=output/"profile-filtered.png";RenderFrame();PreviewCapture.clear();std::strcpy(ProfilePanel::CharacterSearch,"Xbox");RenderFrame();
                    if(ProfilePanel::CharacterRows.size()!=1)throw std::runtime_error("Profile search must compose with platform filtering");
                    ProfilePanel::CharacterSearch[0]=0;RenderFrame();click(Hooks::AuthoringButtons.at("Profile platform Game Pass"));
                    if(!ProfilePanel::PlatformFilter.empty()||ProfilePanel::CharacterRows.size()!=3)throw std::runtime_error("Clicking an active platform must restore all characters");
                    ProfilePanel::OpenedLink.clear();ProfilePanel::OpenLink("Steam","file:///C:/private");ProfilePanel::OpenLink("Nexus","https://evil.invalid/users/123");ProfilePanel::OpenLink("Discord","calc.exe");
                    if(!ProfilePanel::OpenedLink.empty())throw std::runtime_error("Profile links must reject unsafe destinations");
                    if(height>=700){click(Hooks::AuthoringButtons.at("Profile links"));click(Hooks::AuthoringButtons.at("Profile Steam"));if(ProfilePanel::OpenedLink!="https://steamcommunity.com/id/runeschema-qa")throw std::runtime_error("Profile Steam link did not use its validated destination");}
                    {std::lock_guard lock(RuneSchemaProfile::Gate);RuneSchemaProfile::Document=originalProfile;++RuneSchemaProfile::Revision;}
                    ProfilePanel::Dirty=false;ProfilePanel::PlatformFilter.clear();for(int i=0;i<3;++i)RenderFrame();
                }
                {const nlohmann::json peer={{"Guid","aaaaaaaa222222223333333344444444"},{"Name","QA Character"},{"Platform","Game Pass"},{"PlatformAccountName","QA Xbox Account"},{"IsLocal",false}};
                    RuneSchemaProfile::ReceivePublic(peer.at("Guid").get<std::string>(),{{"ProfileId","12345678123412341234123412345678"},{"ProfileName","QA Profile"},{"Social",{{"Steam","https://steamcommunity.com/id/runeschema-qa"},{"Discord","qa.profile"}}},{"Platforms",nlohmann::json::array({"Steam","Game Pass"})}});
                    if(RuneSchemaProfile::Name(peer)!="QA Profile (QA Character)")throw std::runtime_error("Remote profile display dropped its character name");
                    PartyMemberView::ViewProfile(peer);for(int frame=0;frame<3;++frame)RenderFrame();PreviewCapture=output/"player-profile.png";RenderFrame();PreviewCapture.clear();PartyMemberView::ProfileOpen=false;RuneSchemaProfile::ClearShared();RenderFrame();
                }
                HelpyImGuiTheme::Current.workspaces=enabledWorkspaces;
                CurrentPage=Page::Home;for(int i=0;i<3;++i)RenderFrame();
                if(!HomeBannerTexture||!BrandTexture)throw std::runtime_error("Discord banner or centered Home logo is missing");
                for(size_t index=0;index<HomeLinks.size();++index){
                    if(!HomeLinkTextures[index])throw std::runtime_error("Community footer icon is missing");
                    const auto rect=PreviewHomeLinks[index];
                    if(rect.x<0||rect.z>ImGui::GetIO().DisplaySize.x||rect.y<0||rect.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Community footer icon is clipped");
                    PreviewHomeOpenedLink.clear();click(rect);
                    if(PreviewHomeOpenedLink!=HomeLinks[index].Url||CurrentPage!=Page::Home)throw std::runtime_error("Community icon did not request its exact external destination");
                    if(!PreviewHomeCardImage||std::abs((PreviewHomeCard.x+PreviewHomeCard.z)*.5f-(MainSurfacePosition.x+MainSurfaceSize.x*.5f))>HelpyImGuiTheme::Current.X(24))throw std::runtime_error("Community details image is missing or its pane is not centered");
                    if(PreviewHomeCard.y<0||PreviewHomeCard.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Community details card escaped the viewport");
                    if(PreviewHomeCard.w-PreviewHomeCard.y>HelpyImGuiTheme::Current.Y(300)+1)throw std::runtime_error("Social previews must not retain their oversized blank footer");
                    if(index&&std::abs((PreviewHomeCard.w-PreviewHomeCard.y)-(PreviewHomeCards[0].w-PreviewHomeCards[0].y))>1)throw std::runtime_error("Social page-preview cards must share the same height");
                    PreviewCapture=output/(std::string("home-details-")+std::to_string(index)+".png");RenderFrame();PreviewCapture.clear();
                    PreviewHomeOpenedLink.clear();click(PreviewHomeLinkOpen);
                    if(PreviewHomeOpenedLink!=HomeLinks[index].Url)throw std::runtime_error("Community hover-card link is not clickable");
                }
                const auto homeEnvironment=HelpyEnvironment::Current;HelpyEnvironment::Current=HelpyEnvironment::Kind::Dedicated;
                CurrentPage=Page::Home;for(int i=0;i<3;++i)RenderFrame();
                const auto serverTile=PreviewHomeTiles.back();
                if(serverTile.z<=serverTile.x||serverTile.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Dedicated-server Home tile is missing or clipped");
                PreviewCapture=output/"home-server.png";RenderFrame();PreviewCapture.clear();click(serverTile);
                if(HomeExit.Active){HomeExit.Started=ImGui::GetTime()-HelpySplash::ExitSeconds;for(int i=0;i<3;++i)RenderFrame();}
                if(CurrentPage!=Page::DedicatedServer||!PreviewSidebarVisible)throw std::runtime_error("Dedicated Home tile did not open with navigation");
                HelpyEnvironment::Current=homeEnvironment;
                CurrentPage=Page::Home;for(int i=0;i<3;++i)RenderFrame();
                Hooks::MouseEnabled=true;Hooks::Mouse=center(PreviewHomeQuick);Hooks::MouseDown=false;for(int i=0;i<4;++i)RenderFrame();
                if(std::abs((PreviewHomeQuickTooltip.x+PreviewHomeQuickTooltip.z-PreviewHomeQuick.x-PreviewHomeQuick.z)*.5f)>2)throw std::runtime_error("Quick settings details must be centered on their control");
                PreviewCapture=output/"home-quick-tooltip.png";RenderFrame();PreviewCapture.clear();click(PreviewHomeQuick);
                if(PreviewHomeQuick.z-PreviewHomeQuick.x>HelpyImGuiTheme::Current.X(160)||std::abs((PreviewHomeQuick.x+Hooks::Navigation.at("Profile").z)*.5f-PreviewHomeCenter)>2)throw std::runtime_error("Quick settings and Profile must be compact and centered together");
                if(!ImGui::IsPopupOpen("Home quick settings",ImGuiPopupFlags_AnyPopupId))throw std::runtime_error("Home quick settings did not open");
                if(PreviewHomeQuickPopup.x<0||PreviewHomeQuickPopup.y<0||PreviewHomeQuickPopup.z>ImGui::GetIO().DisplaySize.x||PreviewHomeQuickPopup.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Quick settings escaped the viewport");
                if(PreviewHomeQuickPopup.w-PreviewHomeQuickPopup.y>HelpyImGuiTheme::Current.Y(340))throw std::runtime_error("Default Quick settings must remain compact; use hovering details and closed Advanced options");
                PreviewCapture=output/"home-quick-settings.png";RenderFrame();PreviewCapture.clear();
                // Dismiss the non-modal popover without changing any setting.
                Hooks::Mouse={20,20};Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();
                CurrentPage=Page::Map;for(int i=0;i<4;++i)RenderFrame();Hooks::MouseEnabled=true;
                // Exercise uncovered map space: the party overlay intentionally
                // receives clicks over its own player rows, not the map beneath.
                Hooks::Mouse={Hooks::Map.x+(Hooks::Map.z-Hooks::Map.x)*.8f,Hooks::Map.y+(Hooks::Map.w-Hooks::Map.y)*.75f};
                Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<4;++i)RenderFrame();
                if(!MapPanel::Data.Popup||Hooks::MapPopupWindow.x<0||Hooks::MapPopupWindow.y<0||Hooks::MapPopupWindow.z>width||Hooks::MapPopupWindow.w>height)
                    throw std::runtime_error("Responsive map actions are missing or clipped");
                if(std::abs((Hooks::MapOperationRow.x+Hooks::MapOperationRow.z)-(Hooks::MapOptionsBody.x+Hooks::MapOptionsBody.z))>3)
                    throw std::runtime_error("Responsive map mode controls are not centered");
                PreviewCapture=output/"map-options-responsive.png";RenderFrame();PreviewCapture.clear();
                MapPanel::Data.Operation=1;MapPanel::Data.BuildingsWanted=true;MapPanel::Data.BuildingsOffset=0;for(int i=0;i<5;++i)RenderFrame();
                click(Hooks::BuildingBrowse);
                if(!MapPanel::Data.BuildingBrowserOpen||Hooks::BuildingUse.w>ImGui::GetIO().DisplaySize.y||Hooks::BuildingClose.z>ImGui::GetIO().DisplaySize.x)
                    throw std::runtime_error("Expanded building browser actions escaped the viewport");
                PreviewCapture=output/"building-browser-responsive.png";RenderFrame();PreviewCapture.clear();click(Hooks::BuildingClose);
                Hooks::Mouse={20,20};Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();Hooks::MouseEnabled=false;
                MapPanel::Data.Operation=0;CurrentPage=Page::Items;
                // The filter popup remains fully usable at all desktop sizes.
                CurrentPage=Page::Map;Hooks::MouseEnabled=true;for(int i=0;i<3;++i)RenderFrame();
                const auto savedResponsiveCache=HelpyMapCache::Current();auto& responsiveCache=HelpyMapCache::Current();
                const std::string responsiveMarkers=R"({"source":"RuneScape: Dragonwilds Wiki","licence":"CC BY-NC-SA 3.0","built":"2026-10-16","groups":[{"id":"mining","label":"Ores and rocks","categories":[{"name":"Iron","points":[[198000,98000],[208000,98000]]}]}]})";
                responsiveCache.Data=std::make_shared<MapFilters::Dataset>(MapFilters::Parse(responsiveMarkers));responsiveCache.Filters={};++responsiveCache.Revision;
                std::strcpy(MapPanel::Data.MarkerSearch,"iron");for(int i=0;i<2;++i)RenderFrame();
                if(Hooks::MapReferenceMatches!=2)throw std::runtime_error("Responsive map search did not apply cached references");
                Hooks::MapForceGroupsOpen=true;click(Hooks::MapFiltersButton);
                if(!Hooks::MapCategoryControls.contains("mining:Iron")||Hooks::MapFiltersClose.w>height||Hooks::MapFiltersClose.z>width)throw std::runtime_error("Responsive map filter controls are missing or clipped: "+nlohmann::json{{"Width",width},{"Height",height},{"Category",Hooks::MapCategoryControls.contains("mining:Iron")},{"Close",{Hooks::MapFiltersClose.x,Hooks::MapFiltersClose.y,Hooks::MapFiltersClose.z,Hooks::MapFiltersClose.w}}}.dump());
                PreviewCapture=output/"map-filters-responsive.png";RenderFrame();PreviewCapture.clear();
                click(Hooks::MapCategoryControls.at("mining:Iron"));if(Hooks::MapReferenceMatches)throw std::runtime_error("Responsive map category hiding failed");
                click(Hooks::MapFiltersClose);Hooks::MapForceGroupsOpen=false;MapPanel::Data.MarkerSearch[0]=0;
                responsiveCache=savedResponsiveCache;++responsiveCache.Revision;HelpyMapCache::SaveFilters();Hooks::MouseEnabled=false;CurrentPage=Page::Items;
            }
            if(width>=1200&&height>=850){
                namespace Hooks=HelpyPreviewHooks;CurrentPage=Page::Items;
                const auto center=[](const ImVec4& r){return ImVec2((r.x+r.z)*.5f,(r.y+r.w)*.5f);};
                const auto click=[&](const ImVec4& rect){Hooks::MouseEnabled=true;Hooks::Mouse=center(rect);Hooks::MouseDown=false;RenderFrame();Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<4;++i)RenderFrame();};
                for(int i=0;i<4;++i)RenderFrame();
                if(Hooks::IconCache.z<=Hooks::IconCache.x||Hooks::IconCache.w>height)throw std::runtime_error("Item Bay icon cache control is missing or clipped");
                click(Hooks::IconCache);PreviewCapture=output/"item-bay-icon-cache.png";RenderFrame();PreviewCapture.clear();
                if(!HelpyIconPacks::Open||HelpyIconPacks::Advanced)throw std::runtime_error("Cache window must open in its simple view");
                const auto originalWindow=Hooks::WindowControls.at("Icon cache##rsmk-cache/window");
                const auto minimize=Hooks::WindowControls.at("Icon cache##rsmk-cache/minimize"),maximize=Hooks::WindowControls.at("Icon cache##rsmk-cache/maximize");
                if(!(minimize.z<maximize.z&&maximize.z<Hooks::IconCacheClose.x&&std::abs(maximize.y-Hooks::IconCacheClose.y)<1))throw std::runtime_error("Window controls must sit beside the title-bar X");
                click(maximize);if(!HelpyIconPacks::Chrome.Maximized)throw std::runtime_error("Docked cache maximize failed");
                const auto maximizedWindow=Hooks::WindowControls.at("Icon cache##rsmk-cache/window");
                if(maximizedWindow.z-maximizedWindow.x<MainSurfaceSize.x-18||maximizedWindow.w-maximizedWindow.y<MainSurfaceSize.y-18)throw std::runtime_error("Maximize did not fill the Helpy surface");
                PreviewCapture=output/"icon-cache-maximized.png";RenderFrame();PreviewCapture.clear();
                click(Hooks::WindowControls.at("Icon cache##rsmk-cache/maximize"));
                const auto restoredWindow=Hooks::WindowControls.at("Icon cache##rsmk-cache/window");
                if(HelpyIconPacks::Chrome.Maximized||std::abs((originalWindow.z-originalWindow.x)-(restoredWindow.z-restoredWindow.x))>1)throw std::runtime_error("Restore did not retain the cache window size");
                click(Hooks::WindowControls.at("Icon cache##rsmk-cache/minimize"));if(!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::IconCache))throw std::runtime_error("Title-bar minimize did not reach the Helpy taskbar");
                click(Hooks::AuthoringButtons.at("Restore icon cache"));
                if(!(Hooks::CacheImport.y<Hooks::CacheBuild.y&&Hooks::CacheBuild.y<Hooks::CacheExport.y))throw std::runtime_error("Cache controls must flow from import through optional build to export");
                click(Hooks::CacheAdvanced);PreviewCapture=output/"icon-cache-advanced.png";RenderFrame();PreviewCapture.clear();
                click(Hooks::CacheAdvanced);click(Hooks::CacheDetach);
                if(!m_cacheWindow||!m_cacheTarget||!m_cacheSwapChain)throw std::runtime_error("Cache window failed to detach into a native resizable surface");
                CapturePreview(m_device,m_context,m_cacheSwapChain,output/"icon-cache-detached.png");
                click(Hooks::CacheDetach);
                if(m_cacheWindow||HelpyIconPacks::Detached)throw std::runtime_error("Dock back must release only the cache window");
                click(Hooks::IconCacheClose);
                const auto originalBrowser=Hooks::Browser;click(Hooks::BayExpand);
                if(!ItemGridPicker::Bay.Expanded||Hooks::Browser.w-Hooks::Browser.y<originalBrowser.w-originalBrowser.y)throw std::runtime_error("Expand pane did not grow the Item Bay");
                PreviewCapture=output/"item-bay-expanded.png";RenderFrame();PreviewCapture.clear();click(Hooks::BayExpand);
                if(ItemGridPicker::Bay.Expanded)throw std::runtime_error("Restore pane failed");
                {ItemGridPicker::State placeholders;const nlohmann::json fixture={{"Items",nlohmann::json::array({{{"Path","/Game/PH"},{"Name","[PH] Test"}},{{"Path","/Game/Real"},{"Name","Real item"}}})}};
                    ItemGridPicker::Rebuild(fixture,placeholders,1);if(placeholders.Matches.size()!=1)throw std::runtime_error("Placeholder items must be hidden by default");
                    HelpyImGuiTheme::Current.itemShowPlaceholders=true;ItemGridPicker::Rebuild(fixture,placeholders,2);HelpyImGuiTheme::Current.itemShowPlaceholders=false;
                    if(placeholders.Matches.size()!=2)throw std::runtime_error("Author placeholder opt-in did not restore hidden rows");}
                click(Hooks::SourceFilter);
                if(!Hooks::SourceOptions.contains("modded"))throw std::runtime_error("Source filter menu unavailable");
                PreviewCapture=output/"item-source-menu.png";RenderFrame();PreviewCapture.clear();click(Hooks::SourceOptions.at("modded"));
                for(int i=0;i<5;++i)RenderFrame();
                if(LazyCatalog::For(0).Rows.empty()||ItemGridPicker::Bay.Picker.SourceFilter!=ItemSource::Kind::Modded)throw std::runtime_error("Source filter did not execute");
                for(const auto& row:LazyCatalog::For(0).Rows)if(ItemSource::Row(row).Group!=ItemSource::Kind::Modded)throw std::runtime_error("Source filter leaked another source");
                if(!Hooks::ModBadges)throw std::runtime_error("Mod source badge did not render");
                if(ItemGridPicker::SourceTagWidth(LazyCatalog::For(0).Rows.front(),90,20)!=20)throw std::runtime_error("Modded cards must show an icon-only source badge");
                PreviewCapture=output/"item-source-modded.png";RenderFrame();PreviewCapture.clear();
                Hooks::Mouse=center(Hooks::ItemRects.at(items[1]["Path"].get<std::string>()));Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<3;++i)RenderFrame();
                PreviewCapture=output/"item-family-menu.png";RenderFrame();PreviewCapture.clear();click(Hooks::ViewFamily);
                for(int i=0;i<5;++i)RenderFrame();
                if(ItemGridPicker::Bay.Picker.FamilyKey!="fallen_hoplite_dyed_chrome"||LazyCatalog::For(0).Rows.size()!=3)throw std::runtime_error("View family did not retain exact Chrome set");
                PreviewCapture=output/"item-family-chrome.png";RenderFrame();PreviewCapture.clear();
                const auto badgeCount=Hooks::ModBadges;HelpyImGuiTheme::Current.itemGridView=false;for(int i=0;i<3;++i)RenderFrame();
                if(Hooks::ModBadges<=badgeCount)throw std::runtime_error("List view lost mod source badges");
                PreviewCapture=output/"item-family-list.png";RenderFrame();PreviewCapture.clear();HelpyImGuiTheme::Current.itemGridView=true;
                HelpyImGuiTheme::Current.itemSource="all";ItemGridPicker::Bay.Picker.FamilyKey.clear();LazyCatalog::Search(0,{});ItemGridPicker::Bay.Picker.CatalogRevision=0;
                Hooks::MouseEnabled=false;Hooks::Mouse={-100,-100};for(int i=0;i<4;++i)RenderFrame();
            }
            CurrentPage=Page::Authoring;
            {namespace Hooks=HelpyPreviewHooks;
                const auto click=[&](const char* label){const auto rect=Hooks::AuthoringButtons.at(label);if(rect.w>height||rect.z>width)throw std::runtime_error("Authoring control escaped viewport");Hooks::MouseEnabled=true;Hooks::Mouse={(rect.x+rect.z)*.5f,(rect.y+rect.w)*.5f};Hooks::MouseDown=false;RenderFrame();Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<5;++i){AuthoringClient::NextPoll={};RenderFrame();}};
                click("Activate authoring");if(!authoringFixture.Host.Active()||AuthoringClient::Result("Catalogue")["Loaders"].size()!=22)throw std::runtime_error("Authoring activation/catalogue failed");
                AuthoringPanel::Copy(AuthoringPanel::Query,sizeof(AuthoringPanel::Query),"iron ore");click("Search");
                if(authoringFixture.Last["Operation"]!="AssetSearch"||authoringFixture.Last["Arguments"]["Query"]!="iron ore"||AuthoringClient::Result("AssetSearch")["AssetMatches"].size()!=1)throw std::runtime_error("Authoring search wire failed");
                if(width>=1200&&height>=850){
                    click("Asset:/Preview/Items/IronOre.IronOre");if(AuthoringPanel::Entry.empty())throw std::runtime_error("Asset selection did not capture fields");
                    AuthoringPanel::Copy(AuthoringPanel::Fields,sizeof(AuthoringPanel::Fields),"{");click("Build preview");
                    if(AuthoringClient::Error.empty()||!PageErrors[static_cast<size_t>(Page::Authoring)].empty())throw std::runtime_error("Malformed draft did not stay inside the authoring form");
                    AuthoringPanel::Copy(AuthoringPanel::Fields,sizeof(AuthoringPanel::Fields),"{\"PowerLevel\":9}");click("Build preview");
                    if(authoringFixture.Last["Operation"]!="StarterBuild"||authoringFixture.Last["Arguments"]["Fields"]["PowerLevel"]!=9||AuthoringClient::Result("StarterBuild").contains("Error"))throw std::runtime_error("Edited starter did not build through the bridge");
                }
                Hooks::MouseEnabled=false;Hooks::Mouse={-20,-20};
                authoringFixture.Seed();
                for(int i=0;i<IM_ARRAYSIZE(AuthoringPanel::Sections);++i){AuthoringPanel::Current=static_cast<AuthoringPanel::Section>(i);for(int frame=0;frame<3;++frame)RenderFrame();PreviewCapture=output/("authoring-"+std::to_string(i)+".png");RenderFrame();PreviewCapture.clear();}
                AuthoringPanel::Current=AuthoringPanel::Section::Content;
                for(const auto* loader:{"players","nameplates"}){AuthoringPanel::Copy(AuthoringPanel::Loader,sizeof(AuthoringPanel::Loader),loader);AuthoringPanel::Builder=1;RenderFrame();PreviewCapture=output/(std::string("authoring-builder-")+loader+".png");RenderFrame();PreviewCapture.clear();}
                AuthoringPanel::Copy(AuthoringPanel::Loader,sizeof(AuthoringPanel::Loader),"assets");AuthoringPanel::Builder=0;
            }
            CurrentPage=Page::Journal;
            NativeJournalCache::Initialized=true;NativeJournalCache::Loaded=true;NativeJournalCache::Refresh=false;NativeJournalCache::Generation=1;
            NativeJournalCache::Entries=nlohmann::json::array({
                {{"Native",true},{"Path","/Preview/Journal/Vanilla.Vanilla"},{"Name","Native lore sample"},{"Type","Lore"},{"PersistenceID","preview-native"},{"SourceKind","vanilla"},{"SourceName","Dragonwilds"},{"Pages",nlohmann::json::array({"Native page one: cached text remains readable without marking it discovered.","Native page two."})}},
                {{"Native",true},{"Path","/Preview/Journal/Mod.Mod"},{"Name","Modded journal sample"},{"Type","People"},{"PersistenceID","preview-mod"},{"SourceKind","modded"},{"SourceName","Preview mod"},{"Pages",nlohmann::json::array({"A separate mod-owned entry."})}}});
            NativeJournalCache::Status="Synthetic native journal preview; no game or character progress accessed.";
            NativeJournalPanel::Selected="/Preview/Journal/Vanilla.Vanilla";
            for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"journal-native.png";RenderFrame();PreviewCapture.clear();
            JournalAuthorPanel::Template(NativeJournalCache::Entries[0]);for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"journal-author.png";RenderFrame();PreviewCapture.clear();
            CurrentPage=Page::SaveEditor;
            {
                const std::string id="1234567890abcdef1234567890abcdef";
                auto choices=nlohmann::json{{"Files",nlohmann::json::array({
                    {{"Path","xbox"},{"Provider","GamePass"},{"CharacterGuid",id}},
                    {{"Path","steam"},{"Provider","SteamEpic"},{"CharacterGuid",id}},
                    {{"Path","imported"},{"Provider","Imported"},{"CharacterGuid",id}}})}};
                const auto* xbox=SaveEditorPanel::StudioCurrentSave(choices,id,"Game Pass");
                const auto* steam=SaveEditorPanel::StudioCurrentSave(choices,id,"Steam");
                if(!xbox||xbox->at("Path")!="xbox"||!steam||steam->at("Path")!="steam"||SaveEditorPanel::StudioCurrentSave(choices,"invalid","Game Pass"))throw std::runtime_error("Current character selection guessed the wrong provider or GUID");
                choices["Files"].push_back(choices["Files"][0]);
                if(SaveEditorPanel::StudioCurrentSave(choices,id,"Game Pass"))throw std::runtime_error("Ambiguous current character saves must require selection");
            }
            const auto offlineRoster=snapshot["Players"];
            snapshot["Players"]=nlohmann::json::array({{{"Name","Example Xbox character"},{"Guid","1234567890ABCDEF1234567890ABCDEF"},{"IsLocal",true},{"Platform","Game Pass"}}});
            SaveEditorPanel::WorkflowInitialized=false;SaveEditorPanel::ViewedCurrentGuid.clear();SaveEditorPanel::FollowCurrentCharacter=true;
            authoringFixture.CharacterSaveAvailable=false;
            for(int i=0;i<14;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(SaveEditorPanel::Opened||AuthoringClient::Result("SaveEditorList").at("Files").size())throw std::runtime_error("Metadata-only character must wait for its first gameplay save");
            const auto waitingLists=authoringFixture.CharacterSaveLists;
            for(int i=0;i<14;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(authoringFixture.CharacterSaveLists!=waitingLists)throw std::runtime_error("Unmatched current character scanned saves every frame");
            authoringFixture.CharacterSaveAvailable=true;SaveEditorPanel::StudioNextSaveRefresh=ImGui::GetTime()-1;
            for(int i=0;i<14;++i){AuthoringClient::Poll(true);RenderFrame();}RenderFrame();
            if(!SaveEditorPanel::Opened||HelpyPersonalization::Guid(SaveEditorPanel::Browser.value("CharacterGuid",std::string{}))!="1234567890abcdef1234567890abcdef")
                throw std::runtime_error("Current Xbox character did not automatically open an isolated copy and bind Storage");
            const auto matchedLists=authoringFixture.CharacterSaveLists;SaveEditorPanel::StudioNextSaveRefresh=0;
            for(int i=0;i<14;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(authoringFixture.CharacterSaveLists!=matchedLists)throw std::runtime_error("Matched character must stop automatic save discovery");
            if(SaveEditorPanel::RequestProfileCharacter("not-a-guid","Game Pass"))throw std::runtime_error("Profile must reject an invalid character identity");
            SaveEditorPanel::RequestProfileCharacter("1234567890abcdef1234567890abcdef","Game Pass");
            for(int i=0;i<14;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(!SaveEditorPanel::ProfileCharacterGuid.empty()||SaveEditorPanel::FollowCurrentCharacter||!SaveEditorPanel::Opened||std::string(SaveEditorPanel::File)!="preview-character.json")throw std::runtime_error("Profile character action did not select its unique isolated Xbox copy");
            SaveEditorPanel::RequestProfileCharacter("ffffffff222222223333333344444444","Steam");
            for(int i=0;i<14;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(!SaveEditorPanel::ProfileCharacterGuid.empty()||!SaveEditorPanel::Opened||std::string(SaveEditorPanel::File)!="preview-character.json"||SaveEditorPanel::Error.empty())throw std::runtime_error("An unavailable profile character must not discard the existing draft");
            SaveEditorPanel::Error.clear();SaveEditorPanel::FollowCurrentCharacter=true;
            {
                SaveEditorPanel::StudioSelectSave("another-character.json","Another character");
                if(!SaveEditorPanel::Opened||!SaveEditorPanel::StudioSwitchSave||std::string(SaveEditorPanel::File)!="preview-character.json")throw std::runtime_error("Character switch discarded a draft without confirmation");
                SaveEditorPanel::StudioSwitchSave=false;
            }
            PreviewCapture=output/"current-character.png";RenderFrame();PreviewCapture.clear();
            snapshot["Players"]=offlineRoster;SaveEditorPanel::WorkflowInitialized=false;SaveEditorPanel::ViewedCurrentGuid.clear();RenderFrame();
            for(int i=0;i<8;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(!AuthoringClient::Submit("SaveEditorList"))throw std::runtime_error("Save editor list admission failed");AuthoringClient::Poll(true);RenderFrame();
            for(int i=0;i<8;++i){AuthoringClient::Poll(true);RenderFrame();} // Drain automatic studio startup reads before the next user action.
            if(!AuthoringClient::Submit("SaveEditorOpen",{{"Path","preview-character.json"},{"Offline",true}}))throw std::runtime_error("Save editor open admission failed");AuthoringClient::Poll(true);RenderFrame();
            for(int i=0;i<8;++i){AuthoringClient::Poll(true);RenderFrame();} // Appearance tables, inventory and one bounded preview request.
            SaveEditorPanel::StudioShowPreview=false;SaveEditorPanel::FocusInventoryGrid=true;for(int i=0;i<3;++i)RenderFrame();
            if(SaveEditorPanel::InventoryRecordTable||!SaveEditorPanel::SavedPlacardsDrawn)throw std::runtime_error("Saved inventory did not use the shared Item Bay placards");
            if(!HelpyPreviewHooks::InventorySearchId||HelpyPreviewHooks::InventorySearchId==HelpyPreviewHooks::BaySearchId||HelpyPreviewHooks::InventoryRefreshId==HelpyPreviewHooks::BayRefreshId)throw std::runtime_error("Inventory and Item Bay share Search/Refresh IDs");
            if(std::abs((HelpyPreviewHooks::InventoryViewport.w-HelpyPreviewHooks::InventoryViewport.y)-(HelpyPreviewHooks::Browser.w-HelpyPreviewHooks::Browser.y))>1.f)throw std::runtime_error("Inventory and Item Bay grid heights differ");
            for(const auto slot:LoadoutEligibility::Slots){
                SaveEditorPanel::StudioSlot=std::string(slot);SaveEditorPanel::StudioSlotQuery.clear();SaveEditorPanel::StudioSlotRows=nlohmann::json::array();SaveEditorPanel::StudioSlotOffset=0;SaveEditorPanel::StudioSlotMore=SaveEditorPanel::StudioSlotOpen=true;
                for(int i=0;i<5;++i){AuthoringClient::Poll(true);RenderFrame();}
                if(SaveEditorPanel::StudioSlotRows.empty()||!LoadoutEligibility::Matches(SaveEditorPanel::StudioSlotRows[0],slot))throw std::runtime_error("Native loadout picker failed its slot filter");
                SaveEditorPanel::StudioResetSlot();
            }
            SaveEditorPanel::StudioShowPreview=true;RenderFrame();
            {
                CharacterPreviewFrame::Channel testPixels;const auto name="Local\\RuneSchema.CharacterPreview."+std::to_string(GetCurrentProcessId())+".UITest";testPixels.Open(name,true);
                if(!testPixels.Lock())throw std::runtime_error("Native preview transport test lock failed");auto* frame=testPixels.Get();std::memset(frame->Pixels,160,CharacterPreviewFrame::Bytes);frame->Serial=1;std::snprintf(frame->Status,sizeof(frame->Status),"Synthetic transport check · no Unreal rendering in this fixture");testPixels.Unlock();
                CharacterPreviewPanel::Attach(name);RenderFrame();RenderFrame();
                if(CharacterPreviewPanel::Serial!=1||!CharacterPreviewPanel::Texture||CharacterPreviewPanel::Texture->GetTexID()==ImTextureID_Invalid)throw std::runtime_error("Native preview pixels did not reach the D3D texture");
                CharacterPreviewPanel::Close();
            }
            if(!AuthoringClient::Submit("SaveEditorBrowse",{{"Pointer","/GameProgress"}}))throw std::runtime_error("Save editor browse admission failed");AuthoringClient::Poll(true);RenderFrame();
            if(!AuthoringClient::Submit("SaveEditorSet",{{"Pointer","/GameProgress/Health"},{"Value",75},{"Revision",SaveEditorPanel::Browser.at("Revision")}}))throw std::runtime_error("Save editor edit admission failed");AuthoringClient::Poll(true);RenderFrame();
            for(int i=0;i<8;++i){AuthoringClient::Poll(true);RenderFrame();}
            if(!AuthoringClient::Submit("SaveEditorPreview"))throw std::runtime_error("Save editor preview admission failed");AuthoringClient::Poll(true);RenderFrame();
            if(SaveEditorPanel::Token.empty()||SaveEditorPanel::Review.at("Count")!=1||authoringFixture.Character["GameProgress"]["Health"]!=100)throw std::runtime_error("Copy editor changed its source or lost review");
            PreviewCapture=output/"save-editor.png";RenderFrame();PreviewCapture.clear();
            if(!AuthoringClient::Submit("SaveEditorExport",{{"Token",SaveEditorPanel::Token},{"Confirm",true}}))throw std::runtime_error("Save editor export admission failed");AuthoringClient::Poll(true);RenderFrame();
            if(AuthoringClient::Result("SaveEditorExport").at("Document")["GameProgress"]["Health"]!=75||authoringFixture.Character["GameProgress"]["Health"]!=100)throw std::runtime_error("Reviewed copy export did not preserve original");
            if(!AuthoringClient::Submit("SaveEditorStack",{{"Pointer","/GameProgress/Inventory/0"},{"Count",8},{"Revision",SaveEditorPanel::Items.at("Revision")}}))throw std::runtime_error("Stack edit admission failed");AuthoringClient::Poll(true);RenderFrame();
            if(SaveEditorPanel::Items["Items"][0]["Count"]!=8||authoringFixture.Character["GameProgress"]["Inventory"]["0"]["Count"]!=5)throw std::runtime_error("Stack draft changed original or did not update");
            if(!AuthoringClient::Submit("SaveEditorRemoveItems",{{"Pointers",nlohmann::json::array({"/GameProgress/Inventory/0"})},{"Revision",SaveEditorPanel::Items.at("Revision")},{"Confirm",true}}))throw std::runtime_error("Inventory removal admission failed");AuthoringClient::Poll(true);RenderFrame();
            if(!AuthoringClient::Submit("SaveEditorPreview"))throw std::runtime_error("Inventory review admission failed");AuthoringClient::Poll(true);RenderFrame();
            PreviewCapture=output/"save-editor-removal-review.png";RenderFrame();PreviewCapture.clear();
            if(!AuthoringClient::Submit("SaveEditorExport",{{"Token",SaveEditorPanel::Token},{"Confirm",true}}))throw std::runtime_error("Inventory copy export admission failed");AuthoringClient::Poll(true);RenderFrame();
            if(AuthoringClient::Result("SaveEditorExport").at("Document")["GameProgress"]["Inventory"].contains("0")||!authoringFixture.Character["GameProgress"]["Inventory"].contains("0"))throw std::runtime_error("Inventory export changed original or retained removed item");
            CurrentPage=Page::Items;ItemGridPicker::AddToCart(items[0]);
            for(int i=0;i<4;++i)RenderFrame();
            if(HelpyPreviewHooks::Give.w>ImGui::GetIO().DisplaySize.y||HelpyPreviewHooks::Give.z>ImGui::GetIO().DisplaySize.x)throw std::runtime_error("Cart delivery button is outside the viewport");
            const auto stagedPath=items[0].at("Path").get<std::string>();
            ItemGridPicker::SharedCart.ApplyValidation(nlohmann::json::array({{{"Item",stagedPath},{"Valid",true},{"Message","ItemData verified in the current runtime. Delivery rechecks inventory."}}}));
            const auto greenBefore=HelpyPreviewHooks::CartVerifiedBadges;
            PreviewCapture=output/"item-service-cart.png";RenderFrame();PreviewCapture.clear();
            if(HelpyPreviewHooks::CartVerifiedBadges==greenBefore)throw std::runtime_error("Runtime-verified cart item has no green check");
            {const auto badge=HelpyPreviewHooks::DeliveryBadge;const auto corner=HelpyPreviewHooks::DeliveryBadgeBottomLeft;const auto& t=HelpyImGuiTheme::Current;
                if(badge.x<corner.x||badge.x-corner.x>t.X(6)||badge.w>corner.y||corner.y-badge.w>t.Y(6))throw std::runtime_error("Delivery status badge must remain inside the placard bottom-left corner");}
            if(items.size()>1&&width>=1200&&height>=850){
                const auto secondPath=items[1].at("Path").get<std::string>();
                ItemGridPicker::AddToCart(items[1]);CurrentPage=Page::Map;SharedCartVisible=true;
                RenderFrame();HelpyPreviewHooks::CartRects.clear();RenderFrame();
                const auto first=HelpyPreviewHooks::CartRects.at(stagedPath),second=HelpyPreviewHooks::CartRects.at(secondPath);
                if(std::abs(first.y-second.y)>2||second.x<=first.x||first.w-first.y<HelpyImGuiTheme::Current.Y(96)){
                    std::ofstream(output/"cart-layout-debug.txt")<<"first="<<first.x<<","<<first.y<<","<<first.z<<","<<first.w<<" second="<<second.x<<","<<second.y<<","<<second.z<<","<<second.w;
                    throw std::runtime_error("Delivery cart did not render side-by-side full-height placards");
                }
                PreviewCapture=output/"delivery-cart-placards.png";RenderFrame();PreviewCapture.clear();
                ItemGridPicker::RemoveFromCart(secondPath);SharedCartVisible=false;CurrentPage=Page::Items;RenderFrame();
            }
            if(!ItemGridPicker::SharedCart.BeginDelivery(999))throw std::runtime_error("Cart receipt fixture did not start");
            ItemGridPicker::SharedCart.CompleteDelivery(999,{{"GrantResults",nlohmann::json::array({{{"Item",stagedPath},{"Count",1},{"State","Rejected"},{"Message","Item unavailable; skipped without blocking other items."}}})}});
            const auto redBefore=HelpyPreviewHooks::CartRejectedBadges;
            PreviewCapture=output/"item-service-cart-rejected.png";RenderFrame();PreviewCapture.clear();
            if(HelpyPreviewHooks::CartRejectedBadges==redBefore||!ItemGridPicker::SharedCart.Entries().empty())throw std::runtime_error("Rejected cart item was not cleaned with a retained red-X outcome");
            ItemGridPicker::ClearSharedCart();
            if(width>=1200&&height>=850){
                namespace Hooks=HelpyPreviewHooks;
                const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
                const auto center=[](const ImVec4& rect){return ImVec2((rect.x+rect.z)*0.5f,(rect.y+rect.w)*0.5f);};
                const auto mouse=[&](ImVec2 point,bool down,int frames=2){Hooks::MouseEnabled=true;Hooks::Mouse=point;Hooks::MouseDown=down;for(int i=0;i<frames;++i)RenderFrame();};
                {
                    auto& cart=ItemGridPicker::SharedCart;
                    check(cart.Add(items[0]),"Reorder fixture could not stage an item");cart.SetQuantity(stagedPath,4);
                    check(cart.BeginDelivery(1000),"Reorder fixture could not start");
                    check(cart.CompleteDelivery(1000,{{"GrantResults",nlohmann::json::array({{{"Item",stagedPath},{"Count",4},{"State","Confirmed"}}})}})==1,"Reorder fixture was not confirmed");
                    mouse({-100,-100},false,3);
                    for(int heading=0;heading<2;++heading){
                        mouse(center(heading?Hooks::DeliveryHeading:Hooks::DeliveryRects.at(stagedPath)),false);
                        Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;RenderFrame();RenderFrame();
                        check(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId),"Last delivery right-click did not open Reorder");
                        PreviewCapture=output/(heading?"cart-reorder-all.png":"cart-reorder-item.png");RenderFrame();PreviewCapture.clear();
                        const auto action=center(Hooks::ReorderAction);mouse(action,true);mouse(action,false,3);
                        check(cart.Entries().size()==1&&cart.Payload()[0]["Count"]==(heading?8:4)&&!cart.Delivery(),"Reorder did not stage sent quantities without delivering");
                    }
                    ItemGridPicker::ClearSharedCart();mouse({-100,-100},false,3);
                    check(items.size()>2,"Cart selection fixture needs three items");
                    const auto secondPath=items[1].at("Path").get<std::string>(),thirdPath=items[2].at("Path").get<std::string>();
                    for(int i=0;i<3;++i)check(cart.Add(items[i]),"Multi-select fixture could not stage items");
                    SharedCartVisible=true;CurrentPage=Page::Map;mouse({-100,-100},false,3);
                    for(const auto& entry:cart.Entries()){
                        const auto card=Hooks::CartRects.at(entry.Path),select=Hooks::CartChecks.at(entry.Path),quantity=Hooks::CartQuantities.at(entry.Path);
                        check(select.y>=card.w&&std::abs(select.y-quantity.y)<=1.f,"Cart selection must share the quantity row below its card");
                        check(select.z<=quantity.x&&quantity.z<=card.z+1.f,"Cart quantity controls overlap or extend past their card");
                    }
                    const auto click=[&](const ImVec4& rect){const auto at=center(rect);mouse(at,false);mouse(at,true);mouse(at,false,3);};
                    click(Hooks::CartRects.at(stagedPath));check(cart.Selection().size()==1,"Cart click did not select one item");
                    Hooks::ControlDown=true;click(Hooks::CartRects.at(secondPath));Hooks::ControlDown=false;RenderFrame();
                    check(cart.Selection().size()==2,"Ctrl+click did not extend cart selection");
                    click(Hooks::CartChecks.at(stagedPath));check(cart.Selection().size()==1&&cart.Selection().contains(secondPath),"Checkbox did not toggle one selected item");
                    click(Hooks::CartMoveUp);check(cart.Entries()[0].Path==secondPath,"Move Up did not reorder selected cart entry");
                    click(Hooks::CartMoveDown);check(cart.Entries()[1].Path==secondPath,"Move Down did not reorder selected cart entry");
                    PreviewCapture=output/"cart-multi-selection.png";RenderFrame();PreviewCapture.clear();
                    click(Hooks::CartExport);check(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId),"Visible Export did not open preset form");
                    {const auto at=center(Hooks::PresetDialog),expected=ImGui::GetMainViewport()->GetWorkCenter();check(std::abs(at.x-expected.x)<2&&std::abs(at.y-expected.y)<2,"Export preset form is not centered");}
                    PreviewCapture=output/"cart-export-preset.png";RenderFrame();PreviewCapture.clear();
                    ImGui::ClosePopupToLevel(0,true);mouse({-100,-100},false,3);
                    click(Hooks::CartImport);check(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId),"Visible Import did not open preset picker");
                    {const auto at=center(Hooks::PresetDialog),expected=ImGui::GetMainViewport()->GetWorkCenter();check(std::abs(at.x-expected.x)<2&&std::abs(at.y-expected.y)<2,"Import preset form is not centered");}
                    PreviewCapture=output/"cart-import-presets.png";RenderFrame();PreviewCapture.clear();
                    ImGui::ClosePopupToLevel(0,true);mouse({-100,-100},false,3);
                    click(Hooks::CartRemoveSelected);check(cart.Entries().size()==2&&!cart.Selection().size(),"Remove selected did not retain unselected items");
                    cart.SelectAll();check(cart.BeginDelivery(1001),"Selected reorder fixture did not start");
                    cart.CompleteDelivery(1001,{{"GrantResults",nlohmann::json::array({{{"Item",stagedPath},{"Count",1},{"State","Confirmed"}},{{"Item",thirdPath},{"Count",1},{"State","Rejected"}}})}});
                    mouse({-100,-100},false,3);click(Hooks::DeliveryChecks.at(stagedPath));
                    check(cart.OutcomeSelection().size()==1,"Delivery checkbox did not select confirmed item");
                    click(Hooks::ReorderSelected);check(cart.Entries().size()==1&&cart.Entries()[0].Path==stagedPath&&!cart.Delivery(),"Reorder selected did not stage only confirmed selection");
                    PreviewCapture=output/"cart-reorder-selected.png";RenderFrame();PreviewCapture.clear();
                    ItemGridPicker::ClearSharedCart();SharedCartVisible=false;CurrentPage=Page::Items;mouse({-100,-100},false,3);
                }
                const auto drag=[&](ImVec2 from,ImVec2 to,const char* kind=nullptr,const char* capture=nullptr){const auto savedOrder=HelpyNavigationOrder::Orders;mouse(from,false);mouse(from,true);mouse(ImVec2(from.x+12,from.y+12),true);mouse(to,true,3);
                    if(kind){check(DragFeedback::LastFrame==ImGui::GetFrameCount()&&DragFeedback::Kind==kind&&!DragFeedback::Label.empty(),"Held drag card did not identify the current payload");const auto card=DragFeedback::Card;check(card.z>card.x&&card.w>card.y,"Held drag card has no visible bounds");}
                    check(HelpyNavigationOrder::Orders==savedOrder,"Navigation was reordered before releasing the dragged item");
                    if(capture){PreviewCapture=output/capture;RenderFrame();PreviewCapture.clear();}mouse(to,false,3);};
                check(HelpyNavigationOrder::Load(output/("navigation-drag-"+std::to_string(GetCurrentProcessId())+".json")),"Navigation fixture preferences unavailable");
                CurrentPage=Page::Items;for(int i=0;i<3;++i)RenderFrame();
                float lastRouteBottom=0;for(auto page:HelpyNavigation::SidebarPages)if(page!=Page::Home&&page!=Page::Profile&&page!=Page::BugReport&&HelpyNavigation::Available(page,HelpyImGuiTheme::Current.workspaces,HelpyEnvironment::Server()))lastRouteBottom=std::max(lastRouteBottom,Hooks::Navigation.at(PageTitle(page)).w);
                check(Hooks::Navigation.at("Home").y>lastRouteBottom,"Home must stay below the visible reorderable workspace routes");
                const auto homeAtBottom=Hooks::Navigation.at("Home");check(homeAtBottom.y-lastRouteBottom<HelpyImGuiTheme::Current.Y(32),"Home must follow the draggable primary navigation divider without a bottom spacer");
                const auto primaryRow=Hooks::Navigation.at("Players");
                check(std::abs(homeAtBottom.x-primaryRow.x)<2&&std::abs(homeAtBottom.z-primaryRow.z)<2&&std::abs((homeAtBottom.w-homeAtBottom.y)-(primaryRow.w-primaryRow.y))<2&&PageIcon(Page::Home)==HelpyImGuiTheme::Icon::Building,"Home must match the primary navigation row width, height and icon alignment");
                const auto profileAtBottom=Hooks::Navigation.at("Profile");
                check(std::abs(profileAtBottom.x-homeAtBottom.x)<2&&std::abs(profileAtBottom.z-homeAtBottom.z)<2&&profileAtBottom.y>=homeAtBottom.w&&profileAtBottom.w<=MainSurfacePosition.y+MainSurfaceSize.y,"Profile must be a matching full-width row below Home");
                const auto bugBelow=Hooks::Navigation.at("Report a bug");
                check(std::abs(bugBelow.x-homeAtBottom.x)<2&&std::abs(bugBelow.z-homeAtBottom.z)<2&&bugBelow.y>=profileAtBottom.w,"Report a bug must be a matching full-width row below Profile");
                check(Hooks::Navigation.at("Settings").w<homeAtBottom.y,"Settings must default to the primary navigation");
                check(std::abs((homeAtBottom.x+bugBelow.z-Hooks::NavigationRail.x-Hooks::NavigationRail.z)*.5f)<HelpyImGuiTheme::Current.X(2),"Home/Profile/Report group must be centered in the sidebar");
                drag(center(Hooks::Navigation.at("Mods")),center(Hooks::Navigation.at("World")),"Navigation","drag-navigation.png");
                {const auto order=HelpyNavigationOrder::Sidebar();check(std::find(order.begin(),order.end(),int(Page::Mods))<std::find(order.begin(),order.end(),int(Page::Map)),"Sidebar drag did not save reordered route IDs");}
                check(HelpyNavigationOrder::Reset(HelpyNavigationOrder::SidebarSurface),"Sidebar reset failed");
                for(int i=0;i<3;++i)RenderFrame();
                drag(center(Hooks::Navigation.at("Report a bug")),center(Hooks::Navigation.at("World")),"Navigation","drag-navigation-across-divider.png");
                {const auto order=HelpyNavigationOrder::Sidebar();check(std::find(order.begin(),order.end(),int(Page::BugReport))<std::find(order.begin(),order.end(),HelpyNavigationOrder::SidebarDivider),"Footer route did not move across the divider");}
                check(HelpyNavigationOrder::Reset(HelpyNavigationOrder::SidebarSurface),"Sidebar reset failed");
                CurrentPage=Page::Items;for(int i=0;i<3;++i)RenderFrame();
                drag(center(Hooks::WorkspaceTabs.at("Recipes")),center(Hooks::WorkspaceTabs.at("Items")),"Tab","drag-tab.png");
                check(NavigationOrder::Pages("players-tabs",HelpyNavigation::PlayerTabs).front()==Page::Recipes,"Workspace tab drag did not save its route order");
                check(HelpyNavigationOrder::Reset("players-tabs"),"Workspace tab reset failed");
                CurrentPage=Page::Home;HomeExit={};for(int i=0;i<4;++i)RenderFrame();
                const auto mapTile=std::find_if(HomeDestinations.begin(),HomeDestinations.end(),[](const auto& entry){return entry.Target==Page::Map;});drag(center(PreviewHomeTiles[std::distance(HomeDestinations.begin(),mapTile)]),center(PreviewHomeTiles[0]),"Home tile","drag-home.png");
                std::vector<Page> homePages;for(const auto& destination:HomeDestinations)homePages.push_back(destination.Target);
                check(NavigationOrder::Pages("home",homePages).front()==Page::Map,"Home tile drag did not save its route order");
                check(HelpyNavigationOrder::Reset("home"),"Home order reset failed");mouse({-100,-100},false,3);HomeExit={};
                const auto path=items[0].at("Path").get<std::string>();
                CurrentPage=Page::Items;for(int i=0;i<4;++i)RenderFrame();
                const auto originalTheme=HelpyImGuiTheme::Current;
                const auto split=center(Hooks::ResizeHandles.at("item-bay-details"));
                mouse(split,false);mouse(split,true);mouse({split.x,split.y-45},true,3);mouse({split.x,split.y-45},false,3);
                check(HelpyImGuiTheme::Current.itemBayBrowseRatio<originalTheme.itemBayBrowseRatio,"Item Bay divider did not resize the browser");
                const auto card=Hooks::CardScale;const ImVec2 larger{card.x+std::min(110.0f,(card.z-card.x)*.6f),(card.y+card.w)*.5f};
                mouse(larger,false);mouse(larger,true);mouse(larger,false,3);
                check(HelpyImGuiTheme::Current.gridCardHeight>originalTheme.gridCardHeight,"Card size slider did not resize placards");
                HelpyImGuiTheme::Current=originalTheme;HelpyImGuiTheme::SaveCurrent();mouse({-100,-100},false,3);
                check(Hooks::ItemRects.contains(path),"First visible item has no drag surface");
                const auto delay=HelpyImGuiTheme::Current.tooltipDelay;HelpyImGuiTheme::Current.tooltipDelay=0;
                mouse(center(Hooks::ItemRects.at(path)),false,5);
                check(Hooks::TooltipPath==path&&Hooks::TooltipFacts>0,"Hover did not show native item details");
                const auto tooltipBounds=Hooks::Tooltip;mouse(center(Hooks::ItemRects.at(path)),false,30);
                check(Hooks::Tooltip.x==tooltipBounds.x&&Hooks::Tooltip.y==tooltipBounds.y&&Hooks::Tooltip.z==tooltipBounds.z&&Hooks::Tooltip.w==tooltipBounds.w,"Hover details changed geometry while stationary");
                check(tooltipBounds.x>=0&&tooltipBounds.y>=0&&tooltipBounds.z<=width&&tooltipBounds.w<=height,"Hover details escaped viewport");
                mouse(center(tooltipBounds),false,1);check(!ItemGridPicker::DetailsPopup.Open||ItemGridPicker::DetailsPopup.Path!=path,"Moving off the item kept its old details open");
                mouse(center(Hooks::ItemRects.at(path)),false,5);check(ItemGridPicker::DetailsPopup.Open,"Returning to the item did not restore its details");
                PreviewCapture=output/"item-hover.png";RenderFrame();PreviewCapture.clear();
                const auto savedDetails=ItemDetails::Cache[path].Native;
                std::string longDescription;for(int i=0;i<100;++i)longDescription+="A detailed native description with wrapping and additional information. ";
                ItemDetails::Cache[path].Native["Description"]=longDescription;mouse(center(Hooks::ItemRects.at(path)),false,5);
                check(Hooks::TooltipScrollMax>0&&Hooks::Tooltip.w<=height,"Long details did not become a viewport-bounded scrolling popup");
                ImGui::GetIO().AddMouseWheelEvent(0,-4);RenderFrame();RenderFrame();check(Hooks::TooltipScroll>0,"Long item details cannot be scrolled");
                PreviewCapture=output/"item-hover-long.png";RenderFrame();PreviewCapture.clear();ItemDetails::Cache[path].Native=savedDetails;
                mouse({-20,-20},false,1);check(!ItemGridPicker::DetailsPopup.Open,"Hover details did not close immediately after leaving the item");
                HelpyImGuiTheme::Current.tooltipDelay=delay;
                drag(center(Hooks::ItemRects.at(path)),center(Hooks::Cart),"Item","drag-item.png");
                check(ItemGridPicker::SharedCart.Entries().size()==1,"Dragging a browser item did not stage it in the cart");
                check(ItemGridPicker::SharedCart.Payload(3)[0]["Count"]==100,"Dragging an item lost its stack metadata");
                check(Hooks::CartRects.contains(path),"Staged cart item has no return drag surface");
                drag(center(Hooks::CartRects.at(path)),ImVec2(Hooks::Browser.x+30,Hooks::Browser.y+30),"Cart item","drag-cart-item.png");
                check(ItemGridPicker::SharedCart.Entries().empty(),"Dragging a cart item back did not remove it");
                mouse(center(Hooks::ItemRects.at(path)),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;RenderFrame();RenderFrame();
                check(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId),"Right-click did not open item actions");
                mouse(center(Hooks::AddCart),true);mouse(center(Hooks::AddCart),false);
                check(ItemGridPicker::SharedCart.Entries().size()==1,"Context-menu Add to cart did not stage the item");ItemGridPicker::ClearSharedCart();
                CurrentPage=Page::Creation;for(int i=0;i<4;++i)RenderFrame();
                drag(center(Hooks::ItemRects.at(path)),center(Hooks::AuthoringSource));
                // Choose a fixture on the first A-Z row in every workspace,
                // rather than an old discovery-order index now off-screen.
                const auto appearancePath=items[10].at("Path").get<std::string>();drag(center(Hooks::ItemRects.at(appearancePath)),center(Hooks::AuthoringAppearance));
                check(ItemAuthoringPanel::Drafts[0].Source.value("Path",std::string{})==path&&ItemAuthoringPanel::Drafts[0].Appearance.value("Path",std::string{})==appearancePath,"Transmog source or appearance drop failed");
                check(ItemAuthoringPanel::Drafts[0].Ready,"Source inspection was not correlated");check(ItemAuthoringPanel::Drafts[0].Source.value("Masterwork",false),"Dropping a MW item lost its shared placard treatment");
                PreviewCapture=output/"transmog-filled.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::CreationTab),false,3);mouse(center(Hooks::CreationTab),true);mouse(center(Hooks::CreationTab),false,4);
                if(ItemAuthoringPanel::Active!=1){std::ofstream debug(output/"tab-debug.txt");const auto r=Hooks::CreationTab;debug<<r.x<<","<<r.y<<","<<r.z<<","<<r.w<<" mouse "<<Hooks::Mouse.x<<","<<Hooks::Mouse.y;PreviewCapture=output/"tab-failed.png";RenderFrame();PreviewCapture.clear();throw std::runtime_error("Creation tab did not open");}
                drag(center(Hooks::ItemRects.at(appearancePath)),center(Hooks::AuthoringSource));
                check(ItemAuthoringPanel::Drafts[1].Source.value("Path",std::string{})==appearancePath&&ItemAuthoringPanel::Drafts[0].Source.value("Path",std::string{})==path,"Creation changed the independent transmog source");
                std::snprintf(ItemAuthoringPanel::Drafts[1].Editor.data(),ItemAuthoringPanel::Drafts[1].Editor.size(),"%s",R"({"PowerLevel": 10, "@row/WeaponStats/DamageMultiplier": 3.5})");
                PreviewCapture=output/"creation-filled.png";RenderFrame();PreviewCapture.clear();
                ItemAuthoringPanel::Submit(ItemAuthoringPanel::Drafts[1],ItemAuthoring::Request(ItemAuthoringPanel::Drafts[1],ItemAuthoring::Mode::Creation,"",true));for(int i=0;i<4;++i)RenderFrame();
                check(authoringRequest.value("Action",std::string{})=="PrepareClone"&&authoringRequest["StatOverrides"]["WeaponStats"]["DamageMultiplier"]==3.5&&!ItemAuthoringPanel::Drafts[1].Prepared.empty(),"Creation JSONC preview lost its edited native stats");
                CurrentPage=Page::Recipes;for(int i=0;i<4;++i)RenderFrame();
                drag(center(Hooks::ItemRects.at(path)),center(Hooks::Inputs));
                check(RecipeStudioPanel::State().Inputs.size()==1,"Recipe input drop failed");
                drag(center(Hooks::ItemRects.at(path)),center(Hooks::Outputs));
                check(RecipeStudioPanel::State().Outputs.size()==1,"Recipe output drop failed");
                check(RecipeStudioPanel::State().Inputs[0]["ItemData"]==path&&RecipeStudioPanel::State().Outputs[0]["ItemData"]==path,"Recipe drop changed item identity");
                RecipeStudioPanel::State().Inputs=nlohmann::json::array();RecipeStudioPanel::State().Outputs=nlohmann::json::array();
                ItemGridPicker::AddToCart(items[0]);ItemGridPicker::SharedCart.SetQuantity(path,7);SharedCartVisible=true;
                for(int i=0;i<4;++i)RenderFrame();
                const auto second=appearancePath;
                check(Hooks::ItemRects.contains(second),"Recipe bay item is missing its shared drag surface");
                PreviewCapture=output/"shared-cart-before.png";RenderFrame();PreviewCapture.clear();
                drag(center(Hooks::ItemRects.at(second)),center(Hooks::Cart));
                if(ItemGridPicker::SharedCart.Entries().size()!=2){
                    PreviewCapture=output/"shared-cart-failed.png";RenderFrame();PreviewCapture.clear();
                    std::ofstream debug(output/"shared-cart-debug.txt");const auto from=center(Hooks::ItemRects.at(second)),to=center(Hooks::Cart);
                    debug<<"from="<<from.x<<","<<from.y<<" to="<<to.x<<","<<to.y<<" entries="<<ItemGridPicker::SharedCart.Entries().size()<<" status="<<ItemGridPicker::CartStatus;
                }
                check(ItemGridPicker::SharedCart.Entries().size()==2,"Recipe bay could not stage an item into the shared cart");
                PreviewCapture=output/"shared-cart.png";RenderFrame();PreviewCapture.clear();
                CurrentPage=Page::Creation;for(int i=0;i<4;++i)RenderFrame();
                check(ItemGridPicker::SharedCart.Entries().size()==2&&ItemGridPicker::SharedCart.Entries()[0].Quantity==7,"Changing workspaces lost shared cart contents or quantities");
                CurrentPage=Page::Items;for(int i=0;i<4;++i)RenderFrame();
                check(ItemGridPicker::SharedCart.Entries().size()==2,"Item Service did not use the shared cart");
                const auto stagedCart=ItemGridPicker::SharedCart.Payload();
                SpawnToolsPanel::CartRecipientsChosen=true;
                SpawnToolsPanel::CartRecipients={{"preview-player","Local player"},{"preview-other","Other player"}};
                snapshot["Players"].push_back({{"Path","preview-other"},{"Name","Other player"}});
                for(int i=0;i<3;++i)RenderFrame();
                check(Hooks::RecipientRemove.contains("preview-other"),"Selected recipient has no remove control");
                mouse(center(Hooks::RecipientRemove.at("preview-other")),true);mouse(Hooks::Mouse,false,3);
                check(SpawnToolsPanel::EffectiveCartRecipients().size()==1&&ItemGridPicker::SharedCart.Payload()==stagedCart,"Removing a recipient failed or changed cart items");
                PreviewCapture=output/"cart-recipient-remove.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::RecipientsClear),true);mouse(Hooks::Mouse,false,3);
                check(SpawnToolsPanel::EffectiveCartRecipients().empty()&&SpawnToolsPanel::SelectedPlayer.empty()&&ItemGridPicker::SharedCart.Payload()==stagedCart,"Clear recipients failed or changed cart items");
                snapshot["Players"].erase(snapshot["Players"].end()-1);
                SpawnToolsPanel::CartRecipientsChosen=false;SpawnToolsPanel::SelectedPlayer="preview-player";
                SharedCartVisible=false;ItemGridPicker::ClearSharedCart();
                CurrentPage=Page::Map;for(int i=0;i<6;++i)RenderFrame();
                check(MapPanel::Data.Projection.Valid()&&MapPanel::Data.Texture&&MapPanel::Data.Texture->Status==ImTextureStatus_OK,"Map did not load native-format image and alignment");
                check(Hooks::MapPartyWindow.y>=Hooks::Map.w&&Hooks::MapPartyWindow.x>=Hooks::Map.x-1&&Hooks::MapPartyWindow.z<=Hooks::Map.z+1,"Party must occupy a separate section below the map");
                const auto partyBefore=Hooks::MapPartyWindow;for(int i=0;i<12;++i)RenderFrame();
                check(std::abs(Hooks::MapPartyWindow.w-partyBefore.w)<1&&std::abs(Hooks::MapPartyWindow.y-partyBefore.y)<1,"Docked party section drifted between frames");
                PreviewCapture=output/"world-party-docked.png";RenderFrame();PreviewCapture.clear();
                check(Hooks::MapPartyRows.contains("preview-other"),"Map party list omitted a connected member");
                mapPlacement=nlohmann::json::object();
                mouse(center(Hooks::MapPartyRows.at("preview-other")),true);mouse(Hooks::Mouse,false,3);
                check(MapPanel::Data.PartySelection=="preview-other"&&mapPlacement.empty()&&!MapPanel::Data.Popup,"Party LMB did not select without issuing travel");
                const auto partyMenu=[&]{mouse(center(Hooks::MapPartyRows.at("preview-other")),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<3;++i)RenderFrame();};
                partyMenu();const auto contextBefore=Hooks::MapPartyContext;
                for(int i=0;i<120;++i)RenderFrame();
                check(std::abs((Hooks::MapPartyContext.z-Hooks::MapPartyContext.x)-(contextBefore.z-contextBefore.x))<1&&std::abs((Hooks::MapPartyContext.w-Hooks::MapPartyContext.y)-(contextBefore.w-contextBefore.y))<1,"Party context menu self-resized over repeated frames");
                check(Hooks::MapPartyContext.y>=0&&Hooks::MapPartyContext.w<=ImGui::GetIO().DisplaySize.y&&Hooks::MapPartyContext.z<=ImGui::GetIO().DisplaySize.x,"Party menu escaped the viewport");
                mouse(center(Hooks::MapPartyTeleport),true);mouse(Hooks::Mouse,false,3);
                check(mapPlacement.empty()&&MapPanel::Data.PartyTarget=="preview-other"&&!MapPanel::Data.PartyRecall,"Party menu must require confirmation before teleport");
                PreviewCapture=output/"map-party-confirm.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapPartyCancel),true);mouse(Hooks::Mouse,false,3);check(mapPlacement.empty(),"Cancelled party travel submitted a mutation");
                partyMenu();mouse(center(Hooks::MapPartyRecall),true);mouse(Hooks::Mouse,false,3);
                mouse(center(Hooks::MapPartyConfirm),true);mouse(Hooks::Mouse,false,4);
                check(mapPlacement.value("Player",std::string{})=="preview-other"&&mapPlacement.value("TargetPlayer",std::string{})=="preview-player"&&!mapPlacement.contains("TargetLocation"),"Recall did not preserve member-to-host identities");
                mapPlacement=nlohmann::json::object();
                partyMenu();mouse(center(Hooks::MapPartyTeleport),true);mouse(Hooks::Mouse,false,3);
                mouse(center(Hooks::MapPartyConfirm),true);mouse(Hooks::Mouse,false,4);
                check(mapPlacement.value("Player",std::string{})=="preview-player"&&mapPlacement.value("TargetPlayer",std::string{})=="preview-other","Teleport to reversed the moving and destination players");
                const auto savedPartyAuthority=MapPanel::Data.Snapshot["CanBuildAndTeleport"];MapPanel::Data.Snapshot["CanBuildAndTeleport"]=false;
                partyMenu();mapPlacement=nlohmann::json::object();mouse(center(Hooks::MapPartyRecall),true);mouse(Hooks::Mouse,false,3);
                check(mapPlacement.empty(),"Party menu issued travel without authority");
                mouse(ImVec2(-100,-100),true);mouse(Hooks::Mouse,false,3);MapPanel::Data.Snapshot["CanBuildAndTeleport"]=savedPartyAuthority;
                MapPanel::Data.Center={.5,.5};for(int i=0;i<3;++i)RenderFrame();
                {
                    const auto savedMapCache=HelpyMapCache::Current();auto& cache=HelpyMapCache::Current();
                    const std::string fixture=R"({"source":"RuneScape: Dragonwilds Wiki","licence":"CC BY-NC-SA 3.0","built":"2026-10-16","groups":[{"id":"mining","label":"Ores and rocks","categories":[{"name":"Iron","points":[[198000,98000],[208000,98000]]}]},{"id":"trees","label":"Trees","categories":[{"name":"Oak","points":[[190000,108000]]}]}]})";
                    cache.Data=std::make_shared<MapFilters::Dataset>(MapFilters::Parse(fixture));cache.Filters={};cache.CachedBuild="CL-251392";++cache.Revision;
                    std::strcpy(MapPanel::Data.MarkerSearch,"iron");for(int i=0;i<3;++i)RenderFrame();
                    check(Hooks::MapReferenceMatches==2&&Hooks::MapReferenceClusters>0,"Map reference search must filter cached types without a native census");
                    PreviewCapture=output/"map-resource-search.png";RenderFrame();PreviewCapture.clear();
                    Hooks::MapForceGroupsOpen=true;mouse(center(Hooks::MapFiltersButton),true);mouse(Hooks::Mouse,false,3);
                    check(Hooks::MapCategoryControls.contains("mining:Iron"),"Resource filter category control missing");
                    PreviewCapture=output/"map-resource-filters.png";RenderFrame();PreviewCapture.clear();
                    mouse(center(Hooks::MapCategoryControls.at("mining:Iron")),true);mouse(Hooks::Mouse,false,3);
                    check(cache.Filters.Hidden.contains("mining:Iron")&&Hooks::MapReferenceMatches==0,"Map category checkbox did not remove only its reference layer");
                    check(Hooks::MapFiltersClose.w<=height&&Hooks::MapFiltersClose.z<=width,"Map filter close control escaped the viewport");
                    mouse(center(Hooks::MapFiltersClose),true);mouse(Hooks::Mouse,false,3);Hooks::MapForceGroupsOpen=false;
                    MapPanel::Data.MarkerSearch[0]=0;for(int i=0;i<2;++i)RenderFrame();check(Hooks::MapReferenceMatches==1,"Clearing map query did not retain the hidden Iron filter while showing Oak");
                    cache=savedMapCache;++cache.Revision;HelpyMapCache::SaveFilters();for(int i=0;i<2;++i)RenderFrame();
                }
                // Keep the ground fixture away from selectable player/marker pins.
                const ImVec2 mapCenter{Hooks::Map.x+(Hooks::Map.z-Hooks::Map.x)*.4f,Hooks::Map.y+(Hooks::Map.w-Hooks::Map.y)*.65f};
                const auto expectedMapPoint=MapPanel::Data.Projection.ToWorld(MapPanel::Data.View.ToUV({std::floor(mapCenter.x)-Hooks::Map.x,std::floor(mapCenter.y)-Hooks::Map.y}));
                check(Hooks::Map.z-Hooks::Map.x>Hooks::Map.w-Hooks::Map.y,"Map still renders as a narrow square instead of a wide viewport");
                mouse(mapCenter,true);mouse(mapCenter,false,3);
                check(MapPanel::Data.HasPoint&&!MapPanel::Data.Popup,"Left selection unexpectedly opened map actions");
                check(Hooks::MapPartyRows.contains("preview-other"),"Map party list did not show the connected member");
                mouse(mapCenter,false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<3;++i)RenderFrame();
                check(MapPanel::Data.Popup,"Right-click did not open map placement");
                const auto popupBefore=Hooks::MapPopupWindow;for(int i=0;i<120;++i)RenderFrame();
                check(std::abs((Hooks::MapPopupWindow.z-Hooks::MapPopupWindow.x)-(popupBefore.z-popupBefore.x))<1&&std::abs((Hooks::MapPopupWindow.w-Hooks::MapPopupWindow.y)-(popupBefore.w-popupBefore.y))<1,"Map options popup shrank over repeated frames");
                const auto mapPopupCenter=ImGui::GetMainViewport()->GetWorkCenter();
                check(std::abs((Hooks::MapPopupWindow.x+Hooks::MapPopupWindow.z)*.5f-mapPopupCenter.x)<1&&std::abs((Hooks::MapPopupWindow.y+Hooks::MapPopupWindow.w)*.5f-mapPopupCenter.y)<1,"Map action details must be centered in Helpy's viewport");
                MapPanel::Data.Definition=definitions[16];MapPanel::Data.Kind=2;MapPanel::Data.Permanent=false;for(int i=0;i<3;++i)RenderFrame();
                for(int i=0;i<20&&(MapPanel::Data.ProbePending||MapPanel::Data.ProbeWanted||!MapPanel::Data.Ground.value("GroundAvailable",false));++i)RenderFrame();
                check(std::abs((Hooks::MapOperationRow.x+Hooks::MapOperationRow.z)-(Hooks::MapOptionsBody.x+Hooks::MapOptionsBody.z))<3,"Map operation controls are not centered");
                check(std::abs(Hooks::MapSearchField.x-Hooks::MapOptionsBody.x)<2&&std::abs(Hooks::MapSearchField.z-Hooks::MapOptionsBody.z)<2,"Map search field does not fill the symmetric body");
                PreviewCapture=output/"map-spawn-ready.png";RenderFrame();PreviewCapture.clear();
                check(Hooks::MapSpawn.w<=height,"Map action footer escaped the viewport");
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                if(mapPlacement.empty()){std::ofstream debug(output/"map-spawn-debug.txt");debug<<"operation="<<MapPanel::Data.Operation<<" ground="<<MapPanel::Data.Ground.dump()<<" probe="<<MapPanel::Data.ProbePending<<" wanted="<<MapPanel::Data.ProbeWanted<<" selected="<<SpawnToolsPanel::SelectedPlayer<<" button="<<Hooks::MapSpawn.x<<","<<Hooks::MapSpawn.y<<","<<Hooks::MapSpawn.z<<","<<Hooks::MapSpawn.w;}
                check(!mapPlacement.empty()&&mapPlacement["Resource"]==true&&mapPlacement["Permanent"]==false,"Map did not submit the temporary resource choice");
                check(std::abs(mapPlacement["TargetLocation"]["X"].get<double>()-expectedMapPoint.X)<1&&std::abs(mapPlacement["TargetLocation"]["Y"].get<double>()-expectedMapPoint.Y)<1,"Map click changed the selected world position");
                mouse(mapCenter,false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<3;++i)RenderFrame();
                check(MapPanel::Data.Popup,"Map placement could not be opened again");
                MapPanel::Data.Definition=definitions[8];MapPanel::Data.Kind=1;MapPanel::Data.Permanent=true;for(int i=0;i<3;++i)RenderFrame();
                PreviewCapture=output/"map-placement.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement["Resource"]==false&&mapPlacement["Permanent"]==true,"Map did not submit the permanent enemy choice");
                mouse(center(Hooks::Map),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<5;++i)RenderFrame();
                MapPanel::Data.Operation=1;MapPanel::Data.Buildings=nlohmann::json::array();MapPanel::Data.BuildingsWanted=false;MapPanel::Data.BuildingsMore=false;MapPanel::Data.BuildingsOffset=0;for(int i=0;i<5;++i)RenderFrame();
                mouse(center(Hooks::BuildingBrowse),true);mouse(center(Hooks::BuildingBrowse),false,4);
                check(MapPanel::Data.Buildings.size()==2&&MapPanel::Data.Ground.value("GroundAvailable",false),"Opening an empty native building menu did not request its catalogue");
                check(MapPanel::Data.BuildingBrowserOpen&&Hooks::BuildingCards.contains("/Preview/Buildings/IronChest.IronChest"),"Expanded native building menu did not expose the chest card");
                mouse(center(Hooks::BuildingCards.at("/Preview/Buildings/IronChest.IronChest")),true);mouse(center(Hooks::BuildingCards.at("/Preview/Buildings/IronChest.IronChest")),false);
                check(MapPanel::Data.Building.value("Name",std::string{})=="Iron Chest","Building icon card did not select the chest");
                PreviewCapture=output/"map-building-browser.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::BuildingUse),true);mouse(center(Hooks::BuildingUse),false,4);
                MapPanel::Data.Permanent=true;for(int i=0;i<3;++i)RenderFrame();
                const auto validGround=MapPanel::Data.Ground;MapPanel::Data.Ground={{"GroundAvailable",false},{"Error","Ground is unavailable at this point."}};mapPlacement=nlohmann::json::object();
                for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"map-building-blocked.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);check(mapPlacement.empty(),"Unloaded ground sent a building mutation");
                MapPanel::Data.Ground=validGround;for(int i=0;i<3;++i)RenderFrame();
                PreviewCapture=output/"map-buildings.png";RenderFrame();PreviewCapture.clear();mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement["Action"]=="SpawnBuilding"&&mapPlacement["Permanent"]==true&&mapPlacement["Building"]=="/Preview/Buildings/IronChest.IronChest","Map building placement lost its piece or save mode");
                for(int i=0;i<4;++i)RenderFrame();check(MapPanel::Data.PlacementResult.value("Active",false),"Building placement receipt was not shown");
                PreviewCapture=output/"map-building-confirmed.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::Map),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<5;++i)RenderFrame();
                MapPanel::Data.Operation=1;MapPanel::Data.Prefab={{"schema","rsdwtools.buildings.v1"},{"pieces",nlohmann::json::array({{{"piece_id",1},{"piece_data_name","/Preview/Buildings/Wall.Wall"},{"x",0},{"y",0},{"z",0}},{{"piece_id",2},{"piece_data_name","/Preview/Buildings/IronChest.IronChest"},{"x",100},{"y",0},{"z",0}}})}};
                MapPanel::Data.PrefabName="Sample house.json";MapPanel::Data.Yaw=90;for(int i=0;i<4;++i)RenderFrame();
                PreviewCapture=output/"map-prefab-placement.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement["Action"]=="ImportBuildings"&&mapPlacement["Document"]["pieces"].size()==2&&mapPlacement["Yaw"]==90&&mapPlacement["Generation"]==1,"Prefab selection lost its export, rotation or world generation");
                for(int i=0;i<4;++i)RenderFrame();check(MapPanel::Data.PlacementResult.value("Active",false),"Prefab receipt was not shown");
                MapPanel::Data.Prefab=nlohmann::json::object();MapPanel::Data.PrefabName.clear();
                mouse(center(Hooks::Map),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<5;++i)RenderFrame();
                MapPanel::Data.Operation=2;for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"map-teleport.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement["Action"]=="MapTeleport"&&mapPlacement["Player"]=="preview-player"&&mapPlacement["TargetLocation"]["X"]==MapPanel::Data.Point.X,"Map teleport lost the named player or selected point");
                const auto probesBeforeTravel=mapProbeCount;
                MapPanel::Data.NextGroundRead={};
                mouse(center(Hooks::MapLodestone),false,3);mouse(center(Hooks::MapLodestone),true);mouse(center(Hooks::MapLodestone),false,5);
                check(!MapPanel::Data.Popup&&MapPanel::Data.HasPoint,"LMB lodestone selection opened actions");
                Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<5;++i)RenderFrame();
                check(MapPanel::Data.Popup&&MapPanel::Data.Operation==2&&MapPanel::Data.NearMarker&&MapPanel::Data.Point.X==230000,"Lodestone click did not select its teleport destination");
                check(MapPanel::Data.Ground.value("GroundAvailable",false)&&MapPanel::Data.Ground.at("Location")[2]==1234,"Selected teleport point lost its independent native-height reply");
                check(mapProbeCount==probesBeforeTravel&&!MapPanel::Data.ProbeWanted&&!MapPanel::Data.ProbePending,"Repeat travel unnecessarily queued a placement-only ground preview");
                check(Hooks::MapSelected.z>Hooks::MapSelected.x,"Selected map point has no marker");
                PreviewCapture=output/"map-lodestone.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement.value("NearMarker",false),"Lodestone teleport did not request a nearby safe landing");
                mouse(ImVec2(-100,-100),false);Hooks::MouseEnabled=false;
                MapPanel::Data.SpawnPending=999;MapPanel::Data.TravelAction=true;MapPanel::Data.CancelSent=false;
                MapPanel::Data.ActionRequested=std::chrono::steady_clock::now()-std::chrono::seconds(5);previewTravelWaiting=true;
                snapshot["_RequestId"]=uint64_t(999);snapshot["_InProgress"]=true;
                snapshot["MapTravel"]={{"Phase","Preparing"},{"ElapsedSeconds",5},{"Committed",false}};
                snapshot["Status"]="Loading destination terrain...";
                for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"map-travel-progress.png";RenderFrame();PreviewCapture.clear();
                check(MapPanel::Data.SpawnPending==999&&MapPanel::Data.TravelAction,"Travel progress completed a pending map action");
                snapshot["MapTravel"]={{"Phase","Travelling"},{"ElapsedSeconds",5},{"Committed",true}};
                snapshot["Status"]="Native travel in progress; waiting for confirmed arrival...";
                PreviewCapture=output/"map-travel-arriving.png";RenderFrame();PreviewCapture.clear();
                detailReceipts[999]={{"Status","Arrival confirmed by preview fixture."}};previewTravelWaiting=false;
                RenderFrame();check(!MapPanel::Data.SpawnPending&&!MapPanel::Data.TravelAction,"Final arrival receipt did not clear travel progress");
                snapshot.erase("MapTravel");snapshot.erase("_RequestId");snapshot.erase("_InProgress");snapshot["Status"]="Sample data preview - game actions disabled.";
                mouse(center(Hooks::Map),false,3);MapPanel::Data.HoverStableAt=std::chrono::steady_clock::now()-std::chrono::seconds(1);MapPanel::Data.NextGroundRead={};
                for(int i=0;i<4;++i)RenderFrame();check(MapPanel::Data.HoverGround.value("GroundAvailable",false),"Hover did not receive its independent native ground-height response");
                PreviewCapture=output/"map-native-ground-z.png";RenderFrame();PreviewCapture.clear();
                mouse(ImVec2(-100,-100),false);Hooks::MouseEnabled=false;
                for(const auto section:{HelpyNavigation::SpawnSection::Npcs,HelpyNavigation::SpawnSection::Ai}){
                    CurrentPage=Page::Spawns;PreviewWantedSpawn=section;for(int i=0;i<4;++i)RenderFrame();check(CurrentSpawn==section,"Requested spawn section did not render");
                    PreviewCapture=output/(section==HelpyNavigation::SpawnSection::Npcs?"spawns-npcs.png":"spawns-ai.png");RenderFrame();PreviewCapture.clear();
                }
                // The crash used a cached index after an asynchronous empty overlay.
                {SpawnStudioPanel::State state;state.ShowHidden=true;
                    auto rows=nlohmann::json::array({{{"Key","CrashAI"},{"Type","AI"}},{{"Key","CrashNPC"},{"Type","NPC"}}});
                    SpawnStudioPanel::RefreshMatches(state,rows,500,1,false,true);check(state.Matches==std::vector<size_t>{1},"Spawn match fixture did not build");
                    SpawnStudioPanel::RefreshMatches(state,nlohmann::json::array(),500,2,false,true);check(state.Matches.empty(),"Empty catalog retained stale spawn indices");
                    std::swap(rows[0],rows[1]);SpawnStudioPanel::RefreshMatches(state,rows,500,3,false,true);check(state.Matches==std::vector<size_t>{0},"Same host revision missed reordered spawn rows");
                    rows[0]={{"Key","ReplacementAI"},{"Type","AI"}};SpawnStudioPanel::RefreshMatches(state,rows,500,4,false,true);check(state.Matches.empty(),"Equal-sized catalog replacement retained spawn membership");
                    rows=nlohmann::json::array({7});SpawnStudioPanel::RefreshMatches(state,rows,500,5,false,true);check(state.Matches.empty(),"Malformed spawn row was not excluded");
                }
                // Same hide/details behavior for resources, NPCs and enemies.
                for(const auto section:{HelpyNavigation::SpawnSection::Resources,HelpyNavigation::SpawnSection::Npcs,HelpyNavigation::SpawnSection::Ai}){
                    CurrentPage=Page::Spawns;PreviewWantedSpawn=section;for(int i=0;i<5;++i)RenderFrame();
                    auto& state=section==HelpyNavigation::SpawnSection::Resources?SpawnStudioPanel::Resources:section==HelpyNavigation::SpawnSection::Npcs?SpawnStudioPanel::Npcs:SpawnStudioPanel::Enemies;
                    const auto key=section==HelpyNavigation::SpawnSection::Resources?"Resource0":section==HelpyNavigation::SpawnSection::Npcs?"NPC0":"AI0";
                    const auto row=std::find_if(definitions.begin(),definitions.end(),[&](const auto& entry){return entry.value("Key",std::string{})==key;});
                    check(row!=definitions.end(),"Missing spawn browser fixture");SpawnBrowserSettings::Choices.Set(*row,false);++SpawnBrowserSettings::Revision;
                    state.ShowHidden=false;state.Selected=key;state.Rebuild=true;for(int i=0;i<3;++i)RenderFrame();
                    check(Hooks::SpawnDetails==key,"Selected spawn details were not rendered");
                    PreviewCapture=output/(std::string("spawn-details-")+key+".png");RenderFrame();PreviewCapture.clear();
                    SpawnStudioPanel::SetHidden(state,*row,true);for(int i=0;i<3;++i)RenderFrame();
                    check(!state.Selected.size()&&std::none_of(state.Matches.begin(),state.Matches.end(),[&](size_t at){return SpawnToolsPanel::UiSnapshot["Definitions"][at].value("Key",std::string{})==key;}),"Hidden spawn remained visible or selected");
                    state.ShowHidden=true;state.Rebuild=true;for(int i=0;i<3;++i)RenderFrame();
                    check(std::any_of(state.Matches.begin(),state.Matches.end(),[&](size_t at){return SpawnToolsPanel::UiSnapshot["Definitions"][at].value("Key",std::string{})==key;}),"Show hidden did not recover the spawn entry");
                    SpawnStudioPanel::SetHidden(state,*row,false);state.ShowHidden=false;state.Rebuild=true;for(int i=0;i<3;++i)RenderFrame();
                }
                for(const auto section:{HelpyNavigation::ToolSection::ItemCache,HelpyNavigation::ToolSection::Maintenance,HelpyNavigation::ToolSection::Cleanup}){
                    CurrentPage=Page::Tools;PreviewWantedTool=section;for(int i=0;i<4;++i)RenderFrame();check(CurrentTool==section,"Requested tools section did not render");
                    PreviewCapture=output/(section==HelpyNavigation::ToolSection::ItemCache?"tools-item-cache.png":section==HelpyNavigation::ToolSection::Maintenance?"tools-maintenance.png":"tools-cleanup.png");RenderFrame();PreviewCapture.clear();
                }
                // Exercise the second native window/swapchain with the shared
                // context. Input remains synthetic; no OS cursor movement.
                CurrentPage=Page::Creation;ItemGridPicker::DetachedBay=true;
                RenderFrame();check(m_bayWindow&&m_baySwapChain&&m_bayTarget,"Detached Item Bay failed to create native rendering resources");
                RECT mainDesktop{};GetWindowRect(m_hwnd,&mainDesktop);
                SetWindowPos(m_bayWindow,nullptr,mainDesktop.left-740,mainDesktop.top,700,680,SWP_NOZORDER|SWP_NOACTIVATE);
                HelpyIconPacks::Open=HelpyIconPacks::Detached=true;RenderFrame();
                check(m_cacheWindow&&m_cacheTarget&&m_bayTarget,"Item Bay and icon cache must render simultaneously");
                SetWindowPos(m_cacheWindow,nullptr,mainDesktop.right+20,mainDesktop.top,520,480,SWP_NOZORDER|SWP_NOACTIVATE);
                RenderFrame();CapturePreview(m_device,m_context,m_cacheSwapChain,output/"icon-cache-three-windows.png");
                const auto bayHandle=m_bayWindow,cacheHandle=m_cacheWindow;const auto selected=ItemGridPicker::Bay.Selected;
                const auto checkWindowControls=[&](HWND window){
                    constexpr LONG_PTR controls=WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_THICKFRAME;
                    check((GetWindowLongPtrW(window,GWL_STYLE)&controls)==controls,"Detached windows need minimize, maximize/restore and close controls");
                    SendMessageW(window,WM_SYSCOMMAND,SC_MAXIMIZE,0);RenderFrame();check(IsZoomed(window),"Detached window did not maximize");
                    SendMessageW(window,WM_SYSCOMMAND,SC_RESTORE,0);RenderFrame();check(!IsZoomed(window),"Detached window did not restore its size");
                };
                checkWindowControls(m_bayWindow);checkWindowControls(m_cacheWindow);
                SendMessageW(m_bayWindow,WM_SYSCOMMAND,SC_MINIMIZE,0);SendMessageW(m_cacheWindow,WM_SYSCOMMAND,SC_MINIMIZE,0);RenderFrame();
                check(HelpyTaskbar::Current.Count()==2&&!IsWindowVisible(m_bayWindow)&&!IsWindowVisible(m_cacheWindow),"Detached minimize must hide to Helpy taskbar");
                PreviewCapture=output/"helpy-taskbar.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::AuthoringButtons.at("Restore Item Bay")),false);mouse(center(Hooks::AuthoringButtons.at("Restore Item Bay")),true);mouse(center(Hooks::AuthoringButtons.at("Restore Item Bay")),false,3);
                mouse(center(Hooks::AuthoringButtons.at("Restore icon cache")),false);mouse(center(Hooks::AuthoringButtons.at("Restore icon cache")),true);mouse(center(Hooks::AuthoringButtons.at("Restore icon cache")),false,3);
                check(HelpyTaskbar::Current.Count()==0&&m_bayWindow==bayHandle&&m_cacheWindow==cacheHandle&&ItemGridPicker::Bay.Selected==selected,"Taskbar restore must preserve windows and item selection");
                const auto point=MapPanel::Data.Point;const auto zoom=MapPanel::Data.Zoom;DetachedWorkspaces={true,true};RenderFrame();
                check(m_workspaces[0].Window&&m_workspaces[1].Window&&m_workspaces[0].Target&&m_workspaces[1].Target,"Map and Recipes did not create independent native surfaces");
                checkWindowControls(m_workspaces[0].Window);checkWindowControls(m_workspaces[1].Window);
                SetWindowPos(m_workspaces[0].Window,nullptr,mainDesktop.left-1050,mainDesktop.top+50,1000,700,SWP_NOZORDER|SWP_NOACTIVATE);
                SetWindowPos(m_workspaces[1].Window,nullptr,mainDesktop.right+50,mainDesktop.top+80,900,700,SWP_NOZORDER|SWP_NOACTIVATE);RenderFrame();
                CapturePreview(m_device,m_context,m_workspaces[0].Swap,output/"map-detached.png");CapturePreview(m_device,m_context,m_workspaces[1].Swap,output/"recipes-detached.png");
                SendMessageW(m_workspaces[0].Window,WM_SYSCOMMAND,SC_MINIMIZE,0);SendMessageW(m_workspaces[1].Window,WM_SYSCOMMAND,SC_MINIMIZE,0);RenderFrame();
                check(HelpyTaskbar::Current.Count()==2&&!IsWindowVisible(m_workspaces[0].Window)&&!IsWindowVisible(m_workspaces[1].Window),"Map/Recipes minimize did not reach taskbar");
                mouse(center(Hooks::AuthoringButtons.at("Restore Map")),false);mouse(center(Hooks::AuthoringButtons.at("Restore Map")),true);mouse(center(Hooks::AuthoringButtons.at("Restore Map")),false,3);
                mouse(center(Hooks::AuthoringButtons.at("Restore Recipes")),false);mouse(center(Hooks::AuthoringButtons.at("Restore Recipes")),true);mouse(center(Hooks::AuthoringButtons.at("Restore Recipes")),false,3);
                check(HelpyTaskbar::Current.Count()==0&&MapPanel::Data.Point.X==point.X&&MapPanel::Data.Point.Y==point.Y&&MapPanel::Data.Zoom==zoom,"Detached Map lost its selection or zoom");
                SendMessageW(m_workspaces[0].Window,WM_CLOSE,0,0);SendMessageW(m_workspaces[1].Window,WM_CLOSE,0,0);RenderFrame();
                check(!m_workspaces[0].Window&&!m_workspaces[1].Window&&!DetachedWorkspaces[0]&&!DetachedWorkspaces[1]&&m_bayWindow,"Closing Map/Recipes must dock without closing other windows");
                SendMessageW(m_cacheWindow,WM_CLOSE,0,0);RenderFrame();
                check(!m_cacheWindow&&!HelpyIconPacks::Detached&&m_bayWindow,"Closing cache must dock it without touching the Item Bay");
                HelpyIconPacks::Open=false;RenderFrame();
                RuntimeDetailsPanel::Request(RuntimeDetailsPanel::Tab::Attributions);for(int i=0;i<4;++i)RenderFrame();
                mouse(center(Hooks::RuntimeDetach),false);mouse(center(Hooks::RuntimeDetach),true);mouse(center(Hooks::RuntimeDetach),false,4);
                check(RuntimeDetailsPanel::Detached&&m_workspaces[2].Window&&m_workspaces[2].Target,"Runtime details did not create a separate native surface");
                checkWindowControls(m_workspaces[2].Window);
                SetWindowPos(m_workspaces[2].Window,nullptr,mainDesktop.left-750,mainDesktop.top+40,680,640,SWP_NOZORDER|SWP_NOACTIVATE);for(int i=0;i<3;++i)RenderFrame();
                check(RuntimeDetailsPanel::Selected==RuntimeDetailsPanel::Tab::Attributions,"Detaching changed the runtime details tab");
                CapturePreview(m_device,m_context,m_workspaces[2].Swap,output/"runtime-details-detached.png");
                SendMessageW(m_workspaces[2].Window,WM_SYSCOMMAND,SC_MINIMIZE,0);RenderFrame();
                check(HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Runtime)&&!IsWindowVisible(m_workspaces[2].Window),"Runtime details did not minimize to the Helpy taskbar");
                mouse(center(Hooks::AuthoringButtons.at("Restore Runtime")),false);mouse(center(Hooks::AuthoringButtons.at("Restore Runtime")),true);mouse(center(Hooks::AuthoringButtons.at("Restore Runtime")),false,4);
                check(IsWindowVisible(m_workspaces[2].Window)&&!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Runtime),"Runtime details did not restore independently");
                mouse(center(Hooks::RuntimeDetach),false);mouse(center(Hooks::RuntimeDetach),true);mouse(center(Hooks::RuntimeDetach),false,4);
                check(!RuntimeDetailsPanel::Detached&&!m_workspaces[2].Window&&RuntimeDetailsPanel::Open&&RuntimeDetailsPanel::Selected==RuntimeDetailsPanel::Tab::Attributions,"Dock back lost the runtime details state");
                mouse(center(Hooks::RuntimeMinimize),false);mouse(center(Hooks::RuntimeMinimize),true);mouse(center(Hooks::RuntimeMinimize),false,3);
                check(HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Runtime)&&RuntimeDetailsPanel::Open&&!RuntimeDetailsPanel::Detached,"Docked Runtime details did not minimize to the Helpy taskbar");
                mouse(center(Hooks::AuthoringButtons.at("Restore Runtime")),false);mouse(center(Hooks::AuthoringButtons.at("Restore Runtime")),true);mouse(center(Hooks::AuthoringButtons.at("Restore Runtime")),false,4);
                check(!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Runtime)&&RuntimeDetailsPanel::Selected==RuntimeDetailsPanel::Tab::Attributions,"Restoring docked Runtime details lost its tab");
                mouse(center(Hooks::RuntimeClose),false);mouse(center(Hooks::RuntimeClose),true);mouse(center(Hooks::RuntimeClose),false,3);
                check(!RuntimeDetailsPanel::Open&&m_bayWindow,"Closing runtime details must preserve the Item Bay");
                RuntimeDetailsPanel::Request();RuntimeDetailsPanel::Detached=true;for(int i=0;i<3;++i)RenderFrame();
                CapturePreview(m_device,m_context,m_workspaces[2].Swap,output/"runtime-details-branded-detached.png");
                SendMessageW(m_workspaces[2].Window,WM_CLOSE,0,0);RenderFrame();
                check(!m_workspaces[2].Window&&!RuntimeDetailsPanel::Open&&!RuntimeDetailsPanel::Detached,"Native runtime close left a phantom details window");
                Hooks::ItemRects.clear();
                for(int i=0;i<4;++i)RenderFrame();
                mouse(center(Hooks::TransmogTab),false,2);mouse(center(Hooks::TransmogTab),true);mouse(center(Hooks::TransmogTab),false,4);
                check(ItemAuthoringPanel::Active==0,"Detached bay test did not select the transmog workspace");
                ItemAuthoringPanel::Drafts[0].SetSource(items[0]);for(int i=0;i<3;++i)RenderFrame();
                const auto bayPath=items[10].at("Path").get<std::string>();
                check(Hooks::ItemRects.contains(bayPath),"Detached Item Bay did not render the shared catalogue");
                drag(center(Hooks::ItemRects.at(bayPath)),center(Hooks::AuthoringSource));
                if(ItemAuthoringPanel::Drafts[0].Source.value("Path",std::string{})!=bayPath){
                    CapturePreview(m_device,m_context,m_baySwapChain,output/"item-bay-drag-failed.png");
                    PreviewCapture=output/"item-bay-main-drag-failed.png";RenderFrame();PreviewCapture.clear();
                    throw std::runtime_error("Cross-window Item Bay drag did not reach the main transmog source");
                }
                ItemDetails::Cache[bayPath].Native={{"Source",bayPath},{"Name",items[10].value("Name",bayPath)},{"Description","Selected item details remain available while detached."},
                    {"Fields",nlohmann::json::array({{{"NativeName","BaseDamage"},{"Value",12},{"HasValue",true}}})}};
                ItemGridPicker::PinDetails(items[10]);Hooks::BayDetailsFacts=0;for(int i=0;i<3;++i)RenderFrame();
                check(ItemGridPicker::Bay.DetailsExpanded&&Hooks::BayDetailsFacts>0,"Detached Item Bay lost the selected-item stats widget");
                CapturePreview(m_device,m_context,m_baySwapChain,output/"item-bay-detached.png");
                const auto selectedBeforeDock=std::string(ItemGridPicker::Bay.Selected);
                mouse(center(Hooks::BayDock),false);mouse(center(Hooks::BayDock),true);mouse(center(Hooks::BayDock),false,4);
                check(!ItemGridPicker::DetachedBay&&!m_bayWindow&&selectedBeforeDock==ItemGridPicker::Bay.Selected,"Dock back must close only the bay and preserve selection");
                check(ItemGridPicker::Bay.DetailsRow.value("Path",std::string{})==bayPath&&ItemDetails::Find(bayPath),"Docking lost selected native inspection details");
                ItemGridPicker::DetachedBay=true;RenderFrame();check(m_bayWindow!=nullptr,"Item Bay could not reopen");
                SendMessageW(m_bayWindow,WM_CLOSE,0,0);RenderFrame();check(!m_bayWindow&&!ItemGridPicker::DetachedBay,"Closing the separate bay must dock it without closing RSMK");
                const auto accentBefore=HelpyImGuiTheme::Current.accent;auto* letterTexture=BrandAccentTexture;
                const auto epochBefore=HelpyRuntimePreferences::IconCacheEpoch.load();
                HelpyImGuiTheme::Current.accent={0.25f,0.65f,1.0f,1.0f};PreviewCapture=output/"logo-accent-blue.png";RenderFrame();PreviewCapture.clear();
                check(BrandAccentTexture==letterTexture&&letterTexture&&HelpyRuntimePreferences::IconCacheEpoch.load()==epochBefore,"Logo recoloring must reuse its texture and not invalidate item icons");
                HelpyImGuiTheme::Current.accent=accentBefore;
                mouse(ImVec2(-100,-100),false);Hooks::MouseEnabled=false;
                // A failed read on one page must not poison another destination.
                const auto normalRead=SpawnToolRequests::RemoteReadQuick;
                SpawnToolRequests::RemoteReadQuick=[](uint64_t&,int,nlohmann::json&)->bool{throw std::runtime_error("Preview host read failure");};CurrentPage=Page::Items;
                RenderFrame();check(!PageErrors[0].empty(),"Malformed page data was not isolated");
                SpawnToolRequests::RemoteReadQuick=normalRead;
                CurrentPage=Page::Tools;PreviewWantedTool=HelpyNavigation::ToolSection::Maintenance;for(int i=0;i<3;++i)RenderFrame();
                check(PageErrors[static_cast<size_t>(Page::Tools)].empty(),"A failed page prevented navigation to tools");
                check(!PageErrors[0].empty(),"Page error state should persist until explicitly retried");PageErrors[0].clear();
            }
            if(width>=1200&&height>=850){
                namespace Hooks=HelpyPreviewHooks;const auto check=[](bool okay,const char* message){if(!okay)throw std::runtime_error(message);};ImGui::ClosePopupsOverWindow(nullptr,false);CurrentPage=Page::Map;MapPanel::Data.ShowParty=true;MapPanel::Data.PartyDetached=false;HelpyTaskbar::Current.Restore(HelpyTaskbar::Window::Party);auto original=snapshot["Players"];
                const auto equipmentChoice=PlayerPortraitPicker::Choices.Equipment;PlayerPortraitPicker::Choices.Equipment=true;
                const auto compactChoice=PlayerPortraitPicker::Choices.CompactParty;PlayerPortraitPicker::Choices.CompactParty=false;
                const auto powerChoice=PlayerPortraitPicker::Choices.Power;PlayerPortraitPicker::Choices.Power=true;
                // Drain an earlier map receipt before installing this phase's
                // synthetic observation; do not let it overwrite the fixture.
                MapPanel::Data.Next=MapPanel::Clock::now()+std::chrono::seconds(20);for(int i=0;i<3;++i)RenderFrame();
                auto gear=nlohmann::json::array();for(int i=0;i<8;++i)gear.push_back({{"Slot",i},{"Name","Equipped preview"},{"Icon",items[i%items.size()]["Icon"]},{"Durability",{{"Current",i==0?0:100-i*12},{"Maximum",100},{"ObservedUnixMs",PartyMemberView::Now()}}}});
                const auto originalMapPlayers=MapPanel::Data.Snapshot["Players"];
                MapPanel::Data.Snapshot["Players"][0]["Equipment"]={{"Items",gear},{"ObservedUnixMs",PartyMemberView::Now()}};
                MapPanel::Data.Snapshot["Players"][0]["Platform"]="Steam";
                for(const auto* key:{"Health","Stamina","SpecialPower","Hunger","Thirst","RestLevel"})MapPanel::Data.Snapshot["Players"][0]["Stats"][key]={{"Current",73},{"Maximum",100},{"ObservedUnixMs",PartyMemberView::Now()}};
                MapPanel::Data.Snapshot["Players"][0]["Stats"]["PowerLevel"]={{"Value",13},{"ObservedUnixMs",PartyMemberView::Now()}};
                // The synthetic host must publish the same fresh observations:
                // normal map polling/receipt merging may replace its local copy.
                snapshot["Players"]=MapPanel::Data.Snapshot["Players"];SpawnToolsPanel::UiSnapshot["Players"]=snapshot["Players"];
                MapPanel::Data.Next=MapPanel::Clock::now()+std::chrono::seconds(20);for(int i=0;i<8;++i)RenderFrame();PreviewCapture=output/"party-stats-left-gear-right.png";RenderFrame();PreviewCapture.clear();
                const auto gearKey=MapPanel::Data.Snapshot["Players"][0].value("Path",std::string{})+":0";
                const auto memberPath=MapPanel::Data.Snapshot["Players"][0].value("Path",std::string{});
                check(Hooks::PartyPower.contains(memberPath)&&Hooks::PartyPortraits.contains(memberPath),"Party power/portrait stack was not drawn");
                const auto powerRect=Hooks::PartyPower.at(memberPath),portraitRect=Hooks::PartyPortraits.at(memberPath);
                check(Hooks::PartyPlatforms.contains(memberPath)&&Hooks::PartyPlatforms.at(memberPath).y>portraitRect.w,"Player platform must sit beneath its own portrait");
                check(powerRect.w<portraitRect.y&&std::abs(powerRect.x-portraitRect.x)<1&&std::abs(powerRect.z-portraitRect.z)<1,"Power level must be centered above its profile icon without overlapping");
                check(Hooks::PartyDurabilityMeters==8&&Hooks::PartyGear.contains(gearKey),"Party equipped icons did not render native-style durability meters");
                const auto gearRect=Hooks::PartyGear.at(gearKey);check(gearRect.x>Hooks::MapPartyWindow.x+(Hooks::MapPartyWindow.z-Hooks::MapPartyWindow.x)*.3f,"Equipped icons must be on the right of party stats");
                Hooks::MouseEnabled=true;Hooks::MouseDown=false;Hooks::Mouse={(gearRect.x+gearRect.z)*.5f,(gearRect.y+gearRect.w)*.5f};for(int i=0;i<4;++i)RenderFrame();
                check(Hooks::PartyDurabilityDetails,"Equipped-item hover omitted exact durability details");PreviewCapture=output/"party-durability-hover.png";RenderFrame();PreviewCapture.clear();Hooks::MouseEnabled=false;
                PlayerPortraitPicker::Choices.CompactParty=true;Hooks::MouseEnabled=false;for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"party-compact.png";RenderFrame();PreviewCapture.clear();
                const auto compactGear=Hooks::PartyGear.at(gearKey),healthMeter=Hooks::PartyMeters.at(memberPath+":Health");
                check(compactGear.y>healthMeter.w&&healthMeter.z-healthMeter.x<HelpyImGuiTheme::Current.X(230),"Party placards must keep short critical meters above interactive equipment");
                check(!Hooks::PartyMeters.contains(memberPath+":Hunger")&&!Hooks::PartyMeters.contains(memberPath+":Thirst")&&!Hooks::PartyMeters.contains(memberPath+":RestLevel"),"Survival meters belong in party details, not the row");
                const bool oldHoverVitals=PlayerPortraitPicker::Choices.HoverVitals,oldTechnical=PlayerPortraitPicker::Choices.Technical;
                PlayerPortraitPicker::Choices.HoverVitals=true;PlayerPortraitPicker::Choices.Technical=true;
                Hooks::MouseEnabled=true;Hooks::Mouse={(healthMeter.x+healthMeter.z)*.5f,(healthMeter.y+healthMeter.w)*.5f};Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();
                check(Hooks::PartyDetailSections=="Hunger|Thirst|RestLevel|Skills","Member hover must show only survival gauges and skill levels, regardless of old advanced preferences");
                PreviewCapture=output/"party-survival-skills-hover.png";RenderFrame();PreviewCapture.clear();PlayerPortraitPicker::Choices.HoverVitals=oldHoverVitals;PlayerPortraitPicker::Choices.Technical=oldTechnical;
                Hooks::MouseEnabled=true;Hooks::Mouse={(compactGear.x+compactGear.z)*.5f,(compactGear.y+compactGear.w)*.5f};Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();
                check(Hooks::PartyDurabilityDetails,"Compact party equipment must retain its exact hover details");Hooks::MouseEnabled=false;
                PartyMemberView::Inspect(MapPanel::Data.Snapshot["Players"][0]);for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"party-equipment-inspector.png";RenderFrame();PreviewCapture.clear();PartyMemberView::InspectorOpen=false;
                MapPanel::Data.Snapshot["Players"]=originalMapPlayers;snapshot["Players"]=original;PlayerPortraitPicker::Choices.Equipment=equipmentChoice;PlayerPortraitPicker::Choices.Power=powerChoice;PlayerPortraitPicker::Choices.CompactParty=compactChoice;
                auto member=original[0];member["Guid"]="11223344556677889900aabbccddeeff";member["Name"]="Portrait preview";
                PlayerPortraitPicker::Request(member);if(PlayerPortraitPicker::Icons.size()<4)throw std::runtime_error("Original SVG portraits missing from shared gallery");
                for(int i=0;i<6;++i)RenderFrame();PreviewCapture=output/"portrait-themed.png";RenderFrame();PreviewCapture.clear();
                {
                    const auto image=HelpyPaths::Asset("home-banner.png");const auto before=std::filesystem::file_size(image);const auto stamp=std::filesystem::last_write_time(image);
                    POINT point{LONG((PlayerPortraitPicker::DropMin.x+PlayerPortraitPicker::DropMax.x)*.5f),LONG((PlayerPortraitPicker::DropMin.y+PlayerPortraitPicker::DropMax.y)*.5f)};ScreenToClient(m_hwnd,&point);
                    const auto path=image.wstring();const auto memory=GlobalAlloc(GHND,sizeof(DROPFILES)+(path.size()+2)*sizeof(wchar_t));check(memory!=nullptr,"Portrait drop allocation failed");auto* payload=static_cast<DROPFILES*>(GlobalLock(memory));payload->pFiles=sizeof(DROPFILES);payload->pt=point;payload->fWide=TRUE;std::memcpy(reinterpret_cast<char*>(payload)+sizeof(DROPFILES),path.c_str(),(path.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);
                    SendMessageW(m_hwnd,WM_DROPFILES,reinterpret_cast<WPARAM>(memory),0);
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(PlayerPortraitPicker::ImportWorker.valid()&&std::chrono::steady_clock::now()<deadline){RenderFrame();Sleep(1);}
                    check(PlayerPortraitPicker::DraftTexture!=nullptr,"Dropped portrait was not decoded/uploaded");PlayerPortraitPicker::Crop={.65f,.45f,2};
                    PreviewCapture=output/"portrait-image-crop.png";RenderFrame();PreviewCapture.clear();PlayerPortraitPicker::SaveCrop();
                    while(PlayerPortraitPicker::CropWorker.valid()&&std::chrono::steady_clock::now()<deadline){RenderFrame();Sleep(1);}
                    const auto guid=PlayerPortraits::Identity(PlayerPortraitPicker::PictureId(member));check(PlayerPortraitPicker::Choices.Get(guid)=="/HelpyPortrait/"+guid+"."+guid,"Portrait crop preference was not committed");
                    int w=0,h=0;std::vector<unsigned char> cropped;check(ItemIconCache::detail::ReadThumbnail(PlayerPortraitPicker::PortraitFile(guid),w,h,cropped)&&w==256&&h==256,"Saved portrait must be a readable bounded thumbnail");
                    check(std::filesystem::file_size(image)==before&&std::filesystem::last_write_time(image)==stamp,"Portrait import changed the original image");
                }
                ImGui::ClosePopupsOverWindow(nullptr,false);
                {
                    const auto savedPlayers=MapPanel::Data.Snapshot["Players"];auto& avatar=MapPanel::Data.Snapshot["Players"][0];avatar["Guid"]=member["Guid"];avatar["NativePlayerColorVerified"]=true;avatar["NativePlayerColor"]={.04,.3,.8,1};
                    const auto color=PartyMemberView::NativeColor(avatar);check(color.has_value(),"Native multiplayer color was rejected");const auto expectedTint=ImGui::GetColorU32(*color);const auto key=avatar.value("Path",std::string{});
                    MapPanel::Data.Next=MapPanel::Clock::now()+std::chrono::seconds(20);for(int i=0;i<10;++i)RenderFrame();
                    check(Hooks::MapPlayerTints.contains(key)&&Hooks::MapPlayerTints.at(key)==expectedTint,"Map portrait must use its own native multiplayer tint");
                    check(PlayerPortraitPicker::Pictures.contains(PlayerPortraits::Identity(PlayerPortraitPicker::PictureId(member))),"Saved circular portrait did not load for the map");
                    PreviewCapture=output/"portrait-circle-map-tint.png";RenderFrame();PreviewCapture.clear();MapPanel::Data.Snapshot["Players"]=savedPlayers;
                    auto unknown=member;unknown["NativePlayerColorVerified"]=false;unknown["NativePlayerColor"]={1,0,0,1};check(!PartyMemberView::NativeColor(unknown),"Unverified color must not tint a portrait");
                }
                PlayerInventoryPanel::Request(member);for(int i=0;i<8;++i)RenderFrame();
                if(!m_workspaces[4].Window||PlayerInventoryPanel::Snapshot.value("PlayerGuid",std::string{})!=member.value("Guid",std::string{}))throw std::runtime_error("Inventory did not detach or retain the selected character");
                check(PlayerInventoryPanel::Snapshot["Items"].size()==4,"Inventory workbench did not accumulate later native pages");
                check(std::any_of(PlayerInventoryPanel::Snapshot["Items"].begin(),PlayerInventoryPanel::Snapshot["Items"].end(),[](const auto& row){return row.value("Group",std::string{})=="Runes";}),"Native rune page was missing from the unified inventory snapshot");
                check(std::abs((Hooks::LiveInventoryViewport.w-Hooks::LiveInventoryViewport.y)-(Hooks::Browser.w-Hooks::Browser.y))<2,"Live inventory and Item Bay have different viewport heights");
                CapturePreview(m_device,m_context,m_workspaces[4].Swap,output/"inventory-detached.png");
                SendMessageW(m_workspaces[4].Window,WM_SYSCOMMAND,SC_MINIMIZE,0);RenderFrame();
                if(!HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::Inventory))throw std::runtime_error("Inventory minimize missed Helpy taskbar");
                HelpyTaskbar::Current.Restore(HelpyTaskbar::Window::Inventory);RenderFrame();SendMessageW(m_workspaces[4].Window,WM_CLOSE,0,0);RenderFrame();
                if(PlayerInventoryPanel::Open||m_workspaces[4].Window)throw std::runtime_error("Inventory close did not release only its own surface");
                {
                    auto& cache=LazyCatalog::For(0);const auto savedCache=cache;const auto savedSource=HelpyImGuiTheme::Current.itemSource,savedCategory=HelpyImGuiTheme::Current.itemCategory;
                    auto cape=items[0];cape["Path"]="/Preview/CachedCape.CachedCape";cape["Name"]="Cached native cape";cape["NativeSlotVerified"]=true;cape["SlotStrategy"]="Cape";
                    auto head=cape;head["Path"]="/Preview/WrongHead.WrongHead";head["SlotStrategy"]="Head";
                    cache.Rows=nlohmann::json::array({cape,head});cache.RuntimeGeneration=SpawnToolRequests::CurrentGeneration();
                    PlayerInventoryPanel::ChangeGear(member,{{"Slot",3},{"Location","Cape"}});
                    check(PlayerInventoryPanel::GearRows==nlohmann::json::array({cape}),"Gear picker must immediately seed only verified matching cached items");
                    HelpyImGuiTheme::Current.itemSource="vanilla";HelpyImGuiTheme::Current.itemCategory="Resources";Hooks::LiveInventoryViewport={};
                    for(int i=0;i<8;++i)RenderFrame();
                    check(m_workspaces[4].Window&&PlayerInventoryPanel::Bay.ActiveCategory=="All"&&PlayerInventoryPanel::Bay.SourceFilter==ItemSource::Kind::All,"Focused gear picker inherited unrelated global Item Bay filters");
                    check(Hooks::LiveInventoryViewport.z==0&&!PlayerInventoryPanel::Bay.Matches.empty(),"Change gear must show only a populated matching Item Bay, not an empty inventory pane");
                    CapturePreview(m_device,m_context,m_workspaces[4].Swap,output/"matching-gear-bay.png");
                    SendMessageW(m_workspaces[4].Window,WM_CLOSE,0,0);RenderFrame();cache=savedCache;HelpyImGuiTheme::Current.itemSource=savedSource;HelpyImGuiTheme::Current.itemCategory=savedCategory;
                }
                MapPanel::Data.ShowParty=true;MapPanel::Data.PartyDetached=true;for(int i=0;i<4;++i)RenderFrame();
                check(m_workspaces[3].Window!=nullptr,"Party did not create its independent native window");
                const auto partySize=Hooks::MapPartyWindow;for(int i=0;i<12;++i)RenderFrame();
                check(std::abs((partySize.z-partySize.x)-(Hooks::MapPartyWindow.z-Hooks::MapPartyWindow.x))<1&&std::abs((partySize.w-partySize.y)-(Hooks::MapPartyWindow.w-Hooks::MapPartyWindow.y))<1,"Detached party size drifted between frames");
                CapturePreview(m_device,m_context,m_workspaces[3].Swap,output/"party-detached-stable.png");
                const nlohmann::json chest={{"Kind","Chest"},{"Path","preview-chest"},{"Name","Iron Chest"},{"ObjectIndex",42},{"ObjectSerial",7},{"Location",{190000,70000,100}},{"Managed",false},{"Loaded",true}};
                ImGui::ClosePopupsOverWindow(nullptr,false);MapPanel::Data.Popup=false;MapPanel::Data.PartyPicking.clear();MapPanel::Data.MarkerSearch[0]=0;
                MapPanel::Data.Snapshot["Markers"].push_back(chest);MapPanel::Data.ShowChests=true;MapPanel::Data.ShowParty=false;MapPanel::Data.Next=MapPanel::Clock::now()+std::chrono::seconds(20);MapPanel::Data.FitRequested=true;for(int i=0;i<4;++i)RenderFrame();
                const ImVec2 chestAt{(Hooks::MapChest.x+Hooks::MapChest.z)*.5f,(Hooks::MapChest.y+Hooks::MapChest.w)*.5f};
                Hooks::MouseEnabled=true;Hooks::Mouse=chestAt;Hooks::MouseDown=false;RenderFrame();Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<8;++i)RenderFrame();Hooks::MouseEnabled=false;
                if(!PlayerInventoryPanel::Container||!m_workspaces[4].Window||PlayerInventoryPanel::Snapshot.value("Player",std::string{})!="preview-chest"||PlayerInventoryPanel::Snapshot.value("CanDeposit",true))throw std::runtime_error("Map chest click failed identity-bound read-only inspection: "+nlohmann::json{{"Target",PlayerInventoryPanel::Target},{"Snapshot",PlayerInventoryPanel::Snapshot},{"Status",PlayerInventoryPanel::Status},{"MapChest",{Hooks::MapChest.x,Hooks::MapChest.y,Hooks::MapChest.z,Hooks::MapChest.w}},{"MapError",MapPanel::Data.Error}}.dump());
                CapturePreview(m_device,m_context,m_workspaces[4].Swap,output/"map-chest-inspector.png");SendMessageW(m_workspaces[4].Window,WM_CLOSE,0,0);RenderFrame();
            }
            for(const auto& error:PageErrors)if(!error.empty())throw std::runtime_error(error);
            std::ofstream report(output/"render-result.txt");report<<"Rendered all seven production workspaces with synthetic metadata; no game actions called.\n";
            report<<"Cart delivery button stays inside the viewport with a staged item.\n";
            report<<"Steam/Game Pass/server/unknown runtime badges, honest version labels, Credits and MIT Legal notices rendered; runtime footer stays in the viewport.\n";
            if(width>=1200&&height>=850)report<<"Native metadata hover card, browser-to-cart drag and metadata, cart-to-browser return drag, right-click Add to cart, recipe input/output drops, cross-workspace cart drag/persistence, native-format map image/player markers, map coordinates, temporary resource, permanent enemy, native building/teleport-stone and named-player teleport requests, spawn/tool sections, and page error isolation passed.\n";
        }else{
            ShowWindow(m_hwnd,SW_SHOW);m_visible=true;MSG message{};bool done=false;
            while(!done){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT||message.message==WM_CLOSE){done=true;break;}TranslateMessage(&message);DispatchMessageW(&message);}if(!done)RenderFrame();}
        }
    }catch(const std::exception& error){std::filesystem::create_directories(output);std::ofstream(output/"render-error.txt")<<error.what();result=4;}
    RuneSchemaSettingsPanel::Remote={};ItemIconCache::ClearRemote();LazyCatalog::ClearRemote();SpawnToolRequests::ClearRemote();
    RuneSchemaProfile::Shutdown();JournalPanel::Shutdown();SpawnBrowserSettings::Shutdown();PlayerPortraitPicker::Shutdown();CharacterPreviewPanel::Shutdown();DestroyItemBay();DestroyIconCache();for(size_t i=0;i<m_workspaces.size();++i)DestroyWorkspacePanel(i);ImGui_ImplDX11_Shutdown();CharacterPreviewPanel::ReleaseTexture();HelpyWikiIcons::Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();ReleaseBrandTextures();DestroyDevice();
    DestroyWindow(m_hwnd);m_hwnd=nullptr;UnregisterClassW(wc.lpszClassName,m_module);if(SUCCEEDED(com))CoUninitialize();return result;
}
#endif

void HelpyStandaloneWindow::RenderFrame() {
#if !defined(HELPY_PREVIEW)
    if(!LazyCatalog::LocalSubmit)HelpyCachedCatalog::Configure();
#endif
    HelpyDownloads::Initialize();
    HelpyMapCache::Initialize();HelpyMapCache::Poll();HelpyWikiIcons::Initialize();
    if(PendingWikiInitialAssets&&!HelpyMapCache::Busy()&&HelpyMapCache::Current().Data&&!HelpyWikiIcons::Busy()){
        PendingWikiInitialAssets=false;HelpyWikiIcons::Start(MapPanel::WikiNames(),HelpyMapCache::Current().CachedBuild);
    }
    if(HelpyDownloads::Poll()){ReleaseBrandTextures(false);LoadBrandTextures(m_device);}
    const bool packWasBusy=HelpyIconPacks::Busy();HelpyIconPacks::Poll();if(packWasBusy&&!HelpyIconPacks::Busy()&&HelpyIconPacks::CompletionNotify)PushToast(HelpyIconPacks::Status,5.0);
    static uint64_t importedReferences=0;
    if(importedReferences!=HelpyIconPacks::ReferencesRevision){
        importedReferences=HelpyIconPacks::ReferencesRevision;
        NativeJournalCache::NextBundle={};NativeJournalCache::BundleStamp.clear();
        auto& items=LazyCatalog::For(0);items.Cache.clear();if(items.Started)LazyCatalog::Retry(0);
    }
    static uint64_t uiGeneration=0;
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(generation!=uiGeneration){LazyCatalog::ResetForGeneration(generation);uiGeneration=generation;}
#if !defined(HELPY_PREVIEW)
    if(HelpySourceCache::Poll(HostServices::ModDirectory())){
        ItemIconCache::InvalidateSource(true);
        if(HelpySourceCache::CatalogInvalidationRevision==HelpySourceCache::Revision){
            SpawnToolsPanel::InvalidateCartChecks();
            LazyCatalog::InvalidateSources(HelpySourceCache::RemovalRevision==HelpySourceCache::Revision);
            ItemDetails::Cache.clear();ItemDetails::Pending=0;ItemGridPicker::DetailsPopup.Open=false;
            // Keep the presentation snapshot until the bounded refresh commits.
        }else{
            HelpyCacheSources::Plan sources;sources.DeclaredOwners=HelpySourceCache::Current.DeclaredOwners;
            LazyCatalog::RefreshSourceMetadata(sources,HelpySourceCache::Current.NativePlugins);
        }
    }
#endif
    LazyCatalog::PumpBackendSignal();LazyCatalog::CheckTimeouts();
#if !defined(HELPY_PREVIEW)
    // Preview rows are useful before live source inspection completes.
    // Revalidate in place; do not blank the Item Bay during this handoff.
    static auto nextPreviewCheck=std::chrono::steady_clock::time_point{};
    auto& previewCatalog=LazyCatalog::For(0);const auto previewNow=std::chrono::steady_clock::now();
    if(HelpyRuntimePreferences::PassiveRefresh.load()&&previewCatalog.CachePreview&&!previewCatalog.Pending&&!previewCatalog.Refreshing&&previewNow>=nextPreviewCheck){
        nextPreviewCheck=previewNow+std::chrono::seconds(15);previewCatalog.Cache.clear();LazyCatalog::Retry(0);
    }
#endif
    RuntimeConnectionPanel::Poll();
    if(FontRescanRequested.exchange(false)) {
        auto& io=ImGui::GetIO();ImGui_ImplDX11_InvalidateDeviceObjects();io.Fonts->Clear();LoadUiFontFamilies(io);SelectUiFontFamily(HelpyImGuiTheme::Current.fontFamily);io.FontDefault=UiRegularFont;
    }
    HelpyImGuiTheme::Poll();
    HelpyBackgrounds::Pump();
    SourceBanners::Pump();
#if !defined(HELPY_PREVIEW)
    HelpyIconPacks::PollPackaged();
    if(HelpyUpdates::Tick(HelpyImGuiTheme::Current.automaticUpdateChecks,HelpyEnvironment::UpdateChecksAvailable,HelpyEnvironment::RuneSchemaVersion))
        PushToast("RuneSchema update available. See Settings > Updates & runtime.",6.0);
#endif
    ApplyCaptionTheme(m_hwnd,HelpyImGuiTheme::Current);
    PlayerPortraitPicker::Pump();SpawnBrowserSettings::Pump();
    PlayerInventoryPanel::BindRecipient=[] {
        using namespace PlayerInventoryPanel;
        const auto path=ItemPresentation::Text(Target,"Path");const auto guid=ItemPresentation::Text(Target,"Guid");
        const auto* current=SpawnToolsPanel::CurrentCartPlayer(SpawnToolsPanel::UiSnapshot,path);
        if(Container||Personal||ItemGridPicker::SharedCart.Delivery()||Generation!=SpawnToolRequests::CurrentGeneration()||guid.empty()||!current||ItemPresentation::Text(*current,"Guid")!=guid)return false;
        const auto targets=SpawnToolsPanel::EffectiveCartRecipients();
        if(targets.size()!=1||targets[0].Path!=path){SpawnToolsPanel::SelectedPlayer=path;SpawnToolsPanel::CartRecipients={{path,ItemPresentation::Text(*current,"Name")}};SpawnToolsPanel::CartRecipientsChosen=true;SpawnToolsPanel::InvalidateCartChecks();}
        return true;
    };
    PlayerInventoryPanel::RenderCart=[](float height){SpawnToolsPanel::RenderCartDock({0,height},[]{ImGui::TextWrapped("%s · player inventory",PlayerInventoryPanel::Target.value("Name",std::string("Player")).c_str());});};
    const bool bayVisible=SyncItemBay(),cacheVisible=SyncIconCache();const std::array workspaceVisible{SyncWorkspacePanel(0),SyncWorkspacePanel(1),SyncWorkspacePanel(2),SyncWorkspacePanel(3),SyncWorkspacePanel(4)};
    ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();auto& frameIo=ImGui::GetIO();const auto& t=HelpyImGuiTheme::Current;SelectUiFontFamily(t.fontFamily);frameIo.FontDefault=UiRegularFont;frameIo.FontGlobalScale=std::clamp(t.Scale()*t.fontScale,0.50f,4.00f);
    MainSurfacePosition={0,0};MainSurfaceSize=frameIo.DisplaySize;DesktopBayLayout::Canvas bayCanvas;
    if(bayVisible||cacheVisible||std::any_of(workspaceVisible.begin(),workspaceVisible.end(),[](bool visible){return visible;})){
        const auto surface=[](HWND window){RECT r{};POINT p{};GetClientRect(window,&r);ClientToScreen(window,&p);return DesktopBayLayout::Rect{float(p.x),float(p.y),float(r.right),float(r.bottom)};};
        const auto main=surface(m_hwnd);
        bayCanvas=DesktopBayLayout::Arrange(main,bayVisible?surface(m_bayWindow):main,cacheVisible?surface(m_cacheWindow):main,workspaceVisible[0]?surface(m_workspaces[0].Window):main,workspaceVisible[1]?surface(m_workspaces[1].Window):main,workspaceVisible[2]?surface(m_workspaces[2].Window):main,workspaceVisible[3]?surface(m_workspaces[3].Window):main,workspaceVisible[4]?surface(m_workspaces[4].Window):main);
        MainSurfacePosition={bayCanvas.Main.X,bayCanvas.Main.Y};MainSurfaceSize={bayCanvas.Main.Width,bayCanvas.Main.Height};frameIo.DisplaySize={bayCanvas.Width,bayCanvas.Height};
        POINT cursor{};if(GetCursorPos(&cursor))frameIo.AddMousePosEvent(float(cursor.x)-bayCanvas.X,float(cursor.y)-bayCanvas.Y);
    }
#if defined(HELPY_PREVIEW)
    if(HelpyPreviewHooks::MouseEnabled){frameIo.ConfigInputTrickleEventQueue=false;frameIo.AddFocusEvent(true);frameIo.AddMousePosEvent(HelpyPreviewHooks::Mouse.x,HelpyPreviewHooks::Mouse.y);frameIo.AddMouseButtonEvent(0,HelpyPreviewHooks::MouseDown);frameIo.AddMouseButtonEvent(1,HelpyPreviewHooks::RightDown);frameIo.AddKeyEvent(ImGuiMod_Ctrl,HelpyPreviewHooks::ControlDown);}
#endif
    ImGui::NewFrame();HelpyWikiIcons::Poll();
    if(!RuneSchemaProfile::Initialized)RuneSchemaProfile::Load(HostServices::StateDirectory());
    if(!HelpyPersonalization::Initialized)HelpyPersonalization::Load(RuneSchemaProfile::Ready?RuneSchemaProfile::Data("labels.json"):HostServices::StateDirectory()/"personalization.json");
    if(!HelpyNavigationOrder::Initialized)HelpyNavigationOrder::Load(HostServices::StateDirectory()/"navigation-order.json");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BackgroundDraws=0;HelpyPreviewHooks::RuntimeBadgeDraws=0;
#endif
    const auto framePosition=MainSurfacePosition,frameSize=MainSurfaceSize;
    MainSurfacePosition.y+=NativeCaption.Height;MainSurfaceSize.y=std::max(1.f,MainSurfaceSize.y-NativeCaption.Height);
    ItemGridPicker::DetailsSurfaceMin=MainSurfacePosition;ItemGridPicker::DetailsSurfaceSize=MainSurfaceSize;
    LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());
    HelpyWindowChrome::Bounds(MainSurfacePosition,MainSurfaceSize);
    ImGui::SetNextWindowPos(framePosition);ImGui::SetNextWindowSize(frameSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);
    if(ImGui::Begin("##RuneSchemaHelpyStandalone",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        HelpyBackgrounds::DrawWindow();
        if(NativeCaption.Height){
            const float icon=float(NativeCaption.Icon),inset=8.f*NativeCaption.Dpi/96.f;
            const ImVec2 at{framePosition.x+inset,framePosition.y+(NativeCaption.Height-icon)*.5f};
            DrawThemedLogo(ImGui::GetWindowDrawList(),at,{at.x+icon,at.y+icon},t);
            HelpyWindowChrome::RenderNativeCaption(NativeCaption,framePosition,t);
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+float(NativeCaption.Height));
        HelpyImGuiTheme::Scope skin(t);ImFont* workspaceFont=(t.textBold&&UiBoldFont)?UiBoldFont:UiRegularFont;
        if(workspaceFont)ImGui::PushFont(workspaceFont);
        const bool taskbar=HelpyTaskbar::Current.Count()!=0;
        if(taskbar)ImGui::BeginChild("##helpy-workspace-above-taskbar",{0,std::max(1.0f,ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing()*(HelpyTaskbar::Current.Count()>2?2:1)-t.Y(12))},false);
        RenderWorkspace(t);
        if(taskbar){
            ImGui::EndChild();ImGui::Separator();
            const float logoSize=ImGui::GetFrameHeight();const auto logo=ImGui::GetCursorScreenPos();
            ImGui::Dummy({logoSize,logoSize});DrawThemedLogo(ImGui::GetWindowDrawList(),logo,{logo.x+logoSize,logo.y+logoSize},t);
            if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip("RuneSchema · minimized windows");
            const auto restore=[&](const char* label,HelpyTaskbar::Window task,HWND window){
                if(HelpyTaskbar::Current.Contains(task)){if(ImGui::GetContentRegionAvail().x>ImGui::CalcTextSize(label).x+t.X(24))ImGui::SameLine();if(HelpyImGuiTheme::SoftButton(label)){
                    HelpyTaskbar::Current.Restore(task);if(window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);SetFocus(window);}
                }
#if defined(HELPY_PREVIEW)
                    HelpyPreviewHooks::AuthoringButtons[label]=HelpyPreviewHooks::LastRect();
#endif
                }
            };
            restore("Restore Item Bay",HelpyTaskbar::Window::ItemBay,m_bayWindow);
            restore("Restore icon cache",HelpyTaskbar::Window::IconCache,m_cacheWindow);
            restore("Restore Map",HelpyTaskbar::Window::Map,m_workspaces[0].Window);
            restore("Restore Recipes",HelpyTaskbar::Window::Recipes,m_workspaces[1].Window);
            restore("Restore Runtime",HelpyTaskbar::Window::Runtime,m_workspaces[2].Window);
            restore("Restore Party",HelpyTaskbar::Window::Party,m_workspaces[3].Window);
            restore("Restore inventory",HelpyTaskbar::Window::Inventory,m_workspaces[4].Window);
        }
        if(workspaceFont)ImGui::PopFont();
    }
    ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor();if(UiRegularFont)ImGui::PushFont(UiRegularFont);RenderToasts(t);RenderDiagnosticToasts(t);if(UiRegularFont)ImGui::PopFont();
    RuntimeDetailsPanel::RenderDocked(t,MainSurfacePosition,MainSurfaceSize);
    ItemPreviewPanel::Render();
    PartyMemberView::RenderInspector();
    PersonalizationPanel::Render();
    if(bayVisible){
        ItemGridPicker::DetailsSurfaceMin={bayCanvas.Bay.X,bayCanvas.Bay.Y};ItemGridPicker::DetailsSurfaceSize={bayCanvas.Bay.Width,bayCanvas.Bay.Height};
        ImGui::SetNextWindowPos({bayCanvas.Bay.X,bayCanvas.Bay.Y});ImGui::SetNextWindowSize({bayCanvas.Bay.Width,bayCanvas.Bay.Height});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
        if(ImGui::Begin("##RsmkDetachedItemBay",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings)){
            HelpyBackgrounds::DrawWindow();
            HelpyImGuiTheme::Scope skin(t);if(UiRegularFont)ImGui::PushFont(UiRegularFont);
            ImGui::TextUnformatted("Item Bay");ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Dock back"))ItemGridPicker::DetachedBay=false;
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::BayDock=HelpyPreviewHooks::LastRect();
#endif
            ImGui::Separator();ItemGridPicker::RenderingDetachedBay=true;
            try{ItemGridPicker::RenderShared(SpawnToolsPanel::UiSnapshot,SpawnToolsPanel::UiRevision,true);}catch(...){ItemGridPicker::RenderingDetachedBay=false;throw;}
            ItemGridPicker::RenderingDetachedBay=false;if(UiRegularFont)ImGui::PopFont();
        }ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
    }
    if(HelpyIconPacks::Open&&!HelpyIconPacks::Detached)ImGui::SetNextWindowPos({MainSurfacePosition.x+MainSurfaceSize.x*.5f,MainSurfacePosition.y+MainSurfaceSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    HelpyIconPacks::RenderDockedWindow(SpawnToolsPanel::UiSnapshot.value("_CatalogIndexing",false));
    if(cacheVisible){
        ImGui::SetNextWindowPos({bayCanvas.Cache.X,bayCanvas.Cache.Y});ImGui::SetNextWindowSize({bayCanvas.Cache.Width,bayCanvas.Cache.Height});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
        if(ImGui::Begin("##RsmkDetachedIconCache",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings)){
            HelpyBackgrounds::DrawWindow();
            HelpyImGuiTheme::Scope skin(t);if(UiRegularFont)ImGui::PushFont(UiRegularFont);
            HelpyIconPacks::RenderWindowContents(SpawnToolsPanel::UiSnapshot.value("_CatalogIndexing",false));if(UiRegularFont)ImGui::PopFont();
        }ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
    }
    for(size_t i=0;i<m_workspaces.size();++i)if(workspaceVisible[i]){
        const auto rect=WorkspaceRect(i,bayCanvas);
        if(i==3){HelpyImGuiTheme::Scope skin(t);MapPanel::Party({rect.X,rect.Y},{rect.X+rect.Width,rect.Y+rect.Height},true);if(!MapPanel::Data.PartyDetached)CurrentPage=Page::Map;continue;}
        ItemGridPicker::DetailsSurfaceMin={rect.X,rect.Y};ItemGridPicker::DetailsSurfaceSize={rect.Width,rect.Height};
        ImGui::SetNextWindowPos({rect.X,rect.Y});ImGui::SetNextWindowSize({rect.Width,rect.Height});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
        if(ImGui::Begin(WorkspaceImGuiId(i),nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|(i==0?ImGuiWindowFlags_NoBringToFrontOnFocus:ImGuiWindowFlags_None))){
            HelpyBackgrounds::DrawWindow();
            HelpyImGuiTheme::Scope skin(t);
            if(i==4)PlayerInventoryPanel::Contents();else if(i==2)RuntimeDetailsPanel::RenderContents();else{
            ImGui::TextUnformatted(WorkspaceTitle(i));ImGui::SameLine();
            if(HelpyImGuiTheme::SoftButton("Dock back"))WorkspaceDetached(i)=false;
            ImGui::Separator();ImGui::BeginChild("##detached-workspace-body",{0,0});
            try{if(i==0)MapPanel::Render();else SpawnToolsPanel::Render(SpawnToolsPanel::View::Recipes);}catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}
            ImGui::EndChild();
            }
        }ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
    }
    {HelpyImGuiTheme::Scope skin(t);if(UiRegularFont)ImGui::PushFont(UiRegularFont);
    PlayerPortraitPicker::Render();PlayerInventoryPanel::RenderDocked();ItemGridPicker::RenderDetailsPopup();
    if(PlayerInventoryPanel::AddWanted){PlayerInventoryPanel::AddWanted=false;
        const auto path=ItemPresentation::Text(PlayerInventoryPanel::Target,"Path");const auto* target=SpawnToolsPanel::CurrentCartPlayer(SpawnToolsPanel::UiSnapshot,path);
        if(!ItemGridPicker::SharedCart.Delivery()&&PlayerInventoryPanel::Generation==SpawnToolRequests::CurrentGeneration()&&target&&ItemPresentation::Text(*target,"Guid")==ItemPresentation::Text(PlayerInventoryPanel::Target,"Guid")){
            SpawnToolsPanel::SelectedPlayer=path;SpawnToolsPanel::CartRecipients={{path,ItemPresentation::Text(*target,"Name")}};SpawnToolsPanel::CartRecipientsChosen=true;CurrentPage=Page::Items;PushToast("Item Service recipient selected. Review your cart before delivery.",4);
        }else PushToast("Cannot change recipient during delivery or after a player/world change. Refresh and retry.",4);
    }
    if(UiRegularFont)ImGui::PopFont();}
    // Submit once, after every visible panel has declared this frame's demand.
    ItemIconCache::Update();
    ImGui::Render();const float clear[4]{t.window.x,t.window.y,t.window.z,t.window.w};m_context->OMSetRenderTargets(1,&m_target,nullptr);m_context->ClearRenderTargetView(m_target,clear);
    auto mainDraw=*ImGui::GetDrawData();mainDraw.DisplayPos=framePosition;mainDraw.DisplaySize=frameSize;
    HelpySurfaceDraw::Filter(mainDraw);
    ImGui_ImplDX11_RenderDrawData(&mainDraw);
    if(bayVisible){
        m_context->OMSetRenderTargets(1,&m_bayTarget,nullptr);m_context->ClearRenderTargetView(m_bayTarget,clear);
        auto bayDraw=*ImGui::GetDrawData();bayDraw.DisplayPos={bayCanvas.Bay.X,bayCanvas.Bay.Y};bayDraw.DisplaySize={bayCanvas.Bay.Width,bayCanvas.Bay.Height};
        HelpySurfaceDraw::Filter(bayDraw);
        ImGui_ImplDX11_RenderDrawData(&bayDraw);m_baySwapChain->Present(0,0);
    }
    if(cacheVisible){
        m_context->OMSetRenderTargets(1,&m_cacheTarget,nullptr);m_context->ClearRenderTargetView(m_cacheTarget,clear);
        auto cacheDraw=*ImGui::GetDrawData();cacheDraw.DisplayPos={bayCanvas.Cache.X,bayCanvas.Cache.Y};cacheDraw.DisplaySize={bayCanvas.Cache.Width,bayCanvas.Cache.Height};
        HelpySurfaceDraw::Filter(cacheDraw);
        ImGui_ImplDX11_RenderDrawData(&cacheDraw);m_cacheSwapChain->Present(0,0);
    }
    for(size_t i=0;i<m_workspaces.size();++i)if(workspaceVisible[i]){
        auto& panel=m_workspaces[i];const auto rect=WorkspaceRect(i,bayCanvas);
        m_context->OMSetRenderTargets(1,&panel.Target,nullptr);m_context->ClearRenderTargetView(panel.Target,clear);
        auto draw=*ImGui::GetDrawData();draw.DisplayPos={rect.X,rect.Y};draw.DisplaySize={rect.Width,rect.Height};
        HelpySurfaceDraw::Filter(draw);
        ImGui_ImplDX11_RenderDrawData(&draw);panel.Swap->Present(0,0);
    }
#if defined(HELPY_PREVIEW)
    if(!PreviewCapture.empty())CapturePreview(m_device,m_context,m_swapChain,PreviewCapture);
#endif
    m_swapChain->Present(1,0);
}

bool HelpyStandaloneWindow::SyncItemBay(){
    if(!ItemGridPicker::DetachedBay){if(m_bayWindow)DestroyItemBay();return false;}
    if(HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::ItemBay)){if(m_bayWindow)ShowWindow(m_bayWindow,SW_HIDE);return false;}
    if(!m_bayWindow){
        RECT parent{};GetWindowRect(m_hwnd,&parent);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(m_hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        const int width=std::min(720,std::max(480,int(monitor.rcWork.right-monitor.rcWork.left))),height=std::min(700,std::max(360,int(monitor.rcWork.bottom-monitor.rcWork.top)));
        const int x=std::clamp(int((parent.left+parent.right-width)/2),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
        const int y=std::clamp(int((parent.top+parent.bottom-height)/2),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
        wchar_t windowClass[256]{};GetClassNameW(m_hwnd,windowClass,256);
        m_bayWindow=CreateWindowExW(WS_EX_TOOLWINDOW,windowClass,L"RSMK - Item Bay",WS_OVERLAPPEDWINDOW,x,y,width,height,m_hwnd,nullptr,m_module,this);
        if(m_bayWindow)DragAcceptFiles(m_bayWindow,TRUE);
        using Microsoft::WRL::ComPtr;ComPtr<IDXGIDevice> device;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
        DXGI_SWAP_CHAIN_DESC description{};description.BufferCount=2;description.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;description.OutputWindow=m_bayWindow;description.SampleDesc.Count=1;description.Windowed=TRUE;description.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        bool ready=m_bayWindow&&SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&device)))&&SUCCEEDED(device->GetAdapter(&adapter))
            &&SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateSwapChain(m_device,&description,&m_baySwapChain));
        if(ready){ComPtr<ID3D11Texture2D> buffer;ready=SUCCEEDED(m_baySwapChain->GetBuffer(0,IID_PPV_ARGS(&buffer)))&&SUCCEEDED(m_device->CreateRenderTargetView(buffer.Get(),nullptr,&m_bayTarget));}
        if(!ready){DestroyItemBay();ItemGridPicker::DetachedBay=false;PushToast("Could not open the separate Item Bay; it remains docked.",4);return false;}
        SendMessageW(m_bayWindow,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED)));
        SendMessageW(m_bayWindow,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED)));
        m_publishedBayWindow=m_bayWindow;
        ApplyCaptionTheme(m_bayWindow,HelpyImGuiTheme::Current,true);ShowWindow(m_bayWindow,SW_SHOWNOACTIVATE);
    }
    ApplyCaptionTheme(m_bayWindow,HelpyImGuiTheme::Current);
    return IsWindowVisible(m_bayWindow)&&!IsIconic(m_bayWindow)&&m_bayTarget;
}
void HelpyStandaloneWindow::DestroyItemBay(){
    HelpyTaskbar::Current.Forget(HelpyTaskbar::Window::ItemBay);
    m_publishedBayWindow=nullptr;
    if(m_context)m_context->OMSetRenderTargets(0,nullptr,nullptr);
    if(m_bayWindow&&GetCapture()==m_bayWindow)ReleaseCapture();
    if(m_bayTarget){m_bayTarget->Release();m_bayTarget=nullptr;}
    if(m_baySwapChain){m_baySwapChain->Release();m_baySwapChain=nullptr;}
    if(m_bayWindow){DestroyWindow(m_bayWindow);m_bayWindow=nullptr;}
    ItemGridPicker::RenderingDetachedBay=false;
}

bool HelpyStandaloneWindow::SyncIconCache(){
    if(!HelpyIconPacks::Open||!HelpyIconPacks::Detached){if(m_cacheWindow)DestroyIconCache();return false;}
    if(HelpyTaskbar::Current.Contains(HelpyTaskbar::Window::IconCache)){if(m_cacheWindow)ShowWindow(m_cacheWindow,SW_HIDE);return false;}
    if(!m_cacheWindow){
        RECT parent{};GetWindowRect(m_hwnd,&parent);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(m_hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        const int width=std::min(640,int(monitor.rcWork.right-monitor.rcWork.left)),height=std::min(520,int(monitor.rcWork.bottom-monitor.rcWork.top));
        const int x=std::clamp(int((parent.left+parent.right-width)/2),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
        const int y=std::clamp(int((parent.top+parent.bottom-height)/2),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
        wchar_t windowClass[256]{};GetClassNameW(m_hwnd,windowClass,256);
        m_cacheWindow=CreateWindowExW(WS_EX_TOOLWINDOW,windowClass,L"RSMK - Icon cache",WS_OVERLAPPEDWINDOW,x,y,width,height,m_hwnd,nullptr,m_module,this);
        if(m_cacheWindow)DragAcceptFiles(m_cacheWindow,TRUE);
        using Microsoft::WRL::ComPtr;ComPtr<IDXGIDevice> device;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
        DXGI_SWAP_CHAIN_DESC description{};description.BufferCount=2;description.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;description.OutputWindow=m_cacheWindow;description.SampleDesc.Count=1;description.Windowed=TRUE;description.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        bool ready=m_cacheWindow&&SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&device)))&&SUCCEEDED(device->GetAdapter(&adapter))
            &&SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateSwapChain(m_device,&description,&m_cacheSwapChain));
        if(ready){ComPtr<ID3D11Texture2D> buffer;ready=SUCCEEDED(m_cacheSwapChain->GetBuffer(0,IID_PPV_ARGS(&buffer)))&&SUCCEEDED(m_device->CreateRenderTargetView(buffer.Get(),nullptr,&m_cacheTarget));}
        if(!ready){DestroyIconCache();HelpyIconPacks::Detached=false;PushToast("Could not detach the cache window; it remains in Helpy.",4);return false;}
        SendMessageW(m_cacheWindow,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED)));
        SendMessageW(m_cacheWindow,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED)));
        ApplyCaptionTheme(m_cacheWindow,HelpyImGuiTheme::Current,true);
        m_publishedCacheWindow=m_cacheWindow;ShowWindow(m_cacheWindow,SW_SHOWNOACTIVATE);
    }
    ApplyCaptionTheme(m_cacheWindow,HelpyImGuiTheme::Current);
    return IsWindowVisible(m_cacheWindow)&&!IsIconic(m_cacheWindow)&&m_cacheTarget;
}
void HelpyStandaloneWindow::DestroyIconCache(){
    HelpyTaskbar::Current.Forget(HelpyTaskbar::Window::IconCache);
    m_publishedCacheWindow=nullptr;if(m_context)m_context->OMSetRenderTargets(0,nullptr,nullptr);
    if(m_cacheWindow&&GetCapture()==m_cacheWindow)ReleaseCapture();
    if(m_cacheTarget){m_cacheTarget->Release();m_cacheTarget=nullptr;}
    if(m_cacheSwapChain){m_cacheSwapChain->Release();m_cacheSwapChain=nullptr;}
    if(m_cacheWindow){DestroyWindow(m_cacheWindow);m_cacheWindow=nullptr;}
}

#include "DetachedWorkspaces.inl"
bool HelpyStandaloneWindow::CreateDevice() {
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=m_hwnd;
    sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;UINT flags=0;D3D_FEATURE_LEVEL level{};
    constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    auto hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,2,D3D11_SDK_VERSION,&sd,&m_swapChain,&m_device,&level,&m_context);
    if(FAILED(hr)&&HelpyEnvironment::Server()){
        DestroyDevice();
        hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,levels,2,D3D11_SDK_VERSION,&sd,&m_swapChain,&m_device,&level,&m_context);
    }
    if(FAILED(hr))return false;CreateTarget();return m_target!=nullptr;
}
void HelpyStandaloneWindow::DestroyDevice(){DestroyTarget();if(m_swapChain){m_swapChain->Release();m_swapChain=nullptr;}if(m_context){m_context->Release();m_context=nullptr;}if(m_device){m_device->Release();m_device=nullptr;}}
void HelpyStandaloneWindow::CreateTarget(){if(!m_swapChain)return;ID3D11Texture2D* backBuffer{};if(SUCCEEDED(m_swapChain->GetBuffer(0,IID_PPV_ARGS(&backBuffer)))){m_device->CreateRenderTargetView(backBuffer,nullptr,&m_target);backBuffer->Release();}}
void HelpyStandaloneWindow::DestroyTarget(){if(m_target){m_target->Release();m_target=nullptr;}}

LRESULT CALLBACK HelpyStandaloneWindow::WndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    auto* self=reinterpret_cast<HelpyStandaloneWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){const auto* cs=reinterpret_cast<CREATESTRUCTW*>(lParam);self=static_cast<HelpyStandaloneWindow*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    // Detached workspaces retain their native, individually named captions.
    const bool main=self&&(!self->m_hwnd||hwnd==self->m_hwnd);
    if(main){
        if(msg==WM_WINDOWPOSCHANGED||msg==WM_ACTIVATE)NativeCaption.RefreshControls(hwnd);
        if(msg==WM_CREATE){
            NativeCaption.Update(hwnd,GetDpiForWindow(hwnd));
            if(NativeCaption.Height)SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
        if(NativeCaption.Height){
            LRESULT result{};
            if(DwmDefWindowProc(hwnd,msg,wParam,lParam,&result))return result;
            if(msg==WM_ACTIVATE||msg==WM_DWMCOMPOSITIONCHANGED)NativeCaption.Extend(hwnd);
            if(msg==WM_NCCALCSIZE&&wParam){
                auto* rect=reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);const LONG top=rect->rgrc[0].top;
                DefWindowProcW(hwnd,msg,wParam,lParam);
                rect->rgrc[0].top=top+(IsZoomed(hwnd)?NativeCaption.Border:0);return 0;
            }
            if(msg==WM_NCHITTEST){
                result=DefWindowProcW(hwnd,msg,wParam,lParam);
                if(result!=HTCLIENT&&result!=HTCAPTION)return result;
                POINT point{static_cast<short>(LOWORD(lParam)),static_cast<short>(HIWORD(lParam))};ScreenToClient(hwnd,&point);
                RECT rect{};GetClientRect(hwnd,&rect);
                return NativeCaption.HitTest(point.x,point.y,rect.right,IsZoomed(hwnd));
            }
        }
    }
    if(msg==ToggleMessage){if(self&&self->m_visible.load())self->ClosePopup();return 0;}
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(hwnd,msg,wParam,lParam))return true;
    switch(msg) {
    case WM_DROPFILES: {
        const auto drop=reinterpret_cast<HDROP>(wParam);const auto count=DragQueryFileW(drop,0xffffffff,nullptr,0);wchar_t path[32768]{};
        const auto length=count==1?DragQueryFileW(drop,0,nullptr,0):0;
        bool art=false;if(length>0&&length<32768){DragQueryFileW(drop,0,path,32768);const auto extension=HelpyCacheSources::Lower(std::filesystem::path(path).extension().string());art=extension==".png"||extension==".jpg"||extension==".jpeg"||extension==".webp"||extension==".bmp";
            POINT point{};DragQueryPoint(drop,&point);ClientToScreen(hwnd,&point);
            if(art&&PlayerPortraitPicker::CanDrop({float(point.x),float(point.y)})){PlayerPortraitPicker::Import(path);DragFinish(drop);PushToast("Portrait image queued.",3.0);return 0;}
            if(art){if(!SourceBanners::DropOwner.empty()&&ImGui::GetCurrentContext()&&ImGui::GetFrameCount()-SourceBanners::DropFrame<=2)SourceBanners::SetArtFile(SourceBanners::DropOwner,path);else SourceBanners::Status="Open the cache exporter and select one source before dropping its artwork.";}
            else if(extension==".zip"){CurrentPage=Page::Mods;ModsPanel::Import(path);DragFinish(drop);PushToast("Mod ZIP queued for inspection. Imports are disabled until reviewed.",5.0);return 0;}
            else HelpyIconPacks::Start(path,true);
        }else HelpyIconPacks::Status="Drop one cache bundle or source artwork image at a time.";
        DragFinish(drop);PushToast(art?SourceBanners::Status:HelpyIconPacks::Status,5.0);return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(lParam);const UINT dpi=GetDpiForWindow(hwnd);const float scale=dpi?static_cast<float>(dpi)/96.0f:1.0f;
        const bool bay=self&&hwnd!=self->m_hwnd;
        info->ptMinTrackSize.x=static_cast<LONG>((bay?420.0f:HelpyImGuiTheme::Current.minWindowWidth)*scale);
        info->ptMinTrackSize.y=static_cast<LONG>((bay?320.0f:HelpyImGuiTheme::Current.minWindowHeight)*scale);return 0;
    }
    case WM_SIZE:
        if(self&&hwnd!=self->m_hwnd){
            IDXGISwapChain* swap=nullptr;ID3D11RenderTargetView** targetPtr=nullptr;
            if(hwnd==self->m_cacheWindow){swap=self->m_cacheSwapChain;targetPtr=&self->m_cacheTarget;}
            else if(hwnd==self->m_bayWindow){swap=self->m_baySwapChain;targetPtr=&self->m_bayTarget;}
            else for(auto& panel:self->m_workspaces)if(hwnd==panel.Window){swap=panel.Swap;targetPtr=&panel.Target;break;}
            if(!targetPtr)return 0;auto*& target=*targetPtr;
            if(swap&&self->m_device&&wParam!=SIZE_MINIMIZED){
                self->m_context->OMSetRenderTargets(0,nullptr,nullptr);
                if(target){target->Release();target=nullptr;}
                if(SUCCEEDED(swap->ResizeBuffers(0,LOWORD(lParam),HIWORD(lParam),DXGI_FORMAT_UNKNOWN,0))){
                    ID3D11Texture2D* buffer=nullptr;if(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))){self->m_device->CreateRenderTargetView(buffer,nullptr,&target);buffer->Release();}
                }
            }return 0;
        }
        if(self&&HelpyEnvironment::Server())self->m_visible=wParam!=SIZE_MINIMIZED;
        if(self&&self->m_device&&wParam!=SIZE_MINIMIZED){self->DestroyTarget();self->m_swapChain->ResizeBuffers(0,LOWORD(lParam),HIWORD(lParam),DXGI_FORMAT_UNKNOWN,0);self->CreateTarget();}return 0;
    case WM_DPICHANGED:
        if(main){HelpyImGuiTheme::SetRuntimeDpi(static_cast<float>(HIWORD(wParam))/96.f);NativeCaption.Update(hwnd,HIWORD(wParam));}
        if(const auto* rect=reinterpret_cast<RECT*>(lParam))SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;
    case WM_CLOSE:if(self){if(hwnd==self->m_bayWindow){ItemGridPicker::DetachedBay=false;ShowWindow(hwnd,SW_HIDE);}else if(hwnd==self->m_cacheWindow){HelpyIconPacks::Detached=false;ShowWindow(hwnd,SW_HIDE);}else if(hwnd==self->m_hwnd)self->ClosePopup();else for(size_t i=0;i<self->m_workspaces.size();++i)if(hwnd==self->m_workspaces[i].Window){WorkspaceDetached(i)=false;if(i==2)RuntimeDetailsPanel::Open=false;if(i==3)MapPanel::Data.ShowParty=false;if(i==4)PlayerInventoryPanel::Open=false;ShowWindow(hwnd,SW_HIDE);break;}}return 0;
    case WM_SYSCOMMAND:
        if((wParam&0xfff0)==SC_KEYMENU&&lParam!=VK_SPACE)return 0;
        if(self&&(wParam&0xfff0)==SC_MINIMIZE&&(hwnd==self->m_bayWindow||hwnd==self->m_cacheWindow)){
            HelpyTaskbar::Current.Minimize(hwnd==self->m_bayWindow?HelpyTaskbar::Window::ItemBay:HelpyTaskbar::Window::IconCache);
            if(GetCapture()==hwnd)ReleaseCapture();ShowWindow(hwnd,SW_HIDE);return 0;
        }
        if(self&&(wParam&0xfff0)==SC_MINIMIZE)for(size_t i=0;i<self->m_workspaces.size();++i)if(hwnd==self->m_workspaces[i].Window){
            HelpyTaskbar::Current.Minimize(WorkspaceTask(i));if(GetCapture()==hwnd)ReleaseCapture();ShowWindow(hwnd,SW_HIDE);return 0;
        }break;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}
}
