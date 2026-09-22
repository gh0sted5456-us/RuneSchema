#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
namespace DragonWilds {
inline constexpr const char* SkillXPEvent = "/Game/Gameplay/Character/Components/BP_Components_Skill.BP_Components_Skill_C:BP_OnSkillXPChanged";
inline constexpr const char* ToolAttackEvent = "/Script/Dominion.PlayerAttackComponent:Multicast_PerformAttackOnSimulatedProxies";
inline constexpr const char* PlayerDamageEvent = "/Game/Gameplay/Character/Components/BP_Components_PlayerDamage.BP_Components_PlayerDamage_C:BP_OnAnyDamageReceived";
inline constexpr const char* PlayerRespawnEvent = "/Game/Gameplay/Character/Player/BP_PlayerCharacter.BP_PlayerCharacter_C:OnPlayerRespawn";
inline constexpr const char* RespawnFinishedEvent = "/Game/Gameplay/Character/Player/BP_PlayerCharacter.BP_PlayerCharacter_C:Timeline_Respawn_Dissolve__FinishedFunc";
inline constexpr const char* PlayerEmotePlayEvent = "/Script/Dominion.PlayerEmotesComponent:PlayEmote";
inline constexpr const char* PlayerEmoteStopEvent = "/Script/Dominion.PlayerEmotesComponent:StopCurrentEmote";
// This base-class alias is commonly Blueprint-only in the shipped game. The
// filtered ProcessEvent observer still covers it when the concrete BP class
// exposes the event, so it is not treated as a failed native-hook candidate.
inline constexpr const char* PlayerHealBlueprintNativeAlias = "/Script/Dominion.HealthComponent:OnHealEvent";
// The radial-menu route may notify the component before PlayEmote is dispatched.
// This is a native fallback candidate and is intentionally not required in JSON.
inline constexpr const char* PlayerEmoteNotifyEvent = "/Script/Dominion.PlayerEmotesComponent:NotifyEmoteSelected";
inline constexpr const char* PlayerEmoteSelectionChangedEvent = "/Script/Dominion.PlayerEmotesComponent:OnEmoteSelectedChanged";
inline bool ActivityObjectPath(const std::string& path) {
    if (path.empty() || path.size() > 512 || path.front() != '/' || path.find('.') == path.npos) return false;
    for (unsigned char c : path)
        if (!(std::isalnum(c) || c == '/' || c == '_' || c == '.')) return false;
    return true;
}
inline bool ActivityFunctionPath(const std::string& path) {
    if (path.empty() || path.size() > 256 || path.front() != '/') return false;
    const auto separator = path.rfind(':');
    if (separator == path.npos || separator == 0 || separator + 1 >= path.size()
        || path.find(':') != separator || path.substr(0, separator).find('.') == path.npos
        || path.find("..") != path.npos)
        return false;
    for (unsigned char c : path)
        if (!(std::isalnum(c) || c == '/' || c == '_' || c == '.' || c == ':')) return false;
    auto lower = path;
    for (auto& c : lower) if (c >= 'A' && c <= 'Z') c += 32;
    return lower.find("tick") == lower.npos
        && lower.find("updateanimation") == lower.npos;
}
inline std::string ActivityFunctionName(const std::string& path) {
    const auto separator = path.rfind(':');
    return separator == path.npos ? std::string{} : path.substr(separator + 1);
}
inline void ValidateSkillXP(const nlohmann::json& mapping, const nlohmann::json& states) {
    if (!mapping.is_object() || mapping.empty() || mapping.size() > 64)
        throw std::runtime_error("Nameplate.SkillXP requires 1-64 skill-path/state mappings");
    for (const auto& [path, state] : mapping.items())
        if (!ActivityObjectPath(path) || !state.is_string() || state == "Dead" || !states.contains(state.get<std::string>()))
            throw std::runtime_error("SkillXP requires absolute skill paths and configured non-Dead states");
}
inline std::string MatchSkillXP(const nlohmann::json& mapping, const nlohmann::json& parameters) {
    if (!parameters.is_object() || !parameters.contains("SkillData") || !parameters["SkillData"].is_string()
        || !parameters.contains("CurrentXP") || !parameters["CurrentXP"].is_number()
        || !parameters.contains("PreviousXP") || !parameters["PreviousXP"].is_number()) return {};
    const double current = parameters["CurrentXP"].get<double>(), previous = parameters["PreviousXP"].get<double>();
    if (!std::isfinite(current) || !std::isfinite(previous) || previous < 0 || current <= previous) return {};
    const auto found = mapping.find(parameters["SkillData"].get<std::string>());
    return found != mapping.end() ? found->get<std::string>() : std::string{};
}
struct ActivityEventMatch {
    std::string State;
    std::string Action;
    explicit operator bool() const { return !State.empty(); }
};
inline bool ActivityIdentifier(const std::string& text) {
    if (text.empty() || text.size()>64) return false;
    for (unsigned char c:text)
        if (!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return false;
    return true;
}
inline void ValidateActivityEvents(const nlohmann::json& events,const nlohmann::json& states) {
    if (!events.is_array() || events.empty() || events.size()>64)
        throw std::runtime_error("Nameplate.Events requires 1-64 ordered rules");
    for (const auto& event:events) {
        if (!event.is_object() || event.size()<2 || event.size()>4
            || !event.contains("Function") || !event.contains("State")
            || !event["Function"].is_string() || !event["State"].is_string())
            throw std::runtime_error("Each activity event requires Function and State");
        for (const auto& [field, ignored]:event.items())
            if (field!="Function" && field!="State" && field!="Parameters" && field!="Action")
                throw std::runtime_error("Unsupported activity event field: "+field);
        const auto path=event["Function"].get<std::string>();
        if (!ActivityFunctionPath(path))
            throw std::runtime_error(
                "Activity events require an exact native or explicit Blueprint function path; "
                "tick and animation-update functions are not supported");
        const auto state=event["State"].get<std::string>();
        if (!ActivityIdentifier(state) || state=="Dead" || !states.contains(state))
            throw std::runtime_error("Activity event State must name a configured non-Dead state");
        const auto action=event.value("Action",std::string("Pulse"));
        if (action!="Pulse" && action!="Activate" && action!="Deactivate")
            throw std::runtime_error("Activity event Action must be Pulse, Activate, or Deactivate");
        if (!event.contains("Parameters")) continue;
        const auto& conditions=event["Parameters"];
        if (!conditions.is_array() || conditions.size()>8)
            throw std::runtime_error("Activity Parameters requires 0-8 conditions");
        for (const auto& condition:conditions) {
            static constexpr const char* operators[]{"Equals","NotEquals","NonEmpty","Exists","GreaterThan","GreaterOrEqual","LessThan","LessOrEqual"};
            size_t selected=0;const char* operation=nullptr;
            if(condition.is_object())for(const auto* candidate:operators)if(condition.contains(candidate)){++selected;operation=candidate;}
            if (!condition.is_object() || condition.size()!=2 || !condition.contains("Path")
                || selected!=1 || !condition["Path"].is_array()
                || condition["Path"].empty() || condition["Path"].size()>5)
                throw std::runtime_error(
                    "Parameter condition requires Path and exactly one comparison operator");
            for (const auto& part:condition["Path"])
                if (!part.is_string() || !ActivityIdentifier(part.get<std::string>()))
                    throw std::runtime_error("Invalid parameter path name");
            if (std::string_view(operation)=="NonEmpty" || std::string_view(operation)=="Exists") {
                if (!condition[operation].is_boolean())
                    throw std::runtime_error(std::string("Parameter ")+operation+" requires true or false");
            } else {
                const auto& expected=condition[operation];
                if (!(expected.is_boolean() || expected.is_number() || expected.is_string())
                    || (expected.is_number_float() && !std::isfinite(expected.get<double>())))
                    throw std::runtime_error("Parameter comparison requires a finite scalar");
                if(std::string_view(operation)!="Equals"&&std::string_view(operation)!="NotEquals"&&!expected.is_number())
                    throw std::runtime_error("Ordered parameter comparisons require a number");
                if (expected.is_string()) {
                    const auto& path=expected.get_ref<const std::string&>();
                    if (path.empty() || path.size()>512 || path.front()!='/')
                        throw std::runtime_error("String parameter matches require an absolute object path");
                }
            }
        }
    }
}
inline bool ActivityScalarEquals(const nlohmann::json& left,const nlohmann::json& right) {
    if (left.is_string() || right.is_string())
        return left.is_string() && right.is_string() && left==right;
    if (left.is_boolean() || right.is_boolean())
        return left.is_boolean() && right.is_boolean() && left==right;
    if (left.is_number_integer() && right.is_number_integer()) {
        const bool leftNegative=!left.is_number_unsigned() && left.get<int64_t>()<0;
        const bool rightNegative=!right.is_number_unsigned() && right.get<int64_t>()<0;
        if (leftNegative || rightNegative)
            return leftNegative && rightNegative && left.get<int64_t>()==right.get<int64_t>();
        return left.get<uint64_t>()==right.get<uint64_t>();
    }
    // Do not round large integer IDs through a floating-point conversion.
    return left.is_number_float() && right.is_number_float() && left==right;
}
inline bool ActivityNonEmpty(const nlohmann::json& value) {
    if (value.is_array() || value.is_object() || value.is_string()) {
        if (value.is_object()) {
            const auto count=value.find("Count");
            if (count!=value.end() && count->is_number_integer())
                return count->get<int64_t>()>0;
        }
        return !value.empty();
    }
    return false;
}
inline ActivityEventMatch MatchActivityEventRule(const nlohmann::json& events,
    const std::string& function,const nlohmann::json& parameters) {
    for (const auto& event:events) {
        if (event["Function"]!=function) continue;
        bool match=true;
        for (const auto& condition:event.value("Parameters",nlohmann::json::array())) {
            const auto* value=&parameters;
            for (const auto& part:condition["Path"]) {
                if (!value->is_object()) {value=nullptr;break;}
                const auto found=value->find(part.get<std::string>());
                if (found==value->end()) {value=nullptr;break;}
                value=&*found;
            }
            if(condition.contains("Exists")) {if((value!=nullptr)!=condition["Exists"].get<bool>()){match=false;break;}continue;}
            if (!value) {match=false;break;}
            if (condition.contains("NonEmpty")) {
                if (ActivityNonEmpty(*value)!=condition["NonEmpty"].get<bool>()) {match=false;break;}
            } else if(condition.contains("Equals")||condition.contains("NotEquals")) {
                if (!(value->is_boolean() || value->is_number() || value->is_string())) {match=false;break;}
                const bool equal=ActivityScalarEquals(*value,condition.contains("Equals")?condition["Equals"]:condition["NotEquals"]);
                if(equal!=condition.contains("Equals")){match=false;break;}
            } else {
                if(!value->is_number()){match=false;break;}const double actual=value->get<double>();bool ordered=false;
                if(condition.contains("GreaterThan"))ordered=actual>condition["GreaterThan"].get<double>();
                else if(condition.contains("GreaterOrEqual"))ordered=actual>=condition["GreaterOrEqual"].get<double>();
                else if(condition.contains("LessThan"))ordered=actual<condition["LessThan"].get<double>();
                else if(condition.contains("LessOrEqual"))ordered=actual<=condition["LessOrEqual"].get<double>();
                if(!ordered){match=false;break;}
            }
        }
        if (match) return {event["State"].get<std::string>(),
            event.value("Action",std::string("Pulse"))};
    }
    return {};
}
inline std::string MatchActivityEvent(const nlohmann::json& events,
    const std::string& function,const nlohmann::json& parameters) {
    return MatchActivityEventRule(events,function,parameters).State;
}
}
