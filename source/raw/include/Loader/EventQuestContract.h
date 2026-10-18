#pragma once
#include "Loader/EventDefinition.h"
#include "Loader/QuestDefinition.h"

namespace DragonWilds::Events {
inline bool Inside(const std::array<double,3>& position,const std::array<double,3>& center,double meters) {
    double squared=0;
    for(size_t i=0;i<3;++i){if(!std::isfinite(position[i])||!std::isfinite(center[i]))return false;const double d=position[i]-center[i];squared+=d*d;}
    return std::isfinite(meters)&&meters>0&&squared<=meters*meters*10000.0;
}
inline Definition AnchorContract(const Definition& authored,const std::array<double,3>& participant) {
    auto resolved=authored;
    if(!authored.Contract)return resolved;
    if(!authored.EventArea || !Inside(participant,authored.EventArea->Position,authored.Contract->ActivationRadius))
        throw std::runtime_error("Travel to the marked encounter site before activating this contract");
    for(auto& wave:resolved.Waves)for(auto& member:wave) {
        if(authored.Contract->ParticipantOrigin){member.Position[0]+=participant[0];member.Position[1]+=participant[1];}
        const std::array<double,3> point{member.Position[0],member.Position[1],member.RelativeGround?participant[2]:member.Position[2]};
        if(!Inside(point,authored.EventArea->Position,authored.EventArea->RadiusMeters))
            throw std::runtime_error("Encounter member lies outside the authored arena");
    }
    return resolved;
}
inline void ValidateContractQuest(const Definition& event,const Quests::Definition& quest) {
    if(!event.Contract || event.Contract->Quest!=quest.Key || !quest.Stages.empty() || !quest.Kill
        || quest.Required.Count!=1 || quest.Kill->EventKey!=event.Key || quest.Kill->SpawnKey.empty())
        throw std::runtime_error("Contract requires its own single Kill objective scoped to this event and final target spawn");
    size_t targets=0;
    for(size_t wave=0;wave<event.Waves.size();++wave)for(const auto& member:event.Waves[wave])if(member.Spawn==quest.Kill->SpawnKey) {
        if(wave+1!=event.Waves.size())throw std::runtime_error("Contract target must occur only in the final wave");
        ++targets;
    }
    if(targets!=1)throw std::runtime_error("Contract requires exactly one final target spawn");
}
class ContractBoundary {
    double outside=0;
public:
    void Reset(){outside=0;}
    double OutsideSeconds()const{return outside;}
    bool Tick(bool inside,double delta,double grace) {
        if(!std::isfinite(delta)||delta<0||!std::isfinite(grace)||grace<0)return false;
        if(inside){outside=0;return true;}
        outside+=delta;
        return std::isfinite(outside)&&outside<grace;
    }
};
}
