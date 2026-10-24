#include "Core/CookedEnumContract.h"
#include <stdexcept>
static void Check(bool ok){if(!ok)throw std::runtime_error("Cooked enum/skill declaration regression");}
template<class F>static void Reject(F fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main(){
    namespace E=PS::CookedEnumContract;namespace M=PS::CookedPakRegistryManifest;
    const std::string path="/Game/Mods/Shadowmancy/Enums/E_Mode.E_Mode";
    const E::Symbols symbols{{"E_Mode::Idle",0},{"E_Mode::Active",1},{"E_Mode::E_Mode_MAX",2}};
    const auto key=E::Fingerprint("Shadowmancy",path,symbols);
    auto reordered=symbols;std::ranges::reverse(reordered);Check(E::Fingerprint("Shadowmancy",path,reordered)==key);
    auto changed=symbols;changed[1].second=3;Check(E::Fingerprint("Shadowmancy",path,changed)!=key);
    Reject([&]{E::Fingerprint("Other",path,symbols);});
    Reject([&]{E::Fingerprint("Game","/Game/Native/E_Mode.E_Mode",symbols);});
    Reject([&]{E::Fingerprint("Shadowmancy","/Script/Dominion.ESkill",symbols);});
    Reject([&]{E::Fingerprint("Shadowmancy",path,{});});
    for(const auto& bad:E::Symbols{{"Duplicate",1},{"bad\nname",5},{"tooLarge",256},{"negative",-1}}){
        auto invalid=symbols;invalid.push_back(bad);Reject([&]{E::Fingerprint("Shadowmancy",path,invalid);});
    }
    auto duplicates=symbols;duplicates.push_back({"e_mode::idle",5});Reject([&]{E::Fingerprint("Shadowmancy",path,duplicates);});
    Reject([&]{E::RequireLegacyAppendSafe("/Script/Dominion.ESkill");});
    Reject([&]{E::RequireLegacyAppendSafe("/SCRIPT/DOMINION.ESKILL");});
    E::RequireLegacyAppendSafe(path);
    const std::string skill="/Game/Mods/Shadowmancy/Skills/SKILL_Shadowmancy.SKILL_Shadowmancy";
    const std::string perk="/Game/Mods/Shadowmancy/Perks/PERK_First.PERK_First";
    nlohmann::json doc={{"NativeRegistries",{{"Skills",{skill}},{"SkillPerks",{perk}},{"Enums",{path}}}}};
    auto parsed=M::Parse(doc,"Shadowmancy","test");Check(parsed&&parsed->AssetLanes.size()==3);
    M::Reset();M::Publish(doc,"Shadowmancy","first");M::Publish(doc,"Shadowmancy","first");Check(M::Snapshot().size()==1);
    M::Publish(doc,"Shadowmancy","second");Check(M::Snapshot().size()==2);
    auto conflicting=doc;conflicting["NativeRegistries"]["SkillPerks"]={skill};
    Reject([&]{M::Publish(conflicting,"Shadowmancy","third");});Check(M::Snapshot().size()==2);
    for(const auto* lane:{"Skills","SkillPerks","Enums"}){
        auto invalid=doc;invalid["NativeRegistries"][lane]={"/Game/Native/Asset.Asset"};Reject([&]{M::Parse(invalid,"Shadowmancy","test");});
        invalid=doc;invalid["NativeRegistries"][lane]={{{"Path",skill},{"Source","Schema"}}};Reject([&]{M::Parse(invalid,"Shadowmancy","test");});
        invalid=doc;invalid["NativeRegistries"][lane]={path,"/game/mods/shadowmancy/enums/e_mode.e_mode"};Reject([&]{M::Parse(invalid,"Shadowmancy","test");});
    }
    M::Reset();
}
