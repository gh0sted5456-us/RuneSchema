#pragma once
// Helpy's Destroy button: who may destroy which stack, and what Helpy sends and shows.
// Engine-free, shared by the Helpy window and the game, and tested on its own.
//
// Helpy asks the game to run runeschema.item.destroy (Core/ItemDestroy.h) on one whole stack
// the inventory panel shows. The game decides where it runs:
//   - Single player, host or server: this game, for any connected character (the host is the
//     world's administrator). The player's own character keeps its delivery gate.
//   - Client of a server: the request goes to the server, which needs saved Helpy access or the
//     allowlist (a world PIN is not enough), and acts only on the sender's own character.
// A destroy on a player's own character runs the Helpy delivery gate (their switch, every mod's
// pause and any mod's character exception), on the client before sending and on the server.
// The result shown in Helpy never carries the item snapshot; it stays in the game.
#include "Core/ItemDestroy.h"
#include <array>
#include <cstdio>
#include <random>
namespace PS::HelpyItemDestroy {
using Json=nlohmann::json;
inline constexpr std::string_view Action="DestroyItem",Caller="RuneSchema.Helpy";
inline constexpr int ConfirmSeconds=60;

// A random version-4 UUID in lower case, for one confirmed destroy.
inline std::string NewRequestId(){
    std::random_device random;std::array<unsigned char,16> bytes{};
    for(size_t i=0;i<bytes.size();i+=4){const auto value=random();for(size_t j=0;j<4;++j)bytes[i+j]=static_cast<unsigned char>(value>>(8*j));}
    bytes[6]=static_cast<unsigned char>((bytes[6]&0x0F)|0x40);bytes[8]=static_cast<unsigned char>((bytes[8]&0x3F)|0x80);
    char text[37];
    std::snprintf(text,sizeof(text),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return text;
}
// The panel's instance ([a,b,c,d] from the game's GUID) as the 32 hex digits item.destroy expects.
inline std::string InstanceText(const Json& instance){
    if(!instance.is_array()||instance.size()!=4)throw std::invalid_argument("The item has no instance identity.");
    std::string text;bool zero=true;
    for(const auto& part:instance){
        if(!part.is_number_unsigned()&&!(part.is_number_integer()&&part.get<int64_t>()>=0))throw std::invalid_argument("The item's instance identity is invalid.");
        const auto value=part.get<uint64_t>();if(value>0xFFFFFFFFull)throw std::invalid_argument("The item's instance identity is invalid.");
        if(value)zero=false;
        char chunk[9];std::snprintf(chunk,sizeof(chunk),"%08llX",static_cast<unsigned long long>(value));text+=chunk;
    }
    if(zero)throw std::invalid_argument("The item has no instance identity.");
    return text;
}

// What the Helpy window sends for one confirmed stack (SpawnTools action "DestroyItem").
// row: an inventory panel row (Slot, Instance, Path, Count). personal: the Personal storage tab.
inline Json Build(const Json& row,const std::string& playerPath,const std::string& playerGuid,bool personal,uint64_t generation,const std::string& request){
    if(row.value("Equipped",false))throw std::invalid_argument("Equipped items cannot be destroyed. Unequip it first.");
    if(!row.contains("Slot")||!row["Slot"].is_number_integer()||row["Slot"].get<int64_t>()<0)throw std::invalid_argument("The item has no slot.");
    const auto count=row.value("Count",0);if(count<1)throw std::invalid_argument("The stack is empty.");
    const auto path=row.value("Path",std::string{});if(path.empty()||path.front()!='/')throw std::invalid_argument("The item has no definition path.");
    const auto guid=ItemSnapshot::NormalizeGuid(playerGuid);if(guid.empty())throw std::invalid_argument("The character is not identified yet.");
    Json destroy{{"Player",guid},{"Scope",personal?"Personal":"Inventory"},{"Slot",row["Slot"]},{"Instance",InstanceText(row.value("Instance",Json()))},
        {"Item",path},{"Count",count},{"Request",request}};
    (void)ItemDestroy::Parse(destroy); // Refuse here what the game would refuse.
    return {{"Action",Action},{"_Source","HelpyStandalone"},{"Player",playerPath},{"PlayerGuid",guid},{"Generation",generation},{"Confirmed",true},{"Destroy",std::move(destroy)}};
}
// The destroy body from a DestroyItem request, for the game. Requires the explicit confirmation.
inline ItemDestroy::Request Body(const Json& request){
    if(!request.is_object()||request.value("Action",std::string{})!=Action)throw std::invalid_argument("Not a Helpy destroy request.");
    if(request.value("Confirmed",false)!=true)throw std::invalid_argument("Destroy needs the player's confirmation.");
    const auto destroy=request.find("Destroy");if(destroy==request.end())throw std::invalid_argument("The destroy request is incomplete.");
    auto parsed=ItemDestroy::Parse(*destroy);
    if(ItemSnapshot::NormalizeGuid(request.value("PlayerGuid",std::string{}))!=parsed.Exact.Player)throw std::invalid_argument("The destroy target does not match the selected character.");
    return parsed;
}

// Who is asking, where, for whom.
struct Authority{
    bool Network=false;      // Arrived from a client through this server's Helpy authority.
    bool SavedAccess=false;  // The host saved Helpy access for the sender.
    bool Allowlisted=false;  // The sender is on this server's Helpy allowlist.
    bool PinAccess=false;    // The sender unlocked the world PIN.
    bool Own=false;          // The target is the sender's own character.
};
// Whether the delivery gate applies, or throws the refusal.
inline bool Allow(const Authority& who){
    if(who.Network){
        if(!who.SavedAccess&&!who.Allowlisted)
            throw std::runtime_error(who.PinAccess?"A world PIN allows deliveries and teleporting only. Destroying items needs saved Helpy access from the host."
                :"Destroying items on this server needs saved Helpy access from the host or its allowlist.");
        if(!who.Own)throw std::runtime_error("Through a server, Helpy destroys items only on your own character.");
    }
    return who.Own;
}

// What Helpy shows: the outcome, never the snapshot.
inline Json Bounded(const Json& reply,const ItemDestroy::Request& request){
    Json out{{"State",reply.value("State",std::string("Unknown"))},{"Ok",reply.value("Ok",false)},{"Request",request.Id},
        {"Item",request.Exact.Item},{"Count",request.Count},{"Slot",request.Exact.Slot},{"Scope",ItemSnapshot::Name(request.Exact.Where)}};
    if(reply.contains("Phase")&&reply["Phase"].is_string())out["Phase"]=reply["Phase"].get<std::string>().substr(0,40);
    if(reply.contains("Error")&&reply["Error"].is_string())out["Error"]=reply["Error"].get<std::string>().substr(0,300);
    return out;
}
inline std::string Status(const Json& bounded){
    const auto state=bounded.value("State",std::string{});
    if(state=="Confirmed")return "Destroyed: the stack is gone from the live inventory.";
    if(state=="Retained")return "The game kept the stack; nothing was destroyed.";
    if(state=="Refused")return "Not destroyed: "+bounded.value("Error",std::string("the request was refused."));
    return "The result is unknown. Refresh the inventory before doing anything else; Helpy will not retry.";
}
}
