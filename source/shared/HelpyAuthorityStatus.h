#pragma once
#include <nlohmann/json.hpp>
#include <string_view>
namespace PS::HelpyAuthorityStatus {
inline bool Valid(const nlohmann::json& value){
    if(!value.is_object()||value.size()>16||!value.contains("Kind")||!value["Kind"].is_string()
        ||value["Kind"]!="HelpyRemoteAccess1"||!value.contains("Allowed")||!value["Allowed"].is_boolean())return false;
    for(const auto* key:{"Available","PinRequired","Authenticated","ItemDelivery","Teleport"})if(value.contains(key)&&!value[key].is_boolean())return false;
    for(const auto* key:{"Detail","CharacterGuid"})if(value.contains(key)&&(!value[key].is_string()||value[key].get_ref<const std::string&>().size()>(std::string_view(key)=="CharacterGuid"?64u:512u)))return false;
    return true;
}
}
