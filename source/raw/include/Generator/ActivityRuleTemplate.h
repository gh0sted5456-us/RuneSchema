#pragma once
#include "Loader/PlayerActivityEvents.h"
#include "Loader/PlayerArchetype.h"
namespace PS::InspectionTools {
inline nlohmann::json BuildActivityRuleTemplate(const nlohmann::json& event,
    const nlohmann::json& conditions,const std::string& state,const std::string& icon,
    double timeout,const std::string& player) {
    using nlohmann::json;
    if (!event.is_object() || !event.contains("parameters") || !event["parameters"].is_object()
        || !event.contains("function") || !event["function"].is_string())
        throw std::runtime_error("Select a captured event with supported parameters");
    if (player.empty() || player.size()>128)
        throw std::runtime_error("Player selector requires 1-128 characters");
    for (unsigned char c:player) if(c<32)throw std::runtime_error("Invalid player selector");
    if (!std::isfinite(timeout) || timeout<=0 || timeout>3600)
        throw std::runtime_error("Inactivity seconds must be greater than zero and at most 3600");
    // Reuse cooked-path and identifier checks rather than a second schema.
    DragonWilds::NormalizePlayerArchetype({{"Archetype",{{"Name",state},{"Icon",icon}}}});
    const json states={{state,{{"Icon",icon},{"Scale",0.9}}}};
    const json events=json::array({{{"Function",event["function"]},{"State",state},{"Parameters",conditions}}});
    DragonWilds::ValidateActivityEvents(events,states);
    return json::array({{
        {"Id","event-"+state},{"PlayerName",player},
        {"Nameplate",{{"Mode","Hidden"},{"Client","Yes"},{"Server","No"},
            {"ActivityTimeoutSeconds",timeout},{"States",states},{"Events",events}}}
    }});
}
// Scalar suggestions only. Omitted objects and truncated metadata are not conditions.
inline nlohmann::json ActivityParameterSuggestions(const nlohmann::json& parameters) {
    using nlohmann::json;
    json result=json::array();
    const auto visit=[&](auto&& self,const json& value,json path)->void {
        if (result.size()>=128 || path.size()>5) return;
        if (!path.empty() && (value.is_boolean() || value.is_number())) {
            result.push_back({{"Path",path},{"Equals",value}});return;
        }
        if (!value.is_object() || value.contains("omitted")) return;
        for (const auto& [key,child]:value.items()) {
            if (!DragonWilds::ActivityIdentifier(key)) continue;
            auto next=path;next.push_back(key);self(self,child,std::move(next));
            if(result.size()>=128)break;
        }
    };
    visit(visit,parameters,json::array());
    return result;
}
}
