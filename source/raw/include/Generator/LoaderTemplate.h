#pragma once
#include "Generator/AssetTemplate.h"
#include "Generator/AuthoredStarters.h"
#include "Generator/ReferenceExport.h"
#include "Loader/VirtualDefinitionId.h"
namespace PS::LoaderTemplate {
inline bool Clone(const std::string& loader){return Capability(loader).Clone;}
inline bool Patch(const std::string& loader){return Capability(loader).StarterPatch;}
inline bool Append(const std::string& loader){return Capability(loader).Append;}
inline std::string Help(const std::string& loader) {
    if(loader=="events")return "Experimental temporary AI waves. EventOnly /spawns templates do not place permanent actors. Dialogue Event actions Start/Cancel; no quest rewards or persistence yet. Restart required.";
    if(loader=="quests")return "Native fetch quests: FetchQuest example. Optional Objective.Location [X,Y,Z]: QuestMarker example (experimental). Keep quest IDs and persistence IDs unique. Dialogue Quest actions accept or turn in. Restart required; local authoritative testing only.";
    if(loader=="dialogue")return "Experimental native dialogue. Nodes contain text and up to four Next/End choices. Completion grants one item reward per character; RequiresFlag gates stores. Restart to apply. Review the JonesyStory JSONC example.";
    if(loader=="recipes")return "Unlock defaults to false: adding a recipe to a station/category does not grant it. Unlock:true opts into automatic grants. False does not revoke already learned recipes.";
    if(loader=="effects")return "Alias a verified cooked GameplayEffect Blueprint class to a reusable ModName:Id. Definitions do not clone or patch class defaults. Restart to apply.";
    if(loader=="niagara")return "Cooked system, socket, offsets, emitter toggles, and User.* parameters. Reference ModName:Id through VisualEffect.Definition from /equipment, /players, or /spawns. Restart to apply definitions.";
    if(AuthoredStarters::Supports(loader))return "Copied from an installed mod's authored file; it may be inactive. Keep the target identity for an override where supported; use a new identity for a separate variant. Duplicate handling is loader-specific. $Clone is not supported here.";
    if(loader=="assets")return "Clone a recipe-unlocker consumable to retain its Study/Use behavior. Edit RecipesToUnlock (recipe object paths) or BuildingPieceToUnlock. $Clone inherits omitted unlock links; explicit fields replace them. Keep its PersistenceID after installation.";
    if(loader=="raw")return "Selected scalar row fields only; arrays and nested structs are omitted. Table names must be unique. Patch refuses a missing row.";
    if(loader=="blueprints")return "Selected scalar class defaults only; components are not expanded. Restart to apply. Blueprint cloning is not supported.";
    if(loader=="buildings")return "Registers BuildingPieceData, not a Blueprint class. Clone creates a new building identity; Asset uses the existing record. AddTo requires an existing catalogue page. Keep ModName and key stable.";
    if(loader=="players")return "New name-based player rule, not a dump of player state. Review the name selector and choose a unique Id. Repeated names can match multiple players. Never invent event hooks.";
    if(loader=="strings")return "Table-scoped replacement of source text, not a string-table key. Edit the replacement value. Equal text is a no-op. Patch and Clone are not supported.";
    throw std::runtime_error("Unsupported starter loader.");
}
inline nlohmann::json Build(const std::string& loader,const nlohmann::json& entry,nlohmann::json fields,int mode,
    const std::string& mod,const std::string& name,const std::string& id) {
    using nlohmann::json;
    Help(loader);
    if(mode<0 || mode>3 || (mode==1&&!Patch(loader)) || (mode==2&&!Clone(loader)))throw std::runtime_error("Format is not supported for this loader.");
    if(!fields.is_object())throw std::runtime_error("Starter fields must be an object.");
    if(mode==3)return {{"$schema","http://json-schema.org/draft-07/schema#"},{"title",loader+" field reference"},
        {"type","object"},{"properties",fields},{"additionalProperties",true},{"x-runeschema-coverage","selected fields; not a complete loader schema"}};
    if(loader=="dialogue")return fields;
    if(loader=="quests")return fields;
    if(loader=="events")return fields;
    if(AuthoredStarters::Supports(loader)) {
        if(loader=="effects")return {{entry.at("Target").get<std::string>(),fields}};
        if(loader=="vendors" || loader=="npc") {
            if(!fields.contains("Id"))fields["Id"]=entry.at("Target");
            return json::array({fields});
        }
        if(loader=="spawns")return json::array({fields});
        if(loader=="courses")return fields;
        if(loader=="enums")return {{entry.at("Target").get<std::string>(),fields.at("Values")}};
        return {{entry.at("Target").get<std::string>(),fields}};
    }
    if(loader=="assets")return AssetTemplate::Build(entry.at("Path"),std::move(fields),mode,mod,name,id);
    if(loader=="strings")return {{entry.at("Table").get<std::string>(),{{entry.at("Text").get<std::string>(),fields.at("Replacement")}}}};
    if(loader=="players") {
        AssetTemplate::Identifier(name);
        fields["Id"]=name;fields["PlayerName"]=entry.at("Name");return json::array({fields});
    }
    if(loader=="buildings") {
        AssetTemplate::Identifier(mod);AssetTemplate::Identifier(name);
        fields.erase("PersistenceID");fields.erase("InternalName");
        return {{name,{{mode==2?"$Clone":"Asset",entry.at("Path")},{"Properties",fields},{"Unlock",false},
            {"AddTo",{{"Collection","Modded Buildings"},{"PageIndex",0}}}}}};
    }
    const auto target=entry.at("Target").get<std::string>();
    if(loader=="blueprints") {
        if(mode==1)return {{"Patch",{{"$Patch",target},{"$Target",fields}}}};
        return {{target,fields}};
    }
    if(mode==1)return {{"$Patch",target+":"+entry.at("Row").get<std::string>()},{"$Target",fields}};
    return {{target,{{entry.at("Row").get<std::string>(),fields}}}};
}
inline std::string Jsonc(const std::string& loader,int mode,const nlohmann::json& draft) {
    if(mode==4)return ReferenceExport::Jsonc(draft);
    if(mode==3)return "// Field reference only. Do not install in a mod folder.\n// Types without JSON mappings are left open; this is not a full loader schema.\n"+draft.dump(2)+"\n";
    return "// Place in your mod's /"+loader+" folder after review. Export does not install it.\n// "+Help(loader)
        +"\n// "+(mode==1?"$Patch identifies the target; $Target holds changes.":mode==2?"$Clone identifies the source; other fields override inherited values.":"Captured values are unchanged; edit them before installing.")
        +(Append(loader)?"\n// $Append adds entries to supported array fields without replacing existing entries.\n// Put only new entries in $Append; it does not deduplicate. In a patch, place it inside $Target.":"")
        +"\n"+draft.dump(2)+"\n";
}
}
