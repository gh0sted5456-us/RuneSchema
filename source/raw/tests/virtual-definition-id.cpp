#include "Loader/VirtualDefinitionId.h"
#include <cassert>
#include "Loader/GameplayEffectDocument.h"
#include "Loader/DefinitionRegistry.h"

int main() {
    using namespace DragonWilds::VirtualDefinitionId;
    assert(IsCanonical("ArmorMod:Effects/HeavyArmor"));
    assert(IsCanonical("ArmorMod:Nameplates/Elite"));
    assert(!IsCanonical("Effects/HeavyArmor"));
    assert(!IsCanonical("ArmorMod:../HeavyArmor"));
    assert(Qualify("ArmorMod","Effects/HeavyArmor")=="ArmorMod:Effects/HeavyArmor");
    assert(Qualify("ArmorMod","Other:Effects/HeavyArmor")=="Other:Effects/HeavyArmor");
    assert(!IsCanonical("Armor/Mod:Effect"));
    assert(!IsCanonical("ArmorMod:Effects//HeavyArmor"));
    using nlohmann::json;
    DragonWilds::DefinitionRegistry::Niagara["Mod:Fire"]={{"Type","Niagara"},{"System","/Game/Test.Test"},{"Parameters",{{"User.Intensity",1.0},{"User.Enabled",true}}}};
    auto visual=DragonWilds::DefinitionRegistry::Visual(json{{"Definition","Mod:Fire"},{"Parameters",{{"User.Intensity",2.0}}}});
    assert(visual["Type"]=="Niagara" && visual["Parameters"]["User.Enabled"]==true && visual["Parameters"]["User.Intensity"]==2.0);
    assert(DragonWilds::DefinitionRegistry::Niagara["Mod:Fire"]["Parameters"]["User.Intensity"]==1.0);
    bool missing=false;
    try { (void)DragonWilds::DefinitionRegistry::Visual(json{{"Definition","Mod:Missing"}}); } catch(const std::exception&) { missing=true; }
    assert(missing);
    DragonWilds::DefinitionRegistry::Niagara.clear();
    using DragonWilds::GameplayEffectDocument::Parse;
    const std::string path="/Game/Effects/GE_Test.GE_Test_C";
    auto alias=Parse(json{{"Effects/Test",{{"Class",path}}}},"ArmorMod")[0];
    assert(alias.Key=="ArmorMod:Effects/Test" && alias.ClassPath==path);
    assert(Parse(json{{"ArmorMod:Effects/Next",{{"Class",path},{"$Comment","verified"}}}},"ArmorMod").size()==1);
    for(const auto& bad:std::vector<json>{json::array(),json{{"$Patch",path}},json{{"Other:Effects/Test",{{"Class",path}}}},json{{"Effects/Test",{{"$Clone",path}}}},json{{"Effects/Test",{{"Class","/Game/Effects/GE_Test.GE_Test"}}}}}) {
        bool rejected=false;
        try { (void)Parse(bad,"ArmorMod"); } catch(const std::exception&) { rejected=true; }
        assert(rejected);
    }
}
