#include "Generator/LoaderSchemas.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#undef assert
#define assert(expression) do { if(!(expression)) throw std::runtime_error("Assertion failed: " #expression); } while(false)
int RunLoaderSchemas(int argc,char** argv) {
    const auto schemas=PS::JsonSchemaGenerator::LoaderSchemas();
    assert(schemas.size()==22);
    assert(schemas["registry"]["properties"]["NativeRegistries"]["properties"].size()==24);
    for(const auto& family:PS::NativeDataFamilies::Families)assert(schemas["registry"]["properties"]["NativeRegistries"]["properties"][family.Lane]["items"]["type"]=="string");
    for(const auto* lane:{"Skills","SkillPerks","Enums"})assert(schemas["registry"]["properties"]["NativeRegistries"]["properties"][lane]["$ref"]=="#/definitions/modOwnedCookedPaths");
    assert(schemas["registry"]["properties"]["LoaderDefinitions"]["properties"].size()==4);
    assert(schemas["registry"]["properties"]["NativeRegistries"]["properties"]["Buildings"]["items"]["type"]=="string");
    assert(schemas["registry"]["properties"]["LoaderDefinitions"]["properties"]["Buildings"]["additionalProperties"]["additionalProperties"]==false);
    assert(schemas["assets"]["patternProperties"]["^(?!(?:RuneSchema|Modded)$)[^$]"]["properties"]["$WearableBridge"]["type"]=="boolean");
    const auto& humanEquipment=schemas.at("npc").at("anyOf").at(0).at("anyOf").at(0).at("properties").at("Equipment");
    assert(humanEquipment.at("properties").contains("Necklace"));
    assert(humanEquipment.at("properties").contains("Trinket2"));
    assert(humanEquipment.at("not").at("required")==nlohmann::json::array({"Necklace","Trinket2"}));
    assert(!schemas.contains("animations"));
    assert(!schemas.contains("behaviors"));
    assert(schemas["lore"]["patternProperties"]["^[^$]"]["properties"]["Type"]["const"]=="Lore");
    assert(schemas["journal"]["patternProperties"]["^[^$]"]["properties"].contains("StationTableRowHandle"));
    for(const auto* name:{"journal","lore"}) {
        const auto& properties=schemas[name]["patternProperties"]["^[^$]"]["properties"];
        const auto& condition=properties.at("UnlockCondition");
        assert(condition.at("required").at(0)=="UnlockType");
        assert(condition.at("properties").contains("UnlockingItemData"));
        assert(condition.at("properties").contains("UnlockingRecipe"));
        assert(condition.at("properties").contains("UnlockingInteractionActor"));
        assert(condition.at("properties").contains("AIData"));
        assert(condition.at("allOf").size()==4);
        assert(!properties.at("Unlock").contains("default"));
    }
    assert(schemas["lore"]["patternProperties"]["^[^$]"]["properties"].contains("PageDescriptions"));
    assert(schemas.contains("events"));
    assert(schemas.contains("quests"));
    for(const auto* name:{"assets","blueprints","buildings","courses","effects","enums","equipment","journal","nameplates","niagara","players","raw","recipes","spawns","strings","vendors"}) {
        assert(schemas.contains(name));
        assert(schemas[name]["x-runeschema-coverage"]=="structural");
        assert(schemas[name]["$schema"]=="http://json-schema.org/draft-07/schema#");
    }
    const auto check=[&](auto&& self,const nlohmann::json& node)->void {
        if(node.is_object()) {
            for(const auto* key:{"anyOf","allOf","oneOf"})if(node.contains(key)) {
                if(!node[key].is_array())throw std::runtime_error(std::string("Schema composition is not an array: ")+key+" "+node.dump());
                for(const auto& branch:node[key])assert(branch.is_object() || branch.is_boolean());
            }
            if(node.contains("properties"))for(const auto& value:node["properties"])
                assert(value.is_object() || value.is_boolean());
            for(const auto& value:node)self(self,value);
        }else if(node.is_array())for(const auto& value:node)self(self,value);
    };
    check(check,schemas);
    const auto& location=schemas["quests"]["anyOf"][0]["properties"]["Objective"]["oneOf"][0]["properties"]["Location"];
    assert(location["minItems"]==3 && location["maxItems"]==3);
    assert(location["items"]["minimum"]==-100000000 && location["items"]["maximum"]==100000000);
    assert(schemas["quests"]["anyOf"][1]["items"]["properties"]["Objective"]["oneOf"][0]["properties"]["Location"]==location);
    const auto& objective=schemas["quests"]["anyOf"][0]["properties"]["Objective"]["oneOf"][0];
    const auto& kill=schemas["quests"]["anyOf"][0]["properties"]["Objective"]["oneOf"][1];
    assert(kill["properties"]["Type"]["const"]=="Kill" && !kill["properties"].contains("Item"));
    assert(kill["properties"]["AIClasses"]["uniqueItems"]==true);
    assert(kill["properties"].contains("SpawnID"));
    assert(kill["dependencies"]["SpawnID"]==nlohmann::json::array({"EventID"}));
    const auto& questSchema=schemas["quests"]["anyOf"][0];
    assert(questSchema["properties"]["Region"]["type"]=="string");
    assert(!questSchema["properties"]["Region"].contains("enum"));
    assert(schemas["spawns"]["items"]["properties"].contains("GroundReferenceZ"));
    assert(schemas["spawns"]["items"]["properties"]["GroundMigrationRadius"]["default"]==300);
    assert(schemas["spawns"]["items"]["allOf"][0]["then"]["properties"]["GroundMigrationRadius"]["maximum"]==1000);
    assert(schemas["npc"]["anyOf"][0]["anyOf"][0]["properties"]["GroundClearance"]["required"].size()==2);
    assert(humanEquipment.is_object());
    assert(questSchema["properties"]["ShowInCompletedQuestList"]["default"]==true);
    assert(questSchema["properties"]["HideInQuestList"]["default"]==false);
    assert(questSchema["properties"]["CompletedDescription"]["maxLength"]==2048);
    for(const auto* field:{"SubobjectiveDescription","Hint","QuestDescriptionOverride"}) {
        assert(objective["properties"].contains(field));assert(kill["properties"].contains(field));
    }
    assert(questSchema["properties"]["Stages"]["items"]["properties"]["Objectives"]["items"]==questSchema["properties"]["Objective"]);
    assert(questSchema["oneOf"].size()==2);
    assert(questSchema["properties"]["Category"]["enum"]==nlohmann::json::array({"Regular","Story","Task"}));
    assert(questSchema["properties"].contains("TimeOfDay"));
    assert(questSchema["properties"]["Prerequisites"]["uniqueItems"]==true);
    assert(objective["dependencies"]["RadiusMeters"][0]=="Location");
    assert(objective["properties"]["RadiusMeters"]["maximum"]==10000);
    const auto& npcProperties=schemas["npc"]["anyOf"][0]["anyOf"][0]["properties"];
    assert(npcProperties.at("Rotation").at("type")=="object");
    assert(npcProperties.at("Rotation").at("required")==nlohmann::json::array({"Pitch","Yaw","Roll"}));
    const auto& spawnProperties=schemas.at("spawns").at("items").at("properties");
    assert(spawnProperties.at("Building").at("type")=="string");
    assert(schemas["spawns"]["items"]["allOf"][1]["then"]["properties"]["DuplicateRadius"]["exclusiveMinimum"]==0);
    assert(schemas["spawns"]["items"]["allOf"][1]["then"]["properties"]["AllowDeconstruction"]["const"]==true);
    assert(spawnProperties.at("Rotation")==npcProperties.at("Rotation"));
    assert(spawnProperties.at("Location")==npcProperties.at("Location"));
    assert(npcProperties.at("HideMesh").at("type")=="boolean");
    const auto& orb=npcProperties.at("VisualEffect").at("anyOf")[1];
    assert(orb.at("properties").at("Type").at("const")=="Niagara");
    assert(orb.at("properties").at("Parameters").at("maxProperties")==32);
    assert(orb.at("properties").at("LocationOffset").at("required").size()==3);
    const auto& gate=schemas.at("dialogue").at("anyOf")[0].at("properties").at("Nodes").at("additionalProperties").at("properties").at("Choices").at("items").at("properties").at("WhenQuest");
    assert(gate.at("properties").contains("Stage") && gate.at("properties").contains("RepeatReady"));
    const auto& unlock=schemas.at("dialogue").at("anyOf")[0].at("properties").at("Nodes").at("additionalProperties").at("properties").at("Choices").at("items").at("properties").at("RequirementUnlock");
    assert(unlock.at("properties").contains("Quest") && unlock.at("properties").contains("TimeOfDay"));
    const auto& eventMember=schemas.at("events").at("anyOf")[0].at("properties").at("Waves").at("items").at("items");
    assert(eventMember.at("properties").at("GroundToSurface").at("default")==true);
    assert(schemas.at("events").at("anyOf")[0].at("properties").contains("HealthStages"));
    assert(objective.at("properties").contains("ProgressText") && objective.at("properties").contains("AnnounceProgress"));
    const auto& vendorProperties=schemas.at("vendors").at("anyOf")[0].at("anyOf")[0].at("properties");
    assert(vendorProperties.contains("Items") && !vendorProperties.contains("Location") && !vendorProperties.contains("Actor"));
    assert(vendorProperties.at("Repairable").at("default")==false);
    assert(vendorProperties.at("Masterworkable").at("default")==false);
    assert(vendorProperties.contains("CategoryRules"));
    assert(vendorProperties.at("VendorHeaderImage").at("default")=="/Game/Art/UI/Craft/T_DeathShop_Banner.T_DeathShop_Banner");
    const auto& placement=schemas["journal"]["patternProperties"]["^[^$]"]["properties"]["AddTo"];
    assert(placement["oneOf"].size()==2 && placement["properties"].contains("Section")
        && placement["properties"].contains("Category") && placement["properties"].contains("Group"));
    const auto& eventVisual=schemas.at("spawns").at("items").at("allOf")[0].at("then").at("properties").at("VisualEffect");
    assert(eventVisual.at("properties").at("Type").at("const")=="Ghost Glow");
    assert(npcProperties.at("Map").at("properties").at("ShowName").at("default")==true);
    assert(!npcProperties.at("OverheadIcon").at("properties").contains("ShowName"));
    const auto& playerProperties=schemas.at("players").at("items").at("anyOf")[0].at("properties");
    assert(playerProperties.at("Nameplate").at("properties").contains("Events"));
    assert(playerProperties.at("Nameplate").at("properties").at("Events").at("maxItems")==64);
    const auto& nameplate=playerProperties.at("Nameplate");
    assert(nameplate.at("properties").at("States").at("patternProperties").at("^[^$]").at("properties").contains("While"));
    assert(nameplate.at("properties").contains("SkillXP"));
    assert(nameplate.at("properties").at("Events").at("items").at("properties").contains("Action"));
    assert(schemas.at("effects").at("x-runeschema-definition-id")=="ModName:Category/Id");
    assert(schemas.at("effects").at("x-runeschema-reference-forms").size()==2);
    assert(schemas.at("equipment").at("additionalProperties")==false);
    assert(schemas.at("equipment").at("properties").contains("GrantedEffects"));
    assert(schemas.at("equipment").at("properties").contains("Items"));
    const auto& itemProperties=schemas.at("equipment").at("properties").at("Items")
        .at("additionalProperties").at("properties").at("Properties");
    assert(itemProperties.at("type")=="object" && itemProperties.at("maxProperties")==128);
    const auto& processing=schemas.at("buildings").at("anyOf").at(0).at("anyOf").at(0)
        .at("patternProperties").at("^[^$]")
        .at("properties").at("Overrides").at("properties").at("Processing").at("properties");
    assert(processing.contains("AcceptedFuels") && processing.contains("StartingFuelItem")
        && processing.contains("MaxFuelSlots") && processing.contains("AutoStartProcess"));
    const auto& buildingOverrides=schemas.at("buildings").at("anyOf").at(0).at("anyOf").at(0)
        .at("patternProperties").at("^[^$]").at("properties").at("Overrides").at("properties");
    const auto& buildingPlacementFields=buildingOverrides.at("Placement").at("properties");
    assert(buildingPlacementFields.contains("bCanOnlyBeSnapped")
        && buildingPlacementFields.contains("AcceptedPhysicalSurfaces")
        && buildingPlacementFields.contains("RegionBlockList")
        && buildingPlacementFields.contains("SnappingModeOverride"));
    assert(buildingOverrides.at("Stability").at("properties").contains("HorizontalLoss"));
    assert(buildingOverrides.at("DerivedData").at("properties").contains("PlacementZOffset"));
    assert(buildingOverrides.at("Shelter").at("properties").contains("RoofRays"));
    assert(buildingOverrides.at("Health").at("properties").contains("MaxHealth"));
    assert(buildingOverrides.at("Snapping").at("properties").contains("SnappingRadius"));
    assert(schemas.at("assets").dump().find("\"$declaration\"")==std::string::npos);
    assert(schemas.at("journal").dump().find("\"$declaration\"")==std::string::npos);
    assert(schemas.at("lore").dump().find("\"$declaration\"")==std::string::npos);
    assert(schemas.at("quests").at("anyOf").size()==2);
    const auto& recipePlacement=schemas.at("recipes").at("patternProperties").at("^[^$]")
        .at("properties").at("AddTo").at("items");
    assert(schemas.at("recipes").at("patternProperties").at("^[^$]").at("properties")
        .at("PersistenceID").at("pattern")=="^[A-Za-z0-9_-]{21}[AQgw]$");
    assert(recipePlacement.at("properties").contains("Table"));
    assert(recipePlacement.at("properties").contains("DataTable"));
    assert(recipePlacement.at("allOf").at(0).at("oneOf").size()==2);
    assert(recipePlacement.at("allOf").at(1).at("oneOf").size()==2);
    assert(recipePlacement.at("properties").at("Category").at("description").get<std::string>().find("crafting stations")!=std::string::npos);
    assert(recipePlacement.at("properties").at("Array").at("description").get<std::string>().find("processing-station")!=std::string::npos);
    const auto& spheres=schemas.at("assets").at("patternProperties").at("^(?!(?:RuneSchema|Modded)$)[^$]").at("properties").at("$DominionSpheres");
    assert(spheres.at("minProperties")==1 && spheres.at("maxProperties")==32);
    assert(spheres.at("additionalProperties").at("properties").at("Radius").at("maximum")==100000);
    assert(spheres.at("additionalProperties").at("additionalProperties")==false);
    if(argc==2) {
        std::filesystem::create_directories(argv[1]);
        for(const auto& [name,schema]:schemas.items()) {
            std::ofstream file(std::filesystem::path(argv[1])/(name+".schema.json"));
            file<<schema.dump(2);file.flush();assert(file.good());
        }
    }
    return 0;
}
int main(int argc,char** argv) {try{return RunLoaderSchemas(argc,argv);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
