// UE4SS defines verify; the shared contract must compile after its headers.
#define verify(...) static_assert(false,"Unreal verify macro must not expand here")
#include "Core/ItemGrant.h"
#undef verify
#include <iostream>
#include <stdexcept>
namespace G=PS::ItemGrant;using Json=nlohmann::json;
static void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
template<class F>static bool Refused(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){try{
    const Json chest{{"Request","12345678-1234-4123-8123-123456789ABC"},{"Item","/Game/Items/Ore.Ore"},{"Count",5},{"Target",{{"Container",{{"Index",77},{"Serial",9}}}}}};
    const auto request=G::Parse(chest);
    Check(request.Id=="12345678-1234-4123-8123-123456789abc"&&request.To.Is==G::Kind::Container&&request.To.Index==77&&request.To.Serial==9,"chest target");
    const Json character{{"Request","12345678-1234-4123-8123-123456789abd"},{"Item","/Game/Items/Ore.Ore"},{"Count",1},{"Target",{{"Character","{5a3f0c21-b6d8-4e7f-9a1c-2b3d4e5f6071}"},{"Scope","Personal"}}}};
    const auto own=G::Parse(character);
    Check(own.To.Is==G::Kind::Character&&own.To.Character=="5A3F0C21B6D84E7F9A1C2B3D4E5F6071"&&own.To.Where==PS::ItemSnapshot::Scope::Personal,"character target");
    for(const char* text:{R"({"Count":0})",R"({"Count":10001})",R"({"Count":2.5})",R"({"Item":"Game/x"})",R"({"Item":""})",R"({"Request":"nope"})",
        R"({"Request":"00000000-0000-0000-0000-000000000000"})",R"({"Extra":1})",R"({"Target":{}})",R"({"Target":{"Container":{"Index":-1,"Serial":1}}})",
        R"({"Target":{"Container":{"Index":1,"Serial":0}}})",R"({"Target":{"Container":{"Index":1}}})",R"({"Target":{"Character":"bad"}})",
        R"({"Target":{"Character":"5A3F0C21B6D84E7F9A1C2B3D4E5F6071","Scope":"Loadout"}})",
        R"({"Target":{"Character":"5A3F0C21B6D84E7F9A1C2B3D4E5F6071","Scope":"Inventory","Container":{"Index":1,"Serial":1}}})",
        R"({"Target":{"Character":"5A3F0C21B6D84E7F9A1C2B3D4E5F6071","Container":{"Index":1,"Serial":0}}})",
        R"({"Target":{"Character":"5A3F0C21B6D84E7F9A1C2B3D4E5F6071","Scope":7}})"}){
        const auto patch=Json::parse(text);
        auto bad=chest;bad.update(patch);Check(Refused([&]{G::Parse(bad);}),"bad request refused");}
    // A character's own storage component: the Character target with an exact identity.
    {const Json vault{{"Request","12345678-1234-4123-8123-123456789abe"},{"Item","/Game/Items/Ore.Ore"},{"Count",3},
            {"Target",{{"Character","5A3F0C21B6D84E7F9A1C2B3D4E5F6071"},{"Container",{{"Index",501},{"Serial",44}}}}}};
        const auto owned=G::Parse(vault);
        Check(owned.To.Is==G::Kind::Character&&owned.To.Owned&&owned.To.Index==501&&owned.To.Serial==44&&owned.To.Character=="5A3F0C21B6D84E7F9A1C2B3D4E5F6071","owned container target");
        Check(G::TargetJson(owned.To)==vault["Target"],"owned target echoes exactly");
        Check(G::Body(owned)!=G::Body(own),"owned and scoped targets are different bodies");}
    // A plugin's own pause: its id lower-cased, when that is a valid flags mod id.
    Check(G::OwnPauseMod("GrandExchange")=="grandexchange"&&G::OwnPauseMod("My-Mod_2")=="my-mod_2","own pause mod");
    Check(G::OwnPauseMod("RuneSchema.Wiki").empty()&&G::OwnPauseMod("").empty()&&G::OwnPauseMod(std::string(41,'a')).empty(),"no own pause for other ids");
    // Binding: who a Character target may be, and which pause is exempt.
    {namespace P=PS::PluginServerRequest;const std::string A="5A3F0C21B6D84E7F9A1C2B3D4E5F6071",B="ABCDEFABCDEFABCDEFABCDEFABCDEF01";
        const P::Serving remoteOpen{"GrandExchange",A,true,{true,true,""}};
        Check(G::Gate("GrandExchange",&remoteOpen,A,false).Exempt=="grandexchange","bound remote grant exempts only the plugin's own pause");
        Check(Refused([&]{G::Gate("GrandExchange",&remoteOpen,B,false);}),"bound grant cannot target another character");
        Check(Refused([&]{G::Gate("GrandExchange",&remoteOpen,B,true);}),"bound grant cannot target the host either");
        Check(Refused([&]{G::Gate("OtherPlugin",&remoteOpen,A,false);}),"another plugin is not bound by GE's request");
        Check(G::Gate("OtherPlugin",&remoteOpen,A,true).Exempt.empty(),"unbound local grant keeps every pause");
        Check(Refused([&]{G::Gate("GrandExchange",nullptr,A,false);}),"no remote grant outside a request");
        Check(G::Gate("GrandExchange",nullptr,A,true).Exempt.empty(),"an ordinary local grant is not exempt");
        const P::Serving local{"GrandExchange",A,false,{}};
        Check(G::Gate("GrandExchange",&local,A,true).Exempt=="grandexchange","local bound grant: the local flags folder is checked with the exemption");
        for(const auto& sender:std::vector<P::Deliveries>{{false,false,""},{true,false,"helpy"},{true,false,"othermod"},{true,false,"unknown"}}){
            const P::Serving closed{"GrandExchange",A,true,sender};
            Check(Refused([&]{G::Gate("GrandExchange",&closed,A,false);}),"a remote sender's own closed or missing state refuses");}
        try{const P::Serving off{"GrandExchange",A,true,{true,false,"helpy"}};G::Gate("GrandExchange",&off,A,false);}
        catch(const std::exception& error){Check(std::string(error.what()).find("turned off")!=std::string::npos,"the player's switch is named");}}
    int prepares=0,invokes=0,verifies=0;int64_t count=10;G::Requests cache;
    auto prepare=[&]{++prepares;return count;};auto invoke=[&]{++invokes;count+=5;};auto verify=[&]{++verifies;return count;};
    Check(Refused([&]{cache.Run("Fixture",false,request,G::MinCapacity,prepare,invoke,verify);})&&prepares==0,"capability required");
    try{cache.Run("Fixture",true,request,16,prepare,invoke,verify);Check(false,"size probe");}catch(const G::BufferRequired& b){Check(b.Size==G::MinCapacity,"probe size");}
    Check(prepares==0&&invokes==0,"probe never acts");
    const auto first=cache.Run("Fixture",true,request,G::MinCapacity,prepare,invoke,verify);
    Check(first["State"]=="Confirmed"&&first["Ok"]==true&&first["Before"]==10&&first["After"]==15&&invokes==1,"confirmed grant");
    Check(cache.Run("Fixture",true,request,G::MinCapacity,[]()->int64_t{throw std::runtime_error("gone");},invoke,verify)==first&&invokes==1,"replay before lookup");
    auto changed=request;changed.Count=6;Check(Refused([&]{cache.Run("Fixture",true,changed,G::MinCapacity,prepare,invoke,verify);})&&invokes==1,"body bound");
    Check(cache.Run("Other",true,request,G::MinCapacity,prepare,invoke,verify)["State"]=="Confirmed"&&invokes==2,"callers are separate");
    G::Requests lost;const auto unknown=lost.Run("Fixture",true,request,G::MinCapacity,prepare,[&]{++invokes;throw std::runtime_error("after native call");},verify);
    Check(unknown["State"]=="Unknown"&&unknown["Ok"]==false,"failure during the call stays unknown");
    const auto attempted=invokes;Check(lost.Run("Fixture",true,request,G::MinCapacity,prepare,invoke,verify)==unknown&&invokes==attempted,"unknown never retried");
    G::Requests partial;const auto short_=partial.Run("Fixture",true,request,G::MinCapacity,prepare,[&]{count+=4;},verify);
    Check(short_["State"]=="Unknown","a wrong count is unknown");
    G::Requests refused;const auto no=refused.Run("Fixture",true,request,G::MinCapacity,[]()->int64_t{throw std::runtime_error("The inventory cannot accept that quantity.");},invoke,verify);
    Check(no["State"]=="Refused"&&refused.Run("Fixture",true,request,G::MinCapacity,prepare,invoke,verify)==no,"refusals replay too");
    std::cout<<"Item grant contract passed: targets, owned containers, own pause, request binding, capability, size probe, replay, body binding, unknown and refusal.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
