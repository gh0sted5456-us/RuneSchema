#include "Core/HelpyNavigationOrder.h"
#include <cassert>
int main(int argc,char** argv){
    assert(argc==2);namespace N=PS::HelpyNavigationOrder;namespace fs=std::filesystem;
    const auto folder=fs::path(argv[1])/("navigation-order-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));const auto file=folder/"navigation.json";
    const std::vector<int> defaults{11,0,4,13,5};assert(N::Load(file));assert(N::Ordered("sidebar",defaults)==defaults);
    assert(N::Move("sidebar",13,11,defaults));assert((N::Ordered("sidebar",defaults)==std::vector<int>{13,11,0,4,5}));
    assert(N::Load(file));assert(N::Ordered("sidebar",defaults).front()==13);
    assert((N::Ordered("sidebar",{11,0,4,13,5,99})==std::vector<int>{13,11,0,4,5,99}));
    assert((N::Ordered("sidebar",{11,4,5})==std::vector<int>{11,4,5})); // Hidden/removed routes cannot dispatch stale IDs.
    assert(!N::Move("sidebar",99,11,defaults));assert(N::Reset("sidebar"));assert(N::Ordered("sidebar",defaults)==defaults);
    {
        using PS::HelpyNavigation::Page;
        const auto before=PS::ConfigFiles::Read(file);
        auto layout=N::Sidebar();assert(PS::ConfigFiles::Read(file)==before);
        const auto position=[&](int id){return std::find(layout.begin(),layout.end(),id);};
        assert(position(int(Page::Settings))<position(N::SidebarDivider));
        assert(position(int(Page::BugReport))>position(N::SidebarDivider));
        assert(position(int(Page::Notifications))==position(int(Page::BugReport))+1);
        assert(position(int(Page::Home))<position(int(Page::Profile))&&position(int(Page::Profile))<position(int(Page::BugReport)));
        assert(N::Move(N::SidebarSurface,int(Page::BugReport),int(Page::Items),N::SidebarDefaults()));
        assert(N::Load(file));layout=N::Sidebar();assert(position(int(Page::BugReport))<position(N::SidebarDivider));
        assert(N::Move(N::SidebarSurface,int(Page::Settings),int(Page::Profile),N::SidebarDefaults()));
        assert(N::Load(file));layout=N::Sidebar();assert(position(int(Page::Settings))>position(N::SidebarDivider));
        assert(N::Reset(N::SidebarSurface));assert(N::Sidebar()==N::SidebarDefaults());
        auto workspaces=PS::HelpyNavigation::Workspaces{};const auto pages=N::WorkspacePages(workspaces,false);
        assert(std::find(pages.begin(),pages.end(),Page::Profile)!=pages.end());
        for(auto page:pages)assert(page!=Page::BugReport&&PS::HelpyNavigation::Available(page,workspaces,false));
        assert(std::find(pages.begin(),pages.end(),Page::ModMenu2)==pages.end());
        // Old saved layouts must drop the former main entry without resetting other ordering.
        N::Orders["sidebar"]={int(Page::ModMenu2),int(Page::Settings),int(Page::Mods)};
        N::Orders[N::SidebarSurface]={int(Page::ModMenu2),int(Page::Settings),int(Page::Mods)};
        layout=N::Sidebar();assert(layout.front()==int(Page::Settings)&&layout[1]==int(Page::Mods)&&position(int(Page::ModMenu2))==layout.end());
        workspaces.ModMenu=true;const auto enabledPages=N::WorkspacePages(workspaces,false);
        assert(std::find(enabledPages.begin(),enabledPages.end(),Page::ModMenu2)==enabledPages.end());
        assert(std::find(enabledPages.begin(),enabledPages.end(),Page::Mods)!=enabledPages.end());
        N::Orders.erase("sidebar");N::Orders.erase(N::SidebarSurface);
        assert(std::find(pages.begin(),pages.end(),Page::DedicatedServer)==pages.end());
    }
    assert(N::Move("home",4,11,defaults));PS::ConfigFiles::Write(file,"external change");assert(!N::Reset("home"));assert(PS::ConfigFiles::Read(file)=="external change");
    assert(!N::Load(file));assert(!N::Move("home",4,11,defaults));
    bool duplicate=false;try{(void)N::Validate({{"Format",1},{"Orders",{{"home",{1,1}}}}});}catch(...){duplicate=true;}assert(duplicate);
    bool traversal=false;try{(void)N::Validate({{"Format",1},{"Orders",{{"../home",{1}}}}});}catch(...){traversal=true;}assert(traversal);
}
