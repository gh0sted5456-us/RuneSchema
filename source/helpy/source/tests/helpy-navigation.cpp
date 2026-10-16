#include "Core/HelpyNavigation.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyBranding.h"
#include "Core/DesktopBayLayout.h"
#include <stdexcept>
#include <set>
int main(){
    const auto left=PS::DesktopBayLayout::Arrange({100,200,1000,700},{-600,100,600,500});
    if(left.X!=-600||left.Y!=100||left.Main.X!=700||left.Main.Y!=100||left.Bay.X!=0||left.Width!=1700||left.Height!=800)throw std::runtime_error("Detached bay canvas must support negative desktop coordinates");
    const auto right=PS::DesktopBayLayout::Arrange({0,0,1000,700},{1100,0,600,800});
    if(right.Width!=1700||right.Height!=800||right.Bay.X!=1100)throw std::runtime_error("Detached bay desktop projection must preserve both client rectangles");
    using namespace PS::HelpyBranding;
    if(BuildLabel("++dominion+staging-CL-248854")!="CL 248854"||BuildLabel("")!="Not reported"||BuildLabel("0.11.0")!="0.11.0"||BuildLabel("bad-CL-x")!="bad-CL-x")throw std::runtime_error("Build labels must not invent a version");
    std::uint8_t white[]{255,255,255,255},black[]{0,0,0,255},edge[]{255,255,255,100},transparent[]{255,255,255,0};
    MonochromeMask(white);MonochromeMask(black);MonochromeMask(edge);MonochromeMask(transparent);
    std::uint8_t silver[]{110,115,120,200},gold[]{240,180,50,150},letter[]{240,180,50,150},bracket[]{240,180,50,150};
    LogoLayer(silver,50,100,false);LogoLayer(gold,50,100,false);LogoLayer(letter,50,100,true);LogoLayer(bracket,10,100,false);
    if(silver[0]!=110||silver[1]!=115||silver[2]!=120||silver[3]!=200||gold[3]!=0||letter[0]!=240||letter[1]!=240||letter[2]!=240||letter[3]!=150||bracket[3]!=150)
        throw std::runtime_error("Only the central gold letter may move to the accent-tinted layer");
    if(white[3]!=255||black[3]!=0||edge[3]!=100||transparent[3]!=0||black[0]!=255)throw std::runtime_error("Gold logo upload mask must preserve alpha and remove black backgrounds");
    if(std::string_view(Legal).find("RSMK shares RuneSchema's MIT License")==std::string_view::npos||std::string_view(Credits).find("NuLL and Jonesing")==std::string_view::npos)throw std::runtime_error("Credits and project license must remain accessible");
    PS::HelpyEnvironment::Configure({{"Storefront","Game Pass/WinGDK"},{"RuneSchemaVersion","0.7.8.0-alpha"},{"GameVersion","test-build"}});
    if(PS::HelpyEnvironment::RuneSchemaVersion!="0.7.8.0-alpha"||PS::HelpyEnvironment::GameVersion!="test-build")throw std::runtime_error("Host runtime versions were not retained");
    PS::HelpyEnvironment::Configure({{"RuneSchemaVersion",7},{"GameVersion",nullptr}});
    if(!PS::HelpyEnvironment::RuneSchemaVersion.empty()||!PS::HelpyEnvironment::GameVersion.empty())throw std::runtime_error("Malformed version metadata must not appear as a fake version");
    using K=PS::HelpyEnvironment::Kind;
    if(PS::HelpyEnvironment::Decode({{"Storefront","Steam/GOG"}})!=K::Steam
        ||PS::HelpyEnvironment::Decode({{"Storefront","Game Pass/WinGDK"}})!=K::GamePass
        ||PS::HelpyEnvironment::Decode({{"Storefront","Steam/GOG"},{"DedicatedServer",true}})!=K::Dedicated
        ||PS::HelpyEnvironment::Decode(nlohmann::json::object())!=K::Unknown)
        throw std::runtime_error("Environment detection must prefer dedicated server over its store lane");
    using namespace PS::HelpyNavigation;
    std::set<std::string_view> titles;
    for(auto page:Pages){
        if(!Visible(page)||std::string_view(Description(page)).empty())throw std::runtime_error("Workspace is unreachable or unexplained");
        titles.insert(Title(page));
    }
    const std::set<std::string_view> expected{"Item Service","Transmog & Creation","Recipes","Spawns","Map","Tools & Settings","Authoring Tools"};
    if(titles!=expected)throw std::runtime_error("All seven destinations must stay reachable");
}
