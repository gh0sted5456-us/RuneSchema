#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
// Plugin web pages: shown over Helpy's window, captured into the native in-game overlay, hidden
// on the notifications HUD and servers; they follow a window drag at once; plugin pages show
// Helpy's navigation across the top. The pane builds only with the Windows SDK, so this checks source.
static std::string Read(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Missing web pane contract input: "+path.string());return {std::istreambuf_iterator<char>(input),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static size_t At(const std::string& text,const std::string& needle,size_t from=0){const auto found=text.find(needle,from);if(found==text.npos)throw std::runtime_error("Missing: "+needle);return found;}
int main(int argc,char** argv){
    try{
        Check(argc==2,"Expected source directory");const auto root=std::filesystem::path(argv[1]);
        const auto window=Read(root/"helpy/source/src/Generator/HelpyStandaloneWindow.cpp");
        const auto mode=At(window,"const auto webMode=");
        const auto texture=At(window,":m_nativeOverlay.Visible()?HelpyWebPane::Mode::Texture",mode);
        Check(At(window,"HelpyEnvironment::Server()?HelpyWebPane::Mode::Hidden",mode)<texture,"Servers never show web pages");
        Check(texture<At(window,"m_nativeOverlay.NotificationsVisible()?HelpyWebPane::Mode::Hidden:HelpyWebPane::Mode::Window",mode),"The full native overlay captures the page; the notifications HUD hides it");
        Check(window.contains("HelpyWebPane::Frame webPages(m_hwnd,m_module,webMode,m_device);"),"Texture mode draws on Helpy's own device");
        Check(window.contains("case WM_MOVE:if(main&&self&&hwnd==self->m_hwnd)HelpyWebPane::FollowOwner(hwnd);"),"The page follows Helpy's window during a drag");
        const auto plugin=At(window,"if(!CurrentPluginRoute.empty()) {\r\n        RenderPluginTopNavigation(t,pluginTabs);");
        Check(plugin<At(window,"HelpyPluginNavigation::Draw(CurrentPluginRoute,ui)",plugin),"Plugin pages draw Helpy's top navigation before the page");
        const auto top=At(window,"void RenderPluginTopNavigation(");
        const auto workspace=At(window,"void RenderWorkspaceTabs(",top);
        Check(At(window,"OpenSidebarPage(page,t);",top)<workspace&&At(window,"OpenSidebarPage(Page::Home,t);",top)<workspace,"Top navigation opens Helpy's Home and pages");
        Check(At(window,"row.x+(avail-total)*.5f",top)<workspace,"Plugin tabs are centred on the top bar");
        Check(At(window,"InvisibleButton(\"##legal\"",top)<workspace&&At(window,"tab.Legal",top)<workspace,"The \"i\" shows each plugin's legal notice");
        const auto pane=Read(root/"helpy/source/src/Generator/HelpyWebPane.cpp");
        Check(pane.contains("CreateFreeThreaded(")&&pane.contains("CreateForWindow(pane.Window"),"Texture mode captures the page window");
        Check(pane.contains("Show(pane, Offscreen(state.Pixels), HWND_BOTTOM);"),"Texture mode keeps the page window off-screen and below everything");
        Check(pane.contains("--disable-features=CalculateNativeWinOcclusion"),"The page keeps painting while hidden behind the game");
        Check(pane.contains("put_AreDefaultContextMenusEnabled(FALSE)"),"No context menus, which would be uncaptured windows");
        const auto forward=At(pane,"void Forward(Pane& pane");
        Check(At(pane,"WM_MOUSEWHEEL",forward)>forward&&At(pane,"WM_CHAR",forward)>forward&&pane.find("SetForegroundWindow",forward)==std::string::npos,"Input is posted to the page without taking focus from the game");
        Check(pane.contains("Helpy draws the captured page")||pane.contains("Helpy draws the captured page."),"Texture mode is documented in place");
        // Helpy's browser: off by default, its own profile, web addresses only, ad blocker from Helpy's download only.
        const auto theme=Read(root/"helpy/source/include/Generator/HelpyImGuiTheme.h");
        Check(theme.contains("bool webBrowser{false},browserAdBlock{false};")&&theme.contains("runtime.value(\"webBrowser\",false)")&&theme.contains("runtime.value(\"browserAdBlock\",false)"),"The browser and its ad blocker are off until chosen");
        const auto sync=At(window,"void SyncBrowser(){");
        Check(At(window,"theme.webBrowser&&PluginRuntimeServices::ExternalDesktop&&!HelpyEnvironment::Server()",sync)>sync,"No browser on a server or without Helpy's desktop window");
        Check(At(window,"HelpyBrowser::AdBlock(on&&theme.browserAdBlock)",sync)>sync&&At(window,"blocker.State==HelpyBrowser::Blocker::Ready?blocker.Folder",sync)>sync,"Only a verified, unpacked ad blocker is loaded");
        Check(At(window,"HelpyPluginNavigation::Remove(BrowserOwner)",sync)>sync,"Turning the browser off removes its tab");
        Check(At(window,"\"Browser\",\"B\",DrawBrowserTab",sync)>sync&&At(window,"uBlock Origin Lite by Raymond Hill and contributors (GPL-3.0)",sync)>sync,"The Browser tab credits uBlock Origin Lite under the \"i\"");
        Check(pane.contains("(browser ? L\"web-browser\" : L\"plugin-browser\")")&&pane.contains("target.Extensions = browser && options"),"The browser has its own profile; only it can load extensions");
        Check(pane.contains("return pane.Browser ? HelpyBrowserAddress::Web(Utf8(url))")&&pane.contains("if (pane->Browser) return S_OK; // Only web addresses"),"The browser opens only http and https addresses");
        Check(pane.contains("put_IsPasswordAutosaveEnabled(FALSE)")&&pane.contains("put_IsGeneralAutofillEnabled(FALSE)"),"The browser never saves passwords or form data");
        Check(pane.contains("args->put_Cancel(TRUE); }).Get(), &token);")&&pane.contains("COREWEBVIEW2_PERMISSION_STATE_DENY"),"Downloads and site permissions stay blocked");
        Check(pane.contains("const auto wanted = state.Settings.Extension.wstring();")&&pane.contains("extensions->AddBrowserExtension(wanted.c_str()"),"The ad blocker comes only from Helpy's own download");
        const auto installer=Read(root/"helpy/source/src/Generator/HelpyBrowser.cpp");
        const auto install=At(installer,"View Install(View current) {");
        Check(At(installer,"Sha256(zip) != release.Sha256",install)<At(installer,"HelpyZip::Extract(file, staging);",install),"The ad blocker is checked against its published SHA-256 before it is unpacked");
        Check(installer.contains("CacheBundleIO::MetadataLock lock(root / \"installed.json\");"),"Extension publication is shared across Helpy processes");
        Check(installer.contains("void Activated(")&&installer.contains("ConfigFiles::Write(Root()/\"installed.json\",marker.dump(2));"),"The installed marker follows browser activation");
        Check(pane.contains("HelpyTextInput::InputTextWithHint(\"##helpy-browser-address\""),"Browser addresses use Helpy's controller keyboard");
        Check(pane.contains("ImGuiWindowFlags_Tooltip")&&pane.contains("window->Active&&!window->Hidden"),"Visible host tooltips cover the owned WebView");
        Check(pane.contains("FAILED(status)||!list||FAILED(list->get_Count(&count))")&&pane.contains("removals->second=removals->second&&SUCCEEDED(result)"),"Enumeration and removal failures cannot be reported as success");
        Check(pane.contains("HelpyBrowser::Activated(")&&pane.contains("HelpyBrowser::Rejected("),"Browser activation acknowledges success and failed candidates restore the previous version");
        Check(window.contains("HelpyWebPane::Suspend();")&&pane.contains("void Suspend()"),"Hiding Helpy releases page capture even when rendering stops");
        Check(window.contains("theme.webBrowser&&!theme.browserAdBlock&&HelpyWebPane::BrowserExtensionFailed()")&&window.contains("HelpyWebPane::RetryBrowserExtension();"),"Removal failures remain visible and retryable when blocking is off");
        Check(!pane.contains("VideoScript")&&!pane.contains("PollVideo")&&!window.contains("PictureInPicture")&&!theme.contains("browserPictureInPicture"),"PiP and video polling are removed");
        std::cout<<"Helpy web pane contract passed: modes, capture, drag follow, top navigation, browser lifecycle.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
