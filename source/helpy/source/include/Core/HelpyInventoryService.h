#pragma once
#include "Core/HelpyMarkSync.h"
#include "Core/RuneSchemaProfile.h"
#include "Runtime/HelpySettings.h"
namespace PS::HelpyInventoryService {
using Json=nlohmann::json;
inline constexpr uint32_t MutationReplyBytes=4096;
inline bool Mutating(const std::string& action){return action=="Mark"||action=="CycleMark"||action=="ReceiveMarks"||action=="ReceivePublicProfile"||action=="ClearPublicProfiles";}
inline Json Run(const std::filesystem::path& directory,const Json& request){
    const auto action=request.at("Action").get<std::string>();
    if(action=="Capabilities")return {{"GameFavorites",HelpySettings::ExtendFavoritesToGame()},{"MarksRevision",HelpyPersonalization::Revision.load()}};
    if(action=="PublicProfile")return {{"Profile",RuneSchemaProfile::Public()}};
    if(action=="ReadPublicProfile")return {{"Profile",RuneSchemaProfile::PublicFor(request.at("CharacterGuid").get<std::string>(),request.value("Local",false))}};
    if(action=="ReceivePublicProfile"){RuneSchemaProfile::ReceivePublic(request.at("CharacterGuid").get<std::string>(),request.at("Profile"));return {{"Accepted",true}};}
    if(action=="ClearPublicProfiles"){RuneSchemaProfile::ClearShared();return {{"Accepted",true}};}
    if(action=="ExportMarks")return HelpyMarkSync::Export(request);
    if(action=="ReceiveMarks")return HelpyMarkSync::Receive(request.at("Batch"));
    if(action=="Marks"||action=="TypeMarks"||action=="Mark"||action=="CycleMark"){
        const auto& item=request.at("Item");const auto stack=HelpyPersonalization::StackKey(item),type=HelpyPersonalization::TypeKey(item);
        if(stack.empty()||type.empty()||item.value("MarksReadOnly",false))throw std::runtime_error("Stable character/item identity required");
        if(action=="CycleMark"){
            const auto scope=request.at("Scope").get<std::string>();if(scope!="Stack"&&scope!="Type")throw std::runtime_error("Select stack or item-type scope");
            const auto revision=HelpyPersonalization::Revision.load();
            if(!request.at("Revision").is_number_unsigned()||request.at("Revision").get<uint64_t>()!=revision)throw std::runtime_error("Item marks changed; refresh before marking again.");
            const auto marks=scope=="Stack"?HelpyPersonalization::Effective(item):HelpyPersonalization::Get(type);
            const bool favorite=marks.value("Favorite",false);bool junk=false;Json tags=Json::array();
            for(const auto& tag:marks.value("Tags",Json::array())){if(HelpyPersonalization::Fold(tag.get<std::string>())=="junk")junk=true;else tags.push_back(tag);}
            if(favorite)tags.push_back("junk");
            if(!HelpyPersonalization::Set(scope=="Stack"?stack:type,{{"Favorite",!favorite&&!junk},{"Tags",tags}},revision))throw std::runtime_error(HelpyPersonalization::Status);
        }
        if(action=="Mark"){
            const auto scope=request.at("Scope").get<std::string>();if(scope!="Stack"&&scope!="Type")throw std::runtime_error("Select stack or item-type scope");
            const auto key=scope=="Stack"?stack:type;const auto field=request.at("Field").get<std::string>();
            if(!request.at("Value").is_boolean()||(field!="Favorite"&&field!="Locked"&&field!="Junk"))throw std::runtime_error("Invalid inventory mark");
            Json patch;if(field=="Junk"){auto tags=(scope=="Stack"?HelpyPersonalization::Effective(item):HelpyPersonalization::Get(type)).value("Tags",Json::array());Json next=Json::array();
                for(const auto& tag:tags)if(HelpyPersonalization::Fold(tag.get<std::string>())!="junk")next.push_back(tag);if(request["Value"].get<bool>())next.push_back("junk");patch={{"Tags",next}};
            }else patch={{field,request["Value"]}};
            if(!HelpyPersonalization::Set(key,patch))throw std::runtime_error(HelpyPersonalization::Status);
        }
        const auto marks=action=="TypeMarks"?HelpyPersonalization::Get(type):HelpyPersonalization::Effective(item);bool junk=false;
        for(const auto& tag:marks.value("Tags",Json::array()))if(HelpyPersonalization::Fold(tag.get<std::string>())=="junk")junk=true;
        return {{"Marks",marks},{"Junk",junk},{"Revision",HelpyPersonalization::Revision.load()}};
    }
    throw std::runtime_error("Unknown Helpy inventory operation");
}
}
