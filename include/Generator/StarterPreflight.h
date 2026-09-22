#pragma once
#include "Generator/LoaderTemplate.h"
#include "Loader/StringReplacementRules.h"
#include "Loader/HumanNpc.h"
#include "Core/JsonPatchDirective.h"
#include <set>

namespace PS::StarterPreflight {
using nlohmann::json;
inline json Parse(const std::string& text) {
    if(text.size()>65535)throw std::runtime_error("Draft exceeds 64 KiB");
    size_t nodes=0;
    return json::parse(text,[&](int depth,json::parse_event_t,json&){
        if(depth>24 || ++nodes>16384)throw std::runtime_error("Draft nesting or node limit exceeded");
        return true;
    },true,true);
}
template<class Lookup>
json Check(const std::string& loader,const json& draft,const json& baseline,Lookup loaded) {
    const auto& capability=Capability(loader);
    json report={{"Kind","RuneSchemaStarterPreflight1"},{"Loader",loader},
        {"Errors",json::array()},{"Dependencies",json::array()},
        {"ChangesFromCapturedStarter",json::diff(baseline,draft)},
        {"Coverage","Draft comparison only, not current game values or a simulation of loader writes. Loaded references do not prove type, skeleton, pak ownership or multiplayer compatibility. Unresolved references may simply be unloaded."}};
    auto& errors=report["Errors"];
    if(!draft.is_structured())errors.push_back("Loader document must be an object or array");
    if(loader=="strings")try{DragonWilds::StringReplacementRules::Validate(draft);}catch(const std::exception& e){errors.push_back(e.what());}
    if(loader=="npc") {
        const auto validate=[&](const json& body){
            if(!body.is_object()) {errors.push_back("NPC entry must be an object");return;}
            try{DragonWilds::HumanNpc::Validate(body);}catch(const std::exception& e){errors.push_back(e.what());}
        };
        const auto& entries=draft.is_object() && draft.contains("Npcs")?draft.at("Npcs"):draft;
        if(entries.is_array())for(const auto& body:entries)validate(body);else validate(entries);
    }
    std::set<std::string> paths;
    size_t nodes=0;
    const auto visit=[&](auto&& self,const json& value,int depth)->void {
        if(depth>24 || ++nodes>16384)throw std::runtime_error("Preflight traversal limit exceeded");
        if(value.is_object()) {
            if(value.contains("$Clone")&&!capability.Clone)errors.push_back("$Clone is unsupported for this loader");
            if(value.contains("$Append")&&!capability.Append)errors.push_back("$Append is unsupported for this loader");
            if(value.contains("$Patch")&&value["$Patch"].is_string()) {
                try{(void)DragonWilds::JsonPatchDirective::Parse(value,{},loader);}
                catch(const std::exception& e){errors.push_back(e.what());}
            }
        }
        if(value.is_structured())for(const auto& child:value)self(self,child,depth+1);
        else if(value.is_string()) {
            const auto& text=value.get_ref<const std::string&>();
            if(text.starts_with('/') && !text.starts_with("/Script/") && text.find('.')!=std::string::npos) {
                if(text.size()>1024 || text.find_first_of("\r\n\t ")!=std::string::npos)errors.push_back("Malformed object reference");
                else if(paths.size()<128)paths.insert(text);
                else throw std::runtime_error("Dependency limit exceeded (128)");
            }
        }
    };
    visit(visit,draft,0);
    for(const auto& path:paths)report["Dependencies"].push_back({{"Path",path},{"Status",loaded(path)?"loaded":"unresolved; not proof of missing content"}});
    report["Validation"]="Partial checks only; runtime loader validation is still required";
    return report;
}
}
