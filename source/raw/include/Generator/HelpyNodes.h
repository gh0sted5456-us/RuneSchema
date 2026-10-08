#pragma once
// Presentation/lease policy only. Classification and favorites never grant spawn permission.
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
namespace PS::HelpyNodes {
enum class Kind { Npc, AI, Tree, Mineral, OtherResource };
inline std::string Fold(std::string_view value) {
    std::string s(value);for(auto& c:s)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);return s;
}
inline Kind ResourceKind(std::string_view path,std::string_view evidence={}) {
    const auto p=Fold(path),e=Fold(evidence);
    // Prefer the native ancestry/family captured by the loader, then explicit directory families.
    if(e=="tree")return Kind::Tree;
    if(e=="mineral")return Kind::Mineral;
    if(p.find("/trees/")!=p.npos)return Kind::Tree;
    if(p.find("/mining/")!=p.npos)return Kind::Mineral;
    return Kind::OtherResource;
}
inline Kind EntryKind(int slot,std::string_view nativeKind,std::string_view path,std::string_view family={}) {
    if(slot==2)return ResourceKind(path,family);
    return nativeKind=="NPC"?Kind::Npc:Kind::AI;
}
inline const char* Prefix(Kind k) {
    if(k==Kind::Npc)return "@npc:";
    if(k==Kind::AI)return "@ai:";
    return "@resource:";
}
inline std::string FavoriteKey(Kind kind,std::string_view id) {return std::string(Prefix(kind))+std::string(id);}
inline bool ValidFavoriteKey(std::string_view key) {
    std::size_t prefix=0;
    for(const auto* p:{"@npc:","@ai:","@resource:"})if(key.starts_with(p)){prefix=std::char_traits<char>::length(p);break;}
    if(!prefix||key.size()<=prefix||key.size()>4096)return false;
    return std::none_of(key.begin(),key.end(),[](unsigned char c){return c<32||c==127||c=='\\';});
}
inline std::string_view FavoriteId(std::string_view key) {
    if(!ValidFavoriteKey(key))return {};
    return key.substr(key.find(':')+1);
}
inline bool FavoriteIn(int slot,std::string_view key) {
    return slot==1?(key.starts_with("@npc:")||key.starts_with("@ai:")):
        slot==2&&key.starts_with("@resource:");
}
inline bool InTab(Kind kind,int tab,bool includeOther) {
    if(tab==2)return true; // Favorites is narrowed separately by exact typed identity.
    if(kind==Kind::Npc)return tab==0;
    if(kind==Kind::AI)return tab==1;
    if(kind==Kind::Tree)return tab==0;
    return tab==1&&(kind==Kind::Mineral||includeOther);
}
inline constexpr int MinDuration=5,MaxDuration=86400,DefaultDuration=300,MaxTemporary=32;
inline void ValidateDuration(bool permanent,int seconds) {
    if(permanent) {if(seconds!=0)throw std::runtime_error("Permanent NPCs cannot have a temporary duration.");}
    else if(seconds<MinDuration||seconds>MaxDuration)throw std::runtime_error("NPC lifetime must be 5..86400 seconds.");
}
// Monotonic real-time lease: independent of frame rate, menu opening or game time dilation.
// The native loader owns actors and requests destruction; expired is never equivalent to freed.
struct Lease {
    double deadline=0;
    bool retiring=false;
    static Lease Start(double now,int seconds) {
        ValidateDuration(false,seconds);
        if(!std::isfinite(now)||now<0)throw std::runtime_error("NPC lifetime clock unavailable.");
        return {now+seconds,false};
    }
    bool Due(double now)const {return retiring||(std::isfinite(now)&&now>=deadline);}
    void Retire(){retiring=true;}
};
} // namespace PS::HelpyNodes
