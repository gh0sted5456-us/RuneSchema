#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace PS::HelpyRemoteAccess {
// Game-thread only. Native RPC ownership supplies peer/world identities.
class Sessions {
public:
    using Clock=std::chrono::steady_clock;
    enum class Result {Granted,Disabled,Denied,RateLimited,Capacity};
    static bool ValidPin(std::string_view pin){
        if(pin.size()<8||pin.size()>64)return false;
        for(unsigned char c:pin)if(c<33||c>126)return false;
        return true;
    }
    void Reset(){m_peers.clear();m_world.clear();m_pin.clear();m_failures=0;m_failureWindow={};}
    Result Authenticate(const std::string& peer,const std::string& world,bool enabled,
        const std::string& configured,std::string_view supplied,Clock::time_point now=Clock::now()){
        Configure(world,enabled,configured);
        if(!enabled||!ValidPin(configured)||peer.empty()||world.empty())return Result::Disabled;
        if(now-m_failureWindow>=std::chrono::minutes(1)){m_failureWindow=now;m_failures=0;}
        if(m_failures>=60)return Result::RateLimited;
        std::erase_if(m_peers,[&](const auto& row){return row.second.Expires<=now&&now-row.second.Window>=std::chrono::minutes(1);});
        if(!m_peers.contains(peer)&&m_peers.size()>=256)return Result::Capacity;
        auto& entry=m_peers[peer];
        if(now-entry.Window>=std::chrono::minutes(1)){entry.Window=now;entry.Failures=0;}
        if(entry.Failures>=5)return Result::RateLimited;
        unsigned difference=static_cast<unsigned>(configured.size()^supplied.size());
        for(size_t i=0;i<64;++i)difference|=(i<configured.size()?static_cast<unsigned char>(configured[i]):0)
            ^(i<supplied.size()?static_cast<unsigned char>(supplied[i]):0);
        entry.Granted=difference==0;
        entry.Expires=now+(entry.Granted?std::chrono::hours(1):std::chrono::hours(0));
        if(!entry.Granted){++entry.Failures;++m_failures;return Result::Denied;}
        entry.Failures=0;return Result::Granted;
    }
    bool Allowed(const std::string& peer,const std::string& world,bool enabled,const std::string& pin,
        Clock::time_point now=Clock::now()){
        Configure(world,enabled,pin);
        const auto found=m_peers.find(peer);
        if(!enabled||!ValidPin(pin)||found==m_peers.end()||!found->second.Granted||found->second.Expires<=now)return false;
        found->second.Expires=now+std::chrono::hours(1);return true;
    }
    void Revoke(const std::string& peer){const auto found=m_peers.find(peer);if(found!=m_peers.end())found->second.Granted=false;}
private:
    struct Entry {bool Granted=false;unsigned Failures=0;Clock::time_point Window{},Expires{};};
    std::map<std::string,Entry> m_peers;
    std::string m_world,m_pin;
    bool m_enabled=false;
    unsigned m_failures=0;
    Clock::time_point m_failureWindow{};
    void Configure(const std::string& world,bool enabled,const std::string& pin){
        if(world!=m_world||pin!=m_pin||enabled!=m_enabled){Reset();m_world=world;m_pin=pin;m_enabled=enabled;}
    }
};
inline Sessions Server;
inline constexpr auto ItemLane="network:ITEM:/Script/Dominion.ItemSubsystem";
inline bool ItemIdsMatch(const std::map<std::string,std::string>& client,const std::map<std::string,std::string>& authority,bool authorityProof){
    const auto own=authority.find(ItemLane),peer=client.find(ItemLane);
    return own!=authority.end()&&own->second.size()==16
        &&(authorityProof||(peer!=client.end()&&peer->second==own->second));
}
inline bool ClientAllowed=false;
inline bool ClientTeleport=false;
inline std::uint64_t ClientGeneration=0;
inline bool ClientReady(std::uint64_t generation){return ClientAllowed&&ClientGeneration==generation;}
inline bool SelfRequest(std::string_view action,std::string_view target,std::string_view guid,std::string_view controller){
    return (action=="GiveItems"||action=="MapTeleport")&&!guid.empty()
        &&(target.empty()||target==guid||target==controller);
}
}
