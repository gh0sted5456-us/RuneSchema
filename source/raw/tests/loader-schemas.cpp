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
    assert(!schemas.contains("animations"));
    assert(!schemas.contains("behaviors"));
    assert(schemas["lore"]["patternProperties"]["^[^$]"]["properties"]["Type"]["const"]=="Lore");
    assert(schemas["journal"]["patternProperties"]["^[^$]"]["properties"].contains("StationTableRowHandle"));
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
                assert(node[key].is_array());
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
    const auto& declaration=schemas.at("assets").at("properties").at("$declaration");
    assert(declaration.at("oneOf").size()==2);
    assert(declaration.at("oneOf").at(0).at("properties").at("Kind").at("enum")==nlohmann::json::array({"Item","Recipe"}));
    assert(declaration.at("oneOf").at(0).at("required")==nlohmann::json::array({"Kind","Path","PersistenceID"}));
    assert(schemas.at("journal").at("properties").contains("$declaration"));
    assert(schemas.at("lore").at("properties").contains("$declaration"));
    assert(schemas.at("quests").at("anyOf").size()==3);
    const auto& recipePlacement=schemas.at("recipes").at("patternProperties").at("^[^$]")
        .at("properties").at("AddTo").at("items");
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
