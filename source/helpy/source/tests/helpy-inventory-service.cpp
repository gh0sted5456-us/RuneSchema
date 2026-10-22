#include "Core/HelpyInventoryService.h"
#include <cassert>
using namespace PS;using Json=nlohmann::json;
template<class F>void Reject(F f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char**argv){
    assert(argc==2);const auto directory=std::filesystem::path(argv[1])/std::to_string(GetCurrentProcessId());
    assert(HelpyPersonalization::Load(directory/"personalization.json"));
    const Json row={{"Path","/Game/Items/Axe.Axe"},{"PlayerGuid","11111111AAAA22223333333344444444"},{"Instance",{1,2,3,4}},{"Count",2}};
    const auto run=[&](const Json& q){return HelpyInventoryService::Run(directory,q);};
    assert(!run({{"Action","Capabilities"}}).contains("StorageFormat"));
    assert(!HelpyInventoryService::Mutating("PrepareDelete")&&!HelpyInventoryService::Mutating("StorageRead"));
    assert(HelpyInventoryService::Mutating("CycleMark"));
    const auto cycle=[&](const Json& item,const char* scope="Stack"){return run({{"Action","CycleMark"},{"Item",item},{"Scope",scope},{"Revision",HelpyPersonalization::Revision.load()}});};
    assert(HelpyPersonalization::Set(HelpyPersonalization::StackKey(row),{{"Locked",true},{"Nickname","Mine"},{"Tags",Json::array({"keep"})}}));
    const auto firstRevision=HelpyPersonalization::Revision.load();
    assert(cycle(row).at("Marks").at("Favorite")==true);
    assert(HelpyPersonalization::Revision.load()==firstRevision+1);
    assert(cycle(row).at("Junk")==true&&!HelpyPersonalization::Favorite(row));
    assert(!cycle(row).at("Junk").get<bool>()&&!HelpyPersonalization::Favorite(row));
    assert(HelpyPersonalization::Locked(row)&&HelpyPersonalization::Tagged(row,"keep")&&HelpyPersonalization::Effective(row).at("Nickname")=="Mine");
    auto second=row;second["Instance"]={9,10,11,12};assert(!HelpyPersonalization::Favorite(second));
    const auto unchanged=ConfigFiles::Read(directory/"personalization.json");
    Reject([&]{run({{"Action","CycleMark"},{"Item",row},{"Scope","Stack"},{"Revision",firstRevision}});});
    assert(ConfigFiles::Read(directory/"personalization.json")==unchanged);
    auto invalid=row;invalid.erase("Instance");Reject([&]{cycle(invalid);});Reject([&]{cycle(row,"World");});
    assert(!HelpyPersonalization::Set(HelpyPersonalization::StackKey(row),{{"Favorite",true}},firstRevision));
    assert(cycle(second,"Type").at("Marks").at("Favorite")==true&&HelpyPersonalization::Favorite(second));
    cycle(second,"Type");cycle(second,"Type");
    assert(HelpyPersonalization::Set(HelpyPersonalization::StackKey(row),{{"Locked",false}}));
    assert(HelpyPersonalization::Load(directory/"personalization.json"));assert(HelpyPersonalization::Tagged(row,"keep"));
    assert(HelpyPersonalization::Set(HelpyPersonalization::StackKey(row),{{"Tags",nullptr}}));
    run({{"Action","Mark"},{"Item",row},{"Scope","Stack"},{"Field","Favorite"},{"Value",true}});
    assert(HelpyPersonalization::Favorite(row));
    assert(!run({{"Action","TypeMarks"},{"Item",row}}).at("Marks").value("Favorite",false));
    auto other=row;other["Instance"]={5,6,7,8};assert(!HelpyPersonalization::Favorite(other));
    run({{"Action","Mark"},{"Item",row},{"Scope","Type"},{"Field","Junk"},{"Value",true}});
    assert(HelpyPersonalization::Tagged(row,"junk")&&HelpyPersonalization::Tagged(other,"junk"));
    assert(run({{"Action","TypeMarks"},{"Item",row}}).at("Junk").get<bool>());
    assert(HelpyPersonalization::Set(HelpyPersonalization::TypeKey(row),{{"Tags",Json::array({"JuNk"})}}));
    assert(run({{"Action","Marks"},{"Item",other}}).at("Junk").get<bool>());
    for(const auto* action:{"PrepareDelete","ObserveDelete","BeginRestore","FinishRestore","ScrapPlan","BeginScrap","ObserveScrap","FinishScrap","StorageRead"})Reject([&]{run({{"Action",action}});});
    assert(!std::filesystem::exists(directory/"storage"));
    const std::string character="11111111aaaa22223333333344444444";
    const Json profile={{"ProfileId","12345678123412341234123412345678"},{"ProfileName","Account"},{"Social",{{"Discord","my.user"}}},{"Platforms",Json::array({"Game Pass"})}};
    assert(HelpyInventoryService::Mutating("ReceivePublicProfile"));
    assert(run({{"Action","ReceivePublicProfile"},{"CharacterGuid",character},{"Profile",profile}}).at("Accepted")==true);
    assert(run({{"Action","ReadPublicProfile"},{"CharacterGuid",character}}).at("Profile")==profile);
    auto bad=profile;bad["Permissions"]="admin";Reject([&]{run({{"Action","ReceivePublicProfile"},{"CharacterGuid",character},{"Profile",bad}});});
    run({{"Action","ClearPublicProfiles"}});assert(run({{"Action","ReadPublicProfile"},{"CharacterGuid",character}}).at("Profile").is_null());
}
