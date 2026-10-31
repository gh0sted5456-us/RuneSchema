#include "Core/BugReport.h"
#include <cassert>
using namespace PS::BugReport;
int main(){
    Draft draft{"Icons & badges fail #1","Item icons disappear after returning to menu.","Open Helpy, return to menu, open Items.","Icons remain visible.","",6};
    const auto body=Body(draft,{{"Platform","Steam"},{"RuneSchema","0.8.1"}});
    assert(body.find("Cache / icons")!=body.npos&&body.find("Platform: Steam")!=body.npos&&body.find(draft.Steps)!=body.npos);
    assert(body.starts_with("Title: Icons & badges fail #1\nCategory: Cache / icons\n")&&body.find("###")==body.npos);
    assert(std::string_view(NexusPage)=="https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=bugs");
    for(size_t i=0;i<Categories.size();++i){draft.Category=i;assert(Body(draft).find(Categories[i])!=body.npos);}
    draft.Details=std::string(2048,'&');draft.Steps=std::string(2048,'?');
    const auto large=Body(draft);assert(large.find(draft.Details)!=large.npos&&large.find(draft.Steps)!=large.npos);
    const auto rejects=[](auto fn){try{fn();return false;}catch(const std::exception&){return true;}};
    draft.Title="  ";assert(rejects([&]{Body(draft);}));draft.Title="Title\nInjected";assert(rejects([&]{Body(draft);}));
    draft.Title="Title";draft.Category=Categories.size();assert(rejects([&]{Body(draft);}));draft.Category=0;
    draft.Details=std::string(2049,'a');assert(rejects([&]{Body(draft);}));draft.Details=" ";assert(rejects([&]{Body(draft);}));
    draft.Details="Something broke";assert(rejects([&]{Body(draft,{{"Platform","Steam\nprivate"}});}));
    draft.Additional=std::string("hidden\0tail",11);assert(rejects([&]{Body(draft);}));draft.Additional.clear();
    assert(rejects([&]{Body(draft,{{"Platform",std::string("Steam\0private",13)}});}));
    assert(Body(draft).find("Runtime / versions")==std::string::npos);
}
