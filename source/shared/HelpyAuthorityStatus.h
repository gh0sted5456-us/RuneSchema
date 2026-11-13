#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string_view>
#include <limits>
namespace PS::HelpyAuthorityStatus {
inline bool Valid(const nlohmann::json& value){
    if(!value.is_object()||value.size()>16||!value.contains("Kind")||!value["Kind"].is_string()
        ||value["Kind"]!="HelpyRemoteAccess1"||!value.contains("Allowed")||!value["Allowed"].is_boolean())return false;
    for(const auto* key:{"Available","PinRequired","Authenticated","ItemDelivery","Teleport"})if(value.contains(key)&&!value[key].is_boolean())return false;
    for(const auto* key:{"Detail","CharacterGuid"})if(value.contains(key)&&(!value[key].is_string()||value[key].get_ref<const std::string&>().size()>(std::string_view(key)=="CharacterGuid"?64u:512u)))return false;
    if(const auto limit=value.find("MaximumItemCount");limit!=value.end()){
        if(!limit->is_number_integer())return false;
        const auto number=limit->get<double>();
        if(number<0||number>(std::numeric_limits<int>::max)())return false;
    }
    return true;
}
inline bool AcceptUpdate(const nlohmann::json& access,int64_t revision,int64_t& received){
    if(revision<=received||!Valid(access))return false;
    received=revision;
    return true;
}
}
