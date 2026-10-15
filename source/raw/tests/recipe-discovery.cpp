#include "Loader/RecipeDiscovery.h"
#include "Loader/RecipeUnlockPolicy.h"
#include "Core/CookedPakRegistryManifest.h"
#include <stdexcept>
using nlohmann::json;
static void Check(bool value){if(!value)throw std::runtime_error("Recipe discovery regression");}
template<class F>static void Reject(F&& operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main(){
    using namespace PS::RecipeDiscovery;
    const std::string dye="/Example/Items/ITEM_Dye.ITEM_Dye",armor="/Example/Items/ITEM_Armor.ITEM_Armor";
    const json simple={{"Discovery",{{"AllItems",{dye,armor}}}}};
    const auto rule=Parse(simple);Check(rule.has_value());
    Check(rule->Query.at("FirstOperand").at("UnlockType")=="EProgressUnlockType::ItemsPickedUp");
    Check(rule->Query.at("FirstOperand").at("Condition")=="EUnlockMatchCondition::AllMatch");
    Check(rule->Query.at("FirstOperand").at("Items")==json::array({dye,armor}));
    Check(rule->Query.at("bUseSecondOperand")==false);
    Check(!PS::RecipeUnlockPolicy::Automatic(simple));
    Check(!Parse(json::object()));Check(!Parse({{"Discovery","Native"}}));
    Check(PS::RecipeUnlockPolicy::Automatic({{"Unlock",true}}));
    const json compound={{"Discovery",{{"All",json::array({{{"AnyItems",{dye,armor}}},
        {{"AllActors",{"/Example/Stations/BP_DyeStation.BP_DyeStation_C"}}}})}}}};
    const auto combined=Parse(compound);
    Check(combined->Query.at("bUseSecondOperand")==true);
    Check(combined->Query.at("Operator")=="EUnlockMatchOperator::AND");
    Check(combined->Query.at("SecondOperand").at("UnlockType")=="EProgressUnlockType::ActorsInteractedWith");
    auto any=compound;any["Discovery"]={{"Any",compound.at("Discovery").at("All")}};
    Check(Parse(any)->Query.at("Operator")=="EUnlockMatchOperator::OR");
    const json skill={{"Discovery",{{"SkillLevel",{{"Skill","/Game/Skills/SKILL_Artisan.SKILL_Artisan"},{"Level",5}}}}}};
    Check(Parse(skill)->Query.at("FirstOperand").at("RequiredSkillLevel")==5);
    auto badSkill=skill;badSkill["Discovery"]["SkillLevel"]["Level"]=0;Reject([&]{Parse(badSkill);});
    badSkill=skill;badSkill["Discovery"]["SkillLevel"]["Level"]=2.5;Reject([&]{Parse(badSkill);});
    auto ambiguous=simple;ambiguous["Unlock"]=true;Reject([&]{PS::RecipeUnlockPolicy::Automatic(ambiguous);});
    Reject([]{Parse({{"Discovery",{{"AllItems",json::array()}}}});});
    Reject([&]{Parse({{"Discovery",{{"AllItems",{dye,dye}}}}});});
    Reject([]{Parse({{"Discovery",{{"LearnedRecipes",{"Unverified"}}}}});});
    Reject([&]{Parse({{"Discovery",{{"All",json::array({simple,simple,simple})}}}});});
    Reject([&]{Parse({{"Discovery",{{"All",json::array({{{"Any",json::array({simple})}}})}}}});});
    Reject([]{Parse({{"Discovery","Automatic"}});});
    for(const auto& path:{"ITEM_Dye","/Mod/NoDot","/Mod/Bad.","/Mod/Bad Name.Bad","/Script/NotContent.NotContent","/Mod/../Bad.Bad"})Reject([&]{Path(path);});
    // A condition replacement removes only the selected recipe links from a
    // shared native bundle; other recipes retain order, duplicates and policy.
    Check(RetainOtherRecipes({"A","B","A","C"},{"B"})==std::vector<std::string>({"A","A","C"}));
    Check(RetainOtherRecipes({"A"},{"missing"})==std::vector<std::string>({"A"}));
    Check(RetainOtherRecipes({"A"},{"A"}).empty());
    // The same body can ship in a cooked manifest without loose pointer files.
    PS::CookedPakRegistryManifest::Reset();
    const std::string recipe="/Example/Recipes/RECIPE_Dye.RECIPE_Dye";
    Check(PS::CookedPakRegistryManifest::Publish({{"LoaderDefinitions",{{"Recipes",{{recipe,simple}}}}}},"Example","pak:/Example.RSREG_Recipes"));
    const auto cooked=PS::CookedPakRegistryManifest::Definitions("Recipes");
    Check(cooked.size()==1&&Parse(cooked.at(0).Body)->Query==rule->Query);
}
