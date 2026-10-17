#include "Generator/HelpyStandaloneWindow.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/MapPanel.h"
#include "Generator/AuthoringPanel.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#if defined(HELPY_PREVIEW)
#include "../../tests/AuthoringPreviewFixture.h"
#endif
#include "Generator/LazyCatalog.h"
#include "Generator/ItemGridPicker.h"
#include "Generator/ToolRequest.h"
#include "Core/HelpyPopupInput.h"
#include "Core/HelpyNavigation.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyBranding.h"
#include "Core/RuneSchemaUpdateInfo.h"
#include "Runtime/HelpyUpdates.h"
#include "Core/RsmkResources.h"
#include "Core/DesktopBayLayout.h"
#include "Generator/HelpyIconPacks.h"
#include "Generator/LiveLogPanel.h"
#include "Generator/DedicatedServerPanel.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/PluginRuntimeServices.h"
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <imgui.h>
#include <chrono>
#include <ctime>
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
#endif
HelpyPopupInput::HotkeyLatch Hotkey;
bool GameWindow(HWND hwnd) {
    if(HelpyEnvironment::Server())return hwnd&&hwnd==GetConsoleWindow();
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    wchar_t name[128]{};GetClassNameW(hwnd,name,128);
    return hwnd&&pid==GetCurrentProcessId()&&std::wstring_view(name)==L"UnrealWindow";
}
using Page=HelpyNavigation::Page;
Page CurrentPage=Page::Items;
bool SharedCartVisible=false;
ImVec2 MainSurfacePosition{},MainSurfaceSize{};
HelpyNavigation::SpawnSection CurrentSpawn=HelpyNavigation::SpawnSection::Resources;
HelpyNavigation::ToolSection CurrentTool=HelpyNavigation::ToolSection::Maintenance;
std::array<std::string,HelpyNavigation::Pages.size()> PageErrors;
ID3D11ShaderResourceView* BrandTexture=nullptr;
ID3D11ShaderResourceView* BrandAccentTexture=nullptr;
ID3D11ShaderResourceView* SteamTexture=nullptr;
ID3D11ShaderResourceView* EpicTexture=nullptr;
ID3D11ShaderResourceView* XboxTexture=nullptr;
enum class Notice {None,Credits,Legal,Info};
Notice RequestedNotice=Notice::None;
#if defined(HELPY_PREVIEW)
bool PreviewSelectSections=false;
HelpyNavigation::SpawnSection PreviewWantedSpawn=HelpyNavigation::SpawnSection::Resources;
HelpyNavigation::ToolSection PreviewWantedTool=HelpyNavigation::ToolSection::Maintenance;
std::filesystem::path PreviewCapture;
bool PreviewDismissNotice=false;
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
const char* PageTitle(Page page) {
    return HelpyNavigation::Title(page);
}
HelpyImGuiTheme::Icon PageIcon(Page page) {
    using I=HelpyImGuiTheme::Icon;switch(page){case Page::Items:return I::Item;case Page::Spawns:return I::Resource;
    case Page::Creation:case Page::Recipes:return I::Lab;case Page::Map:return I::Map;
    case Page::Authoring:return I::Authoring;case Page::Tools:return I::Tools;case Page::Settings:return I::Settings;case Page::LiveLog:return I::LiveLog;case Page::DedicatedServer:return I::Server;default:return I::Info;}
}
bool NavItem(Page page,const HelpyImGuiTheme::Theme& t) {
    const bool active=CurrentPage==page;ImGui::PushID(static_cast<int>(page));
    ImGui::PushStyleColor(ImGuiCol_Header,active?t.navigationActive:t.navigation);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,t.navigationHover);ImGui::PushStyleColor(ImGuiCol_HeaderActive,t.navigationActive);
    const float w=ImGui::GetContentRegionAvail().x;const bool compact=w<t.X(120.0f);
    const float textWidth=std::max(1.0f,w-t.X(46.0f));
    const auto textSize=ImGui::CalcTextSize(PageTitle(page),nullptr,false,textWidth);
    const float h=std::max(t.Y(t.navigationItemHeight),textSize.y+t.Y(18.0f));
    const bool clicked=ImGui::Selectable("##nav",active,ImGuiSelectableFlags_None,ImVec2(w,h));
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const float icon=t.Y(t.navigationIconSize);
    HelpyImGuiTheme::DrawIcon(PageIcon(page),ImVec2(compact?(a.x+b.x)*0.5f:a.x+t.X(18.0f),(a.y+b.y)*0.5f),icon,active?t.accentHover:t.icon);
    if(!compact)ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(a.x+t.X(36.0f),a.y+(h-textSize.y)*.5f),ImGui::GetColorU32(active?t.text:t.textDisabled),PageTitle(page),nullptr,textWidth);
    else if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",PageTitle(page));
    ImGui::PopStyleColor(3);ImGui::PopID();if(clicked)CurrentPage=page;return clicked;
}

ID3D11ShaderResourceView* LoadBrand(ID3D11Device* device,const char* asset="RuneSchema.png",bool monochrome=false,int logoLayer=0,bool darkForeground=false) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
       FAILED(factory->CreateDecoderFromFilename(HelpyPaths::Asset(asset).c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||
       FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    UINT width=0,height=0;if(FAILED(converter->GetSize(&width,&height))||!width||!height||width>2048||height>2048)return nullptr;
    std::vector<unsigned char> pixels(width*height*4);
    if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return nullptr;
    if(monochrome)for(size_t index=0;index<pixels.size();index+=4)HelpyBranding::MonochromeMask(pixels.data()+index,darkForeground);
    if(logoLayer)for(size_t index=0;index<pixels.size();index+=4)HelpyBranding::LogoLayer(pixels.data()+index,static_cast<unsigned>(index/4%width),width,logoLayer==2);
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};ComPtr<ID3D11Texture2D> texture;
    ID3D11ShaderResourceView* view=nullptr;
    if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)))device->CreateShaderResourceView(texture.Get(),nullptr,&view);
    return view;
}

