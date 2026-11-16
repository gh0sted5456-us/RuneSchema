#pragma once
#include "Core/ModDataRelay.h"
#include "../../../shared/ModDownloadLink.h"

namespace PS::ServerModList {
inline constexpr size_t MaxMods=1024;
inline const ModDataRelay::Channel Route{"RuneSchema:server-mods",1,ModDataRelay::MaxBytes,false,true,true};
inline bool Text(const nlohmann::json& value,size_t maximum){
    if(!value.is_string())return false;
    const auto& text=value.get_ref<const std::string&>();
    return text.size()<=maximum&&std::none_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;});
}
inline nlohmann::json Validate(const nlohmann::json& rows){
    if(!rows.is_array()||rows.size()>MaxMods)throw std::runtime_error("Server mod list exceeds its row limit");
    nlohmann::json clean=nlohmann::json::array();std::set<std::string> ids;
    for(const auto& row:rows){
        if(!row.is_object()||!row.contains("Id")||!Text(row.at("Id"),256)||row.at("Id")==""
            ||!row.contains("Name")||!Text(row.at("Name"),256)||row.at("Name")==""
            ||!row.contains("Version")||!Text(row.at("Version"),64)
            ||!ids.insert(row.at("Id").get<std::string>()).second)throw std::runtime_error("Invalid server mod list row");
        clean.push_back({{"Id",row.at("Id")},{"Name",row.at("Name")},{"Version",row.at("Version")}});
        if(row.contains("ModId")){
            if(!Text(row.at("ModId"),128)||row.at("ModId")=="")throw std::runtime_error("Invalid server mod author identity");
            clean.back()["ModId"]=row.at("ModId");
        }
        if(row.contains("Website")){
            if(!row.at("Website").is_string())throw std::runtime_error("Invalid server mod download link");
            const auto& url=row.at("Website").get_ref<const std::string&>();
            if(!url.empty()&&!ModDownloadLink::Valid(url))throw std::runtime_error("Invalid server mod download link");
            if(!url.empty())clean.back()["Website"]=url;
        }
    }
    if(clean.dump().size()>Route.Limit)throw std::runtime_error("Server mod list exceeds its byte limit");
    return clean;
}
inline std::string Encode(const nlohmann::json& rows){
    auto text=nlohmann::json{{"Kind","RuneSchemaServerMods"},{"Mods",Validate(rows)}}.dump();
    if(text.size()>Route.Limit)throw std::runtime_error("Server mod list envelope exceeds its byte limit");return text;
}
inline nlohmann::json Decode(const std::string& text){
    if(text.size()>Route.Limit)throw std::runtime_error("Server mod list exceeds its byte limit");
    const auto document=nlohmann::json::parse(text,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
        if(depth>4)throw std::runtime_error("Server mod list nesting exceeds its limit");return true;});
    if(!document.is_object()||document.value("Kind",std::string{})!="RuneSchemaServerMods")throw std::runtime_error("Invalid server mod list envelope");
    return Validate(document.at("Mods"));
}
}
