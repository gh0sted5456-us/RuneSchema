#pragma once
#include <compare>
#include <cstdint>
#include <map>
#include <utility>

namespace DragonWilds::Quests::Summons {
struct NativeAllegiance {
    bool Friendly=false,Confused=false,FollowsConfuser=false;
    bool Certified() const {return Friendly && Confused && FollowsConfuser;}
};
// Identity comparisons are intentionally direct: subordinate followers must be
// enrolled on the player too, rather than inheriting credit through an AI chain.
template<class Object>
bool MatchesEnrollment(Object attacker,Object caster,Object actorOwner,Object actorInstigator,
    Object componentOwner,Object follower,bool certified) {
    return attacker && caster && attacker!=caster && actorOwner==caster && actorInstigator==caster
        && componentOwner==caster && follower==attacker && certified;
}

struct ObjectIdentity {
    int32_t Index=-1,Serial=0;
    auto operator<=>(const ObjectIdentity&) const = default;
    bool Valid() const {return Index>=0 && Serial>0;}
};

// Observe the original native dispatch, never replay it. Track direct kills as
// well so a second fatal notification cannot credit a previously direct kill as
// a summon kill. Ordinary repeated vanilla calls keep their original behavior.
class NativeDispatches {
    ObjectIdentity m_world;
    uint64_t m_epoch=0;
    std::map<std::pair<ObjectIdentity,ObjectIdentity>,bool> m_delivered;
public:
    enum class Decision {First,VanillaRepeat,Duplicate,Rejected};
    Decision Reserve(uint64_t epoch,ObjectIdentity world,ObjectIdentity victim,ObjectIdentity player,bool summon) {
        if(!epoch || !world.Valid() || !victim.Valid() || !player.Valid())return Decision::Rejected;
        if(m_world!=world || m_epoch!=epoch) {m_delivered.clear();m_world=world;m_epoch=epoch;}
        const auto [entry,inserted]=m_delivered.emplace(std::make_pair(victim,player),summon);
        return inserted?Decision::First:(!summon && !entry->second?Decision::VanillaRepeat:Decision::Duplicate);
    }
    bool Allow(uint64_t epoch,ObjectIdentity world,ObjectIdentity victim,ObjectIdentity player,bool summon) {
        const auto decision=Reserve(epoch,world,victim,player,summon);
        return decision==Decision::First || decision==Decision::VanillaRepeat;
    }
    void Cancel(uint64_t epoch,ObjectIdentity world,ObjectIdentity victim,ObjectIdentity player) {
        if(m_epoch==epoch && m_world==world)m_delivered.erase(std::make_pair(victim,player));
    }
    template<class IsLive>void Prune(IsLive live) {
        for(auto entry=m_delivered.begin();entry!=m_delivered.end();)
            if(!live(entry->first.first) || !live(entry->first.second))entry=m_delivered.erase(entry);
            else ++entry;
    }
};
}