void LoadBrandTextures(ID3D11Device* device) {
    BrandTexture=LoadBrand(device,"RuneSchema.png",false,1);
    BrandAccentTexture=LoadBrand(device,"RuneSchema.png",false,2);
    if(!BrandTexture||!BrandAccentTexture){
        if(BrandTexture){BrandTexture->Release();BrandTexture=nullptr;}
        if(BrandAccentTexture){BrandAccentTexture->Release();BrandAccentTexture=nullptr;}
        BrandTexture=LoadBrand(device);
    }
    SteamTexture=LoadBrand(device,"steam.png",true);
    // Render the supplied black-on-white mark as an accent-tinted upload
    // mask, not a white rectangle. The original PNG is kept byte-for-byte.
    EpicTexture=LoadBrand(device,"epic.png",true,0,true);
    XboxTexture=LoadBrand(device,"xbox.png",true);
    ItemGridPicker::ModLogo=ImTextureRef(BrandTexture);ItemGridPicker::ModLogoAccent=ImTextureRef(BrandAccentTexture);
}
void ReleaseBrandTextures() {
    ItemGridPicker::ModLogo={};ItemGridPicker::ModLogoAccent={};
    for(auto** view:{&BrandTexture,&BrandAccentTexture,&SteamTexture,&EpicTexture,&XboxTexture})if(*view){(*view)->Release();*view=nullptr;}
}
void RenderNotices(const HelpyImGuiTheme::Theme& t) {
    const auto title=[](Notice notice){return notice==Notice::Credits?"Credits":notice==Notice::Legal?"Legal notice":"Info";};
    if(RequestedNotice!=Notice::None){ImGui::OpenPopup(title(RequestedNotice));RequestedNotice=Notice::None;}
    const auto display=ImGui::GetIO().DisplaySize;
    for(const auto notice:{Notice::Credits,Notice::Legal,Notice::Info}) {
        ImGui::SetNextWindowSize(ImVec2(std::min(t.X(640),display.x-t.X(32)),std::min(t.Y(notice==Notice::Credits?340:530),display.y-t.Y(32))),ImGuiCond_Appearing);
        bool open=true;
        if(ImGui::BeginPopupModal(title(notice),&open,ImGuiWindowFlags_NoSavedSettings)) {
            if(ImGui::BeginChild("##notice-body",ImVec2(0,-ImGui::GetFrameHeightWithSpacing()-t.Y(8)))) {
                if(notice==Notice::Info){
                    ImGui::TextUnformatted(HelpyEnvironment::Label());ImGui::Separator();
                    ImGui::TextWrapped("RuneSchema %s",HelpyEnvironment::RuneSchemaVersion.empty()?"Not reported":HelpyEnvironment::RuneSchemaVersion.c_str());
                    ImGui::TextWrapped("%s %s",HelpyBranding::Name,HelpyBranding::Version);
                    ImGui::TextWrapped("Game build %s",HelpyBranding::BuildLabel(HelpyEnvironment::GameVersion).c_str());
                    if(!HelpyEnvironment::GameVersion.empty())ImGui::TextWrapped("Executable: %s",HelpyEnvironment::GameVersion.c_str());
                    if(HelpyEnvironment::Steam.contains("BuildId"))ImGui::TextWrapped("Installed Steam build ID: %s",HelpyEnvironment::Steam["BuildId"].dump().c_str());
                    ImGui::TextWrapped("UE4SS %s",HelpyEnvironment::UE4SS.value("Build",std::string("Not reported")).c_str());
                    ImGui::TextWrapped("%s",HelpyEnvironment::UE4SS.value("Capabilities",std::string{}).c_str());
                    ImGui::TextWrapped("%s",HelpyEnvironment::UE4SS.value("Source",std::string{}).c_str());
                    ImGui::Separator();
                    const auto& news=HelpyUpdates::Current();
                    if(news.GameUpdateDate>0){
                        const auto stamp=static_cast<time_t>(news.GameUpdateDate);tm utc{};char date[64]{};
                        if(gmtime_s(&utc,&stamp)==0)std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M UTC",&utc);
                        ImGui::TextWrapped("Latest Steam update announcement: %s",news.GameUpdateTitle.c_str());
                        ImGui::TextWrapped("Published %s",date);
                        ImGui::TextDisabled("Announcement date; not proof of the installed build's release date.");
                        if(HelpyImGuiTheme::SoftButton("Read official update")){
                            const std::wstring url(news.GameUpdateUrl.begin(),news.GameUpdateUrl.end());ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                        }
                    }else ImGui::TextWrapped("Steam update date: %s",news.GameUpdateError.empty()?"Not checked yet":news.GameUpdateError.c_str());
                    ImGui::TextWrapped("Manual checks are available in Settings > Updates & runtime. Steam announcements may not match Game Pass or a beta branch.");
                }else ImGui::TextWrapped("%s",notice==Notice::Credits?HelpyBranding::Credits:HelpyBranding::Legal);
            }
            ImGui::EndChild();
            if(HelpyImGuiTheme::SoftButton("Close"))ImGui::CloseCurrentPopup();
#if defined(HELPY_PREVIEW)
            if(PreviewDismissNotice){ImGui::CloseCurrentPopup();PreviewDismissNotice=false;}
#endif
            ImGui::EndPopup();
        }
    }
}
void RenderInformationBar(const HelpyImGuiTheme::Theme& t) {
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    ImGui::AlignTextToFramePadding();ImGui::TextDisabled("%s %s",HelpyBranding::Name,HelpyBranding::Version);
    const float links=ImGui::CalcTextSize("InfoCreditsLegal").x+ImGui::GetStyle().FramePadding.x*6+ImGui::GetStyle().ItemSpacing.x*2;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX()+t.X(110),ImGui::GetWindowWidth()-links-ImGui::GetStyle().WindowPadding.x));
    if(HelpyImGuiTheme::SoftButton("Info"))RequestedNotice=Notice::Info;
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::InfoButton=HelpyPreviewHooks::LastRect();
#endif
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Credits"))RequestedNotice=Notice::Credits;
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Legal"))RequestedNotice=Notice::Legal;
    ImGui::PopStyleColor();ImGui::Separator();RenderNotices(t);
#if defined(HELPY_PREVIEW)
    const auto bar=ImGui::GetWindowPos();HelpyPreviewHooks::InformationBar={bar.x,bar.y,bar.x+ImGui::GetWindowWidth(),ImGui::GetCursorScreenPos().y};
#endif
}
bool RuneSchemaUpdateAvailable(){const auto& state=HelpyUpdates::Current();return
    RuneSchemaUpdateInfo::Compare(HelpyEnvironment::RuneSchemaVersion,state.MainVersion)==RuneSchemaUpdateInfo::Comparison::Newer
    ||RuneSchemaUpdateInfo::Compare(HelpyEnvironment::RuneSchemaVersion,state.ReleaseVersion)==RuneSchemaUpdateInfo::Comparison::Newer;}
float RuntimeFooterHeight(const HelpyImGuiTheme::Theme& t,float width) {
    return t.Y(68)+ImGui::GetTextLineHeight();
}
void RenderRuntimeFooter(const HelpyImGuiTheme::Theme& t) {
    ImGui::Separator();ImGui::Dummy(ImVec2(1,t.Y(4)));
    const auto base=ImGui::GetCursorScreenPos();const float size=std::min(t.Y(28),t.X(28));
    const float width=ImGui::GetContentRegionAvail().x;
    const auto label=ImGui::CalcTextSize(HelpyEnvironment::Label());
    const bool shared=HelpyEnvironment::SharedStoreBadge();
    const float marksWidth=shared?size*1.8f+t.X(3):size;
    const ImVec2 start(base.x+std::max(0.0f,(width-marksWidth-t.X(8)-label.x)*.5f),base.y);
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
    ImGui::SetCursorScreenPos(ImVec2(start.x+marksWidth+t.X(8),start.y+(size-ImGui::GetTextLineHeight())*.5f));
    const bool ready=SpawnToolRequests::IsAvailable();
    ImGui::TextUnformatted(HelpyEnvironment::Label());
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",HelpyEnvironment::Label(),ready?"Runtime ready":"Waiting for world");
    ImGui::SetCursorScreenPos(ImVec2(base.x+std::max(0.0f,(width-ImGui::CalcTextSize("F2  close / reopen").x)*.5f),start.y+size+t.Y(8)));
    ImGui::PushStyleColor(ImGuiCol_Text,t.textDisabled);
    ImGui::TextUnformatted("F2  close / reopen");ImGui::PopStyleColor();
    const auto status=ready?t.success:t.textDisabled;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(start.x+size-t.X(2),start.y+size-t.Y(2)),t.X(3),ImGui::GetColorU32(status));
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::RuntimeFooter={base.x,base.y,base.x+width,ImGui::GetCursorScreenPos().y};
#endif
}

void ApplyCaptionTheme(HWND window,const HelpyImGuiTheme::Theme& theme) {
    const auto rgb=[](const ImVec4& color){return RGB(static_cast<BYTE>(color.x*255),static_cast<BYTE>(color.y*255),static_cast<BYTE>(color.z*255));};
    const BOOL dark=(theme.window.x+theme.window.y+theme.window.z)<1.5f;
    const COLORREF caption=rgb(theme.navigation),text=rgb(theme.text),border=rgb(theme.border);
    static HWND previousWindow=nullptr;static COLORREF previousCaption=0,previousText=0,previousBorder=0;static BYTE previousOpacity=0;
    const auto opacity=static_cast<BYTE>(std::clamp(theme.windowOpacity,0.60f,1.0f)*255);
    if(previousWindow==window&&previousCaption==caption&&previousText==text&&previousBorder==border&&previousOpacity==opacity)return;
    SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_LAYERED);
    SetLayeredWindowAttributes(window,0,opacity,LWA_ALPHA);previousOpacity=opacity;
    previousWindow=window;previousCaption=caption;previousText=text;previousBorder=border;
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

