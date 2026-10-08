#pragma once
#include "Loader/EquipmentEffectRules.h"
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

namespace DragonWilds::EquipmentItemRules {
struct Assignment {
    std::optional<std::string> associatedSkill;
    std::optional<std::string> skillUsed;
    std::optional<std::string> skillPerkRequiredToEquip;
    std::optional<EquipmentEffectRules::Assignment> grantedEffects;
    nlohmann::json properties=nlohmann::json::object();
};
using Rules=std::map<std::string,Assignment>;

inline bool AssetPath(const std::string& value) {
    return value.starts_with('/') && value.find('.')!=std::string::npos && value.size()<=1024
        && value.find("..") == std::string::npos && value.find_first_of("\r\n\t") == std::string::npos;
}

inline void Merge(Rules& current,const nlohmann::json& value) {
    if(!value.is_object() || value.size()>256)
        throw std::runtime_error("Items requires an object map with at most 256 item paths");
    auto staged=current;
    for(const auto& [path,body]:value.items()) {
        if(!AssetPath(path))throw std::runtime_error("Items key must be a cooked item object path");
        if(!body.is_object() || body.empty())throw std::runtime_error("Items rule must be a non-empty object");
        Assignment next=staged[path];
        for(const auto& [field,fieldValue]:body.items()) {
            if(field=="$Comment")continue;
            if(field=="Properties") {
                if(!fieldValue.is_object() || fieldValue.empty() || fieldValue.size()>128)
                    throw std::runtime_error("Items.Properties must be a non-empty object with at most 128 reflected fields");
                for(const auto& [name,propertyValue]:fieldValue.items()) {
                    if(name.empty() || name.size()>256 || name.starts_with('$')
                        || name=="PersistenceID" || name=="InternalName"
                        || name=="GrantedEffects" || name=="AssociatedSkill"
                        || name=="SkillUsed" || name=="SkillPerkRequiredToEquip")
                        throw std::runtime_error("Items.Properties contains a protected or reserved field: "+name);
                    next.properties[name]=propertyValue;
                }
                continue;
            }
            if(field=="GrantedEffects") {
                EquipmentEffectRules::Rules parsed;
                EquipmentEffectRules::Merge(parsed,nlohmann::json{{path,fieldValue}});
                next.grantedEffects=std::move(parsed.at(path));
                continue;
            }
            auto assign=[&](std::optional<std::string>& destination,const char* label) {
                if(!fieldValue.is_string() || !AssetPath(fieldValue.get<std::string>()))
                    throw std::runtime_error(std::string(label)+" must be a cooked object path");
                destination=fieldValue.get<std::string>();
            };
            if(field=="AssociatedSkill")assign(next.associatedSkill,"AssociatedSkill");
            else if(field=="SkillUsed")assign(next.skillUsed,"SkillUsed");
            else if(field=="SkillPerkRequiredToEquip")assign(next.skillPerkRequiredToEquip,"SkillPerkRequiredToEquip");
            else throw std::runtime_error("Unsupported Items field: "+field);
        }
        staged[path]=std::move(next);
    }
    current=std::move(staged);
}
}
