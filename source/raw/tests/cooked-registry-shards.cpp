#include "Core/CookedRegistryShards.h"
#include "Core/SaveCleanup.h"
#include <map>
#include <stdexcept>
using namespace PS::CookedRegistryShards;
namespace M=PS::CookedPakRegistryManifest;
static void Check(bool value){if(!value)throw std::runtime_error("Cooked registry shard regression");}
template<class F>static void Reject(F&& action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Check(rejected);}
static Asset Make(Json document,std::string owner="Dye"){const auto text=document.dump();return {std::move(owner),ParseJson(text),text.size()};}
static std::string Child(const std::string& id){const auto name="RSREG_Dye_"+id;return "/Game/Mods/Dye/Registry/Shards/"+name+"."+name;}
static Json Payload(const std::string& recipe){
    const Json body={{"Unlock",false},{"Discovery",{{"AllItems",{"/Example/Items/ITEM_Dye.ITEM_Dye"}}}}};
    return {{"SchemaVersion",1},{"Entries",Json::array()},{"RegistryShard",true},{"LoaderDefinitions",{{"Recipes",{{recipe,body}}}}}};
}
int main(){
    M::Reset();PS::RegistryEnrollment::Reset();
    const std::string recipeA="/Example/Recipes/RECIPE_A.RECIPE_A",recipeB="/Example/Recipes/RECIPE_B.RECIPE_B";
    const auto a=Child("0001"),b=Child("0002"),rootPath=RootPath("Dye");
    Check(ReservedShardPath(a)&&ReservedShardPath(Fold(a))&&!ReservedShardPath(rootPath));
    const auto root=Make({{"SchemaVersion",1},{"Entries",Json::array()},{"RegistryShards",{a,b}}});
    std::map<std::string,Asset> assets{{a,Make(Payload(recipeA))},{b,Make(Payload(recipeB))}};
    std::vector<std::string> visited;
    const auto load=[&](const std::string& path){visited.push_back(path);return assets.at(path);};
    const auto group=Resolve(rootPath,root,load);
    Check(visited==std::vector<std::string>({a,b})&&group.Paths.size()==3&&group.Manifests.size()==2&&group.References==2);
    Check(M::Snapshot().empty()); // Resolution alone never publishes a prefix.
    M::PublishBatch(group.Manifests);Check(M::Definitions("Recipes").size()==2);
    const auto count=M::Snapshot().size();M::PublishBatch(group.Manifests);Check(M::Snapshot().size()==count);
    const auto fail=[&](const Asset& index){const auto before=M::Snapshot().size();Reject([&]{const auto candidate=Resolve(rootPath,index,load);M::PublishBatch(candidate.Manifests);});Check(M::Snapshot().size()==before);};
    assets.erase(b);fail(root);assets[b]=Make(Payload(recipeB));
    auto wrong=assets[b];wrong.Owner="OtherMod";assets[b]=wrong;fail(root);assets[b]=Make(Payload(recipeB));
    auto nested=Payload(recipeB);nested["RegistryShards"]={a};assets[b]=Make(nested);fail(root);assets[b]=Make(Payload(recipeB));
    auto marker=Payload(recipeB);marker.erase("RegistryShard");assets[b]=Make(marker);fail(root);
    marker=Payload(recipeB);marker["RegistryShard"]=false;assets[b]=Make(marker);fail(root);
    marker=Payload(recipeB);marker["Entries"].push_back({{"Id","not a native shard"}});assets[b]=Make(marker);fail(root);
    marker=Payload(recipeB);marker["Unknown"]=true;assets[b]=Make(marker);fail(root);assets[b]=Make(Payload(recipeB));
    auto conflict=Payload(recipeA);conflict["LoaderDefinitions"]["Recipes"][recipeA]["Unlock"]=true;assets[b]=Make(conflict);fail(root);assets[b]=Make(Payload(recipeB));
    for(const auto& path:std::vector<std::string>{"/Other/Registry/RSREG_Dye_0001.RSREG_Dye_0001",
        "/Game/Mods/Other/Registry/Shards/RSREG_Dye_0001.RSREG_Dye_0001",
        "/Game/Mods/Dye/Registry/Shards/../RSREG_Dye_0001.RSREG_Dye_0001",
        "/Game/Mods/Dye/Registry/Shards/Nested/RSREG_Dye_0001.RSREG_Dye_0001",
        "/Game/Mods/Dye/Registry/Shards/RSREG_Dye_0001.Different",
        "/Game/Mods/Dye/Registry/Shards/RSREG_Dye_0001.RSREG_Dye_0001:Subobject",rootPath}){
        auto invalid=root;invalid.Document["RegistryShards"][1]=path;visited.clear();fail(invalid);Check(visited.empty());
    }
    auto duplicate=root;duplicate.Document["RegistryShards"]={a,a};visited.clear();fail(duplicate);Check(visited.empty());
    duplicate.Document["RegistryShards"]={Child("A"),Child("a")};fail(duplicate);
    auto tooMany=root;tooMany.Document["RegistryShards"]=Json::array();for(int i=0;i<257;++i)tooMany.Document["RegistryShards"].push_back(Child(std::to_string(i)));fail(tooMany);
    auto wrongVersion=root;wrongVersion.Document["SchemaVersion"]=2;fail(wrongVersion);
    auto empty=root;empty.Document["RegistryShards"]=Json::array();fail(empty);
    auto rootShard=root;rootShard.Document["RegistryShard"]=true;fail(rootShard);
    Reject([&]{Resolve("/Game/Other.Root",root,load);});
    Reject([]{ParseJson("{\"Entries\":[],\"Entries\":[]}");});
    Reject([]{ParseJson(std::string(MaxDocumentBytes+1,' '));});
    Reject([]{ParseJson(std::string(70,'[')+"0"+std::string(70,']'));});
    auto oversized=assets[b];oversized.Bytes=MaxDocumentBytes+1;assets[b]=oversized;fail(root);assets[b]=Make(Payload(recipeB));
    // Aggregate byte and reference ceilings are independent of per-page caps.
    auto byteRoot=root;byteRoot.Document["RegistryShards"]=Json::array();
    for(int i=0;i<256;++i){auto path=Child("Byte"+std::to_string(i));byteRoot.Document["RegistryShards"].push_back(path);auto value=Make(Payload(recipeA));value.Bytes=MaxDocumentBytes;assets[path]=std::move(value);}fail(byteRoot);
    Json paths=Json::array();for(int i=0;i<1024;++i){const auto name="I"+std::to_string(i);paths.push_back("/E/"+name+"."+name);}
    const Json refPage={{"SchemaVersion",1},{"Entries",Json::array()},{"RegistryShard",true},{"NativeRegistries",{{"Items",paths},{"Recipes",paths},{"Quests",paths},{"EquipmentEffects",paths}}}};
    auto refRoot=root;refRoot.Document["RegistryShards"]=Json::array();
    for(int i=0;i<65;++i){auto path=Child("Ref"+std::to_string(i));refRoot.Document["RegistryShards"].push_back(path);assets[path]=Make(refPage);}fail(refRoot);
    // A conflict against previously published declarations cannot commit any
    // earlier page from this batch, and must not authorize save pruning.
    auto late=group.Manifests;late[1].LoaderDefinitions["Recipes"][recipeA]={{"Unlock",true}};
    Reject([&]{M::PublishBatch(late);});Check(M::Snapshot().size()==count);
    PS::SaveCleanup::RegistrySnapshot registry;for(int i=0;i<500;++i)registry.Items.insert("I"+std::to_string(i));
    for(int i=0;i<300;++i)registry.Recipes.insert("R"+std::to_string(i));Check(registry.Ready());
    PS::RegistryEnrollment::Reject("pak:"+rootPath,"a required shard was unavailable");Check(!registry.Ready());PS::RegistryEnrollment::Reset();
    // A catalogue larger than a single 4096-entry lane resolves with no
    // Asset Registry enumeration at all. Preserve exact recipe identities.
    M::Reset();assets.clear();auto largeRoot=root;largeRoot.Document["RegistryShards"]=Json::array();
    constexpr int recipeCount=21982,pageSize=200;
    for(int start=0;start<recipeCount;start+=pageSize){Json definitions=Json::object();
        for(int i=start;i<std::min(start+pageSize,recipeCount);++i){const auto name="RECIPE_"+std::to_string(i);definitions["/Example/Recipes/"+name+"."+name]={{"Unlock",false},{"Discovery",{{"AllItems",{"/Example/ITEM_Dye.ITEM_Dye","/Example/ITEM_Armor.ITEM_Armor"}}}}};}
        const auto path=Child("Large"+std::to_string(start));largeRoot.Document["RegistryShards"].push_back(path);
        assets[path]=Make({{"SchemaVersion",1},{"Entries",Json::array()},{"RegistryShard",true},{"LoaderDefinitions",{{"Recipes",definitions}}}});
    }
    // Recalculate actual index bytes after constructing its child list.
    largeRoot=Make(largeRoot.Document);const auto large=Resolve(rootPath,largeRoot,load);
    Check(large.References==recipeCount&&large.Paths.size()==111);M::PublishBatch(large.Manifests);
    Check(M::Definitions("Recipes").size()==recipeCount);
}