void RenderIconCacheSettings(bool advanced) {
    if(!ImGui::CollapsingHeader("Icon cache"))return;
    SpawnToolsPanel::RefreshSnapshot(0);
    CatalogProgressView::Render(SpawnToolsPanel::UiSnapshot);
    static bool acknowledgeFullScan=false;
    ImGui::Checkbox("Allow full scan to load additional candidate assets",&acknowledgeFullScan);
    ImGui::BeginDisabled(!acknowledgeFullScan||HelpyIconPacks::Busy()||HelpyIconPacks::Building()||SpawnToolRequests::IsWaiting()||!SpawnToolRequests::IsAvailable()||SpawnToolsPanel::UiSnapshot.value("_CatalogIndexing",false));
    if(ImGui::Button("Full scan game catalogue")){if(SpawnToolRequests::TrySubmit({{"Action","IndexQuickCatalog"},{"_Source","HelpySettings"}}))acknowledgeFullScan=false;}
    ImGui::EndDisabled();
    ImGui::TextWrapped("Full scan discovers catalogue entries, not a download of all textures. Icons are cached on demand; dump/export includes those already collected.");
    bool persistent=HelpyRuntimePreferences::IconCacheLimitMB.load()>0;
    ImGui::BeginDisabled(HelpyIconPacks::Building()||HelpyIconPacks::Busy());
    if(ImGui::Checkbox("Keep icons between launches",&persistent)){HelpyRuntimePreferences::IconCacheLimitMB.store(persistent?200:0);HelpyImGuiTheme::SaveCurrent();}
    if(advanced&&persistent){int megabytes=HelpyRuntimePreferences::IconCacheLimitMB.load();
        if(ImGui::InputInt("Cache limit (MB)",&megabytes,16,128)){HelpyRuntimePreferences::IconCacheLimitMB.store(std::clamp(megabytes,1,16384));HelpyImGuiTheme::SaveCurrent();}}
    ImGui::EndDisabled();
    static ItemIconCache::DiskStats stats{};static auto next=std::chrono::steady_clock::time_point{};const auto now=std::chrono::steady_clock::now();
    if(now>=next){stats=ItemIconCache::CurrentDiskStats();next=now+std::chrono::seconds(3);}
    ImGui::Text("%.2f MB / %zu cached icons",static_cast<double>(stats.Bytes)/(1024.0*1024.0),stats.Files);
    ImGui::TextWrapped("Icons are cached as you browse, grouped by registered content source. Changed sources use a new cache; cached files never add items or register missing mods.");
    HelpyIconPacks::Render(SpawnToolsPanel::UiSnapshot.value("_CatalogIndexing",false));
    ImGui::BeginDisabled(HelpyIconPacks::Busy()||HelpyIconPacks::Building());
    if(HelpyImGuiTheme::SoftButton("Clean Icon Cache")){const bool ok=ItemIconCache::CleanDiskCache();stats=ItemIconCache::CurrentDiskStats();next=now+std::chrono::seconds(3);PushToast(ok?"Icon cache cleaned. Visible icons can be cached again.":"Some cache files could not be removed.",3.0);}
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
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&HelpyUpdates::CooldownSeconds()>0)ImGui::SetTooltip("Next manual check in %d seconds",HelpyUpdates::CooldownSeconds());
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
        if(ImGui::BeginPopupModal("Delete color preset?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
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
        RenderIconCacheSettings(false);
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

    RenderIconCacheSettings(true);

    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::SeparatorText("Theme file & reset");
    if(HelpyImGuiTheme::SoftButton("Reload ui.jsonc (F5)")){HelpyImGuiTheme::RequestReload();pendingSave=false;}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Open ui.jsonc")){const auto path=HelpyImGuiTheme::FilePath();ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,path.parent_path().c_str(),SW_SHOWNORMAL);}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Reset to RuneSchema defaults"))ImGui::OpenPopup("Reset RuneSchema UI?");
    if(ImGui::BeginPopupModal("Reset RuneSchema UI?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {ImGui::TextWrapped("Restore all RSMK UI colors, typography, scaling, spacing, icon/card sizing, and icon cache limit to the shipped RuneSchema defaults?");if(HelpyImGuiTheme::SoftButton("Reset defaults")){HelpyImGuiTheme::ResetDefaults();ItemGridPicker::ResetViewPreference();SidebarWidthLogical=-1.0f;pendingSave=false;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
    ImGui::TextWrapped("%s",HelpyImGuiTheme::Status.c_str());if(pendingSave&&!ImGui::IsAnyItemActive()){HelpyImGuiTheme::SaveCurrent();pendingSave=false;}

    ImGui::Dummy(ImVec2(1,t.Y(12.0f)));ImGui::SeparatorText("Hotkeys");ImGui::Text("F2");ImGui::SameLine(t.X(70.0f));ImGui::TextUnformatted("Toggle RSMK");ImGui::Text("F5");ImGui::SameLine(t.X(70.0f));ImGui::TextUnformatted("Reload live skin");
}
void RenderWorkspace(const HelpyImGuiTheme::Theme& t) {
    try{SpawnToolsPanel::RefreshSnapshot(SpawnToolsPanel::UiCategory==-99?0:SpawnToolsPanel::UiCategory);}
    catch(const std::exception& error){
        PageErrors[static_cast<size_t>(CurrentPage)]=error.what();
        SpawnToolsPanel::UiSnapshot.erase("Players");SpawnToolsPanel::ReconcileCartRecipients(SpawnToolsPanel::UiSnapshot);
    }catch(...){
        PageErrors[static_cast<size_t>(CurrentPage)]="The player snapshot could not be refreshed.";
        SpawnToolsPanel::UiSnapshot.erase("Players");SpawnToolsPanel::ReconcileCartRecipients(SpawnToolsPanel::UiSnapshot);
    }
    SpawnToolsPanel::PollCartDelivery();ItemDetails::Poll();
    ItemGridPicker::ItemAction=[](const ItemGridPicker::Result& action){
        SpawnToolsPanel::RefreshSnapshot(SpawnToolsPanel::UiCategory);
        if(SpawnToolsPanel::SelectedPlayer.empty()||!SpawnToolsPanel::CurrentCartPlayer(SpawnToolsPanel::UiSnapshot,SpawnToolsPanel::SelectedPlayer)){ItemGridPicker::CartStatus="Choose a current player before giving items.";SharedCartVisible=true;return;}
        if(SpawnToolRequests::IsWaiting()){ItemGridPicker::CartStatus="Wait for the current operation before giving items.";return;}
        auto items=action.ActionItems;if(items.empty())items=nlohmann::json::array({{{"Item",action.ActionPath},{"Count",action.ActionCount}}});
        if(!SpawnToolRequests::TrySubmit({{"Action","GiveItems"},{"_Source","HelpySharedItemBay"},{"Player",SpawnToolsPanel::SelectedPlayer},{"Items",std::move(items)}}))ItemGridPicker::CartStatus="The runtime did not accept the item request.";
    };
    if(SidebarWidthLogical<0.0f)SidebarWidthLogical=t.navigationWidth;
    const float totalLogical=ImGui::GetContentRegionAvail().x/std::max(0.01f,t.Scale());
    const float minimumContent=360.0f;
    const float dynamicMax=std::max(t.navigationMinWidth,totalLogical-minimumContent-t.splitterWidth-t.contentGutter);
    SidebarWidthLogical=std::clamp(SidebarWidthLogical,t.navigationMinWidth,std::min(t.navigationMaxWidth,dynamicMax));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,t.navigation);
    ImGui::BeginChild("##helpy-nav",ImVec2(t.X(SidebarWidthLogical),0),false);
    ImGui::Dummy(ImVec2(1,t.Y(10.0f)));
    if(BrandTexture){
        const float size=std::min(t.X(62.0f),ImGui::GetContentRegionAvail().x);ImGui::SetCursorPosX((ImGui::GetWindowWidth()-size)*0.5f);
        const auto at=ImGui::GetCursorScreenPos();ImGui::Image(ImTextureRef(BrandTexture),ImVec2(size,size));
        if(BrandAccentTexture)ImGui::GetWindowDrawList()->AddImage(ImTextureRef(BrandAccentTexture),at,{at.x+size,at.y+size},{0,0},{1,1},ImGui::GetColorU32(t.accent));
    }
    const auto centered=[](const char* text){ImGui::SetCursorPosX(std::max(0.0f,(ImGui::GetWindowWidth()-ImGui::CalcTextSize(text).x)*0.5f));ImGui::TextUnformatted(text);};
    centered("RuneSchema");
    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::Separator();ImGui::Dummy(ImVec2(1,t.Y(5.0f)));
    const float footer=RuntimeFooterHeight(t,ImGui::GetContentRegionAvail().x);
    if(ImGui::BeginChild("##helpy-destinations",ImVec2(0,std::max(t.Y(40),ImGui::GetContentRegionAvail().y-footer)),false))
        for(auto page:HelpyNavigation::Pages)if(page!=Page::DedicatedServer||HelpyEnvironment::Server())NavItem(page,t);
    ImGui::EndChild();RenderRuntimeFooter(t);
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
    RenderInformationBar(t);
    ImGui::Dummy(ImVec2(1,t.Y(8.0f)));ImGui::PushFont(nullptr,ImGui::GetFontSize()*1.65f);ImGui::TextUnformatted(PageTitle(CurrentPage));ImGui::PopFont();
    const bool cartAllowed=CurrentPage==Page::Creation||CurrentPage==Page::Recipes||CurrentPage==Page::Spawns||CurrentPage==Page::Map;
    if(cartAllowed){
        const auto caption="Cart ("+std::to_string(ItemGridPicker::SharedCart.Entries().size())+")";
        if(ImGui::GetContentRegionAvail().x>t.X(130))ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton(caption.c_str()))SharedCartVisible=!SharedCartVisible;
    }
    ImGui::PushStyleColor(ImGuiCol_Text,t.textDisabled);ImGui::TextWrapped("%s",HelpyNavigation::Description(CurrentPage));ImGui::PopStyleColor();
    ImGui::Separator();
    const std::string scrollId="##helpy-scroll-"+std::to_string(static_cast<int>(CurrentPage));
    const bool hasBayDock=CurrentPage==Page::Creation||CurrentPage==Page::Recipes||(CurrentPage==Page::Spawns&&SpawnToolsPanel::ExtraLootBay);
    const bool dockBay=hasBayDock&&!ItemGridPicker::DetachedBay;
    ItemGridPicker::DockedBay=hasBayDock;
    const float bayHeight=ItemBayLayout::PaneHeight(t.Y(t.dockedBayHeight),std::max(1.0f,ImGui::GetContentRegionAvail().y-t.Y(150)),t.Y(160));
    const float editorHeight=dockBay?std::max(t.Y(110),ImGui::GetContentRegionAvail().y-bayHeight-ImGui::GetFrameHeightWithSpacing()):0;
    const bool scrollVisible=HelpyImGuiTheme::BeginScroll(scrollId.c_str(),ImVec2(0,editorHeight),false);
    if(scrollVisible) {
        ImGui::Dummy(ImVec2(1,t.Y(4.0f)));
        auto& pageError=PageErrors[static_cast<size_t>(CurrentPage)];
        if(!pageError.empty()) {
            ImGui::TextColored(t.error,"This page could not finish loading.");ImGui::TextWrapped("%s",pageError.c_str());
            if(HelpyImGuiTheme::SoftButton("Retry page"))pageError.clear();
        } else {
            ImGuiErrorRecoveryState recovery;ImGui::ErrorRecoveryStoreState(&recovery);
            try {
                switch(CurrentPage) {
                case Page::Items:SpawnToolsPanel::Render(SpawnToolsPanel::View::Items);break;
                case Page::Creation:SpawnToolsPanel::Render(SpawnToolsPanel::View::ItemLab);break;
                case Page::Recipes:SpawnToolsPanel::Render(SpawnToolsPanel::View::Recipes);break;
                case Page::Map:MapPanel::Render();break;
                case Page::LiveLog:LiveLogPanel::Render();break;
                case Page::DedicatedServer:DedicatedServerPanel::Render();break;
                case Page::Settings:
                    if(ImGui::BeginTabBar("Configuration sections")){
                        if(ImGui::BeginTabItem("RuneSchema")){RuneSchemaSettingsPanel::Render();ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("Appearance")){RenderSettings(t);ImGui::EndTabItem();}
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
                    if(ImGui::BeginTabBar("Spawn sections")) {
                        if(ImGui::BeginTabItem("Resources",nullptr,SectionFlags(selectedSection==HelpyNavigation::SpawnSection::Resources))){CurrentSpawn=HelpyNavigation::SpawnSection::Resources;SpawnToolsPanel::ForcedActorMode=-1;SpawnToolsPanel::Render(SpawnToolsPanel::View::Resources);ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("NPCs",nullptr,SectionFlags(selectedSection==HelpyNavigation::SpawnSection::Npcs))){CurrentSpawn=HelpyNavigation::SpawnSection::Npcs;SpawnToolsPanel::ForcedActorMode=0;SpawnToolsPanel::Render(SpawnToolsPanel::View::Ai);ImGui::EndTabItem();}
                        if(ImGui::BeginTabItem("AI / Enemies",nullptr,SectionFlags(selectedSection==HelpyNavigation::SpawnSection::Ai))){CurrentSpawn=HelpyNavigation::SpawnSection::Ai;SpawnToolsPanel::ForcedActorMode=1;SpawnToolsPanel::Render(SpawnToolsPanel::View::Ai);ImGui::EndTabItem();}
                        ImGui::EndTabBar();
                    }}break;
                case Page::Tools:
                    {auto selectedSection=CurrentTool;
#if defined(HELPY_PREVIEW)
                    if(PreviewSelectSections)selectedSection=PreviewWantedTool;
#endif
                    if(ImGui::BeginTabBar("Tools sections")) {
                        if(ImGui::BeginTabItem("Maintenance",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::Maintenance))) {
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
                        if(ImGui::BeginTabItem("Session cleanup",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::Cleanup))){CurrentTool=HelpyNavigation::ToolSection::Cleanup;SpawnToolsPanel::RenderSessionCleanup();ImGui::EndTabItem();}
                        if(t.advancedMode&&ImGui::BeginTabItem("Building tools",nullptr,SectionFlags(selectedSection==HelpyNavigation::ToolSection::Buildings))){CurrentTool=HelpyNavigation::ToolSection::Buildings;SpawnToolsPanel::Render(SpawnToolsPanel::View::Buildings);ImGui::EndTabItem();}
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
        ImGui::SetNextWindowSize(ImVec2(t.X(320),t.Y(440)),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(MainSurfacePosition.x+std::max(0.0f,MainSurfaceSize.x-t.X(340)),MainSurfacePosition.y+t.Y(95)),ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Your cart",&SharedCartVisible,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)){
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
    if(Hotkey.Poll(down)&&HelpyPopupInput::CanToggle(GameWindow(foreground),foreground==m_publishedWindow.load()||(foreground&&foreground==m_publishedBayWindow.load())))Toggle();
}
void HelpyStandaloneWindow::ReloadTheme(){if(m_threadId)PostThreadMessageW(m_threadId,ReloadMessage,0,0);}
void HelpyStandaloneWindow::Shutdown(){if(!m_thread.joinable())return;m_stop=true;HelpyIconPacks::Progress.Cancel=true;if(m_threadId)PostThreadMessageW(m_threadId,StopMessage,0,0);m_thread.join();HelpyIconPacks::Shutdown();LiveLogPanel::Shutdown();m_threadId=0;m_ready=false;}
std::string HelpyStandaloneWindow::Error() const{std::scoped_lock lock(m_errorMutex);return m_error;}
void HelpyStandaloneWindow::SetError(std::string value){std::scoped_lock lock(m_errorMutex);m_error=std::move(value);}
bool HelpyStandaloneWindow::OpenPopup() {
    const auto game=GetForegroundWindow();if(!HelpyEnvironment::Server()&&!GameWindow(game))return false;
    m_gameWindow=HelpyEnvironment::Server()?nullptr:game;
    ShowWindow(m_hwnd,IsIconic(m_hwnd)?SW_RESTORE:SW_SHOW);m_visible=true;
    if(m_bayWindow&&ItemGridPicker::DetachedBay)ShowWindow(m_bayWindow,SW_SHOWNOACTIVATE);
    SetForegroundWindow(m_hwnd);SetFocus(m_hwnd);
    LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());QueuePlayerRefresh(true);return true;
}
void HelpyStandaloneWindow::ClosePopup(){
    // Closing Helpy must not steal focus back from an unrelated application.
    const bool returnToGame=HelpyPopupInput::ReturnToGame(GetForegroundWindow()==m_hwnd||(m_bayWindow&&GetForegroundWindow()==m_bayWindow),GameWindow(m_gameWindow));
    m_visible=false;ShowWindow(m_hwnd,HelpyEnvironment::Server()?SW_MINIMIZE:SW_HIDE);
    if(m_bayWindow)ShowWindow(m_bayWindow,SW_HIDE);
    if(GetCapture()==m_hwnd||(m_bayWindow&&GetCapture()==m_bayWindow)){ReleaseCapture();if(ImGui::GetCurrentContext())ImGui::GetIO().AddMouseButtonEvent(0,false);}
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
        if(ImGui::GetCurrentContext()){
            if(ImGui::GetIO().BackendRendererUserData)ImGui_ImplDX11_Shutdown();
            if(ImGui::GetIO().BackendPlatformUserData)ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        DestroyItemBay();ReleaseBrandTextures();DestroyDevice();
        if(m_hwnd)DestroyWindow(m_hwnd);m_hwnd=nullptr;
        UnregisterClassW(wc.lpszClassName,m_module);m_ready=false;
        if(SUCCEEDED(com))CoUninitialize();
    };
    std::unique_ptr<void,decltype(cleanup)> resources(reinterpret_cast<void*>(1),cleanup);
    RegisterClassExW(&wc);
    m_hwnd=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,HelpyBranding::WindowTitle,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,900,nullptr,nullptr,m_module,this);
    if(!m_hwnd){SetError("CreateWindowExW failed");m_started=true;return;}
    DragAcceptFiles(m_hwnd,TRUE);
    SendMessageW(m_hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(wc.hIcon));
    SendMessageW(m_hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(wc.hIconSm));
    ApplyCaptionTheme(m_hwnd,HelpyImGuiTheme::Current);
    if(!CreateDevice()){SetError("Direct3D 11 initialization failed");m_started=true;return;}
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
    HelpyImGuiTheme::Poll();
    LoadUiFontFamilies(io);
    LoadBrandTextures(m_device);
    ImGui::StyleColorsDark();ImGui_ImplWin32_Init(m_hwnd);ImGui_ImplDX11_Init(m_device,m_context);ShowWindow(m_hwnd,SW_HIDE);UpdateWindow(m_hwnd);
    m_publishedWindow=m_hwnd;
    // A dedicated host has no UnrealWindow/F2 entry point. Expose a normal
    // taskbar window without taking focus from its launcher or console.
    if(HelpyEnvironment::Server()){
        CurrentPage=Page::Authoring;
        ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);m_visible=true;
        LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());QueuePlayerRefresh(true);
    }
    m_ready=true;m_started=true;
    MSG msg{};bool done=false,themeKeyDown=false;
    while(!done&&!m_stop.load()) {
        // Hotkeys belong to Helpy's window thread. No engine tick callback,
        // viewport input hook, or UE4SS C++ ABI is needed by this plugin.
        PollHotkey((GetAsyncKeyState(VK_F2)&0x8000)!=0);
        const bool themeDown=(GetAsyncKeyState(VK_F5)&0x8000)!=0;
        const HWND foreground=GetForegroundWindow();
        if(themeDown&&!themeKeyDown&&(foreground==m_hwnd||(m_bayWindow&&foreground==m_bayWindow)||GameWindow(foreground)))HelpyImGuiTheme::RequestReload();
        themeKeyDown=themeDown;
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT){done=true;break;}
            if(msg.hwnd==nullptr&&msg.message==ToggleMessage) {
                if(m_visible.load())ClosePopup();else OpenPopup();
                continue;
            }
            if(msg.hwnd==nullptr&&msg.message==ReloadMessage){HelpyImGuiTheme::RequestReload();continue;}
            if(msg.hwnd==nullptr&&msg.message==StopMessage){done=true;break;}
            if(msg.hwnd==m_hwnd&&msg.wParam==VK_F2&&(msg.message==WM_KEYDOWN||msg.message==WM_KEYUP)) {
                if(msg.message==WM_KEYDOWN){if(Hotkey.Press())ClosePopup();}else Hotkey.Release();
                continue;
            }
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(done)break;
        if(m_visible.load()&&std::chrono::steady_clock::now()>=NextPlayerRefresh)QueuePlayerRefresh(true);if(m_visible.load()&&!IsIconic(m_hwnd))RenderFrame();else Sleep(16);
    }
}

