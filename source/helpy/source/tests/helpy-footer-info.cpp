#include <fstream>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include "Core/HelpyBranding.h"
#include "Core/HelpyNativeFrame.h"
int main(int argc,char** argv){
    for(const UINT dpi:{96u,144u,192u}){
        PS::HelpyNativeFrame::Metrics frame;frame.Dpi=dpi;frame.Border=MulDiv(8,dpi,96);frame.Height=MulDiv(31,dpi,96);frame.Icon=MulDiv(16,dpi,96);
        if(frame.HitTest(400,0,800,false)!=HTTOP||frame.HitTest(0,0,800,false)!=HTTOPLEFT||frame.HitTest(799,0,800,false)!=HTTOPRIGHT||
            frame.HitTest(400,frame.Height-1,800,false)!=HTCAPTION||frame.HitTest(8,frame.Height/2,800,false)!=HTSYSMENU||
            frame.HitTest(400,frame.Height,800,false)!=HTCLIENT||frame.HitTest(400,0,800,true)!=HTCAPTION)
            throw std::runtime_error("Image caption must preserve resize, dragging and system-menu hit targets at each DPI");
        const int width=MulDiv(801,dpi,96);frame.Controls={width-MulDiv(138,dpi,96),frame.Border,width,frame.Height};
        constexpr int hits[]{HTMINBUTTON,HTMAXBUTTON,HTCLOSE};
        for(int column=0;column<3;++column){const auto rect=frame.ControlRect(column);for(int x:{int(rect.left),int(rect.right-1)})
            if(frame.HitTest(x,(rect.top+rect.bottom)/2,width,false)!=hits[column])throw std::runtime_error("Caption glyph and native hit bounds differ at fractional DPI");}
    }
    if(std::wstring(PS::HelpyBranding::WindowTitle)!=L"RuneSchema")throw std::runtime_error("Keep an accessible task-switching name without the old RSMK title");
    if(std::string(PS::HelpyBranding::Credits).find("Abyss (RSDW Modding Community)")==std::string::npos)
        throw std::runtime_error("Corrupt-save bypass origin credit is missing");
    if(argc!=2)throw std::runtime_error("Expected standalone UI source");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("UI source unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    if(source.find("Page CurrentPage=Page::Home;")==source.npos)throw std::runtime_error("The landing page must be the initial workspace");
    const auto begin=source.find("void RenderRuntimeBadge(const HelpyImGuiTheme::Theme& t,float logicalSize) {"),end=source.find("void ApplyCaptionTheme(",begin);
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
    if(source.contains("TextDisabled(\"Helpy taskbar\")")||!source.contains("DrawThemedLogo(ImGui::GetWindowDrawList(),logo,{logo.x+logoSize,logo.y+logoSize},t)"))throw std::runtime_error("The taskbar label must be the existing RuneSchema logo, with restore buttons retained");
    const auto frame=source.substr(source.find("void HelpyStandaloneWindow::RenderFrame()"));
    const auto render=frame.substr(0,frame.find("bool HelpyStandaloneWindow::SyncItemBay"));
    if(render.contains("GetDpiForWindow(")||!source.contains("case WM_DPICHANGED:")||
        !source.contains("static std::unordered_map<HWND,Caption> captions")||
        !source.contains("caption=rgb(theme.window)"))throw std::runtime_error("Caption caching and event-driven DPI must stay inexpensive");
    for(const auto* required:{"DwmDefWindowProc(hwnd,msg,wParam,lParam,&result)","WM_NCCALCSIZE","WM_NCHITTEST","NativeCaption.Update(hwnd,HIWORD(wParam))","ImGui::GetCursorPosY()+float(NativeCaption.Height)","lParam!=VK_SPACE","HelpyWindowChrome::RenderNativeCaption(NativeCaption,framePosition,t)","RenderInformationBar(t,false)"})
        if(!source.contains(required))throw std::runtime_error("Image caption lost native frame handling or content insets");
    if(source.contains("BeginTabItem(\"Advanced Diagnostics\"")||source.contains("Diagnostics workspace")||
        !source.contains("case Page::LiveLog:\n                    DiagnosticsPanel::Render"))throw std::runtime_error("Diagnostics must have one workspace without a duplicate outer tab layer");
}
