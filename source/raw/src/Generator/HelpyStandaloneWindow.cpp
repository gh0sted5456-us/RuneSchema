#include "Generator/HelpyStandaloneWindow.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/LazyCatalog.h"
#include "Generator/ItemGridPicker.h"
#include "Generator/ToolRequest.h"
#include "Core/HelpyPopupInput.h"
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <imgui.h>
#include <chrono>
#include <algorithm>
#include <string>
#include <vector>
#include <random>
#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);

namespace PS {
namespace {
constexpr UINT ToggleMessage=WM_APP+0x521,ReloadMessage=WM_APP+0x522,StopMessage=WM_APP+0x523;
constexpr UINT RoutedKeyMessage=WM_APP+0x524,ReleaseGameKeysMessage=WM_APP+0x525;
// No desktop-wide keyboard hook or stored input history. This route is bound
// only to the foreground game's window thread and only while Helpy is visible.
struct PopupRoute {
    std::atomic<HWND> Game=nullptr,Popup=nullptr;
    std::atomic<bool> Visible=false;
    std::atomic<unsigned> InFlight=0;
    HelpyPopupInput::HotkeyLatch Hotkey;
};
PopupRoute InputRoute;
unsigned UiModifierSnapshot=0;
constexpr std::pair<int,unsigned> ModifierKeys[]={{VK_CONTROL,1},{VK_SHIFT,2},{VK_MENU,4},{VK_LWIN,8},{VK_RWIN,4096},
    {VK_LSHIFT,16},{VK_RSHIFT,32},{VK_LCONTROL,64},{VK_RCONTROL,128},{VK_LMENU,256},{VK_RMENU,512}};
void ApplyUiModifierSnapshot(unsigned mods) {
    // SetKeyboardState changes only this UI thread's input-state table, never
    // the game's. The non-activating window cannot rely on its own OS key queue.
    BYTE state[256]{};if(!GetKeyboardState(state))return;
    for(const auto& [key,bit]:ModifierKeys)state[key]=static_cast<BYTE>((state[key]&0x7f)|((mods&bit)?0x80:0));
    SetKeyboardState(state);
}
bool GameWindow(HWND hwnd) {
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    wchar_t name[128]{};GetClassNameW(hwnd,name,128);
    return hwnd&&pid==GetCurrentProcessId()&&std::wstring_view(name)==L"UnrealWindow";
}
LRESULT CALLBACK PopupInputHook(int code,WPARAM removing,LPARAM data) {
    InputRoute.InFlight.fetch_add(1);
    struct Leave{~Leave(){InputRoute.InFlight.fetch_sub(1);}} leave;
    auto* msg=code==HC_ACTION&&data?reinterpret_cast<MSG*>(data):nullptr;
    const auto game=InputRoute.Game.load(),popup=InputRoute.Popup.load();
    if(msg&&game&&popup&&removing==PM_REMOVE&&msg->hwnd==game&&msg->message==ReleaseGameKeysMessage) {
        // Release keys pressed before opening so game movement cannot remain
        // latched when their key-up is routed to Helpy instead of the viewport.
        if(GetForegroundWindow()==game) {
            auto proc=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(game,GWLP_WNDPROC));
            if(proc)for(UINT key=8;key<256;++key)if(GetKeyState(key)&0x8000)
                CallWindowProcW(proc,game,WM_KEYUP,key,(LPARAM{1}<<31)|(LPARAM{1}<<30)|(MapVirtualKeyW(key,MAPVK_VK_TO_VSC)<<16)|1);
            if(proc) {
                POINT point{};GetCursorPos(&point);ScreenToClient(game,&point);const auto position=MAKELPARAM(point.x,point.y);
                for(const auto& [key,message]:{std::pair{VK_LBUTTON,WM_LBUTTONUP},std::pair{VK_RBUTTON,WM_RBUTTONUP},std::pair{VK_MBUTTON,WM_MBUTTONUP}})
                    if(GetKeyState(key)&0x8000)CallWindowProcW(proc,game,message,0,position);
                if(GetKeyState(VK_XBUTTON1)&0x8000)CallWindowProcW(proc,game,WM_XBUTTONUP,MAKEWPARAM(0,XBUTTON1),position);
                if(GetKeyState(VK_XBUTTON2)&0x8000)CallWindowProcW(proc,game,WM_XBUTTONUP,MAKEWPARAM(0,XBUTTON2),position);
            }
        }
        msg->message=WM_NULL;msg->wParam=0;msg->lParam=0;
    }
    if(msg&&HelpyPopupInput::Route(InputRoute.Visible.load(),GetForegroundWindow()==game,
        msg->hwnd==game||(game&&IsChild(game,msg->hwnd)),removing==PM_REMOVE)) {
        const auto message=msg->message;
        const bool key=message==WM_KEYDOWN||message==WM_KEYUP||message==WM_SYSKEYDOWN||message==WM_SYSKEYUP;
        const bool text=message==WM_CHAR||message==WM_SYSCHAR||message==WM_UNICHAR;
        const bool mouse=message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST;
        const bool systemSwitch=key&&(msg->wParam==VK_LWIN||msg->wParam==VK_RWIN||(msg->wParam==VK_TAB&&(GetKeyState(VK_MENU)&0x8000)));
        if(!systemSwitch&&(key||text||mouse||message==WM_INPUT)) {
            if(key&&msg->wParam==VK_F2) {
                if((message==WM_KEYDOWN||message==WM_SYSKEYDOWN)&&InputRoute.Hotkey.Press())PostMessageW(popup,ToggleMessage,0,0);
                else if(message==WM_KEYUP||message==WM_SYSKEYUP)InputRoute.Hotkey.Release();
            }else if(key) {
                unsigned mods=0;for(const auto& [key,bit]:ModifierKeys)if(GetKeyState(key)&0x8000)mods|=bit;
                const auto packed=(static_cast<WPARAM>(message)<<32)|(static_cast<WPARAM>(mods)<<16)|(msg->wParam&0xffff);
                if(PostMessageW(popup,RoutedKeyMessage,packed,msg->lParam)&&message==WM_KEYDOWN)TranslateMessage(msg);
            }else if(text)PostMessageW(popup,message,msg->wParam,msg->lParam);
            else if(mouse) {
                POINT point{static_cast<short>(LOWORD(msg->lParam)),static_cast<short>(HIWORD(msg->lParam))};
                if(message!=WM_MOUSEWHEEL&&message!=WM_MOUSEHWHEEL){ClientToScreen(msg->hwnd,&point);ScreenToClient(popup,&point);}
                PostMessageW(popup,message,msg->wParam,MAKELPARAM(point.x,point.y));
            }else if(message==WM_INPUT)DefWindowProcW(msg->hwnd,message,msg->wParam,msg->lParam);
            msg->message=WM_NULL;msg->wParam=0;msg->lParam=0;
        }
    }
    return CallNextHookEx(nullptr,code,removing,data);
}
enum class Page : unsigned char { Items,Ai,Resources,ItemLab,Buildings,Cleanup,Settings };
Page CurrentPage=Page::Items;
float SidebarWidthLogical=-1.0f;
ImFont* UiRegularFont=nullptr;
ImFont* UiBoldFont=nullptr;
struct UiFontFamily {std::string Name;ImFont* Regular{};ImFont* Bold{};std::string Source;bool Available{};};
std::vector<UiFontFamily> UiFontFamilies;
const UiFontFamily* FindUiFontFamily(const std::string& name){for(const auto& f:UiFontFamilies)if(f.Name==name)return &f;return nullptr;}
const UiFontFamily* ActiveUiFontFamily(const std::string& requested){
    if(const auto* f=FindUiFontFamily(requested);f&&f->Available)return f;
    if(const auto* f=FindUiFontFamily("Segoe UI");f&&f->Available)return f;
    for(const auto& f:UiFontFamilies)if(f.Available)return &f;return nullptr;
}
void SelectUiFontFamily(const std::string& requested){
    if(const auto* f=ActiveUiFontFamily(requested)){UiRegularFont=f->Regular;UiBoldFont=f->Bold?f->Bold:f->Regular;}
}
std::filesystem::path FindRuneScapeFontFile() {
    std::vector<std::filesystem::path> roots{
        HostServices::ModDirectory()/"plugins"/"RuneSchema.Helpy"/"assets"/"fonts",
        HostServices::WorkingDirectory()/"Mods"/"RuneSchema"/"plugins"/"RuneSchema.Helpy"/"assets"/"fonts",
        HostServices::WorkingDirectory()/"plugins"/"RuneSchema.Helpy"/"assets"/"fonts"
    };
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
    UiFontFamilies.clear();
    const auto add=[&](const char* name,std::vector<std::filesystem::path> regular,std::vector<std::filesystem::path> bold){
        UiFontFamily f;f.Name=name;std::filesystem::path rp,bp;std::error_code ec;
        for(const auto& x:regular)if(!x.empty()&&std::filesystem::is_regular_file(x,ec)){rp=x;break;}
        for(const auto& x:bold)if(!x.empty()&&std::filesystem::is_regular_file(x,ec)){bp=x;break;}
        if(!rp.empty()){f.Regular=io.Fonts->AddFontFromFileTTF(rp.string().c_str(),13.0f);if(!bp.empty())f.Bold=io.Fonts->AddFontFromFileTTF(bp.string().c_str(),13.0f);if(!f.Bold)f.Bold=f.Regular;f.Available=f.Regular!=nullptr;f.Source=rp.string();}
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
    if(const auto* f=ActiveUiFontFamily("Segoe UI")){UiRegularFont=f->Regular;UiBoldFont=f->Bold;io.FontDefault=UiRegularFont;}
    else {UiRegularFont=io.Fonts->AddFontDefault();UiBoldFont=UiRegularFont;io.FontDefault=UiRegularFont;}
}
std::atomic<bool> FontRescanRequested{false};
struct Toast {std::string Text;std::chrono::steady_clock::time_point Until;};
std::vector<Toast> Toasts;
std::chrono::steady_clock::time_point NextPlayerRefresh{};
void PushToast(std::string text,double seconds=3.0){Toasts.push_back({std::move(text),std::chrono::steady_clock::now()+std::chrono::milliseconds(static_cast<int>(seconds*1000.0))});if(Toasts.size()>4)Toasts.erase(Toasts.begin());}
bool QueuePlayerRefresh(bool automatic){
    const bool queued=SpawnToolRequests::TrySubmit({{"Action","QuickPlayers"},{"_Source","HelpyStandalone"}});
    if(automatic)PushToast(queued?"Background: refreshing players...":"Background player refresh skipped: runtime is busy.",queued?2.6:3.2);
    NextPlayerRefresh=std::chrono::steady_clock::now()+std::chrono::minutes(1);return queued;
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
const char* PageTitle(Page page) {
    switch(page){case Page::Items:return "Items";case Page::Ai:return "AI";case Page::Resources:return "Resources";
    case Page::ItemLab:return "Item Lab";case Page::Buildings:return "Buildings";case Page::Cleanup:return "Cleanup";default:return "Settings";}
}
HelpyImGuiTheme::Icon PageIcon(Page page) {
    using I=HelpyImGuiTheme::Icon;switch(page){case Page::Items:return I::Item;case Page::Ai:return I::Ai;case Page::Resources:return I::Resource;
    case Page::ItemLab:return I::Lab;case Page::Buildings:return I::Building;case Page::Cleanup:return I::Cleanup;default:return I::Appearance;}
}
bool NavItem(Page page,const HelpyImGuiTheme::Theme& t) {
    const bool active=CurrentPage==page;ImGui::PushID(static_cast<int>(page));
    ImGui::PushStyleColor(ImGuiCol_Header,active?t.navigationActive:t.navigation);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,t.navigationHover);ImGui::PushStyleColor(ImGuiCol_HeaderActive,t.navigationActive);
    const float h=t.Y(t.navigationItemHeight);const float w=ImGui::GetContentRegionAvail().x;
    const bool clicked=ImGui::Selectable("##nav",active,ImGuiSelectableFlags_None,ImVec2(w,h));
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const float icon=t.Y(t.navigationIconSize);
    HelpyImGuiTheme::DrawIcon(PageIcon(page),ImVec2(a.x+t.X(18.0f),(a.y+b.y)*0.5f),icon,active?t.accentHover:t.icon);
    ImGui::GetWindowDrawList()->AddText(ImVec2(a.x+t.X(36.0f),a.y+(h-ImGui::GetTextLineHeight())*.5f),ImGui::GetColorU32(active?t.text:t.textDisabled),PageTitle(page));
    ImGui::PopStyleColor(3);ImGui::PopID();if(clicked)CurrentPage=page;return clicked;
}

struct UserColorPreset { std::string Name; nlohmann::json Colors; };

void CopyThemeColors(HelpyImGuiTheme::Theme& dst,const HelpyImGuiTheme::Theme& src) {
#define COPY_COLOR(name) dst.name=src.name
    COPY_COLOR(window);COPY_COLOR(panel);COPY_COLOR(panelAlt);COPY_COLOR(navigation);COPY_COLOR(navigationHover);COPY_COLOR(navigationActive);
    COPY_COLOR(accent);COPY_COLOR(accentHover);COPY_COLOR(accentActive);COPY_COLOR(text);COPY_COLOR(textDisabled);
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
        {"accent",ColorJson(t.accent)},{"accentHover",ColorJson(t.accentHover)},{"accentActive",ColorJson(t.accentActive)},
        {"text",ColorJson(t.text)},{"textDisabled",ColorJson(t.textDisabled)},
        {"control",ColorJson(t.control)},{"controlHover",ColorJson(t.controlHover)},{"controlActive",ColorJson(t.controlActive)},
        {"button",ColorJson(t.button)},{"buttonHover",ColorJson(t.buttonHover)},{"buttonActive",ColorJson(t.buttonActive)},
        {"selection",ColorJson(t.selection)},{"border",ColorJson(t.border)},{"separator",ColorJson(t.separator)},
        {"scrollbarTrack",ColorJson(t.scrollbarTrack)},{"scrollbar",ColorJson(t.scrollbar)},{"scrollbarHover",ColorJson(t.scrollbarHover)},{"scrollbarActive",ColorJson(t.scrollbarActive)},
        {"icon",ColorJson(t.icon)},{"success",ColorJson(t.success)},{"warning",ColorJson(t.warning)},{"error",ColorJson(t.error)},{"info",ColorJson(t.info)},{"scrollFade",ColorJson(t.scrollFade)}
    };
}
void ApplyThemeColorsJson(HelpyImGuiTheme::Theme& t,const nlohmann::json& c) {
#define READ_COLOR(name) t.name=HelpyImGuiTheme::ReadColor(c,#name,t.name)
    READ_COLOR(window);READ_COLOR(panel);READ_COLOR(panelAlt);READ_COLOR(navigation);READ_COLOR(navigationHover);READ_COLOR(navigationActive);
    READ_COLOR(accent);READ_COLOR(accentHover);READ_COLOR(accentActive);READ_COLOR(text);READ_COLOR(textDisabled);
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
        ConfigFiles::Write(ThemePresetPath(),"// RuneSchema HELPY user color presets.\n"+nlohmann::json{{"version",1},{"presets",entries}}.dump(2)+"\n");
        return true;
    } catch(...) {return false;}
}
std::string TrimPresetName(std::string name) {
    const auto first=name.find_first_not_of(" \t\r\n");if(first==std::string::npos)return {};
    const auto last=name.find_last_not_of(" \t\r\n");name=name.substr(first,last-first+1);if(name.size()>48)name.resize(48);return name;
}
constexpr const char* BuiltinColorPresetNames[]={
    "OldSchool RuneScape","RuneSchema .7","Dark Mode","Light Mode",
    "Cyberpunk","Light Sky","Dark Sky","Mid Sky"
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
    case 1: break;
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

void RenderSettings(const HelpyImGuiTheme::Theme& t) {
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
        ItemIconCache::Update();ImGui::PushID("sizing-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(168.0f)),true);ImGui::TextUnformatted("Sizing preview");ImGui::Separator();
        const float listIcon=t.Y(theme.listIconSize),rowH=std::max(listIcon,t.Y(32.0f));const auto row=ImGui::GetCursorScreenPos();const ImVec2 ia(row.x+t.X(6.0f),row.y+(rowH-listIcon)*.5f),ib(ia.x+listIcon,ia.y+listIcon);drawPreviewIcon(ia,ib);ImGui::SetCursorScreenPos(ImVec2(ib.x+t.X(8.0f),row.y+(rowH-ImGui::GetTextLineHeight())*.5f));ImGui::TextUnformatted(previewItemName.c_str());ImGui::SetCursorScreenPos(ImVec2(row.x,row.y+rowH+t.Y(8.0f)));
        const float cardW=std::min(t.X(theme.gridMinCardWidth),ImGui::GetContentRegionAvail().x),cardH=std::min(t.Y(theme.gridCardHeight),t.Y(88.0f));const auto a=ImGui::GetCursorScreenPos(),b=ImVec2(a.x+cardW,a.y+cardH);auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(a,b,ImGui::GetColorU32(theme.panelAlt),t.X(theme.childRounding));d->AddRect(a,b,ImGui::GetColorU32(theme.border),t.X(theme.childRounding));const float gridIcon=std::min(t.Y(theme.gridIconSize),cardH-t.Y(28.0f));const ImVec2 ga(a.x+(cardW-gridIcon)*.5f,a.y+t.Y(6.0f)),gb(ga.x+gridIcon,ga.y+gridIcon);drawPreviewIcon(ga,gb);const auto label=previewItemName.size()>22?previewItemName.substr(0,19)+"...":previewItemName;const auto ts=ImGui::CalcTextSize(label.c_str());d->AddText(ImVec2(a.x+(cardW-ts.x)*.5f,b.y-t.Y(22.0f)),ImGui::GetColorU32(theme.text),label.c_str());ImGui::Dummy(ImVec2(cardW,cardH));
        ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();
    };
    const auto surfacePreview=[&](){ImGui::PushID("surface-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(116.0f)),true);ImGui::TextUnformatted("Surface preview");ImGui::Separator();ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panelAlt);ImGui::BeginChild("##alt",ImVec2(t.X(220.0f),t.Y(54.0f)),true);ImGui::TextUnformatted("Panel alternate / card");ImGui::Button("Control surface");ImGui::EndChild();ImGui::PopStyleColor();ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();};
    const auto accentPreview=[&](){ImGui::PushID("accent-preview");ImGui::PushStyleColor(ImGuiCol_ChildBg,theme.panel);ImGui::BeginChild("##preview",ImVec2(0,t.Y(105.0f)),true);ImGui::TextUnformatted("Interaction preview");ImGui::Separator();ImGui::PushStyleColor(ImGuiCol_Button,theme.accent);ImGui::PushStyleColor(ImGuiCol_ButtonHovered,theme.accentHover);ImGui::PushStyleColor(ImGuiCol_ButtonActive,theme.accentActive);ImGui::Button("Accent action");ImGui::PopStyleColor(3);ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Button,theme.selection);ImGui::Button("Selected");ImGui::PopStyleColor();ImGui::SameLine();HelpyImGuiTheme::DrawIcon(HelpyImGuiTheme::Icon::Item,ImVec2(ImGui::GetCursorScreenPos().x+t.X(12.0f),ImGui::GetCursorScreenPos().y+ImGui::GetFrameHeight()*.5f),t.Y(16.0f),theme.icon);ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopID();};
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

    if(ImGui::CollapsingHeader("Color theme presets",ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if(ImGui::Button("Apply")) {
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
            if(ImGui::Button("Delete"))ImGui::OpenPopup("Delete color preset?");
        }
        if(ImGui::BeginPopupModal("Delete color preset?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto custom=selectedPreset>=builtinCount?static_cast<size_t>(selectedPreset-builtinCount):userPresets.size();
            if(custom<userPresets.size())ImGui::TextWrapped("Delete custom color preset \"%s\"?",userPresets[custom].Name.c_str());
            if(ImGui::Button("Delete preset")&&custom<userPresets.size()) {
                const auto deleted=userPresets[custom].Name;userPresets.erase(userPresets.begin()+custom);
                if(SaveUserColorPresets(userPresets)){selectedPreset=std::min(1,builtinCount-1);PushToast("Deleted custom color preset: "+deleted,2.5);}
                else PushToast("Could not save theme-presets.jsonc after deleting preset.",3.5);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(1,t.Y(3.0f)));
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Save current colors");ImGui::SameLine(t.X(220.0f));ImGui::SetNextItemWidth(std::max(t.X(180.0f),ImGui::GetContentRegionAvail().x-t.X(150.0f)));
        ImGui::InputTextWithHint("##preset-name","Preset name",presetName,sizeof(presetName));ImGui::SameLine();
        if(ImGui::Button("Save preset")) {
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

    if(!theme.advancedMode) {
        ImGui::SeparatorText("Size");
        if(ImGui::SliderFloat("Window scale",&theme.userScale,0.75f,2.0f,"%.2fx"))changed();
        if(ImGui::SliderFloat("Text scale",&theme.fontScale,0.8f,2.0f,"%.2fx"))changed();
        ImGui::TextDisabled("Advanced mode adds detailed layout, colors, cache and authoring controls.");return;
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
        sectionActions("display-sizing",[&](){theme.dpiAware=defaults.dpiAware;theme.autoReload=defaults.autoReload;theme.verboseTooltips=defaults.verboseTooltips;theme.tooltipDelay=defaults.tooltipDelay;theme.userScale=defaults.userScale;theme.fontScale=defaults.fontScale;theme.density=defaults.density;theme.resultListHeight=defaults.resultListHeight;theme.itemResultHeight=defaults.itemResultHeight;theme.aiResultHeight=defaults.aiResultHeight;theme.itemGridView=defaults.itemGridView;theme.aiGridView=defaults.aiGridView;theme.itemCategory=defaults.itemCategory;theme.listIconSize=defaults.listIconSize;theme.gridIconSize=defaults.gridIconSize;theme.gridCardHeight=defaults.gridCardHeight;theme.gridMinCardWidth=defaults.gridMinCardWidth;theme.navigationItemHeight=defaults.navigationItemHeight;theme.navigationIconSize=defaults.navigationIconSize;theme.loadingSpinnerRadius=defaults.loadingSpinnerRadius;theme.loadingSpinnerThickness=defaults.loadingSpinnerThickness;theme.overscanRows=defaults.overscanRows;});
        checkbox("DPI aware",theme.dpiAware,defaults.dpiAware);checkbox("Auto reload ui.jsonc",theme.autoReload,defaults.autoReload);checkbox("Verbose tooltips",theme.verboseTooltips,defaults.verboseTooltips);ImGui::TextDisabled("Adds internal paths, IDs, classes and other available metadata to hover tooltips without triggering extra runtime queries.");slider("Tooltip delay",theme.tooltipDelay,0.0f,2.0f,defaults.tooltipDelay,"%.2f s");
        slider("User scale",theme.userScale,0.60f,2.50f,defaults.userScale);slider("Font scale",theme.fontScale,0.70f,1.75f,defaults.fontScale);slider("Density",theme.density,0.65f,1.50f,defaults.density);
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
        else if(theme.fontFamily=="RuneScape")ImGui::TextDisabled("RuneScape font not loaded. HELPY scans assets/fonts for any RuneScape TTF/OTF.");
        else ImGui::TextDisabled("Selected font is unavailable on this Windows install; Segoe UI fallback is active.");
        if(ImGui::SmallButton("Rescan font files"))FontRescanRequested=true;
        color("Text",theme.text,defaults.text,&theme.textBold,defaults.textBold);color("Disabled text",theme.textDisabled,defaults.textDisabled);color("Success",theme.success,defaults.success);color("Warning",theme.warning,defaults.warning);color("Error",theme.error,defaults.error);color("Info",theme.info,defaults.info);
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

    if(ImGui::CollapsingHeader("Icon cache")) {
        sectionActions("icon-cache",[&](){HelpyRuntimePreferences::IconCacheLimitMB.store(200);});
        int cacheMB=HelpyRuntimePreferences::IconCacheLimitMB.load();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Persistent cache limit (MB)");ImGui::SameLine(t.X(220.0f));ImGui::SetNextItemWidth(std::max(t.X(130.0f),ImGui::GetContentRegionAvail().x-t.X(72.0f)));
        if(ImGui::InputInt("##cache-mb",&cacheMB,16,128)){cacheMB=std::clamp(cacheMB,0,16384);HelpyRuntimePreferences::IconCacheLimitMB.store(cacheMB);changed();}ImGui::SameLine();if(ImGui::SmallButton("Reset##cache")){HelpyRuntimePreferences::IconCacheLimitMB.store(200);cacheMB=200;changed();}
        if(cacheMB==0)ImGui::TextDisabled("0 MB: live/session-only cache. Temporary transport files are discarded between runs.");else ImGui::TextDisabled("Persistent item-icon cache uses LRU eviction near this limit.");
        static ItemIconCache::DiskStats cacheStats{};static auto nextCacheStats=std::chrono::steady_clock::time_point{};const auto cacheNow=std::chrono::steady_clock::now();if(cacheNow>=nextCacheStats){cacheStats=ItemIconCache::CurrentDiskStats();nextCacheStats=cacheNow+std::chrono::seconds(1);}
        ImGui::Text("Current icon cache: %.2f MB (%zu files)",static_cast<double>(cacheStats.Bytes)/(1024.0*1024.0),cacheStats.Files);
        if(ImGui::Button("Clean Icon Cache")){const bool cleaned=ItemIconCache::CleanDiskCache();cacheStats=ItemIconCache::CurrentDiskStats();nextCacheStats=cacheNow+std::chrono::seconds(1);PushToast(cleaned?"Icon cache cleaned.":"Icon cache cleanup completed with a filesystem warning.",3.0);}
    }

    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::SeparatorText("Theme file & reset");
    if(ImGui::Button("Reload ui.jsonc (F5)")){HelpyImGuiTheme::RequestReload();pendingSave=false;}ImGui::SameLine();if(ImGui::Button("Open ui.jsonc")){const auto path=HelpyImGuiTheme::FilePath();ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,path.parent_path().c_str(),SW_SHOWNORMAL);}ImGui::SameLine();if(ImGui::Button("Reset to RuneSchema defaults"))ImGui::OpenPopup("Reset RuneSchema UI?");
    if(ImGui::BeginPopupModal("Reset RuneSchema UI?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {ImGui::TextWrapped("Restore all Helpy UI colors, typography, scaling, spacing, icon/card sizing, and icon cache limit to the shipped RuneSchema defaults?");if(ImGui::Button("Reset defaults")){HelpyImGuiTheme::ResetDefaults();ItemGridPicker::ResetViewPreference();SidebarWidthLogical=-1.0f;pendingSave=false;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
    ImGui::TextWrapped("%s",HelpyImGuiTheme::Status.c_str());if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}

    ImGui::Dummy(ImVec2(1,t.Y(12.0f)));ImGui::SeparatorText("Hotkeys");ImGui::Text("F2");ImGui::SameLine(t.X(70.0f));ImGui::TextUnformatted("Toggle HELPY");ImGui::Text("F5");ImGui::SameLine(t.X(70.0f));ImGui::TextUnformatted("Reload live skin");
}
void RenderWorkspace(const HelpyImGuiTheme::Theme& t) {
    SpawnToolsPanel::PollCartDelivery();
    if(!t.advancedMode&&(CurrentPage==Page::ItemLab||CurrentPage==Page::Buildings||CurrentPage==Page::Cleanup))CurrentPage=Page::Items;
    if(SidebarWidthLogical<0.0f)SidebarWidthLogical=t.navigationWidth;
    const float totalLogical=ImGui::GetContentRegionAvail().x/std::max(0.01f,t.Scale());
    const float minimumContent=360.0f;
    const float dynamicMax=std::max(t.navigationMinWidth,totalLogical-minimumContent-t.splitterWidth-t.contentGutter);
    SidebarWidthLogical=std::clamp(SidebarWidthLogical,t.navigationMinWidth,std::min(t.navigationMaxWidth,dynamicMax));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,t.navigation);
    ImGui::BeginChild("##helpy-nav",ImVec2(t.X(SidebarWidthLogical),0),false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::SetCursorPosX(t.X(15.0f));ImGui::TextUnformatted("RuneSchema");
    ImGui::SetCursorPosX(t.X(15.0f));ImGui::PushStyleColor(ImGuiCol_Text,t.accentHover);ImGui::TextUnformatted("HELPY 2.0");ImGui::PopStyleColor();
    ImGui::SetCursorPosX(t.X(15.0f));ImGui::TextDisabled("by NuLL");
    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::Separator();ImGui::Dummy(ImVec2(1,t.Y(5.0f)));
    NavItem(Page::Items,t);NavItem(Page::Ai,t);NavItem(Page::Resources,t);if(t.advancedMode){NavItem(Page::ItemLab,t);NavItem(Page::Buildings,t);}
    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::Separator();ImGui::Dummy(ImVec2(1,t.Y(5.0f)));
    if(t.advancedMode)NavItem(Page::Cleanup,t);NavItem(Page::Settings,t);
    const float footer=t.Y(34.0f);
    if(ImGui::GetContentRegionAvail().y>footer)ImGui::SetCursorPosY(ImGui::GetWindowHeight()-footer);
    ImGui::SetCursorPosX(t.X(15.0f));ImGui::TextDisabled("F2  close");
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

    ImGui::SameLine(0,t.X(t.contentGutter));ImGui::BeginChild("##helpy-content",ImVec2(0,0),false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::Dummy(ImVec2(1,t.Y(5.0f)));const float headerInset=t.X(t.contentPaddingX);ImGui::SetCursorPosX(ImGui::GetCursorPosX()+headerInset);ImGui::TextUnformatted(PageTitle(CurrentPage));
    const char* modeLabel=t.advancedMode?"Advanced":"Simple";const float modeWidth=ImGui::CalcTextSize("Advanced").x+ImGui::GetStyle().FramePadding.x*2.0f;
    ImGui::SameLine();ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-modeWidth-headerInset));
    if(ImGui::SmallButton(modeLabel)){HelpyImGuiTheme::Current.advancedMode=!t.advancedMode;HelpyImGuiTheme::SaveCurrent();}
    if(ImGui::IsItemHovered())ImGui::SetTooltip(t.advancedMode?"Switch to Simple mode":"Switch to Advanced mode");
    ImGui::Separator();
    const bool scrollVisible=HelpyImGuiTheme::BeginScroll("##helpy-scroll",ImVec2(0,0),false);
    if(scrollVisible) {
        ImGui::Dummy(ImVec2(1,t.Y(4.0f)));
        switch(CurrentPage) {
        case Page::Items:SpawnToolsPanel::Render(SpawnToolsPanel::View::Items);break;
        case Page::Ai:SpawnToolsPanel::Render(SpawnToolsPanel::View::Ai);break;
        case Page::Resources:SpawnToolsPanel::Render(SpawnToolsPanel::View::Resources);break;
        case Page::ItemLab:SpawnToolsPanel::Render(SpawnToolsPanel::View::ItemLab);break;
        case Page::Buildings:SpawnToolsPanel::Render(SpawnToolsPanel::View::Buildings);break;
        case Page::Cleanup:SpawnToolsPanel::RenderSessionCleanup();break;
        case Page::Settings:RenderSettings(t);break;
        }
    }
    HelpyImGuiTheme::EndScroll(t);ImGui::EndChild();
}
}

bool HelpyStandaloneWindow::Start() {
    if(m_thread.joinable())return m_ready.load();m_stop=false;m_started=false;m_thread=std::thread([this]{ThreadMain();});
    for(int i=0;i<500&&!m_started.load();++i)Sleep(2);return m_ready.load();
}
void HelpyStandaloneWindow::Toggle(){if(m_threadId)PostThreadMessageW(m_threadId,ToggleMessage,0,0);}
void HelpyStandaloneWindow::PollHotkey(bool down){if(InputRoute.Hotkey.Poll(down)&&GameWindow(GetForegroundWindow()))Toggle();}
void HelpyStandaloneWindow::ReloadTheme(){if(m_threadId)PostThreadMessageW(m_threadId,ReloadMessage,0,0);}
void HelpyStandaloneWindow::Shutdown(){if(!m_thread.joinable())return;m_stop=true;if(m_threadId)PostThreadMessageW(m_threadId,StopMessage,0,0);m_thread.join();m_threadId=0;m_ready=false;}
std::string HelpyStandaloneWindow::Error() const{std::scoped_lock lock(m_errorMutex);return m_error;}
void HelpyStandaloneWindow::SetError(std::string value){std::scoped_lock lock(m_errorMutex);m_error=std::move(value);}
bool HelpyStandaloneWindow::OpenPopup() {
    const auto game=GetForegroundWindow();if(!GameWindow(game))return false;
    DWORD pid=0;const DWORD thread=GetWindowThreadProcessId(game,&pid);
    if(!thread||pid!=GetCurrentProcessId())return false;
    m_gameWindow=game;InputRoute.Game=game;InputRoute.Popup=m_hwnd;
    m_inputHook=SetWindowsHookExW(WH_GETMESSAGE,PopupInputHook,nullptr,thread);
    if(!m_inputHook){InputRoute.Game=nullptr;InputRoute.Popup=nullptr;SetError("Could not attach the game-local Helpy input route");return false;}
    SetWindowLongPtrW(m_hwnd,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game));
    m_clipSaved=GetClipCursor(&m_savedClip)!=0;ClipCursor(nullptr);GetClipCursor(&m_releasedClip);
    RECT gameRect{},popupRect{};GetWindowRect(game,&gameRect);GetWindowRect(m_hwnd,&popupRect);
    SetWindowPos(m_hwnd,HWND_TOPMOST,gameRect.left+std::max(0L,(gameRect.right-gameRect.left-(popupRect.right-popupRect.left))/2),
        gameRect.top+std::max(0L,(gameRect.bottom-gameRect.top-(popupRect.bottom-popupRect.top))/2),0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);m_visible=true;InputRoute.Visible=true;
    PostMessageW(game,ReleaseGameKeysMessage,0,0);ImGui::GetIO().AddFocusEvent(true);
    LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());QueuePlayerRefresh(true);return true;
}
void HelpyStandaloneWindow::ReleaseInputRoute() {
    InputRoute.Visible=false;
    if(m_inputHook){UnhookWindowsHookEx(m_inputHook);m_inputHook=nullptr;}
    InputRoute.Game=nullptr;InputRoute.Popup=nullptr;
    while(InputRoute.InFlight.load())Sleep(1);
    RECT clip{};
    if(m_clipSaved&&GetForegroundWindow()==m_gameWindow&&GetClipCursor(&clip)&&EqualRect(&clip,&m_releasedClip))ClipCursor(&m_savedClip);
    m_clipSaved=false;
}
void HelpyStandaloneWindow::ClosePopup(){m_visible=false;ReleaseInputRoute();UiModifierSnapshot=0;ApplyUiModifierSnapshot(0);ShowWindow(m_hwnd,SW_HIDE);if(ImGui::GetCurrentContext())ImGui::GetIO().AddFocusEvent(false);}

void HelpyStandaloneWindow::ThreadMain() {
    m_threadId=GetCurrentThreadId();
    WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,WndProc,0,0,m_module,nullptr,LoadCursorW(nullptr,IDC_ARROW),nullptr,nullptr,L"RuneSchema.Helpy.Window",nullptr};
    RegisterClassExW(&wc);
    m_hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,wc.lpszClassName,L"RuneSchema HELPY by NuLL",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1120,780,nullptr,nullptr,m_module,this);
    if(!m_hwnd){SetError("CreateWindowExW failed");m_started=true;UnregisterClassW(wc.lpszClassName,m_module);return;}
    HICON appIconBig=nullptr,appIconSmall=nullptr;const auto iconPath=HostServices::WorkingDirectory()/"Mods"/"RuneSchema"/"plugins"/"RuneSchema.Helpy"/"assets"/"RuneSchema.ico";
    if(std::filesystem::is_regular_file(iconPath)){appIconBig=static_cast<HICON>(LoadImageW(nullptr,iconPath.c_str(),IMAGE_ICON,32,32,LR_LOADFROMFILE));appIconSmall=static_cast<HICON>(LoadImageW(nullptr,iconPath.c_str(),IMAGE_ICON,16,16,LR_LOADFROMFILE));if(appIconBig)SendMessageW(m_hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(appIconBig));if(appIconSmall)SendMessageW(m_hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(appIconSmall));}
    BOOL dark=TRUE;DwmSetWindowAttribute(m_hwnd,20,&dark,sizeof(dark));
    if(!CreateDevice()){SetError("Direct3D 11 initialization failed");DestroyWindow(m_hwnd);m_hwnd=nullptr;m_started=true;UnregisterClassW(wc.lpszClassName,m_module);return;}
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
    LoadUiFontFamilies(io);
    ImGui::StyleColorsDark();ImGui_ImplWin32_Init(m_hwnd);ImGui_ImplDX11_Init(m_device,m_context);ShowWindow(m_hwnd,SW_HIDE);UpdateWindow(m_hwnd);
    m_ready=true;m_started=true;
    MSG msg{};bool done=false;
    while(!done&&!m_stop.load()) {
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT){done=true;break;}
            if(msg.hwnd==nullptr&&msg.message==ToggleMessage) {
                if(m_visible.load())ClosePopup();else OpenPopup();
                continue;
            }
            if(msg.hwnd==nullptr&&msg.message==ReloadMessage){HelpyImGuiTheme::RequestReload();continue;}
            if(msg.hwnd==nullptr&&msg.message==StopMessage){done=true;break;}
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(done)break;
        if(m_visible.load()&&GetForegroundWindow()!=m_gameWindow)ClosePopup();
        if(m_visible.load()){ClipCursor(nullptr);GetClipCursor(&m_releasedClip);}
        if(m_visible.load()&&std::chrono::steady_clock::now()>=NextPlayerRefresh)QueuePlayerRefresh(true);if(m_visible.load()&&!IsIconic(m_hwnd))RenderFrame();else Sleep(16);
    }
    ClosePopup();ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();DestroyDevice();
    if(m_hwnd)DestroyWindow(m_hwnd);m_hwnd=nullptr;if(appIconBig)DestroyIcon(appIconBig);if(appIconSmall)DestroyIcon(appIconSmall);UnregisterClassW(wc.lpszClassName,m_module);m_ready=false;
}

void HelpyStandaloneWindow::RenderFrame() {
    static uint64_t uiGeneration=0;
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(generation!=uiGeneration){LazyCatalog::ResetForGeneration(generation);uiGeneration=generation;}
    LazyCatalog::PumpBackendSignal();
    const UINT dpi=GetDpiForWindow(m_hwnd);HelpyImGuiTheme::SetRuntimeDpi(dpi?static_cast<float>(dpi)/96.0f:1.0f);
    if(FontRescanRequested.exchange(false)) {
        auto& io=ImGui::GetIO();ImGui_ImplDX11_InvalidateDeviceObjects();io.Fonts->Clear();LoadUiFontFamilies(io);SelectUiFontFamily(HelpyImGuiTheme::Current.fontFamily);io.FontDefault=UiRegularFont;
    }
    ApplyUiModifierSnapshot(UiModifierSnapshot);ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();auto& frameIo=ImGui::GetIO();const auto& preTheme=HelpyImGuiTheme::Current;SelectUiFontFamily(preTheme.fontFamily);frameIo.FontDefault=UiRegularFont;frameIo.FontGlobalScale=std::clamp(preTheme.Scale()*preTheme.fontScale,0.50f,4.00f);ImGui::NewFrame();HelpyImGuiTheme::Poll();const auto& t=HelpyImGuiTheme::Current;SelectUiFontFamily(t.fontFamily);frameIo.FontGlobalScale=std::clamp(t.Scale()*t.fontScale,0.50f,4.00f);
    LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());
    ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);
    if(ImGui::Begin("##RuneSchemaHelpyStandalone",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        HelpyImGuiTheme::Scope skin(t);ImFont* workspaceFont=(t.textBold&&UiBoldFont)?UiBoldFont:UiRegularFont;
        if(workspaceFont)ImGui::PushFont(workspaceFont);RenderWorkspace(t);if(workspaceFont)ImGui::PopFont();
    }
    ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor();if(UiRegularFont)ImGui::PushFont(UiRegularFont);RenderToasts(t);if(UiRegularFont)ImGui::PopFont();
    ImGui::Render();const float clear[4]{t.window.x,t.window.y,t.window.z,t.window.w};m_context->OMSetRenderTargets(1,&m_target,nullptr);m_context->ClearRenderTargetView(m_target,clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());m_swapChain->Present(1,0);
}

