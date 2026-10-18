#include "Core/HelpyNavigation.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyBranding.h"
#include "Core/HelpySplash.h"
#include "Core/RuntimeDuration.h"
#include "Core/BackgroundPolicy.h"
#include "Core/DesktopBayLayout.h"
#include "Core/HelpyTaskbar.h"
#include <stdexcept>
#include <set>
int main(){
    {using namespace PS::BackgroundPolicy;
        if(!Filename("My art.PNG")||!Filename("Scene.jpeg")||Filename("../Scene.png")||Filename("C:\\Scene.png")||Filename("Scene.png.exe")||Filename("Scene.svg")||Filename(std::string("a\0.png",6)))throw std::runtime_error("Backgrounds must be local supported image filenames");
        const auto wide=Cover(1000,500,500,500),tall=Cover(500,1000,500,500);
        if(wide.Left!=.25f||wide.Right!=.75f||wide.Top!=0||tall.Top!=.25f||tall.Bottom!=.75f||Strength(-1)!=0||Strength(1)!=.3f)throw std::runtime_error("Background fitting must preserve aspect and stay subdued");
    }
    {using namespace PS::RuntimeDuration;
        if(Format(0)!="0 ms"||Format(999)!="999 ms"||Format(1000)!="1 s"||Format(26327)!="26.3 s"||Format(90000)!="1.5 min"||Format(-1)!="Not available"||Format(26327,Unit::Milliseconds)!="26327 ms"||Format(26327,Unit::Seconds)!="26.327 s"||Format(26327,Unit::Minutes)!="0.439 min"||Format(1,Unit::Seconds)!="0.001 s"||Format(60000,Unit::Minutes)!="1 min")throw std::runtime_error("Startup durations must support readable automatic and explicit units");
    }
    using PS::HelpySplash::Blend;
    const auto near=[](float a,float b){return std::abs(a-b)<.0001f;};
    if(!near(Blend(0),0)||!near(Blend(3.9),0)||!near(Blend(6),.5f)||!near(Blend(8),1)||!near(Blend(11.9),1)||!near(Blend(14),.5f)||!near(Blend(16),0)||!near(Blend(22),.5f))throw std::runtime_error("Splash must repeat four-second hold, slow four-second crossfade, hold, crossfade");
    for(int step=0;step<=200;++step){const auto time=4.0+step*.02;const auto forward=Blend(time),back=Blend(time+8.0);
        if(forward<0||forward>1||back<0||back>1||!near(forward+back,1)|| (step&&forward<Blend(time-.02)))throw std::runtime_error("Splash crossfade must be smooth, monotonic and complementary");
    }
    if(Blend(-1)!=0||Blend(0,false)!=1||Blend(14,false)!=1)throw std::runtime_error("Reduced-motion splash must hold the RuneSchema logo");
    {
        using namespace PS::HelpySplash;Exit exit;
        if(!exit.Begin(100,1)||exit.Kind!=ExitKind::RuneSchema||exit.Begin(101,0)||exit.Started!=100)throw std::runtime_error("Exit must freeze its selected logo and reject repeated Enter");
        const auto first=exit.At(100),middle=exit.At(100+ExitSeconds*.5),last=exit.At(101);
        if(first.Finished||first.Travel!=0||!near(first.Fade,1)||middle.Finished||middle.Travel<=middle.Progress||!last.Finished||last.Travel!=1||last.Fade!=0||!exit.At(100,false).Finished)throw std::runtime_error("Exit easing, duration or reduced-motion completion regressed");
        Exit game;game.Begin(0,.49f);if(game.Kind!=ExitKind::Dragonwilds)throw std::runtime_error("Mixed-logo Enter must select the dominant wordmark");
        const int w=32,h=24;std::vector<std::uint8_t> pixels(w*h*4);
        const auto block=[&](int left,int top,int right,int bottom){for(int y=top;y<bottom;++y)for(int x=left;x<right;++x){const auto i=(y*w+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=180;pixels[i+3]=255;}};
        block(1,3,6,21);block(11,2,23,22);block(27,3,31,21);const auto original=pixels;auto parts=SplitLogo(pixels,w,h);
        if(pixels!=original||parts[0].X!=1||parts[1].X!=11||parts[2].X!=27||parts[0].Width!=5||parts[1].Width!=12||parts[2].Width!=4)throw std::runtime_error("Logo splitting must isolate complete brackets without modifying the asset");
        size_t retained=0;for(const auto& part:parts)for(size_t i=3;i<part.Pixels.size();i+=4)if(part.Pixels[i])++retained;
        if(retained!=5*18+12*20+4*18||SplitLogo({},w,h)[0].Width)throw std::runtime_error("Logo layers lost or duplicated pixels, or accepted malformed input");
        // Thin antialiased shadow contact must not glue the right bracket to R.
        for(int x=23;x<27;++x){const auto i=(12*w+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=10;pixels[i+3]=30;}
        parts=SplitLogo(pixels,w,h);
        const auto alphaAt=[](const Part& p,int x,int y){return x>=p.X&&x<p.X+p.Width&&y>=p.Y&&y<p.Y+p.Height?p.Pixels[((y-p.Y)*p.Width+x-p.X)*4+3]:0;};
        if(alphaAt(parts[2],29,12)!=255||alphaAt(parts[1],29,12)!=0||alphaAt(parts[1],20,12)!=255)throw std::runtime_error("Shadow-connected bracket must move independently of the R");
        retained=0;for(const auto& part:parts)for(size_t i=3;i<part.Pixels.size();i+=4)if(part.Pixels[i])++retained;
        if(retained!=5*18+12*20+4*18+4)throw std::runtime_error("Shadow separation must preserve every artwork pixel exactly once");
        pixels=original;
        for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;parts=SplitLogo(pixels,w,h);
        if(parts[0].X!=1||parts[1].X!=11||parts[2].X!=27)throw std::runtime_error("Opaque black logo backgrounds must not join the three parts");
    }
    if(PS::HelpyBranding::RuntimeIconSize<32)throw std::runtime_error("Runtime icon must remain larger than the old 24-pixel mark");
    {using G=PS::HelpyBranding::GameWordmark;
        if(G::Width<1500||G::Height<1000||G::Left<0||G::Top<0||G::Right>1||G::Bottom>1||G::Left>=G::Right||G::Top>=G::Split||G::Split>=G::Bottom||!near(G::Aspect,(G::Bottom-G::Top)*G::Height/((G::Right-G::Left)*G::Width)))throw std::runtime_error("Full-resolution wordmark crop, line split and display aspect must agree");
    }
    PS::HelpyTaskbar::State tasks;using Window=PS::HelpyTaskbar::Window;
    tasks.Minimize(Window::ItemBay);tasks.Minimize(Window::IconCache);tasks.Minimize(Window::ItemBay);
    if(tasks.Count()!=2)throw std::runtime_error("Taskbar must not duplicate minimized windows");
    tasks.Restore(Window::ItemBay);if(tasks.Count()!=1||tasks.Contains(Window::ItemBay))throw std::runtime_error("Restoring one task must preserve others");
    tasks.Forget(Window::IconCache);if(tasks.Count())throw std::runtime_error("Closing/docking must remove stale taskbar entries");
    tasks.Minimize(Window::Runtime);if(tasks.Count()!=1||!tasks.Contains(Window::Runtime))throw std::runtime_error("Runtime details must have an independent taskbar entry");tasks.Restore(Window::Runtime);
    tasks.Minimize(Window::Party);if(tasks.Count()!=1||!tasks.Contains(Window::Party))throw std::runtime_error("Party must minimize independently");tasks.Restore(Window::Party);
    const auto left=PS::DesktopBayLayout::Arrange({100,200,1000,700},{-600,100,600,500});
    if(left.X!=-600||left.Y!=100||left.Main.X!=700||left.Main.Y!=100||left.Bay.X!=0||left.Width!=1700||left.Height!=800)throw std::runtime_error("Detached bay canvas must support negative desktop coordinates");
    const auto right=PS::DesktopBayLayout::Arrange({0,0,1000,700},{1100,0,600,800});
    if(right.Width!=1700||right.Height!=800||right.Bay.X!=1100)throw std::runtime_error("Detached bay desktop projection must preserve both client rectangles");
    const auto three=PS::DesktopBayLayout::Arrange({0,0,1000,700},{-700,50,600,800},{100,900,500,400});
    if(three.Width!=1700||three.Height!=1300||three.Main.X!=700||three.Bay.X!=0||three.Cache.X!=800||three.Cache.Y!=900)
        throw std::runtime_error("Main, bay and cache surfaces must preserve independent desktop projection");
    const auto five=PS::DesktopBayLayout::Arrange({0,0,1000,700},{-700,50,600,800},{100,900,500,400},{-1700,-100,900,700},{1200,-200,800,700});
    if(five.X!=-1700||five.Y!=-200||five.Width!=3700||five.Height!=1500||five.Map.X!=0||five.Recipes.X!=2900||five.Main.X!=1700)
        throw std::runtime_error("Five detached surfaces must preserve negative coordinates and independent mapping");
    const auto six=PS::DesktopBayLayout::Arrange({0,0,1000,700},{-700,50,600,800},{100,900,500,400},{-1700,-100,900,700},{1200,-200,800,700},{-2400,1400,680,640});
    if(six.X!=-2400||six.Y!=-200||six.Runtime.X!=0||six.Runtime.Y!=1600||six.Width!=4400||six.Height!=2240||six.Main.X!=2400)
        throw std::runtime_error("Runtime details must project independently on a six-surface desktop canvas");
    const auto seven=PS::DesktopBayLayout::Arrange({0,0,1000,700},{0,0,1000,700},{0,0,1000,700},{0,0,1000,700},{0,0,1000,700},{0,0,1000,700},{-400,-200,360,360});
    if(seven.X!=-400||seven.Y!=-200||seven.Party.X!=0||seven.Main.X!=400||seven.Width!=1400)throw std::runtime_error("Party must have an independent detached surface");
    using namespace PS::HelpyBranding;
    {std::uint8_t shadowGold[]{80,60,55,255};LogoLayer(shadowGold,50,100,true);if(shadowGold[0]!=shadowGold[1]||shadowGold[1]!=shadowGold[2]||shadowGold[3]!=255)throw std::runtime_error("The logo's shaded gold must be recolorable too");}
    if(BuildLabel("++dominion+staging-CL-248854")!="CL 248854"||BuildLabel("")!="Not reported"||BuildLabel("0.11.0")!="0.11.0"||BuildLabel("bad-CL-x")!="bad-CL-x")throw std::runtime_error("Build labels must not invent a version");
    std::uint8_t white[]{255,255,255,255},black[]{0,0,0,255},edge[]{255,255,255,100},transparent[]{255,255,255,0};
    MonochromeMask(white);MonochromeMask(black);MonochromeMask(edge);MonochromeMask(transparent);
    std::uint8_t silver[]{110,115,120,200},gold[]{240,180,50,150},letter[]{240,180,50,150},bracket[]{240,180,50,150};
    LogoLayer(silver,50,100,false);LogoLayer(gold,50,100,false);LogoLayer(letter,50,100,true);LogoLayer(bracket,10,100,false);
    if(silver[0]!=110||silver[1]!=115||silver[2]!=120||silver[3]!=200||gold[3]!=0||letter[0]!=240||letter[1]!=240||letter[2]!=240||letter[3]!=150||bracket[3]!=150)
        throw std::runtime_error("Only the central gold letter may move to the accent-tinted layer");
    if(white[3]!=255||black[3]!=0||edge[3]!=100||transparent[3]!=0||black[0]!=255)throw std::runtime_error("Gold logo upload mask must preserve alpha and remove black backgrounds");
    std::uint8_t epicBackground[]{255,255,255,255},epicLetter[]{0,0,0,255},epicShield[]{211,211,211,255};
    MonochromeMask(epicBackground,true);MonochromeMask(epicLetter,true);MonochromeMask(epicShield,true);
    if(epicBackground[3]!=0||epicLetter[3]!=255||epicShield[3]!=44||epicLetter[0]!=255)
        throw std::runtime_error("Epic mark must tint its dark letters and translucent shield, not its white canvas");
    if(std::string_view(Legal).find("RSMK shares RuneSchema's MIT License")==std::string_view::npos||std::string_view(Credits).find("NuLL and Jonesing")==std::string_view::npos)throw std::runtime_error("Credits and project license must remain accessible");
    PS::HelpyEnvironment::Configure({{"Storefront","Game Pass/WinGDK"},{"RuneSchemaVersion","0.8.0.0"},{"GameVersion","test-build"}});
    if(PS::HelpyEnvironment::RuneSchemaVersion!="0.8.0.0"||PS::HelpyEnvironment::GameVersion!="test-build")throw std::runtime_error("Host runtime versions were not retained");
    PS::HelpyEnvironment::Configure({{"RuneSchemaVersion",7},{"GameVersion",nullptr}});
    if(!PS::HelpyEnvironment::RuneSchemaVersion.empty()||!PS::HelpyEnvironment::GameVersion.empty())throw std::runtime_error("Malformed version metadata must not appear as a fake version");
    using K=PS::HelpyEnvironment::Kind;
    PS::HelpyEnvironment::Configure({{"DedicatedServer",true}});
    if(PS::HelpyEnvironment::NativeIconCaptureAvailable)throw std::runtime_error("Server desktop must not poll a headless game GPU");
    PS::HelpyEnvironment::Configure({{"Storefront","Game Pass/WinGDK"}});
    if(!PS::HelpyEnvironment::NativeIconCaptureAvailable)throw std::runtime_error("Client renderer capture must remain available");
    PS::HelpyEnvironment::Current=K::SteamEpic;if(!PS::HelpyEnvironment::SharedStoreBadge())throw std::runtime_error("Shared Win64 badge must show Steam and Epic together");
    for(const auto kind:{K::Steam,K::Epic,K::GamePass,K::Dedicated}){PS::HelpyEnvironment::Current=kind;if(PS::HelpyEnvironment::SharedStoreBadge())throw std::runtime_error("Identified runtime must keep its own platform badge");}
    if(PS::HelpyEnvironment::Decode({{"Storefront","Steam/Epic"}})!=K::SteamEpic
        ||PS::HelpyEnvironment::Decode({{"Storefront","Steam/Epic"},{"Platform","Steam"}})!=K::Steam
        ||PS::HelpyEnvironment::Decode({{"Storefront","Steam/Epic"},{"Platform","Epic Games"}})!=K::Epic
        ||PS::HelpyEnvironment::Decode({{"Storefront","Steam/GOG"}})!=K::SteamEpic
        ||PS::HelpyEnvironment::Decode({{"Storefront","Steam/GOG"},{"Steam",{{"BuildId",25812671}}}})!=K::Steam
        ||PS::HelpyEnvironment::Decode({{"Storefront","Game Pass/WinGDK"}})!=K::GamePass
        ||PS::HelpyEnvironment::Decode({{"Storefront","Steam/Epic"},{"DedicatedServer",true},{"Platform","Steam"}})!=K::Dedicated
        ||PS::HelpyEnvironment::Decode(nlohmann::json::object())!=K::Unknown)
        throw std::runtime_error("Environment detection must prefer dedicated server over its store lane");
    PS::HelpyEnvironment::Configure({{"Storefront","Steam/Epic"},{"Platform","Epic Games"},{"Steam",{{"BuildId",123}}}});
    if(std::string_view(PS::HelpyEnvironment::Label())!="Epic Games"||!PS::HelpyEnvironment::Steam.empty())
        throw std::runtime_error("Epic must not present Steam build metadata as its own installed build");
    using namespace PS::HelpyNavigation;
    std::set<std::string_view> titles;
    for(auto page:Pages){
        if(!Visible(page,{true,true,true,true})||std::string_view(Description(page)).empty())throw std::runtime_error("Enabled workspace is unreachable or unexplained");
        titles.insert(Title(page));
    }
    const std::set<std::string_view> expected{"Home","Item Service","Transmog & Creation","Recipes","Spawns","Map","Journal","Tools","Settings","Authoring Tools","Live Log","Server / Co-op","Save Editor"};
    if(titles!=expected)throw std::runtime_error("All destinations must stay reachable");
    for(auto page:{Page::Authoring,Page::Tools,Page::LiveLog,Page::SaveEditor})if(Visible(page))throw std::runtime_error("Optional workspace must require opt-in");
    if(!Visible(Page::Settings)||!Visible(Page::Items)||!Visible(Page::LiveLog,{false,false,true,false})||Visible(Page::Tools,{false,false,true,false}))throw std::runtime_error("Workspace toggles must be independent; Settings stays reachable");
    for(int mask=0;mask<64;++mask){
        const Workspaces enabled{bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8),bool(mask&16),bool(mask&32)};
        for(auto page:Pages){
            if(Available(page,enabled,false)!=(Visible(page,enabled)&&(page!=Page::DedicatedServer||enabled.ServerTools))||Available(page,enabled,true)!=Visible(page,enabled))
                throw std::runtime_error("Home and sidebar must share workspace and server availability");
            if(ShowSidebar(page)==(page==Page::Home))throw std::runtime_error("Home must collapse the sidebar; tools must restore it");
        }
    }
}
