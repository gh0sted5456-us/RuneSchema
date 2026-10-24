#include "Core/BugReport.h"
#include <cassert>
using namespace PS::BugReport;
int main(){
    Draft draft{"Icons & badges fail #1","Item icons disappear after returning to menu.","Open Helpy, return to menu, open Items.","Icons remain visible.","",6};
    const auto body=Body(draft,{{"Platform","Steam"},{"RuneSchema","0.8.1"}});
    assert(body.find("Cache / icons")!=body.npos&&body.find("Platform: Steam")!=body.npos&&body.find(draft.Steps)!=body.npos);
    const auto submission=Prepare(draft,body);
    assert(!submission.CopyBody&&submission.Url.starts_with(std::string(NewIssue)+"?title=Icons%20%26%20badges%20fail%20%231&body="));
    assert(submission.Url.find("&labels=")==submission.Url.npos&&submission.Url.find('#')==submission.Url.npos);
    assert(Encode("\xC3\xA9 /+?&=")=="%C3%A9%20%2F%2B%3F%26%3D");
    for(size_t i=0;i<Categories.size();++i){draft.Category=i;assert(Body(draft).find(Categories[i])!=body.npos);}
    draft.Details=std::string(2048,'&');draft.Steps=std::string(2048,'?');
    const auto large=Body(draft);const auto fallback=Prepare(draft,large);
    assert(fallback.CopyBody&&fallback.Url.find("&body=")==fallback.Url.npos&&large.find(draft.Details)!=large.npos);
    const auto rejects=[](auto fn){try{fn();return false;}catch(const std::exception&){return true;}};
    draft.Title="  ";assert(rejects([&]{Body(draft);}));draft.Title="Title\nInjected";assert(rejects([&]{Body(draft);}));
    draft.Title="Title";draft.Category=Categories.size();assert(rejects([&]{Body(draft);}));draft.Category=0;
    draft.Details=std::string(2049,'a');assert(rejects([&]{Body(draft);}));draft.Details=" ";assert(rejects([&]{Body(draft);}));
    draft.Details="Something broke";assert(rejects([&]{Body(draft,{{"Platform","Steam\nprivate"}});}));
    draft.Additional=std::string("hidden\0tail",11);assert(rejects([&]{Body(draft);}));draft.Additional.clear();
    assert(rejects([&]{Body(draft,{{"Platform",std::string("Steam\0private",13)}});}));
    assert(Body(draft).find("Runtime / versions")==std::string::npos);
}
