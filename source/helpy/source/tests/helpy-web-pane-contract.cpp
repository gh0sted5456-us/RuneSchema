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
        Check(At(window,"OpenSidebarPage(page,t);",top)<At(window,"void RenderWorkspaceTabs(",top),"Top navigation opens Helpy's pages");
        const auto pane=Read(root/"helpy/source/src/Generator/HelpyWebPane.cpp");
        Check(pane.contains("CreateFreeThreaded(")&&pane.contains("CreateForWindow(pane.Window"),"Texture mode captures the page window");
        Check(pane.contains("Show(pane, Offscreen(state.Pixels), HWND_BOTTOM);"),"Texture mode keeps the page window off-screen and below everything");
        Check(pane.contains("--disable-features=CalculateNativeWinOcclusion"),"The page keeps painting while hidden behind the game");
        Check(pane.contains("put_AreDefaultContextMenusEnabled(FALSE)"),"No context menus, which would be uncaptured windows");
        const auto forward=At(pane,"void Forward(Pane& pane");
        Check(At(pane,"WM_MOUSEWHEEL",forward)>forward&&At(pane,"WM_CHAR",forward)>forward&&pane.find("SetForegroundWindow",forward)==std::string::npos,"Input is posted to the page without taking focus from the game");
        Check(pane.contains("Helpy draws the captured page")||pane.contains("Helpy draws the captured page."),"Texture mode is documented in place");
        std::cout<<"Helpy web pane contract passed: modes, capture, drag follow, top navigation.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
