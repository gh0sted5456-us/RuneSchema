#pragma once
#include "Generator/AuthoringPolicy.h"
#include "Generator/ClonePresentation.h"
#include <nlohmann/json.hpp>
namespace PS::Authoring {
inline nlohmann::json ParseCloneJson(const std::string& text) {
    if(text.size()>MaxCloneDocumentBytes)throw std::runtime_error("Clone JSON exceeds the document limit.");
    return nlohmann::json::parse(text,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&) {
        if(depth>16)throw std::runtime_error("Clone JSON nesting exceeds 16 levels.");
        return true;
    });
}
inline nlohmann::json CloneRequest(const std::string& source,const std::string& name,
    const std::string& icon,nlohmann::json overrides,bool permanent,bool give,int count,
    bool acknowledged,const std::string& player,const std::string& modTag="RuneSchema",
    const std::string& persistenceId="",const std::string& appearance="",
    const std::string& meshField="",const std::string& mesh="") {
    ValidateObjectPath(source);
    if(player.empty())throw std::runtime_error("Select an authoritative player first.");
    if(!acknowledged)throw std::runtime_error("Acknowledge disposable-save testing first.");
    if(name.size()>256 || name.find('\0')!=name.npos)throw std::runtime_error("Invalid clone display name.");
    if(!icon.empty())ValidateObjectPath(icon);
    if(count<1 || count>10000)throw std::runtime_error("Clone quantity must be 1..10000.");
    if(!overrides.is_object() || overrides.size()>MaxCloneFields || overrides.dump().size()>MaxCloneDocumentBytes)
        throw std::runtime_error("Overrides must be a bounded JSON object.");
    for(const auto& [key,value]:overrides.items()) {
        if(!SafeFieldName(key))throw std::runtime_error("Managed or invalid override field: "+key);
        if(value.dump().size()>MaxCloneValueBytes)throw std::runtime_error("Override field exceeds 16 KiB: "+key);
    }
    if(modTag.size()>64)throw std::runtime_error("Mod tag exceeds 64 bytes.");
    if(!persistenceId.empty()&&!ClonePresentation::ValidId(persistenceId,modTag))throw std::runtime_error("Regenerate the clone PersistenceID for this mod tag.");
    if(!appearance.empty())ValidateObjectPath(appearance);
    if(!mesh.empty()){ValidateObjectPath(mesh);if(!ClonePresentation::VisualField(meshField))throw std::runtime_error("Choose a supported visual slot.");}
    if(!appearance.empty()&&!mesh.empty())throw std::runtime_error("Choose copy appearance or a cooked mesh, not both.");
    return {{"ModTag",modTag},{"PersistenceID",persistenceId},{"AppearanceSource",appearance},{"MeshField",meshField},{"Mesh",mesh},
        {"Action","CreateClone"},{"Source",source},{"Name",name},{"Icon",icon},
        {"Overrides",std::move(overrides)},{"Permanent",permanent},{"Give",give},
        {"Count",count},{"AcknowledgeExperimental",acknowledged},{"Player",player}};
}
} // namespace PS::Authoring
