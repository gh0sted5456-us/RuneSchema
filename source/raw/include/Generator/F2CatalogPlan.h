#pragma once
// Engine-independent discovery policy. A path hint is NEVER spawn permission.
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace PS::F2Catalog {
inline constexpr std::size_t MaxEntries=32768, MaxRoots=128;
enum class Kind { Enemy, Resource, Item, ClassCandidate, Npc };
struct Candidate {
    std::string path,name;
    Kind kind=Kind::ClassCandidate;
    bool operator==(const Candidate&)const=default;
};
inline const char* KindName(Kind kind) {
    switch(kind){case Kind::Npc:return "NPC";case Kind::Enemy:return "Enemy";case Kind::Resource:return "Resource";case Kind::Item:return "Item";default:return "Class";}
}
inline std::optional<Kind> ParseKind(std::string_view value) {
    if(value=="NPC"||value=="Npc")return Kind::Npc;
    if(value=="Enemy"||value=="AI")return Kind::Enemy;
    if(value=="Resource")return Kind::Resource;
    if(value=="Item")return Kind::Item;
    if(value=="Class")return Kind::ClassCandidate;
    return {};
}
inline std::string Lower(std::string text) {
    for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c+('a'-'A'));
    return text;
}
inline bool ValidRoot(std::string_view path) {
    if(path.size()<3||path.size()>1024||path.front()!='/'||path.back()=='/'
        ||path.find("//")!=path.npos||path.find("..")!=path.npos)return false;
    const auto lower=Lower(std::string(path));
    if(lower.starts_with("/script/")||lower=="/script"||lower.starts_with("/engine/")||lower=="/engine"
        ||lower.starts_with("/temp/")||lower=="/temp"||lower.starts_with("/memory/")||lower=="/memory")return false;
    for(unsigned char c:path)if(!(std::isalnum(c)||c=='_'||c=='/'||c=='-'))return false;
    return true;
}
inline bool ValidObjectPath(std::string_view path) {
    if(path.size()>2048)return false;
    const auto dot=path.rfind('.');
    if(dot==path.npos||dot+1==path.size()||!ValidRoot(path.substr(0,dot)))return false;
    for(unsigned char c:path.substr(dot+1))if(!(std::isalnum(c)||c=='_'||c=='-'))return false;
    return true;
}
inline bool Under(std::string_view path,std::string_view root) {
    const auto p=Lower(std::string(path)),r=Lower(std::string(root));
    return p.starts_with(r)&&p.size()>r.size()&&(p[r.size()]=='/'||p[r.size()]=='.');
}
inline bool UnderAny(std::string_view path,const std::vector<std::string>& roots) {
    return std::any_of(roots.begin(),roots.end(),[&](const auto& root){return Under(path,root);});
}
inline std::vector<std::string> ResourceRoots() {
    return {"/Game/Gameplay/World/Trees","/Game/Gameplay/World/Mining",
        "/DowdunReach/Gameplay/World","/ScornedWilderness/Gameplay/World","/UmbralSands/Gameplay/World"};
}
inline std::vector<std::string> EnemyRoots() {
    return {"/Game/Gameplay/AI","/Game/Gameplay/NPCs","/DowdunReach/Gameplay/AI",
        "/ScornedWilderness/Gameplay/AI","/UmbralSands/Gameplay/AI"};
}
struct Sources {
    std::vector<std::string> resources=ResourceRoots(),enemies=EnemyRoots();
    bool onlineReference=true;
    std::string dataset="1.0.0.2",reference="main";
    int referenceCacheHours=12; // Legacy decode compatibility only; Helpy reference updates are manual.
};
inline bool SimpleToken(std::string_view text) {
    if(text.empty()||text.size()>128||text.find("..")!=text.npos)return false;
    return std::all_of(text.begin(),text.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-'||c=='.';});
}
inline bool Matches(std::string_view value,std::string_view query) {
    const auto h=Lower(std::string(value)),q=Lower(std::string(query));std::size_t at=0;
    while(at<q.size()) {
        const auto begin=q.find_first_not_of(' ',at);if(begin==q.npos)break;
        const auto end=q.find(' ',begin);const auto token=q.substr(begin,end==q.npos?q.size()-begin:end-begin);
        if(h.find(token)==h.npos)return false;
        if(end==q.npos)break;
        at=end+1;
    }
    return true;
}
inline std::optional<Candidate> FromExport(std::string path,const Sources& sources) {
    // Inputs are relative to the Archive RSDragonwilds folder, not arbitrary filesystem paths.
    if(!path.ends_with(".json"))return {};
    path.resize(path.size()-5);
    if(path.starts_with("Content/"))path="/Game/"+path.substr(8);
    else if(path.starts_with("Plugins/GameFeatures/")) {
        path.erase(0,21);const auto slash=path.find('/');
        if(slash==path.npos||path.substr(slash,9)!="/Content/")return {};
        path="/"+path.substr(0,slash)+"/"+path.substr(slash+9);
    }else return {};
    const auto slash=path.rfind('/');const auto name=path.substr(slash+1);
    if(name.starts_with("ITEM_")&&name.find("MeshData")==name.npos) {
        const auto object=path+"."+name;
        if(ValidObjectPath(object))return Candidate{object,name,Kind::Item};
    }
    if(!name.starts_with("BP_"))return {};
    Kind kind;
    if(UnderAny(path,sources.resources))kind=Kind::Resource;
    else if(UnderAny(path,sources.enemies))kind=Lower(path).find("/gameplay/npcs/")!=std::string::npos?Kind::Npc:Kind::Enemy;
    else return {};
    const auto object=path+"."+name+(name.ends_with("_C")?"":"_C");
    if(!ValidObjectPath(object))return {};
    return Candidate{object,name,kind};
}
class Plan {
    std::map<std::string,std::size_t> paths;
    std::vector<bool> processed;
    std::vector<std::size_t> priority;
    std::size_t cursor=0,done=0;
public:
    std::vector<Candidate> entries;
    bool Add(Candidate value) {
        if(!ValidObjectPath(value.path)||value.name.size()>512)throw std::runtime_error("Invalid F2 index path/name");
        const auto key=Lower(value.path);
        if(paths.contains(key))return false;
        if(entries.size()>=MaxEntries)throw std::runtime_error("F2 catalogue candidate limit reached; coverage is partial");
        const auto index=entries.size();entries.push_back(std::move(value));
        try {processed.push_back(false);}catch(...) {entries.pop_back();throw;}
        try {paths.emplace(key,index);}catch(...) {processed.pop_back();entries.pop_back();throw;}
        return true;
    }
    void Prioritize(std::string_view query) {
        priority.clear();if(query.size()<2||query.size()>2048)return;
        for(std::size_t i=0;i<entries.size();++i)if(!processed[i]&&Matches(entries[i].path+" "+entries[i].name,query)) {
            priority.push_back(i);if(priority.size()==48)break;
        }
        std::reverse(priority.begin(),priority.end());
    }
    std::optional<Candidate> Pop() {
        while(!priority.empty()) {
            const auto i=priority.back();priority.pop_back();
            if(!processed[i]){processed[i]=true;++done;return entries[i];}
        }
        while(cursor<entries.size()&&processed[cursor])++cursor;
        if(cursor==entries.size())return {};
        processed[cursor]=true;++done;return entries[cursor++];
    }
    std::size_t Done()const{return done;}
    std::size_t Total()const{return entries.size();}
    bool Empty()const{return done==entries.size();}
};
} // namespace PS::F2Catalog