bool HelpyStandaloneWindow::CreateDevice() {
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=m_hwnd;
    sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;UINT flags=0;D3D_FEATURE_LEVEL level{};
    constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    const auto hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,2,D3D11_SDK_VERSION,&sd,&m_swapChain,&m_device,&level,&m_context);
    if(FAILED(hr))return false;CreateTarget();return m_target!=nullptr;
}
void HelpyStandaloneWindow::DestroyDevice(){DestroyTarget();if(m_swapChain){m_swapChain->Release();m_swapChain=nullptr;}if(m_context){m_context->Release();m_context=nullptr;}if(m_device){m_device->Release();m_device=nullptr;}}
void HelpyStandaloneWindow::CreateTarget(){if(!m_swapChain)return;ID3D11Texture2D* backBuffer{};if(SUCCEEDED(m_swapChain->GetBuffer(0,IID_PPV_ARGS(&backBuffer)))){m_device->CreateRenderTargetView(backBuffer,nullptr,&m_target);backBuffer->Release();}}
void HelpyStandaloneWindow::DestroyTarget(){if(m_target){m_target->Release();m_target=nullptr;}}

LRESULT CALLBACK HelpyStandaloneWindow::WndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    auto* self=reinterpret_cast<HelpyStandaloneWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){const auto* cs=reinterpret_cast<CREATESTRUCTW*>(lParam);self=static_cast<HelpyStandaloneWindow*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(msg==ToggleMessage){if(self&&self->m_visible.load())self->ClosePopup();return 0;}
    if(msg==RoutedKeyMessage&&ImGui::GetCurrentContext()) {
        if(!self||!self->m_visible.load()||GetForegroundWindow()!=self->m_gameWindow)return 0;
        const UINT original=static_cast<UINT>(wParam>>32);const auto key=wParam&0xffff;const unsigned mods=static_cast<unsigned>((wParam>>16)&0xffff);
        UiModifierSnapshot=mods;ApplyUiModifierSnapshot(mods);
        ImGui_ImplWin32_WndProcHandler(hwnd,original,key,lParam);auto& io=ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl,(mods&1)!=0);io.AddKeyEvent(ImGuiMod_Shift,(mods&2)!=0);io.AddKeyEvent(ImGuiMod_Alt,(mods&4)!=0);io.AddKeyEvent(ImGuiMod_Super,(mods&(8|4096))!=0);return 0;
    }
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(hwnd,msg,wParam,lParam))return true;
    switch(msg) {
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(lParam);const UINT dpi=GetDpiForWindow(hwnd);const float scale=dpi?static_cast<float>(dpi)/96.0f:1.0f;
        info->ptMinTrackSize.x=static_cast<LONG>(HelpyImGuiTheme::Current.minWindowWidth*scale);
        info->ptMinTrackSize.y=static_cast<LONG>(HelpyImGuiTheme::Current.minWindowHeight*scale);return 0;
    }
    case WM_SIZE:if(self&&self->m_device&&wParam!=SIZE_MINIMIZED){self->DestroyTarget();self->m_swapChain->ResizeBuffers(0,LOWORD(lParam),HIWORD(lParam),DXGI_FORMAT_UNKNOWN,0);self->CreateTarget();}return 0;
    case WM_DPICHANGED:if(const auto* rect=reinterpret_cast<RECT*>(lParam))SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;
    case WM_CLOSE:if(self)self->ClosePopup();return 0;
    case WM_SYSCOMMAND:if((wParam&0xfff0)==SC_KEYMENU)return 0;break;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}
}
