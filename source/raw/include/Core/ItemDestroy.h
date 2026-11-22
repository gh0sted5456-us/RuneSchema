#pragma once
#include "Core/ItemSnapshot.h"
#include <charconv>
#include <map>
#include <tuple>
#include <utility>
// Volatile native-call dedupe only. Grand Exchange must retain its durable Txn/Attempt journal.
namespace PS::ItemDestroy {
using Json=nlohmann::json;
inline constexpr std::string_view Service="runeschema.item.destroy",Capability="item.destroy",Schema="runeschema.item-destroy/1";
inline constexpr size_t MinCapacity=256*1024,MaxRequests=4096,MaxCachedBytes=16*1024*1024;
struct BufferRequired{size_t Size;};
enum class State{Confirmed,Retained,Unknown};
inline std::string_view Name(State state){return state==State::Confirmed?"Confirmed":state==State::Retained?"Retained":"Unknown";}
struct Request{ItemSnapshot::Request Exact;std::string Id;int Count{};};
inline Request Parse(Json input){
    if(!input.is_object())throw std::invalid_argument("A destroy request must be an object.");
    Request request;const auto id=input.find("Request"),count=input.find("Count");
    if(id==input.end()||!id->is_string())throw std::invalid_argument("Request must be a UUID.");
    request.Id=id->get<std::string>();
    if(request.Id.size()!=36)throw std::invalid_argument("Request must be a UUID.");
    for(size_t i=0;i<request.Id.size();++i){auto& c=request.Id[i];
        if(i==8||i==13||i==18||i==23){if(c!='-')throw std::invalid_argument("Request must be a UUID.");}
        else{if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))throw std::invalid_argument("Request must be a UUID.");if(c>='A'&&c<='F')c=char(c-'A'+'a');}}
    if(request.Id=="00000000-0000-0000-0000-000000000000")throw std::invalid_argument("A zero Request UUID is invalid.");
    if(count==input.end()||!count->is_number_integer()||*count<1||*count>1000000)throw std::invalid_argument("Count must be a whole stack between 1 and 1000000.");
    request.Count=count->get<int>();input.erase("Count");input.erase("Request");request.Exact=ItemSnapshot::Parse(input);
    if(request.Exact.Where==ItemSnapshot::Scope::Loadout)throw std::invalid_argument("Equipped items cannot be destroyed.");
    return request;
}
inline Json Body(const Request& r){
    Json body{{"Player",r.Exact.Player},{"Scope",ItemSnapshot::Name(r.Exact.Where)},{"Slot",r.Exact.Slot},{"Instance",r.Exact.Instance},{"Item",r.Exact.Item},{"Count",r.Count},{"Request",r.Id}};
    if(r.Exact.Where==ItemSnapshot::Scope::Owned)body["Container"]=ItemSnapshot::ContainerJson(r.Exact);
    return body;
}
inline Json Refusal(std::string_view phase,std::string_view error){return {{"Schema",Schema},{"State","Refused"},{"Ok",false},{"Phase",phase},{"Error",std::string(error.substr(0,300))},{"Observed",{{"InMemory",true}}}};}
inline void CheckSnapshot(const Request& r,const Json& snapshot){
    if(!snapshot.is_object()||!snapshot.value("Ok",false)||snapshot.at("Schema")!=std::string(ItemSnapshot::Schema)
        ||snapshot.at("Item")!=r.Exact.Item||snapshot.at("SourceInstance")!=r.Exact.Instance)
        throw std::runtime_error("The exact live item could not be captured.");
    const auto& observed=snapshot.at("Observed");
    if(r.Exact.Where==ItemSnapshot::Scope::Owned&&(!observed.contains("Container")||observed.at("Container")!=ItemSnapshot::ContainerJson(r.Exact)))
        throw std::runtime_error("The live item observation belongs to another container.");
    if(observed.at("Player")!=r.Exact.Player||observed.at("Scope")!=std::string(ItemSnapshot::Name(r.Exact.Where))||observed.at("Slot")!=r.Exact.Slot||observed.at("InMemory")!=true)
        throw std::runtime_error("The live item observation belongs to another target.");
    int live=0,counts=0;
    for(const auto& field:snapshot.at("Fields"))if(field.at("Name")=="Count"){
        const auto text=field.at("Value").get<std::string>();const auto parsed=std::from_chars(text.data(),text.data()+text.size(),live);
        if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())throw std::runtime_error("The native item count is unreadable.");++counts;}
    if(counts!=1||live!=r.Count)throw std::runtime_error("Destroy requires the whole current stack. Split it in game first.");
    const auto digest=snapshot.at("Digest").get<std::string>();
    if(digest.size()!=71||!digest.starts_with("sha256:")||digest.substr(7).find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("The native snapshot digest is invalid.");
}
class Requests{
    struct Entry{std::string Body;Json Reply;};
    std::map<std::tuple<std::string,std::string,std::string>,Entry> Entries;
    size_t Bytes=0;
public:
    // No eviction: losing an Unknown or a first result could turn a replay into a second action.
    template<class Prepare,class Invoke,class Verify>
    Json Run(const std::string& caller,bool permitted,const Request& request,size_t capacity,Prepare prepare,Invoke invoke,Verify verifyState){
        if(!permitted)throw std::invalid_argument("The caller has no item.destroy capability.");
        if(capacity<MinCapacity)throw BufferRequired{MinCapacity};
        const auto key=std::make_tuple(caller,request.Exact.Player,request.Id);const auto body=Body(request).dump();
        if(const auto found=Entries.find(key);found!=Entries.end()){
            if(found->second.Body!=body)throw std::invalid_argument("Request UUID conflicts with its first body.");
            const auto needed=found->second.Reply.dump().size()+1;if(needed>capacity)throw BufferRequired{needed};
            return found->second.Reply;
        }
        if(Entries.size()>=MaxRequests||Bytes+4096>MaxCachedBytes)throw std::runtime_error("Native destroy receipt cache is full; no action attempted.");
        Json snapshot,reply;
        try{snapshot=prepare();CheckSnapshot(request,snapshot);}
        catch(const std::exception& error){reply=Refusal("ExactItem",error.what());}
        catch(...){reply=Refusal("ExactItem","Native item preparation failed; nothing was invoked.");}
        const bool refused=!reply.is_null();
        if(!refused)reply={{"Schema",Schema},{"State","Confirmed"},{"Ok",true},{"Request",request.Id},{"Snapshot",snapshot},{"Observed",{{"InMemory",true}}}};
        // Bound the complete longest possible result before reserving or invoking, not only a fixed minimum.
        const auto needed=reply.dump().size()+1;if(needed>capacity)throw BufferRequired{needed};
        const auto stored=needed+body.size()+1024;if(stored>MaxCachedBytes-Bytes)throw std::runtime_error("Native destroy receipt cache is full; no action attempted.");
        if(!refused){reply["State"]="Unknown";reply["Ok"]=false;}
        auto [entry,inserted]=Entries.emplace(key,Entry{body,std::move(reply)});Bytes+=stored;
        if(refused)return entry->second.Reply;
        try{invoke();const auto state=verifyState();entry->second.Reply["State"]=Name(state);entry->second.Reply["Ok"]=state==State::Confirmed;}
        catch(...){/* The pre-reserved Unknown remains the first result, including failure during Invoke. */}
        return entry->second.Reply;
    }
};
}
