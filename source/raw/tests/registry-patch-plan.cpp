#include "Loader/CharacterCustomizationPlan.h"
#include <cassert>
#include <iostream>

using nlohmann::json;

int main() {
    const auto generic=json::parse(R"({
      "schema":"runeschema.registry-patch/v1","modId":"ExampleMod","profile":"dragonwilds.dataTableOwnedRows.v1",
      "patches":[
        {"id":"base","target":{"kind":"dataTable","objectPath":"/Game/Mods/Example/DT_Test.DT_Test","expectedRowStruct":"/Script/Example.TestRow"},
         "operation":"addRow","row":{"name":"auto","values":{"Enabled":true}}},
        {"id":"second","dependsOn":["base"],"target":{"kind":"dataTable","shortName":"DT_Test","expectedRowStruct":"/Script/Example.TestRow"},
         "operation":"copyRow","row":{"name":"RS_Test","template":{"rowName":"Vanilla"},"values":{"Weight":2}}}
      ]})");
    const auto document=DragonWilds::RegistryPatch::ParseDocument(generic,"ExampleMod","patches/test.json");
    const auto plan=DragonWilds::RegistryPatch::BuildPlan({document});
    assert(plan.size()==2&&plan[0].Id=="base"&&plan[1].Id=="second");
    assert(DragonWilds::RegistryPatch::StableRowName("RS",plan[0].CanonicalId)==DragonWilds::RegistryPatch::StableRowName("RS",plan[0].CanonicalId));
    auto bad=generic;bad["modId"]="AnotherMod";bool rejected=false;try{(void)DragonWilds::RegistryPatch::ParseDocument(bad,"ExampleMod","bad.json");}catch(...){rejected=true;}assert(rejected);
    bad=generic;bad["patches"][0]["operation"]="removeRow";rejected=false;try{(void)DragonWilds::RegistryPatch::ParseDocument(bad,"ExampleMod","bad.json");}catch(...){rejected=true;}assert(rejected);

    const auto hair=json::parse(R"({"schema":"runeschema.character-customization/v1","modId":"HairMod","entries":[{
      "id":"hair-51","type":"hair","visible":true,"display":{"name":"Hair 51","sortOrder":51},
      "compatibility":{"bodyType":"both","faceTypes":"all","eyeTypes":"all"},
      "zones":{"zone1":{"mesh":"/Game/Mods/Hair/Mesh.SK_Hair","animClass":"/Game/Player/ABP_Hair.ABP_Hair_C"}}
    }]})");
    const auto expanded=DragonWilds::CharacterCustomization::Expand(hair,"HairMod","character_customization/hair.json");
    assert(expanded.size()==3);
    assert(expanded[0].Row.at("name").get<std::string>().starts_with("RS_HZ_"));
    assert(expanded[1].Row.at("name").get<std::string>().starts_with("RS_HP_"));
    assert(expanded[2].Op==DragonWilds::RegistryPatch::Operation::AppendUnique);
    auto hidden=hair;hidden["entries"][0]["visible"]=false;
    assert(DragonWilds::CharacterCustomization::Expand(hidden,"HairMod","hidden.json").size()==2);
    std::cout<<"registry patch plan passed\n";
}
