#pragma once
// runeschema.server.request: a plugin's in-game half asks the authoritative game (the server it
// is connected to, or this game when it hosts) to run that same plugin's server half.
//
// RuneSchema carries the request over its owning-player bridge and adds who sent it; the
// payload cannot name another player. The server runs only the service "<Plugin>.server"
// registered by the same plugin id, so a server without the plugin refuses. RuneSchema never
// grants, removes or changes anything because of a request: the plugin's server half decides,
// and acts through capability-gated core services (item.grant, item.destroy).
//
// The result comes back to "<Plugin>.server-result" on the sending game (or inline when local).
//
// Deliveries: the sending game reports its own Helpy delivery-flags result for its character,
// with this plugin's own pause exempt, because a server cannot read a remote player's flags.
// item.grant uses it for a grant to that character while serving this request; the server
// still checks its own flags folder. It is trusted the way Helpy trusts a client's check before
// sending: a modified client can only skip its own player's switches.
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
namespace PS::PluginServerRequest {
using Json=nlohmann::json;
inline constexpr std::string_view Service="runeschema.server.request",Capability="server.request";
inline constexpr std::string_view Channel="plugin.request",ResultChannel="plugin.result";
// Both directions share the bridge's inline envelope (4 KiB); the wrapper needs some of it.
// A server half's own reply is at most MaxReplyBytes, so the wrapped result stays within MaxResultBytes.
inline constexpr size_t MaxBodyBytes=3000,MaxResultBytes=3000,MaxReplyBytes=2700,MaxPending=64;
inline constexpr auto Expiry=std::chrono::seconds(120);
inline std::string ServerService(std::string_view plugin){return std::string(plugin)+".server";}
inline std::string ResultService(std::string_view plugin){return std::string(plugin)+".server-result";}
inline bool Token(std::string_view text,size_t limit){
    if(text.empty()||text.size()>limit)return false;
    for(const unsigned char c:text)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return false;
    return true;
}
struct Request{std::string Action,Id;Json Body;};
// From the plugin: {"Action":token,"Request":uuid,"Body":{...}}. Body is at most 3000 bytes serialized.
inline Request Parse(const Json& input){
    if(!input.is_object())throw std::invalid_argument("A server request must be an object.");
    for(const auto& [key,value]:input.items())if(key!="Action"&&key!="Request"&&key!="Body")throw std::invalid_argument("Unknown server request field: "+key);
    Request request;
    const auto action=input.find("Action");
    if(action==input.end()||!action->is_string()||!Token(action->get<std::string>(),64))throw std::invalid_argument("Action must be a token of at most 64 characters.");
    request.Action=action->get<std::string>();
    const auto id=input.find("Request");
    if(id==input.end()||!id->is_string())throw std::invalid_argument("Request must be a UUID.");
    request.Id=id->get<std::string>();
    if(request.Id.size()!=36||request.Id=="00000000-0000-0000-0000-000000000000")throw std::invalid_argument("Request must be a UUID.");
    for(size_t i=0;i<36;++i){const char c=request.Id[i];const bool dash=i==8||i==13||i==18||i==23;
        if(dash?c!='-':!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::invalid_argument("Request must be a lower-case UUID.");}
    const auto body=input.find("Body");
    request.Body=body==input.end()?Json::object():*body;
    if(!request.Body.is_object()||request.Body.dump().size()>MaxBodyBytes)throw std::invalid_argument("Body must be an object of at most 3000 bytes.");
    return request;
}
// Open, or the reason it is not: "helpy" (the player's switch), "unknown" (the character could not
// be identified while a pause exists), or the pausing mod's id.
struct Deliveries{bool Known=false,Open=false;std::string By;};
inline Json DeliveriesJson(const Deliveries& d){
    if(d.Open)return {{"Open",true}};
    return {{"Open",false},{"By",d.By.empty()?std::string("unknown"):d.By}};
}
// Absent (an older sender) is Known=false; a malformed value is refused with the request.
inline Deliveries ParseDeliveries(const Json* value){
    Deliveries result;if(!value)return result;
    if(!value->is_object()||value->size()<1||value->size()>2||!value->contains("Open")||!(*value)["Open"].is_boolean())
        throw std::invalid_argument("Invalid sender delivery state.");
    result.Known=true;result.Open=(*value)["Open"].get<bool>();
    for(const auto& [key,field]:value->items())if(key!="Open"&&key!="By")throw std::invalid_argument("Invalid sender delivery state.");
    if(result.Open){if(value->size()!=1)throw std::invalid_argument("Invalid sender delivery state.");return result;}
    const auto by=value->find("By");
    if(by==value->end()||!by->is_string()||!Token(by->get<std::string>(),40))throw std::invalid_argument("Invalid sender delivery state.");
    result.By=by->get<std::string>();return result;
}
// What travels to the server (the channel entity carries the plugin id).
inline Json Wire(const Request& r,const Deliveries& sender){return {{"Request",r.Id},{"Body",r.Body},{"Deliveries",DeliveriesJson(sender)}};}
// What the server half receives: the sender is supplied by RuneSchema, never by the payload.
inline Json ServerCall(std::string_view plugin,const Request& r,std::string_view character,bool remote){
    return {{"Plugin",plugin},{"Action",r.Action},{"Request",r.Id},{"Body",r.Body},{"From",{{"Character",character},{"Remote",remote}}}};
}
// The request a server half is handling right now, on the game thread, for the core services it calls.
struct Serving{std::string Plugin,Character;bool Remote=false;Deliveries Sender;};
// The rule every core service that acts on a player's character applies to a plugin caller
// (item.grant, item.destroy). Returns whether the caller is bound to that character's own request.
//   - Inside the caller's own "<Id>.server" callback, only the character that sent it.
//   - Outside it, only a character local to this game (single player, the host's own).
inline bool RequireSender(std::string_view caller,const Serving* serving,std::string_view character,bool local){
    const bool bound=serving&&serving->Plugin==caller;
    if(bound&&serving->Character!=character)throw std::runtime_error("While serving a request, a plugin can act only on the character that sent it.");
    if(!bound&&!local)throw std::runtime_error("A plugin can act on another player's character only while serving that player's own request.");
    return bound;
}
// Why a request failed, for the plugin to act on without reading the message:
//   NotInstalled  the server has no "<Plugin>.server" (the plugin isn't installed there). Nothing ran.
//   NotReady      the sender's character or item-ID sync isn't ready yet. Nothing ran; retry later.
//   Invalid       RuneSchema refused the request itself (malformed, or not the sender's current
//                 character). Nothing ran.
//   Refused       the server half ran and failed, or its reply couldn't be delivered. It may have acted;
//                 ask it about the Request before trying again.
inline constexpr std::string_view NotInstalled="NotInstalled",NotReady="NotReady",Invalid="Invalid",Refused="Refused";
// What the sending plugin receives, inline or through "<Plugin>.server-result".
inline Json Result(std::string_view action,std::string_view id,bool ok,const Json& value,std::string_view reason=Refused){
    Json result={{"Action",action},{"Request",id},{"Ok",ok}};
    if(ok)result["Result"]=value;else{result["Error"]=value.is_string()?value.get<std::string>().substr(0,400):std::string("The server refused the request.");result["Reason"]=reason;}
    if(result.dump().size()>MaxResultBytes)return {{"Action",action},{"Request",id},{"Ok",false},{"Error","The server's reply exceeded the transport limit."},{"Reason",Refused}};
    return result;
}
// Requests this game sent and still expects an answer to, by bridge revision. Unsolicited or
// late answers are dropped; at most 64 are outstanding.
class Pending{
public:
    using Clock=std::chrono::steady_clock;
    struct Entry{std::string Plugin,Action,Id;Clock::time_point Sent;};
    bool Add(int64_t revision,Entry entry){
        Expire(entry.Sent);
        if(revision<=0||m_entries.size()>=MaxPending||m_entries.contains(revision))return false;
        m_entries.emplace(revision,std::move(entry));return true;
    }
    // The entry for this answer, if this game sent it to this plugin and it hasn't expired.
    std::optional<Entry> Take(int64_t revision,std::string_view plugin,std::string_view id,Clock::time_point now){
        Expire(now);
        const auto found=m_entries.find(revision);
        if(found==m_entries.end()||found->second.Plugin!=plugin||found->second.Id!=id)return std::nullopt;
        auto entry=std::move(found->second);m_entries.erase(found);return entry;
    }
    size_t Size()const{return m_entries.size();}
    // Whether another request can wait for an answer now (expired ones no longer count).
    bool Full(Clock::time_point now){Expire(now);return m_entries.size()>=MaxPending;}
private:
    void Expire(Clock::time_point now){std::erase_if(m_entries,[&](const auto& item){return now-item.second.Sent>Expiry;});}
    std::map<int64_t,Entry> m_entries;
};
}
