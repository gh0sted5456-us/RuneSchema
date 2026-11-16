#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

namespace PS::NativeServerAdmin {
using Json=nlohmann::json;
inline bool Token(const Json& value){if(!value.is_string())return false;const auto& text=value.get_ref<const std::string&>();const auto colon=text.find(':');return text.size()<=96&&colon!=text.npos&&colon>0&&colon+1<text.size()&&text.find(':',colon+1)==text.npos&&text.find_first_not_of("0123456789:")==text.npos;}
inline std::string Guid(std::string value){
    if(value.size()!=32||value.find_first_not_of("0123456789abcdefABCDEF")!=value.npos||value==std::string(32,'0'))throw std::runtime_error("Refresh the connected player identity before changing admin rights.");
    for(auto& c:value)if(c>='a'&&c<='f')c-=32;return value;
}
inline std::string Operation(const Json& request){
    const auto operation=request.at("Operation").get<std::string>();
    if(operation!="Read"&&operation!="Grant"&&operation!="Revoke"&&operation!="Authorize"&&operation!="Unauthorize"&&operation!="AuthorizeAll"&&operation!="AutoAuthorize")throw std::runtime_error("Unsupported server administration operation.");
    if(operation!="Read"){
        if(operation=="AutoAuthorize"&&!request.at("Enabled").is_boolean())throw std::runtime_error("Invalid automatic authorization setting.");
        const bool target=operation!="AuthorizeAll"&&operation!="AutoAuthorize";
        if(target)(void)Guid(request.at("PlayerGuid").get<std::string>());
        for(const auto* key:{"WorldToken","TargetToken"}){if(!target&&std::string_view(key)=="TargetToken")continue;
            if(!Token(request.at(key)))throw std::runtime_error("Refresh the current world and player before changing admin rights.");
        }
    }
    return operation;
}
inline void Authorize(bool nativeOwner,bool targetOwner,bool sameWorld,bool sameTarget){
    if(!nativeOwner)throw std::runtime_error("Only the native server owner may grant or revoke admin. RuneSchema PIN access does not grant this permission.");
    if(!sameWorld||!sameTarget)throw std::runtime_error("The world or player connection changed. Refresh before changing admin rights.");
    if(targetOwner)throw std::runtime_error("The server owner's role cannot be changed here.");
}
inline bool Snapshot(const Json& value){try{
    if(!value.is_object()||value.at("Kind")!="RuneSchemaServerControl1"||!Token(value.at("WorldToken"))||!value.at("CanManage").is_boolean()
        ||!value.at("Reason").is_string()||value.at("Reason").get_ref<const std::string&>().size()>1024||!value.at("Players").is_array()||value.at("Players").size()>256)return false;
    for(const auto* key:{"CanAuthorize","AutoAuthorize"})if(value.contains(key)&&!value.at(key).is_boolean())return false;
    for(const auto& row:value.at("Players")){
        if(!row.is_object()||!row.at("Name").is_string()||row.at("Name").get_ref<const std::string&>().size()>1024||!row.at("Owner").is_boolean()||!Token(row.at("TargetToken")))return false;
        (void)Guid(row.at("Guid").get<std::string>());const auto& role=row.at("RoleStatus");
        if(!role.is_object()||!role.at("NativeKnown").is_boolean()||!role.at("Role").is_string())return false;
        const auto name=role.at("Role").get<std::string>();if(name!="owner"&&name!="admin"&&name!="host"&&name!="member"&&name!="unknown")return false;
        if((name=="owner"||name=="admin"||name=="member")&&!role.at("NativeKnown").get<bool>())return false;
        if(row.contains("Authorized")&&!row.at("Authorized").is_boolean())return false;
        for(const auto* key:{"PlatformAccountName","Platform"})if(row.contains(key)&&(!row.at(key).is_string()||row.at(key).get_ref<const std::string&>().size()>128))return false;
        if(row.contains("PlayerId")&&!row.at("PlayerId").is_number_integer())return false;
    }return true;
}catch(...){return false;}}
}