#if defined(HELPY_PREVIEW)
namespace {
void CapturePreview(ID3D11Device* device,ID3D11DeviceContext* context,IDXGISwapChain* swap,const std::filesystem::path& path) {
    using Microsoft::WRL::ComPtr;
    const auto check=[](HRESULT result){if(FAILED(result))throw std::runtime_error("Preview render capture failed");};
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
    nlohmann::json snapshot={{"Status","Sample data preview - game actions disabled."},{"Items",items},{"Definitions",definitions},{"Players",nlohmann::json::array({{{"Path","preview-player"},{"Name","Preview player"}}})},{"CatalogReady",true},{"CatalogStatus","Sample catalog for layout review"}};
    snapshot["Players"][0]["RoleStatus"]=PlayerStatus::Describe(14,{1,2,4,8,128},true);
    std::deque<nlohmann::json> queries;uint64_t revision=10;
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
            detailReceipts[id]={{"MapSnapshot",{{"BridgeVersion",1},{"CanBuildAndTeleport",true},{"Projection",projection.ToJson()},{"Image",{{"File",mapFixture.string()},{"Status","Ready"}}},
                {"Markers",nlohmann::json::array({{{"Name","Test lodestone"},{"Path","preview-lodestone"},{"Kind","Lodestone"},{"Managed",false},{"Location",{230000,125000,100}}},{{"Name","Placed iron node"},{"Path","preview-node"},{"Kind","Resource"},{"Managed",true},{"Location",{155000,125000,100}}}})},
                {"Players",nlohmann::json::array({{{"Name","Preview player"},{"Path","preview-player"},{"Location",{198000,91450,100}}}})}}}};return true;
        }
        if(action=="InspectClone"&&!request.value("InspectDetails",false)){
            detailReceipts[id]={{"CloneSource",{{"Source",request.at("Source")},{"SoftDeleteField","bSoftDelete"},{"Fields",nlohmann::json::array({{{"Name","PowerLevel"},{"NativeName","PowerLevel"},{"Editable",true},{"HasValue",true},{"Value",5}},{{"Name","@row/WeaponStats/DamageMultiplier"},{"NativeName","DamageMultiplier"},{"Editable",true},{"HasValue",true},{"Value",1.2}}})}}}};return true;
        }
        if(action=="PrepareClone"){
            authoringRequest=request;nlohmann::json prepared={{"State","Prepared"},{"InternalName","RS_v5_"+request.at("PersistenceID").get<std::string>()}};
            prepared["Documents"]["assets"]["/Preview/Clones/Result.Result"]={{"$Clone",request.at("Source")},{"PersistenceID",request.at("PersistenceID")}};
            prepared["Documents"]["raw"]={{"/Preview/Stats.Table",{{request.at("PersistenceID").get<std::string>(),{{"DamageMultiplier",3.5}}}}}};
            detailReceipts[id]={{"CloneResult",prepared},{"Status","Synthetic export prepared; no game actions called."}};return true;
        }
        if(action=="MapProbe"){++mapProbeCount;const auto& point=request.at("TargetLocation");detailReceipts[id]={{"MapPoint",{{"GroundAvailable",true},{"Location",{point["X"],point["Y"],100}}}}};return true;}
        if(action=="MapBuildings"){detailReceipts[id]={{"MapBuildings",{{"Rows",nlohmann::json::array({
            {{"Path","/Preview/Buildings/TeleportStone.TeleportStone"},{"Name","Teleport stone"},{"Type","Building"},{"CategoryId",0},{"Category","Utility"},{"Group","Travel"},{"Icon",items[0]["Icon"]}},
            {{"Path","/Preview/Buildings/IronChest.IronChest"},{"Name","Iron Chest"},{"Type","Building"},{"CategoryId",1},{"Category","Furniture"},{"Group","Storage"},{"Icon",items[0]["Icon"]}}})},
            {"Categories",nlohmann::json::array({{{"Id",0},{"Name","Utility"}},{{"Id",1},{"Name","Furniture"}}})},{"NextOffset",2},{"HasMore",false}}}};return true;}
        if((action=="Spawn"||action=="SpawnBuilding"||action=="MapTeleport")&&request.value("_Source",std::string{})=="HelpyMap"){
            mapPlacement=request;detailReceipts[id]={{"Status","Synthetic map action checked; no game action called."}};
            if(action=="SpawnBuilding")detailReceipts[id]["SpawnResult"]={{"Active",true},{"Permanent",request.value("Permanent",false)},{"Building",request.at("Building")},{"Location",{request["TargetLocation"]["X"],request["TargetLocation"]["Y"],100}}};
            return true;}
        if(action=="RecipeWorkspaceCatalog"){
            detailReceipts[id]={{"RecipeWorkspaceCatalog",{{"Stations",nlohmann::json::array({{{"Id","preview-station"},{"DisplayName","Crafting station"},{"Icon",QuickDecorations::CraftingStationIcon},{"Path","/Preview/Station"}}})},{"Recipes",nlohmann::json::array()}}}};return true;
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
        if(request.value("Action",std::string{})!="SettingsRead")return false;
        reply={{"Ok",true},{"ExpectedContents","preview"},{"Path","Preview only / settings/settings.ini"},
            {"Document",{{"advancedLogging",false},{"authoringTools",true},{"persistence",{{"allowCorruptCharacterEntry",false},{"quests",true}}}}},
            {"Editable",{{"/advancedLogging",false},{"/authoringTools",true},{"/persistence/allowCorruptCharacterEntry",false},{"/persistence/quests",true}}},
            {"BypassStatus","Disabled for this preview session"}};return true;
    };
    SpawnToolsPanel::SyncCartWorld();SpawnToolsPanel::SelectedPlayer="preview-player";
    int result=0;
    try {
        if(capture){
            if(!BrandTexture||!SteamTexture||!EpicTexture||!XboxTexture)throw std::runtime_error("Packaged application or platform logo failed to load");
            std::filesystem::create_directories(output);std::error_code ignored;
            std::filesystem::remove(output/"render-error.txt",ignored);std::filesystem::remove(output/"render-result.txt",ignored);PreviewSelectSections=true;
            LiveLogPanel::Preview=true;LiveLogPanel::History.Clear();
            LiveLogPanel::History.Append("[2026-10-16T12:30:00.000Z thread=14] [RuneSchema] [summary] recipes loaded=46515 errors=0");
            LiveLogPanel::History.Append("[2026-10-16T12:30:01.000Z thread=14] [RuneSchema] [warning] [PERFORMANCE][SLOW-WORK] phase=npcs.spawn:ExampleVendor elapsed_ms=185");
            LiveLogPanel::History.Append("[2026-10-16T12:30:02.000Z thread=14] [RuneSchema] [error] [PERSISTENCE-PRUNER] Duplicate quest owner marker. ExampleMod and OldExampleMod declare the same quest.");
            LiveLogPanel::Selected=LiveLogPanel::History.Rows.back().Id;
            const std::array pairs{std::pair{Page::Items,"item-service"},std::pair{Page::Creation,"transmog-creation"},std::pair{Page::Recipes,"recipes"},std::pair{Page::Spawns,"spawns"},std::pair{Page::Map,"map"},std::pair{Page::Tools,"tools"},std::pair{Page::Authoring,"authoring-inactive"},std::pair{Page::Settings,"settings"},std::pair{Page::LiveLog,"live-log"}};
            for(const auto& [page,name]:pairs){CurrentPage=page;for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/(std::string(name)+".png");RenderFrame();PreviewCapture.clear();}
            const auto previousEnvironment=HelpyEnvironment::Current;HelpyEnvironment::Current=HelpyEnvironment::Kind::Dedicated;
            DedicatedServerPanel::Preview=true;DedicatedServerPanel::Initialized=true;DedicatedServerPanel::File=L"Preview only/RSDragonwilds/Saved/Config/WindowsServer/DedicatedServer.ini";
            DedicatedServerPanel::Draft.emplace("[/Script/Dominion.DedicatedServerSettings]\r\nOwnerId=example-account-id\r\nServerName=Example RuneSchema server\r\nAdminPassword=preview-not-a-real-password\r\nDefaultWorldName=ExampleWorld\r\nWorldPassword=\r\nKnownPlayerList=(UserId=fixture,LastAdminPassword=must-stay-redacted)\r\n");
            CurrentPage=Page::DedicatedServer;for(int i=0;i<4;++i)RenderFrame();PreviewCapture=output/"dedicated-server.png";RenderFrame();PreviewCapture.clear();HelpyEnvironment::Current=previousEnvironment;
            CurrentPage=Page::Settings;PreviewUpdatesTab=true;for(int i=0;i<4;++i)RenderFrame();
            PreviewCapture=output/"updates-runtime.png";RenderFrame();PreviewCapture.clear();PreviewUpdatesTab=false;
            CurrentPage=Page::Items;
            for(const auto kind:{HelpyEnvironment::Kind::Steam,HelpyEnvironment::Kind::Epic,HelpyEnvironment::Kind::SteamEpic,HelpyEnvironment::Kind::GamePass,HelpyEnvironment::Kind::Dedicated,HelpyEnvironment::Kind::Unknown}) {
                HelpyEnvironment::Current=kind;
                for(int i=0;i<3;++i)RenderFrame();
                if(HelpyPreviewHooks::RuntimeFooter.w>ImGui::GetIO().DisplaySize.y||HelpyPreviewHooks::RuntimeFooter.y<HelpyPreviewHooks::InformationBar.y)
                    throw std::runtime_error("Runtime footer escaped the viewport");
                const auto info=HelpyPreviewHooks::InfoButton;
                if(info.x<0||info.y<0||info.z<=info.x||info.w<=info.y||info.z>ImGui::GetIO().DisplaySize.x||info.w>HelpyPreviewHooks::InformationBar.w)
                    throw std::runtime_error("Info button is clipped or missing from the top information bar");
                const auto source=HelpyPreviewHooks::SourceFilter;
                if(source.x<0||source.z>ImGui::GetIO().DisplaySize.x||source.w>ImGui::GetIO().DisplaySize.y)throw std::runtime_error("Source filter escaped the viewport");
                const char* name=kind==HelpyEnvironment::Kind::Steam?"runtime-steam":kind==HelpyEnvironment::Kind::Epic?"runtime-epic":kind==HelpyEnvironment::Kind::SteamEpic?"runtime-steam-epic":kind==HelpyEnvironment::Kind::GamePass?"runtime-gamepass":kind==HelpyEnvironment::Kind::Dedicated?"runtime-server":"runtime-unknown";
                PreviewCapture=output/(std::string(name)+".png");RenderFrame();PreviewCapture.clear();
            }
            HelpyEnvironment::Current=HelpyEnvironment::Kind::GamePass;
            for(const auto notice:{Notice::Credits,Notice::Legal,Notice::Info}) {
                RequestedNotice=notice;for(int i=0;i<3;++i)RenderFrame();
                PreviewCapture=output/(notice==Notice::Credits?"credits.png":notice==Notice::Legal?"legal.png":"info.png");RenderFrame();PreviewCapture.clear();
                PreviewDismissNotice=true;RenderFrame();RenderFrame();
            }
            // The expanded browser must remain usable at the minimum desktop
            // size, not just in the full-size interactive fixture below.
            {
                namespace Hooks=HelpyPreviewHooks;
                const auto center=[](const ImVec4& r){return ImVec2((r.x+r.z)*.5f,(r.y+r.w)*.5f);};
                const auto click=[&](const ImVec4& rect){Hooks::Mouse=center(rect);Hooks::MouseDown=true;for(int i=0;i<2;++i)RenderFrame();Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();};
                CurrentPage=Page::Map;for(int i=0;i<4;++i)RenderFrame();Hooks::MouseEnabled=true;Hooks::Mouse=center(Hooks::Map);
                Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<4;++i)RenderFrame();
                MapPanel::Data.Operation=1;MapPanel::Data.BuildingsWanted=true;MapPanel::Data.BuildingsOffset=0;for(int i=0;i<5;++i)RenderFrame();
                click(Hooks::BuildingBrowse);
                if(!MapPanel::Data.BuildingBrowserOpen||Hooks::BuildingUse.w>ImGui::GetIO().DisplaySize.y||Hooks::BuildingClose.z>ImGui::GetIO().DisplaySize.x)
                    throw std::runtime_error("Expanded building browser actions escaped the viewport");
                PreviewCapture=output/"building-browser-responsive.png";RenderFrame();PreviewCapture.clear();click(Hooks::BuildingClose);
                Hooks::Mouse={20,20};Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<3;++i)RenderFrame();Hooks::MouseEnabled=false;
                MapPanel::Data.Operation=0;CurrentPage=Page::Items;
            }
            if(width>=1200&&height>=850){
                namespace Hooks=HelpyPreviewHooks;CurrentPage=Page::Items;
                const auto center=[](const ImVec4& r){return ImVec2((r.x+r.z)*.5f,(r.y+r.w)*.5f);};
                const auto click=[&](const ImVec4& rect){Hooks::MouseEnabled=true;Hooks::Mouse=center(rect);Hooks::MouseDown=false;RenderFrame();Hooks::MouseDown=true;RenderFrame();Hooks::MouseDown=false;for(int i=0;i<4;++i)RenderFrame();};
                for(int i=0;i<4;++i)RenderFrame();click(Hooks::SourceFilter);
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
            CurrentPage=Page::Items;ItemGridPicker::AddToCart(items[0]);
            for(int i=0;i<4;++i)RenderFrame();
            if(HelpyPreviewHooks::Give.w>ImGui::GetIO().DisplaySize.y||HelpyPreviewHooks::Give.z>ImGui::GetIO().DisplaySize.x)throw std::runtime_error("Cart delivery button is outside the viewport");
            PreviewCapture=output/"item-service-cart.png";RenderFrame();PreviewCapture.clear();ItemGridPicker::ClearSharedCart();
            if(width>=1200&&height>=850){
                namespace Hooks=HelpyPreviewHooks;
                const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
                const auto center=[](const ImVec4& rect){return ImVec2((rect.x+rect.z)*0.5f,(rect.y+rect.w)*0.5f);};
                const auto mouse=[&](ImVec2 point,bool down,int frames=2){Hooks::MouseEnabled=true;Hooks::Mouse=point;Hooks::MouseDown=down;for(int i=0;i<frames;++i)RenderFrame();};
                const auto drag=[&](ImVec2 from,ImVec2 to){mouse(from,false);mouse(from,true);mouse(ImVec2(from.x+12,from.y+12),true);mouse(to,true,3);mouse(to,false,3);};
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
                drag(center(Hooks::ItemRects.at(path)),center(Hooks::Cart));
                check(ItemGridPicker::SharedCart.Entries().size()==1,"Dragging a browser item did not stage it in the cart");
                check(ItemGridPicker::SharedCart.Payload(3)[0]["Count"]==100,"Dragging an item lost its stack metadata");
                check(Hooks::CartRects.contains(path),"Staged cart item has no return drag surface");
                drag(center(Hooks::CartRects.at(path)),ImVec2(Hooks::Browser.x+30,Hooks::Browser.y+30));
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
                const auto mapCenter=center(Hooks::Map);
                const auto expectedMapPoint=MapPanel::Data.Projection.ToWorld(MapPanel::Data.View.ToUV({std::floor(mapCenter.x)-Hooks::Map.x,std::floor(mapCenter.y)-Hooks::Map.y}));
                check(Hooks::Map.z-Hooks::Map.x>Hooks::Map.w-Hooks::Map.y,"Map still renders as a narrow square instead of a wide viewport");
                mouse(center(Hooks::Map),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<3;++i)RenderFrame();
                check(MapPanel::Data.Popup,"Right-click did not open map placement");
                MapPanel::Data.Definition=definitions[16];MapPanel::Data.Kind=2;MapPanel::Data.Permanent=false;for(int i=0;i<3;++i)RenderFrame();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(!mapPlacement.empty()&&mapPlacement["Resource"]==true&&mapPlacement["Permanent"]==false,"Map did not submit the temporary resource choice");
                check(std::abs(mapPlacement["TargetLocation"]["X"].get<double>()-expectedMapPoint.X)<1&&std::abs(mapPlacement["TargetLocation"]["Y"].get<double>()-expectedMapPoint.Y)<1,"Map click changed the selected world position");
                mouse(mapCenter,false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<3;++i)RenderFrame();
                check(MapPanel::Data.Popup,"Map placement could not be opened again");
                MapPanel::Data.Definition=definitions[8];MapPanel::Data.Kind=1;MapPanel::Data.Permanent=true;for(int i=0;i<3;++i)RenderFrame();
                PreviewCapture=output/"map-placement.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement["Resource"]==false&&mapPlacement["Permanent"]==true,"Map did not submit the permanent enemy choice");
                mouse(center(Hooks::Map),false);Hooks::RightDown=true;RenderFrame();Hooks::RightDown=false;for(int i=0;i<5;++i)RenderFrame();
                MapPanel::Data.Operation=1;MapPanel::Data.BuildingsWanted=true;MapPanel::Data.BuildingsOffset=0;for(int i=0;i<5;++i)RenderFrame();
                check(MapPanel::Data.Buildings.size()==2&&MapPanel::Data.Ground.value("GroundAvailable",false),"Native building menu or ground coordinate response was lost");
                mouse(center(Hooks::BuildingBrowse),true);mouse(center(Hooks::BuildingBrowse),false,4);
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
                MapPanel::Data.Operation=2;for(int i=0;i<3;++i)RenderFrame();PreviewCapture=output/"map-teleport.png";RenderFrame();PreviewCapture.clear();
                mouse(center(Hooks::MapSpawn),true);mouse(center(Hooks::MapSpawn),false);
                check(mapPlacement["Action"]=="MapTeleport"&&mapPlacement["Player"]=="preview-player"&&mapPlacement["TargetLocation"]["X"]==MapPanel::Data.Point.X,"Map teleport lost the named player or selected point");
                const auto probesBeforeTravel=mapProbeCount;
                MapPanel::Data.NextGroundRead={};
                mouse(center(Hooks::MapLodestone),false,3);mouse(center(Hooks::MapLodestone),true);mouse(center(Hooks::MapLodestone),false,5);
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
                for(const auto section:{HelpyNavigation::ToolSection::Maintenance,HelpyNavigation::ToolSection::Cleanup}){
                    CurrentPage=Page::Tools;PreviewWantedTool=section;for(int i=0;i<4;++i)RenderFrame();check(CurrentTool==section,"Requested tools section did not render");
                    PreviewCapture=output/(section==HelpyNavigation::ToolSection::Maintenance?"tools-maintenance.png":"tools-cleanup.png");RenderFrame();PreviewCapture.clear();
                }
                // Exercise the second native window/swapchain with the shared
                // context. Input remains synthetic; no OS cursor movement.
                CurrentPage=Page::Creation;ItemGridPicker::DetachedBay=true;
                RenderFrame();check(m_bayWindow&&m_baySwapChain&&m_bayTarget,"Detached Item Bay failed to create native rendering resources");
                RECT mainDesktop{};GetWindowRect(m_hwnd,&mainDesktop);
                SetWindowPos(m_bayWindow,nullptr,mainDesktop.left-740,mainDesktop.top,700,680,SWP_NOZORDER|SWP_NOACTIVATE);
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
    DestroyItemBay();ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();ReleaseBrandTextures();DestroyDevice();
    DestroyWindow(m_hwnd);m_hwnd=nullptr;UnregisterClassW(wc.lpszClassName,m_module);if(SUCCEEDED(com))CoUninitialize();return result;
}
#endif

void HelpyStandaloneWindow::RenderFrame() {
    const bool packWasBusy=HelpyIconPacks::Busy();HelpyIconPacks::Poll();if(packWasBusy&&!HelpyIconPacks::Busy())PushToast(HelpyIconPacks::Status,5.0);
    static uint64_t uiGeneration=0;
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(generation!=uiGeneration){LazyCatalog::SourcesRemoved=false;LazyCatalog::ResetForGeneration(generation);uiGeneration=generation;}
#if !defined(HELPY_PREVIEW)
    if(HelpySourceCache::Poll(HostServices::ModDirectory())){
        ItemIconCache::InvalidateSource(true);
        if(HelpySourceCache::CatalogInvalidationRevision==HelpySourceCache::Revision){
            LazyCatalog::InvalidateSources(HelpySourceCache::RemovalRevision==HelpySourceCache::Revision);
            ItemDetails::Cache.clear();ItemDetails::Pending=0;ItemGridPicker::DetailsPopup.Open=false;
            SpawnToolsPanel::UiSnapshot.erase("Items");SpawnToolsPanel::UiSnapshot.erase("Definitions");
        }else{
            HelpyCacheSources::Plan sources;sources.DeclaredOwners=HelpySourceCache::Current.DeclaredOwners;
            LazyCatalog::RefreshSourceMetadata(sources,HelpySourceCache::Current.NativePlugins);
        }
    }
#endif
    LazyCatalog::PumpBackendSignal();LazyCatalog::CheckTimeouts();
    const UINT dpi=GetDpiForWindow(m_hwnd);HelpyImGuiTheme::SetRuntimeDpi(dpi?static_cast<float>(dpi)/96.0f:1.0f);
    if(FontRescanRequested.exchange(false)) {
        auto& io=ImGui::GetIO();ImGui_ImplDX11_InvalidateDeviceObjects();io.Fonts->Clear();LoadUiFontFamilies(io);SelectUiFontFamily(HelpyImGuiTheme::Current.fontFamily);io.FontDefault=UiRegularFont;
    }
    HelpyImGuiTheme::Poll();
#if !defined(HELPY_PREVIEW)
    if(HelpyUpdates::Tick(HelpyImGuiTheme::Current.automaticUpdateChecks,HelpyEnvironment::UpdateChecksAvailable,HelpyEnvironment::RuneSchemaVersion))
        PushToast("RuneSchema update available. See Settings > Updates & runtime.",6.0);
#endif
    ApplyCaptionTheme(m_hwnd,HelpyImGuiTheme::Current);
    const bool bayVisible=SyncItemBay();
    ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();auto& frameIo=ImGui::GetIO();const auto& t=HelpyImGuiTheme::Current;SelectUiFontFamily(t.fontFamily);frameIo.FontDefault=UiRegularFont;frameIo.FontGlobalScale=std::clamp(t.Scale()*t.fontScale,0.50f,4.00f);
    MainSurfacePosition={0,0};MainSurfaceSize=frameIo.DisplaySize;DesktopBayLayout::Canvas bayCanvas;
    if(bayVisible){
        RECT mainRect{},bayRect{};POINT mainOrigin{},bayOrigin{};GetClientRect(m_hwnd,&mainRect);GetClientRect(m_bayWindow,&bayRect);ClientToScreen(m_hwnd,&mainOrigin);ClientToScreen(m_bayWindow,&bayOrigin);
        bayCanvas=DesktopBayLayout::Arrange({float(mainOrigin.x),float(mainOrigin.y),float(mainRect.right),float(mainRect.bottom)},
            {float(bayOrigin.x),float(bayOrigin.y),float(bayRect.right),float(bayRect.bottom)});
        MainSurfacePosition={bayCanvas.Main.X,bayCanvas.Main.Y};MainSurfaceSize={bayCanvas.Main.Width,bayCanvas.Main.Height};frameIo.DisplaySize={bayCanvas.Width,bayCanvas.Height};
        POINT cursor{};if(GetCursorPos(&cursor))frameIo.AddMousePosEvent(float(cursor.x)-bayCanvas.X,float(cursor.y)-bayCanvas.Y);
    }
#if defined(HELPY_PREVIEW)
    if(HelpyPreviewHooks::MouseEnabled){frameIo.ConfigInputTrickleEventQueue=false;frameIo.AddFocusEvent(true);frameIo.AddMousePosEvent(HelpyPreviewHooks::Mouse.x,HelpyPreviewHooks::Mouse.y);frameIo.AddMouseButtonEvent(0,HelpyPreviewHooks::MouseDown);frameIo.AddMouseButtonEvent(1,HelpyPreviewHooks::RightDown);}
#endif
    ImGui::NewFrame();
    ItemGridPicker::DetailsSurfaceMin=MainSurfacePosition;ItemGridPicker::DetailsSurfaceSize=MainSurfaceSize;
    LazyCatalog::FirstOpen(SpawnToolRequests::CurrentGeneration());
    ImGui::SetNextWindowPos(MainSurfacePosition);ImGui::SetNextWindowSize(MainSurfaceSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);
    if(ImGui::Begin("##RuneSchemaHelpyStandalone",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        HelpyImGuiTheme::Scope skin(t);ImFont* workspaceFont=(t.textBold&&UiBoldFont)?UiBoldFont:UiRegularFont;
        if(workspaceFont)ImGui::PushFont(workspaceFont);RenderWorkspace(t);if(workspaceFont)ImGui::PopFont();
    }
    ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor();if(UiRegularFont)ImGui::PushFont(UiRegularFont);RenderToasts(t);if(UiRegularFont)ImGui::PopFont();
    if(bayVisible){
        ItemGridPicker::DetailsSurfaceMin={bayCanvas.Bay.X,bayCanvas.Bay.Y};ItemGridPicker::DetailsSurfaceSize={bayCanvas.Bay.Width,bayCanvas.Bay.Height};
        ImGui::SetNextWindowPos({bayCanvas.Bay.X,bayCanvas.Bay.Y});ImGui::SetNextWindowSize({bayCanvas.Bay.Width,bayCanvas.Bay.Height});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,t.window);ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
        if(ImGui::Begin("##RsmkDetachedItemBay",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings)){
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
    ItemGridPicker::RenderDetailsPopup();
    // Submit once, after every visible panel has declared this frame's demand.
    ItemIconCache::Update();
    ImGui::Render();const float clear[4]{t.window.x,t.window.y,t.window.z,t.window.w};m_context->OMSetRenderTargets(1,&m_target,nullptr);m_context->ClearRenderTargetView(m_target,clear);
    auto mainDraw=*ImGui::GetDrawData();mainDraw.DisplayPos=MainSurfacePosition;mainDraw.DisplaySize=MainSurfaceSize;
    ImGui_ImplDX11_RenderDrawData(&mainDraw);
    if(bayVisible){
        m_context->OMSetRenderTargets(1,&m_bayTarget,nullptr);m_context->ClearRenderTargetView(m_bayTarget,clear);
        auto bayDraw=*ImGui::GetDrawData();bayDraw.DisplayPos={bayCanvas.Bay.X,bayCanvas.Bay.Y};bayDraw.DisplaySize={bayCanvas.Bay.Width,bayCanvas.Bay.Height};
        ImGui_ImplDX11_RenderDrawData(&bayDraw);m_baySwapChain->Present(0,0);
    }
#if defined(HELPY_PREVIEW)
    if(!PreviewCapture.empty())CapturePreview(m_device,m_context,m_swapChain,PreviewCapture);
#endif
    m_swapChain->Present(1,0);
}

bool HelpyStandaloneWindow::SyncItemBay(){
    if(!ItemGridPicker::DetachedBay){if(m_bayWindow)DestroyItemBay();return false;}
    if(!m_bayWindow){
        RECT parent{};GetWindowRect(m_hwnd,&parent);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(m_hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        const int width=std::min(720,std::max(480,int(monitor.rcWork.right-monitor.rcWork.left))),height=std::min(700,std::max(360,int(monitor.rcWork.bottom-monitor.rcWork.top)));
        const int x=std::clamp(int(parent.right+12),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
        const int y=std::clamp(int(parent.top),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
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
        ApplyCaptionTheme(m_bayWindow,HelpyImGuiTheme::Current);ShowWindow(m_bayWindow,SW_SHOWNOACTIVATE);
    }
    ApplyCaptionTheme(m_bayWindow,HelpyImGuiTheme::Current);
    return IsWindowVisible(m_bayWindow)&&!IsIconic(m_bayWindow)&&m_bayTarget;
}
void HelpyStandaloneWindow::DestroyItemBay(){
    m_publishedBayWindow=nullptr;
    if(m_context)m_context->OMSetRenderTargets(0,nullptr,nullptr);
    if(m_bayWindow&&GetCapture()==m_bayWindow)ReleaseCapture();
    if(m_bayTarget){m_bayTarget->Release();m_bayTarget=nullptr;}
    if(m_baySwapChain){m_baySwapChain->Release();m_baySwapChain=nullptr;}
    if(m_bayWindow){DestroyWindow(m_bayWindow);m_bayWindow=nullptr;}
    ItemGridPicker::RenderingDetachedBay=false;
}

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
    if(msg==ToggleMessage){if(self&&self->m_visible.load())self->ClosePopup();return 0;}
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(hwnd,msg,wParam,lParam))return true;
    switch(msg) {
    case WM_DROPFILES: {
        const auto drop=reinterpret_cast<HDROP>(wParam);const auto count=DragQueryFileW(drop,0xffffffff,nullptr,0);wchar_t path[32768]{};
        const auto length=count==1?DragQueryFileW(drop,0,nullptr,0):0;
        if(length>0&&length<32768){DragQueryFileW(drop,0,path,32768);HelpyIconPacks::Start(path,true);}
        else HelpyIconPacks::Status="Drop one .rsmk-icons file at a time.";
        DragFinish(drop);PushToast(HelpyIconPacks::Status,5.0);return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(lParam);const UINT dpi=GetDpiForWindow(hwnd);const float scale=dpi?static_cast<float>(dpi)/96.0f:1.0f;
        const bool bay=self&&hwnd!=self->m_hwnd;
        info->ptMinTrackSize.x=static_cast<LONG>((bay?420.0f:HelpyImGuiTheme::Current.minWindowWidth)*scale);
        info->ptMinTrackSize.y=static_cast<LONG>((bay?320.0f:HelpyImGuiTheme::Current.minWindowHeight)*scale);return 0;
    }
    case WM_SIZE:
        if(self&&hwnd!=self->m_hwnd){
            if(self->m_baySwapChain&&self->m_device&&wParam!=SIZE_MINIMIZED){
                self->m_context->OMSetRenderTargets(0,nullptr,nullptr);
                if(self->m_bayTarget){self->m_bayTarget->Release();self->m_bayTarget=nullptr;}
                if(SUCCEEDED(self->m_baySwapChain->ResizeBuffers(0,LOWORD(lParam),HIWORD(lParam),DXGI_FORMAT_UNKNOWN,0))){
                    ID3D11Texture2D* buffer=nullptr;if(SUCCEEDED(self->m_baySwapChain->GetBuffer(0,IID_PPV_ARGS(&buffer)))){self->m_device->CreateRenderTargetView(buffer,nullptr,&self->m_bayTarget);buffer->Release();}
                }
            }return 0;
        }
        if(self&&HelpyEnvironment::Server())self->m_visible=wParam!=SIZE_MINIMIZED;
        if(self&&self->m_device&&wParam!=SIZE_MINIMIZED){self->DestroyTarget();self->m_swapChain->ResizeBuffers(0,LOWORD(lParam),HIWORD(lParam),DXGI_FORMAT_UNKNOWN,0);self->CreateTarget();}return 0;
    case WM_DPICHANGED:if(const auto* rect=reinterpret_cast<RECT*>(lParam))SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;
    case WM_CLOSE:if(self){if(hwnd==self->m_bayWindow){ItemGridPicker::DetachedBay=false;ShowWindow(hwnd,SW_HIDE);}else self->ClosePopup();}return 0;
    case WM_SYSCOMMAND:if((wParam&0xfff0)==SC_KEYMENU)return 0;break;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}
}
