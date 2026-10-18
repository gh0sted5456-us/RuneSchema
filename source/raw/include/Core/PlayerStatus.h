#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
namespace PS::PlayerStatus {
using Json=nlohmann::json;
struct Flags {
    uint8_t Admin=0,Build=0,OpenChests=0,Chat=0,Owner=0;
    bool Valid() const {
        unsigned used=0;for(const auto bit:{Admin,Build,OpenChests,Chat,Owner}) {
            if(!bit||(bit&(bit-1))||(used&bit))return false;used|=bit;
        }return true;
    }
};
inline Json Describe(unsigned mask,const Flags& flags,bool host) {
    Json rights=Json::array();const unsigned known=flags.Admin|flags.Build|flags.OpenChests|flags.Chat|flags.Owner;
    const bool valid=flags.Valid()&&mask<=255&&!(mask&~known);
    if(valid) {
        if(mask&flags.Build)rights.push_back("Build");
        if(mask&flags.OpenChests)rights.push_back("Open chests");
        if(mask&flags.Chat)rights.push_back("Chat");
    }
    const std::string role=valid&&(mask&flags.Owner)?"owner":valid&&(mask&flags.Admin)?"admin":host?"host":valid?"member":"unknown";
    return {{"Kind","RuneSchemaPlayerRole1"},{"Role",role},{"NativeKnown",valid},{"Host",host},{"Privileges",std::move(rights)}};
}
inline bool Valid(const Json& value) {
    try {
        if(!value.is_object()||value.at("Kind")!="RuneSchemaPlayerRole1"||!value.at("NativeKnown").is_boolean()||!value.at("Host").is_boolean())return false;
        const auto role=value.at("Role").get<std::string>();
        const bool known=value.at("NativeKnown").get<bool>(),host=value.at("Host").get<bool>();
        if(role!="owner"&&role!="admin"&&role!="host"&&role!="member"&&role!="unknown")return false;
        if((role=="owner"||role=="admin"||role=="member")&&!known)return false;
        if(role=="host"&&!host)return false;
        if(role=="unknown"&&(known||host))return false;
        const auto& rights=value.at("Privileges");if(!rights.is_array()||rights.size()>3)return false;
        std::set<std::string> seen;for(const auto& right:rights) {
            const auto text=right.get<std::string>();
            if((text!="Build"&&text!="Open chests"&&text!="Chat")||!seen.insert(text).second||!known)return false;
        }return true;
    }catch(...){return false;}
}
// Internal presentation envelope only. Never use this as an authorization token.
inline Json Snapshot(const Json& players,uint64_t generation) {
    static std::atomic<uint64_t> sequence=0;
    return {{"Kind","HelpyPlayerRoles1"},{"Generation",generation},{"Sequence",++sequence},{"Players",players}};
}
struct Cache {
    using Clock=std::chrono::steady_clock;
    static constexpr auto Lifetime=std::chrono::seconds(12);
    uint64_t Generation=0,Sequence=0;Clock::time_point Received{};
    std::map<std::string,Json> ByPath,ByGuid;
    static std::string Guid(const std::string& text){
        if(text.size()!=32||text.find_first_not_of("0123456789abcdefABCDEF")!=text.npos||text.find_first_not_of('0')==text.npos)return {};
        auto value=text;for(auto& ch:value)if(ch>='a'&&ch<='f')ch=static_cast<char>(ch-'a'+'A');return value;
    }
    void Reset(uint64_t generation) {Generation=generation;Sequence=0;Received={};ByPath.clear();ByGuid.clear();}
    bool Apply(const Json& snapshot,uint64_t generation,Clock::time_point now=Clock::now()) {
        if(Generation!=generation)Reset(generation);
        try {
            if(!snapshot.is_object()||snapshot.at("Kind")!="HelpyPlayerRoles1"
                ||snapshot.at("Generation").get<uint64_t>()!=generation)return false;
            const auto sequence=snapshot.at("Sequence").get<uint64_t>();
            if(!sequence||sequence<=Sequence)return false;
            const auto& rows=snapshot.at("Players");if(!rows.is_array()||rows.size()>256)return false;
            std::map<std::string,Json> paths,ids;std::set<std::string> ambiguous;
            for(const auto& row:rows) {
                if(!row.is_object()||!row.contains("RoleStatus")||!Valid(row["RoleStatus"]))continue;
                const auto path=row.value("Path",std::string{});
                if(path.empty()||path.size()>2048||paths.contains(path))return false;
                paths[path]=row["RoleStatus"];
                const auto id=Guid(row.value("Guid",std::string{}));if(!id.empty()) {
                    if(ids.contains(id))ambiguous.insert(id);
                    else ids[id]=row["RoleStatus"];
                }
            }
            for(const auto id:ambiguous)ids.erase(id);
            ByPath=std::move(paths);ByGuid=std::move(ids);Sequence=sequence;Received=now;return true;
        }catch(...){return false;}
    }
    const Json* Find(const std::string& path,const std::string& guid,Clock::time_point now=Clock::now()) const {
        if(!Sequence||now<Received||now-Received>=Lifetime)return nullptr;
        const auto id=Guid(guid);if(!guid.empty()){if(id.empty())return nullptr;const auto found=ByGuid.find(id);return found==ByGuid.end()?nullptr:&found->second;}
        if(!path.empty()){const auto found=ByPath.find(path);if(found!=ByPath.end())return &found->second;}
        return nullptr;
    }
};
}
