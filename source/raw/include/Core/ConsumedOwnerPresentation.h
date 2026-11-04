#pragma once
#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace PS::ConsumedOwnerPresentation {
using Clock=std::chrono::steady_clock;
inline bool Identifier(const std::string& value){
    return !value.empty()&&value.size()<=96&&std::all_of(value.begin(),value.end(),[](unsigned char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';
    });
}
inline void ValidateDeclaration(const nlohmann::json& authority,const nlohmann::json& consumer){
    if(!authority.contains("OwnerPresentation")){
        if(consumer.is_object()&&consumer.value("Audience",std::string{})=="Owner")
            throw std::runtime_error("Owner-only consumers require a consumed-item OwnerPresentation declaration");
        return;
    }
    const auto& owner=authority["OwnerPresentation"];
    if(authority.value("Action",std::string{})!="ConsumedItemAuthority"||!owner.is_object()||owner.size()!=2
        ||!owner.contains("Connection")||!owner["Connection"].is_string()||!Identifier(owner["Connection"].get<std::string>())
        ||!owner.contains("Key")||!owner["Key"].is_string()||!Identifier(owner["Key"].get<std::string>())
        ||!consumer.is_object()||consumer.value("Audience",std::string{})!="Owner"
        ||!consumer.contains("Function")||!consumer["Function"].is_string()||!Identifier(consumer["Function"].get<std::string>())
        ||consumer.value("Connection",std::string{})!=owner["Connection"].get<std::string>()
        ||!consumer.contains("AllowedKeys")||!consumer["AllowedKeys"].is_array()
        ||std::find(consumer["AllowedKeys"].begin(),consumer["AllowedKeys"].end(),owner["Key"])==consumer["AllowedKeys"].end())
        throw std::runtime_error("OwnerPresentation requires consumed-item authority and its declared owner-only consumer key");
}
struct Receipt {
    std::string Owner,Key;
    uint64_t Id=0;
    Clock::time_point Expires;
    bool NativeDone=false,BridgeClaimed=false,BridgeDone=false,Sent=false;
};
struct Claim {uint64_t Id;bool AlreadyExecuted;};
class Ledger {
    struct Observation {std::string Owner,Key;Clock::time_point Expires;};
    std::vector<Receipt> Receipts;
    std::vector<Observation> Observations;
    uint64_t Next=0;
    static void Identity(const std::string& owner,const std::string& key){
        if(owner.empty()||key.empty()||owner.size()>512||key.size()>193)
            throw std::runtime_error("Consumed presentation identity is invalid");
    }
public:
    static constexpr size_t Capacity=4096;
    void Prune(Clock::time_point now){
        std::erase_if(Receipts,[&](const auto& row){return row.Expires<=now;});
        std::erase_if(Observations,[&](const auto& row){return row.Expires<=now;});
    }
    uint64_t Removed(const std::string& owner,const std::string& key,Clock::time_point now){
        Identity(owner,key);Prune(now);
        if(Receipts.size()>=Capacity||Next==uint64_t(std::numeric_limits<int64_t>::max()))
            throw std::runtime_error("Consumed presentation receipt capacity reached");
        Receipt row{owner,key,++Next,now+std::chrono::seconds(5)};
        auto observation=std::find_if(Observations.begin(),Observations.end(),[&](const auto& value){return value.Owner==owner&&value.Key==key;});
        if(observation!=Observations.end()){row.NativeDone=true;Observations.erase(observation);}
        Receipts.push_back(std::move(row));return Next;
    }
    void NativeCompleted(const std::string& owner,const std::string& key,Clock::time_point now){
        Identity(owner,key);Prune(now);
        auto row=std::find_if(Receipts.begin(),Receipts.end(),[&](const auto& value){return value.Owner==owner&&value.Key==key&&!value.NativeDone;});
        if(row!=Receipts.end()){row->NativeDone=true;return;}
        if(Observations.size()>=Capacity)throw std::runtime_error("Consumed presentation observation capacity reached");
        Observations.push_back({owner,key,now+std::chrono::seconds(5)});
    }
    std::optional<Claim> ClaimBridge(const std::string& owner,const std::string& key,Clock::time_point now){
        Identity(owner,key);Prune(now);
        auto row=std::find_if(Receipts.begin(),Receipts.end(),[&](const auto& value){return value.Owner==owner&&value.Key==key&&!value.BridgeClaimed;});
        if(row==Receipts.end())return std::nullopt;
        row->BridgeClaimed=true;return Claim{row->Id,row->NativeDone};
    }
    void BridgeCompleted(const std::string& owner,uint64_t id,Clock::time_point now){
        Prune(now);
        auto row=std::find_if(Receipts.begin(),Receipts.end(),[&](const auto& value){return value.Owner==owner&&value.Id==id&&value.BridgeClaimed;});
        if(row==Receipts.end())throw std::runtime_error("Consumed presentation has no admitted removal receipt");
        row->BridgeDone=true;
    }
    std::vector<Receipt> TakeReady(Clock::time_point now){
        Prune(now);std::vector<Receipt> result;
        // Retain sent receipts so a late bridge request can recognize a native callback already executed.
        for(auto& row:Receipts)if(!row.Sent&&(row.NativeDone||row.BridgeDone)){
            row.Sent=true;result.push_back(row);
        }
        return result;
    }
    void Clear(){Receipts.clear();Observations.clear();}
};
class ReplayWindow {
    struct State {uint64_t Highest=0;std::bitset<Ledger::Capacity> Seen;};
    std::map<std::string,State> Sources;
public:
    bool Accept(const std::string& source,uint64_t id){
        if(source.empty()||source.size()>512||id==0||id>uint64_t(std::numeric_limits<int64_t>::max()))return false;
        if(!Sources.contains(source)&&Sources.size()>=128)return false;
        auto& state=Sources[source];
        if(id>state.Highest){
            const auto distance=id-state.Highest;
            if(distance>=Ledger::Capacity)state.Seen.reset();else state.Seen<<=size_t(distance);
            state.Highest=id;
        }
        const auto offset=state.Highest-id;
        if(offset>=Ledger::Capacity||state.Seen.test(size_t(offset)))return false;
        state.Seen.set(size_t(offset));return true;
    }
    void Clear(){Sources.clear();}
};
}
