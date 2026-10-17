#include "Core/AuthoringCleanupPreview.h"
#include <cassert>
using namespace PS::SaveCleanup;
template<class F>bool Rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){
    auto registry=std::make_shared<RegistrySnapshot>();
    for(int i=0;i<500;++i)registry->Items.insert("item"+std::to_string(i));
    for(int i=0;i<300;++i)registry->Recipes.insert("recipe"+std::to_string(i));
    registry->QuestsComplete=true;registry->JournalsComplete=true;
    Json source={{"GameProgress",{{"Character",{{"Name","Preserve me"}}},{"Inventory",{{"0",{{"ItemData","missing"}}},{"1",{{"ItemData","item1"}}}}}}}};
    const auto original=source;AuthoringPreview session;session.Inspect(source);
    assert(Rejects([&]{session.Consume(source,"none",true,registry);}));
    auto incomplete=std::make_shared<RegistrySnapshot>(*registry);incomplete->JournalsComplete=false;
    assert(Rejects([&]{session.Preview(source,{{"PruneInvalid",true}},incomplete);}));
    const auto preview=session.Preview(source,{{"PruneInvalid",true}},registry);const auto token=preview["Token"].get<std::string>();
    assert(preview["Count"]>0);assert(Rejects([&]{session.Consume(source,token,false,registry);}));
    assert(Rejects([&]{session.Consume(source,"wrong",true,registry);}));
    auto changed=source;changed["GameProgress"]["Character"]["Name"]="Changed";
    assert(Rejects([&]{session.Consume(changed,token,true,registry);}));
    assert(Rejects([&]{session.Consume(source,token,true,registry);}));
    const auto second=session.Preview(source,{{"PruneInvalid",true}},registry);auto altered=std::make_shared<RegistrySnapshot>(*registry);altered->Items.insert("missing");
    assert(Rejects([&]{session.Consume(source,second["Token"],true,altered);}));
    const auto veto=session.Preview(source,{{"PruneInvalid",true}},registry);PS::RegistryEnrollment::Reject("fixture","declaration failed");
    assert(Rejects([&]{session.Consume(source,veto["Token"],true,registry);}));
    assert(Rejects([&]{session.Preview(source,{{"PruneInvalid",true}},registry);}));PS::RegistryEnrollment::Reset();
    const auto third=session.Preview(source,{{"PruneInvalid",true}},registry);const auto result=session.Consume(source,third["Token"],true,registry);
    assert(result.Removed==third["Removed"]);assert(!result.Save["GameProgress"]["Inventory"].contains("0"));
    assert(result.Save["GameProgress"]["Inventory"]["1"]==source["GameProgress"]["Inventory"]["1"]);
    assert(result.Save["GameProgress"]["Character"]==source["GameProgress"]["Character"]&&source==original);
    assert(Rejects([&]{session.Consume(source,third["Token"],true,registry);}));
    session.Inspect(result.Save);const auto empty=session.Preview(result.Save,{{"PruneInvalid",true}},registry);
    assert(Rejects([&]{session.Consume(result.Save,empty["Token"],true,registry);}));
    session.Reset();assert(Rejects([&]{session.Preview(source,{{"PruneInvalid",true}},registry);}));
}
