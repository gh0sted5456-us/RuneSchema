#include "Core/DiagnosticCenterModel.h"
#include <cassert>
#include <chrono>
#include <string>
using namespace PS;
int main(){
    using namespace DiagnosticCenterModel;
    Store store;
    const auto when=Clock::now();
    auto first=LiveLog::Parse("[2026-10-21T00:00:00Z thread=1] [RuneSchema] [error] [AssetLoader] Missing /Game/A");
    first.Id=71;
    const auto id=store.Accept(first,when);
    assert(id&&store.UnreadIssues()==1&&store.Events().back().SourceLogId==71);
    auto repeat=LiveLog::Parse("[2026-10-21T00:00:01Z thread=2] [RuneSchema] [error] [AssetLoader] Missing /Game/A");
    assert(store.Accept(repeat,when+std::chrono::seconds(1))==id);
    assert(store.FindGroup(id)->Count==2&&store.UnreadIssues()==1);
    store.MarkViewed(id,true);assert(store.UnreadIssues()==0&&store.FindGroup(id)->Acknowledged);
    store.Accept(repeat);assert(store.UnreadIssues()==1&&!store.FindGroup(id)->Acknowledged);
    auto second=LiveLog::Parse("[RuneSchema] [error] [AssetLoader] Missing /Game/B");
    assert(store.Accept(second)!=id);assert(store.UnreadIssues()==2);
    auto warning=LiveLog::Parse("[RuneSchema] [warning] module unresolved");
    const auto wid=store.Accept(warning);assert(store.FindGroup(wid)->Category=="Subsystem: RuneSchema");
    auto debug=LiveLog::Parse("[RuneSchema] [debug] pass complete");
    store.Accept(debug);assert(store.UnreadIssues()==3);
    assert(store.Groups().size()==4&&store.Events().size()==6);
    Store bounded({2,512,2});
    bounded.Accept(first);bounded.Accept(first);bounded.Accept(second);
    assert(bounded.Events().size()==2&&bounded.Dropped()==1);
    assert(bounded.FindGroup(bounded.Events().front().GroupId));
    bounded.Accept(warning);assert(bounded.Events().size()==2&&bounded.Dropped()==2);
    assert(bounded.Groups().size()==2);
    auto huge=LiveLog::Parse("[RuneSchema] [error] "+std::string(1000,'x'));
    bounded.Accept(huge);assert(bounded.Events().empty()&&bounded.Groups().empty());
    bounded.Clear();assert(bounded.Events().empty()&&bounded.UnreadIssues()==0);
    for(const auto& code:Diagnostics::All){
        const auto entry=LiveLog::Parse("[RuneSchema] [error] ["+std::string(code.Code)+"] asset blueprint property");
        assert(CategoryOf(entry)==code.Subsystem);
    }
    assert(Diagnostics::Category("RS-REC-001",true)=="Recipes");
    assert(Diagnostics::Category("RS-JRN-001",true)=="Journal");
    assert(Diagnostics::Category("RS-FUTURE-999",true)=="Other diagnostics");
}
