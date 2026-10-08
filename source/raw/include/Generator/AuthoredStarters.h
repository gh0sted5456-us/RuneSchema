#pragma once
#include "Generator/AssetTemplate.h"
#include "Generator/LoaderCapabilities.h"
namespace PS::AuthoredStarters {
inline bool Supports(const std::string& loader) {
    return Capability(loader).AuthoredSearch;
}
inline nlohmann::json Entries(const nlohmann::json& document,const std::string& loader,
    const std::string& query={},const std::string& prefix={}) {
    using nlohmann::json;json rows=json::array();
    const auto add=[&](const std::string& key,const json& body){
        if(rows.size()>=100 || key.starts_with('$') || !body.is_structured())return;
        if(body.is_object() && body.contains("$Patch"))return;
        if(!query.empty() && !AssetTemplate::Matches(query,prefix+" "+key+" "+body.dump()))return;
        rows.push_back({{"Target",key},{"Body",body}});
    };
    if(loader=="npc") {
        const auto& entries=document.is_object() && document.contains("Npcs") ? document.at("Npcs") : document;
        const auto single=[&](const json& body){if(body.is_object()&&body.contains("Id")&&body["Id"].is_string())add(body["Id"],body);};
        if(entries.is_array())for(const auto& body:entries)single(body);else single(entries);
    }else if(loader=="vendors") {
        // Mirror the vendor loader's array, single, wrapper and keyed-map forms.
        const auto visit=[&](auto&& self,const json& value,int depth)->void {
            if(depth>24)throw std::runtime_error("Vendor wrapper nesting exceeds limit.");
            if(value.is_array())for(const auto& body:value)self(self,body,depth+1);
            else if(value.is_object()) {
                if(value.contains("Vendors")){self(self,value["Vendors"],depth+1);return;}
                if(value.contains("Location") || value.contains("Items") || value.contains("Actor") || value.contains("Mesh") || value.contains("VisualSource")) {
                    for(const auto* id:{"Id","Name","DisplayName","MerchantName"})
                        if(value.contains(id) && value[id].is_string()){add(value[id],value);return;}
                    return;
                }
                for(const auto& [key,body]:value.items())add(key,body);
            }
        };
        visit(visit,document,0);
    }else if(loader=="spawns"||loader=="courses") {
        const auto single=[&](const json& body){if(body.is_object()&&body.contains("Id")&&body["Id"].is_string())add(body["Id"],body);};
        if(document.is_array())for(const auto& body:document)single(body);else single(document);
    }else if(document.is_object())for(const auto& [key,body]:document.items())add(key,body);
    return rows;
}
inline nlohmann::json Reference(const nlohmann::json& fields) {
    nlohmann::json result=nlohmann::json::object();size_t count=0;
    for(const auto& [key,value]:fields.items()) {
        if(++count>256)break;
        result[key]={{"type",value.type_name()},{"description","Authored field; inferred from this definition."}};
        if(value.is_number_integer())result[key]["type"]="integer";
        else if(value.is_number())result[key]["type"]="number";
    }return result;
}
}
