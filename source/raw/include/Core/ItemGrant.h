#pragma once
// runeschema.item.grant: put a whole quantity of one item definition into one authoritative
// inventory, through the game's own InventoryComponent:AddItemByData. This header is the
// engine-free half (request rules and the replay cache), shared with tests;
// Runtime/ItemGrantService.cpp does the native work on the game thread.
//
// Targets: a connected character's Inventory or Personal container (the Helpy delivery flags
// apply, as for any delivery to a character), or a world container such as a chest, named by
// its object identity. A player's own containers are reachable only as a Character target.
// The cache is volatile, like item.destroy's: a consumer keeps its own durable journal.
#include "Core/ItemSnapshot.h"
#include <map>
#include <tuple>
namespace PS::ItemGrant {
using Json=nlohmann::json;
inline constexpr std::string_view Service="runeschema.item.grant",Capability="item.grant",Schema="runeschema.item-grant/1";
inline constexpr size_t MinCapacity=4096,MaxRequests=4096,MaxCachedBytes=4*1024*1024;
inline constexpr int MaxCount=10000;
struct BufferRequired{size_t Size;};
enum class State{Confirmed,Unknown};
inline std::string_view Name(State state){return state==State::Confirmed?"Confirmed":"Unknown";}
enum class Kind{Character,Container};
struct Target{Kind Is=Kind::Character;std::string Character;ItemSnapshot::Scope Where=ItemSnapshot::Scope::Inventory;int32_t Index=-1,Serial=0;};
struct Request{std::string Id,Item;int Count=0;Target To;};

// A lower-case canonical UUID; the all-zero UUID is refused.
inline std::string Uuid(const Json& value){
    if(!value.is_string())throw std::invalid_argument("Request must be a UUID.");
    auto id=value.get<std::string>();
    if(id.size()!=36)throw std::invalid_argument("Request must be a UUID.");
    for(size_t i=0;i<id.size();++i){auto& c=id[i];
        if(i==8||i==13||i==18||i==23){if(c!='-')throw std::invalid_argument("Request must be a UUID.");continue;}
        if(c>='A'&&c<='F')c=static_cast<char>(c-'A'+'a');
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::invalid_argument("Request must be a UUID.");}
    if(id=="00000000-0000-0000-0000-000000000000")throw std::invalid_argument("A zero Request UUID is invalid.");
    return id;
}
// {"Request":uuid,"Item":"/Game/...","Count":n,"Target":{"Character":guid,"Scope":"Inventory"|"Personal"}}
// {"Request":uuid,"Item":"/Game/...","Count":n,"Target":{"Container":{"Index":i,"Serial":s}}}
inline Request Parse(const Json& input){
    if(!input.is_object())throw std::invalid_argument("A grant request must be an object.");
    for(const auto& [key,value]:input.items())if(key!="Request"&&key!="Item"&&key!="Count"&&key!="Target")throw std::invalid_argument("Unknown grant field: "+key);
    Request request;request.Id=Uuid(input.value("Request",Json{}));
    const auto item=input.find("Item");
    if(item==input.end()||!item->is_string())throw std::invalid_argument("Item must be the item definition's object path.");
    request.Item=item->get<std::string>();
    if(request.Item.empty()||request.Item.size()>ItemSnapshot::MaxItemPath||request.Item.front()!='/'||request.Item.find('\0')!=std::string::npos)
        throw std::invalid_argument("Item must be the item definition's object path.");
    const auto count=input.find("Count");
    if(count==input.end()||!count->is_number_integer()||*count<1||*count>MaxCount)throw std::invalid_argument("Count must be a whole number from 1 to 10000.");
    request.Count=count->get<int>();
    const auto target=input.find("Target");
    if(target==input.end()||!target->is_object()||target->size()<1||target->size()>2)throw std::invalid_argument("Target must name one character or one container.");
    if(target->contains("Character")){
        for(const auto& [key,value]:target->items())if(key!="Character"&&key!="Scope")throw std::invalid_argument("Unknown target field: "+key);
        request.To.Is=Kind::Character;
        const auto& guid=target->at("Character");
        if(!guid.is_string()||(request.To.Character=ItemSnapshot::NormalizeGuid(guid.get<std::string>())).empty())throw std::invalid_argument("Character must be a character GUID.");
        const auto scope=target->value("Scope",std::string("Inventory"));
        if(scope=="Inventory")request.To.Where=ItemSnapshot::Scope::Inventory;
        else if(scope=="Personal")request.To.Where=ItemSnapshot::Scope::Personal;
        else throw std::invalid_argument("Scope must be Inventory or Personal.");
    }else if(target->contains("Container")){
        if(target->size()!=1)throw std::invalid_argument("A container target has no other fields.");
        const auto& container=target->at("Container");
        if(!container.is_object()||container.size()!=2||!container.contains("Index")||!container.contains("Serial")
            ||!container["Index"].is_number_integer()||!container["Serial"].is_number_integer())throw std::invalid_argument("Container must be {Index, Serial} from ObjectIdentity.");
        const auto index=container["Index"].get<int64_t>(),serial=container["Serial"].get<int64_t>();
        if(index<0||index>INT32_MAX||serial<1||serial>INT32_MAX)throw std::invalid_argument("Container identity is out of range.");
        request.To.Is=Kind::Container;request.To.Index=static_cast<int32_t>(index);request.To.Serial=static_cast<int32_t>(serial);
    }else throw std::invalid_argument("Target must name one character or one container.");
    return request;
}
inline Json TargetJson(const Target& to){
    if(to.Is==Kind::Character)return {{"Character",to.Character},{"Scope",ItemSnapshot::Name(to.Where)}};
    return {{"Container",{{"Index",to.Index},{"Serial",to.Serial}}}};
}
inline Json Body(const Request& r){return {{"Request",r.Id},{"Item",r.Item},{"Count",r.Count},{"Target",TargetJson(r.To)}};}
inline Json Refusal(std::string_view phase,std::string_view error){return {{"Schema",Schema},{"State","Refused"},{"Ok",false},{"Phase",phase},{"Error",std::string(error.substr(0,300))}};}

// Replay before any live lookup; Unknown reserved before the native call and never evicted.
class Requests{
    struct Entry{std::string Body;Json Reply;};
    std::map<std::tuple<std::string,std::string>,Entry> Entries;
    size_t Bytes=0;
public:
    // prepare() returns the count already in the target, or throws a refusal; invoke() calls the game;
    // verify() returns the count afterwards. Confirmed only when it grew by exactly Count.
    template<class Prepare,class Invoke,class Verify>
    Json Run(const std::string& caller,bool permitted,const Request& request,size_t capacity,Prepare prepare,Invoke invoke,Verify verifyState){
        if(!permitted)throw std::invalid_argument("The caller has no item.grant capability.");
        if(capacity<MinCapacity)throw BufferRequired{MinCapacity};
        const auto key=std::make_tuple(caller,request.Id);const auto body=Body(request).dump();
        if(const auto found=Entries.find(key);found!=Entries.end()){
            if(found->second.Body!=body)throw std::invalid_argument("Request UUID conflicts with its first body.");
            return found->second.Reply;
        }
        if(Entries.size()>=MaxRequests||body.size()+2048>MaxCachedBytes-Bytes)throw std::runtime_error("Native grant receipt cache is full; no action attempted.");
        Json reply;int64_t before=0;
        try{before=prepare();}
        catch(const std::exception& error){reply=Refusal("Target",error.what());}
        catch(...){reply=Refusal("Target","Native grant preparation failed; nothing was invoked.");}
        const bool refused=!reply.is_null();
        if(!refused)reply={{"Schema",Schema},{"State","Unknown"},{"Ok",false},{"Request",request.Id},{"Item",request.Item},{"Count",request.Count},{"Target",TargetJson(request.To)},{"Before",before}};
        auto [entry,inserted]=Entries.emplace(key,Entry{body,std::move(reply)});Bytes+=body.size()+2048;
        if(refused)return entry->second.Reply;
        try{
            invoke();const auto after=verifyState();
            entry->second.Reply["After"]=after;
            if(after-before==request.Count){entry->second.Reply["State"]="Confirmed";entry->second.Reply["Ok"]=true;}
        }catch(...){/* The reserved Unknown stays the first result, including a failure during the native call. */}
        return entry->second.Reply;
    }
};
}
