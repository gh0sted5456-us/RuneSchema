#include "Core/ItemAuthoringDraft.h"
#include <cassert>
#include <memory>
using namespace PS::ItemAuthoring;
template<class F> void rejects(F&& f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
int main(){
    auto owner=std::make_unique<Draft>();auto& d=*owner;
    d.SetSource({{"Path","/Game/Items/Sword.Sword"},{"Name","Sword"}});assert(d.InspectWanted&&!d.Ready);
    d.Inspection={{"Fields",Json::array({{{"Name","PowerLevel"},{"Editable",true}},{{"Name","@row/WeaponStats/DamageMultiplier"},{"Editable",true}},{{"Name","PersistenceID"},{"Editable",false}}})}};d.Ready=true;
    d.Appearance={{"Path","/Game/Items/Axe.Axe"}};
    std::snprintf(d.Editor.data(),d.Editor.size(),"%s",R"({ // source values
      "PowerLevel": 10, "@row/WeaponStats/DamageMultiplier": 3.5
    })");
    const auto creation=Request(d,Mode::Creation,"",true);assert(creation["Action"]=="PrepareClone"&&creation["Give"]==false&&creation["Permanent"]==true&&!creation.contains("Player"));
    assert(creation["Overrides"]["PowerLevel"]==10&&creation["StatOverrides"]["WeaponStats"]["DamageMultiplier"]==3.5&&creation["AppearanceSource"]=="");
    const auto transmog=Request(d,Mode::Transmog,"",true);assert(transmog["Source"]=="/Game/Items/Sword.Sword"&&transmog["AppearanceSource"]=="/Game/Items/Axe.Axe"&&transmog["Overrides"].empty()&&transmog["StatOverrides"].empty());
    d.Source["Icon"]="/Game/UI/Sword.Sword";d.Appearance["Icon"]="/Mod/UI/Axe.Axe";
    assert(Preview(d,Mode::Transmog)["Icon"]=="/Mod/UI/Axe.Axe");
    d.Result={{"State","Prepared"},{"Icon","/Mod/UI/NativeSkin.NativeSkin"}};
    assert(Preview(d,Mode::Transmog)["Icon"]=="/Mod/UI/NativeSkin.NativeSkin");
    d.Result={{"State","Created"},{"Icon","/Mod/UI/NativeSkin.NativeSkin"},{"Path","/Game/RuneSchema/Items/Clone.Clone"}};d.Created=true;
    assert(Preview(d,Mode::Transmog)["Path"]=="/Game/RuneSchema/Items/Clone.Clone");
    d.Changed();assert(d.Result.empty()&&Preview(d,Mode::Transmog)["Icon"]=="/Mod/UI/Axe.Axe");
    d.Appearance["Icon"]="";assert(Preview(d,Mode::Transmog)["Icon"]=="/Game/UI/Sword.Sword");
    rejects([&]{Request(d,Mode::Creation,"player",false);});d.Acknowledged=true;assert(Request(d,Mode::Creation,"player",false)["Action"]=="CreateClone");
    Json direct,stats;rejects([&]{SplitValues({{"PersistenceID","bad"}},d.Inspection,direct,stats);});rejects([&]{SplitValues({{"Unknown",1}},d.Inspection,direct,stats);});rejects([]{ParseValues("[]");});rejects([]{ParseValues("{bad}");});
    const auto id=d.Id;d.Created=true;d.Changed();assert(d.Id.empty()&&!d.Created&&!d.Acknowledged);
    auto other=std::make_unique<Draft>();other->SetSource({{"Path","/Game/Items/Ore.Ore"}});assert(d.Source["Path"]=="/Game/Items/Sword.Sword"&&d.Appearance["Path"]=="/Game/Items/Axe.Axe");
    const auto root=std::filesystem::temp_directory_path()/("helpy-authoring-"+std::to_string(GetCurrentProcessId()));
    const auto internal="RS_v5_"+id;const auto target="/Game/Items/"+internal+"."+internal;
    Json prepared={{"State","Prepared"},{"InternalName",internal}};
    prepared["Documents"]["assets"][target]={{"$Clone","/Game/Items/Sword.Sword"},{"PersistenceID",id}};
    prepared["Documents"]["raw"]["/Game/Stats.Table"][id]={{"DamageMultiplier",3.5}};
    const auto first=Export(root,prepared),second=Export(root,prepared);assert(first!=second);
    assert(Json::parse(PS::ConfigFiles::Read(first/"assets"/(internal+".jsonc")),nullptr,true,true)==prepared["Documents"]["assets"]);
    assert(Json::parse(PS::ConfigFiles::Read(first/"raw"/(internal+".jsonc")),nullptr,true,true)==prepared["Documents"]["raw"]);
    auto bad=prepared;bad["InternalName"]="../escape";rejects([&]{Export(root,bad);});
    std::filesystem::remove_all(root);
}
