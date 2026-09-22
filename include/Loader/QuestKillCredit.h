#pragma once
#include "Loader/QuestDefinition.h"
#include <tuple>

namespace DragonWilds::Quests {
// Input must come from an authoritative, verified kill-credit route, not a
// damage guess, actor disappearance, or caller-supplied display name.
struct KillSignal {
    uint64_t WorldEpoch=0;
    int32_t ObjectIndex=-1,ObjectSerial=0;
    std::string CreditedCharacter,VictimClass,EventKey;
    std::vector<std::string> VictimBaseClasses;
    bool ConfirmedDeath=false,Authoritative=false;
    std::optional<std::array<double,3>> Position;
    std::string SpawnKey;
};
enum class KillResult { Ignored, Counted, Duplicate, Capped };
class KillCredits {
    using Key=std::tuple<std::string,std::string,int32_t,int32_t>;
    struct Pending {int Before,After;bool Committed=false;};
    std::map<Key,Pending> credits;
    std::map<Key,size_t> stageRoutes;
    uint64_t epoch=0;
public:
    void BeginWorld(uint64_t value) {
        if(!value || value<=epoch)throw std::runtime_error("Kill-credit world epoch must increase");
        epoch=value;credits.clear();stageRoutes.clear();
    }
    size_t StageForSignal(const std::string& key,const KillSignal& signal,size_t stage) {
        const Key identity{key,signal.CreditedCharacter,signal.ObjectIndex,signal.ObjectSerial};
        const auto found=stageRoutes.find(identity);if(found!=stageRoutes.end())return found->second;
        if(stageRoutes.size()>=8192)throw std::runtime_error("Stage kill route budget exhausted");
        stageRoutes.emplace(identity,stage);return stage;
    }
    template<class Read,class Write>
    KillResult Apply(const Definition& quest,const KillSignal& signal,const std::string& character,
        bool questActive,Read read,Write write) {
        if(!quest.Kill || !questActive || !signal.Authoritative || !signal.ConfirmedDeath
            || !epoch || signal.WorldEpoch!=epoch || signal.ObjectIndex<0 || signal.ObjectSerial<=0
            || character.empty() || signal.CreditedCharacter!=character)return KillResult::Ignored;
        const auto& target=*quest.Kill;
        // A marked AOR is itself the authoritative provenance boundary. This
        // lets matching native, event and authoring-tool enemies count at the
        // same location without impersonating an event identity. Unmarked
        // objectives retain strict event provenance.
        const bool areaScoped=quest.Marker && quest.Marker->RadiusMeters;
        if(!target.EventKey.empty() && !areaScoped && target.EventKey!=signal.EventKey)return KillResult::Ignored;
        if(!target.SpawnKey.empty() && target.SpawnKey!=signal.SpawnKey)return KillResult::Ignored;
        bool matches=false;
        for(const auto& allowed:target.Classes) {
            if(allowed==signal.VictimClass)matches=true;
            if(target.IncludeDerived)for(const auto& base:signal.VictimBaseClasses)if(base==allowed)matches=true;
        }
        if(!matches)return KillResult::Ignored;
        if(quest.Marker && quest.Marker->RadiusMeters) {
            if(!signal.Position)return KillResult::Ignored;
            for(double coordinate:*signal.Position)if(!std::isfinite(coordinate))return KillResult::Ignored;
            if(std::hypot((*signal.Position)[0]-quest.Marker->Position[0],(*signal.Position)[1]-quest.Marker->Position[1])>*quest.Marker->RadiusMeters*100.0)return KillResult::Ignored;
        }
        const Key key{quest.Key,character,signal.ObjectIndex,signal.ObjectSerial};
        auto found=credits.find(key);
        if(found!=credits.end() && found->second.Committed)return KillResult::Duplicate;
        const int current=read();
        if(current<0 || current>quest.Required.Count)throw std::runtime_error("Native kill counter outside objective bounds");
        if(found==credits.end()) {
            if(current==quest.Required.Count)return KillResult::Capped;
            if(credits.size()>=8192)throw std::runtime_error("Kill-credit identity budget exhausted");
            // Do not process a later kill while an earlier write for this
            // character/objective has an unresolved postcondition.
            for(const auto& [other,pending]:credits)if(std::get<0>(other)==quest.Key && std::get<1>(other)==character && !pending.Committed)
                throw std::runtime_error("Prior kill counter update is unresolved");
            found=credits.emplace(key,Pending{current,current+1}).first;
        }
        auto& pending=found->second;
        if(current!=pending.Before && current!=pending.After)throw std::runtime_error("Kill counter changed during recovery");
        if(current==pending.Before)write(pending.After);
        if(read()!=pending.After)throw std::runtime_error("Kill counter update was not confirmed");
        pending.Committed=true;
        return KillResult::Counted;
    }
};
}
