#include "Core/HelpyInventoryService.h"
#include <cassert>
using namespace PS;using Json=nlohmann::json;
template<class F>void Reject(F f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(int argc,char**argv){
    assert(argc==2);const auto directory=std::filesystem::path(argv[1])/std::to_string(GetCurrentProcessId());
    assert(HelpyPersonalization::Load(directory/"personalization.json"));
    const Json row={{"Path","/Game/Items/Axe.Axe"},{"PlayerGuid","11111111AAAA22223333333344444444"},{"Instance",{1,2,3,4}},{"Count",2}};
    const auto run=[&](const Json& q){return HelpyInventoryService::Run(directory,q);};
    assert(run({{"Action","Capabilities"}})["NativeRestore"]==false);
    assert(HelpyInventoryService::Mutating("PrepareDelete")&&!HelpyInventoryService::Mutating("StorageRead"));
    run({{"Action","Mark"},{"Item",row},{"Scope","Stack"},{"Field","Favorite"},{"Value",true}});
    assert(HelpyPersonalization::Favorite(row));
    auto other=row;other["Instance"]={5,6,7,8};assert(!HelpyPersonalization::Favorite(other));
    run({{"Action","Mark"},{"Item",row},{"Scope","Type"},{"Field","Junk"},{"Value",true}});
    assert(HelpyPersonalization::Tagged(row,"junk")&&HelpyPersonalization::Tagged(other,"junk"));
    const Json recipe={{"Path","/Game/Recipes/Axe.Axe"},{"Inputs",Json::array({{{"Path","/Game/Items/Wood.Wood"},{"Count",3}}})},
        {"Outputs",Json::array({{{"Path","/Game/Items/Axe.Axe"},{"Count",1}}})},{"Deterministic",true}};
    const Json request={{"Action","ScrapPlan"},{"Item",row},{"Quantity",2},{"Recipes",Json::array({recipe})}};
    Reject([&]{run(request);});
    run({{"Action","Mark"},{"Item",row},{"Scope","Stack"},{"Field","Favorite"},{"Value",false}});
    assert(run(request)["Plan"]["Refunds"][0]["Count"]==6);
    Reject([&]{run({{"Action","Mark"},{"Item",row},{"Scope","World"},{"Field","Favorite"},{"Value",false}});});
    assert(run({{"Action","StorageRead"},{"CharacterGuid",row["PlayerGuid"]}})["Items"].empty());
    const Json stored={{"AssetPath",row["Path"]},{"PersistenceID","Test-Axe-01"},{"InstanceGuid","aaaaaaaa111111112222222233333333"},{"Count",2},
        {"NativeItem",{{"ItemData","Test-Axe-01"},{"GUID","aaaaaaaa111111112222222233333333"},{"Count",2},{"ModState",std::string(8192,'x')}}}};
    run({{"Action","PrepareDelete"},{"CharacterGuid",row["PlayerGuid"]},{"CaptureId","delete1"},{"Item",stored}});
    run({{"Action","ObserveDelete"},{"CharacterGuid",row["PlayerGuid"]},{"CaptureId","delete1"},{"Result","Removed"}});
    const auto revision=run({{"Action","StorageRead"},{"CharacterGuid",row["PlayerGuid"]}}).at("Revision");
    const auto ack=run({{"Action","BeginRestore"},{"CharacterGuid",row["PlayerGuid"]},{"StorageId","delete1"},{"Attempt","restore1"},{"Revision",revision},{"AlreadyPresent",false}});
    assert(ack.at("Prepared")==true&&ack.dump().size()<HelpyInventoryService::MutationReplyBytes&&!ack.contains("Items"));
    HelpySettings::Configure(directory/"plugin");
    auto scrap=stored;scrap["InstanceGuid"]="00000001000000020000000300000004";scrap["NativeItem"]["GUID"]=scrap["InstanceGuid"];
    const auto plan=run(request).at("Plan");
    const Json begin={{"Action","BeginScrap"},{"CharacterGuid",row["PlayerGuid"]},{"CaptureId","scrap1"},{"Item",scrap},{"Plan",plan}};
    Reject([&]{run(begin);});assert(!HelpyStorage::Store(directory/"storage",row["PlayerGuid"]).Read()["Items"].contains("scrap1"));
    assert(HelpySettings::SetScrapRefunds(true));
    run({{"Action","Mark"},{"Item",row},{"Scope","Stack"},{"Field","Locked"},{"Value",true}});Reject([&]{run(begin);});
    run({{"Action","Mark"},{"Item",row},{"Scope","Stack"},{"Field","Locked"},{"Value",false}});
    assert(run(begin)["Prepared"]==true);Reject([&]{run(begin);});
    assert(HelpySettings::SetScrapRefunds(false)); // A preference change must not strand an already-started receipt.
    run({{"Action","ObserveScrap"},{"CharacterGuid",row["PlayerGuid"]},{"CaptureId","scrap1"},{"Result","Removed"}});
    run({{"Action","FinishScrap"},{"CharacterGuid",row["PlayerGuid"]},{"CaptureId","scrap1"},{"Confirmed",true},
        {"Drops",Json::array({{{"Path","/Game/Items/Wood.Wood"},{"Count",6},{"Actor","/Game/Maps/World/L_World.L_World:PersistentLevel.Refund"}}})}});
    const auto visible=run({{"Action","StorageRead"},{"CharacterGuid",row["PlayerGuid"]}});assert(visible["Items"].size()==1);
    Reject([&]{run({{"Action","StorageRead"},{"CharacterGuid","../other"}});});
}
