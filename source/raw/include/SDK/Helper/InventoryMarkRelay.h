#pragma once
#include "SDK/Helper/InventoryMarkService.h"
#include "SDK/WeakObjectHandle.h"
#include "Runtime/NetworkContext.h"
#include "SDK/Helper/PlayerStatus.h"
namespace PS {
class InventoryMarkRelay {
    using Json=nlohmann::json;using Clock=std::chrono::steady_clock;using Object=RC::Unreal::UObject;
    WeakObjectHandle World,Controller;
    std::string Character;Json Pending=Json::object();
    size_t Offset=0;uint64_t Version=0,LocalRevision=UINT64_MAX,Generation=0;unsigned Attempts=0;
    Clock::time_point Next{},Resync{},Warning{},PublishAt{},ProfileNext{};
    Json ProfilePending=Json::object();uint64_t ProfileSerial=0;size_t ProfileCursor=0;unsigned ProfileAttempts=0;
    static std::string Key(std::string value){std::string out;for(unsigned char c:value){if(c=='-'||c=='{'||c=='}')continue;if(!std::isxdigit(c))return {};out+=static_cast<char>(std::tolower(c));}return out.size()==32&&out!=std::string(32,'0')?out:std::string{};}
    void Profiles(Object* world,Clock::time_point now,const std::function<bool(const std::string&,const std::string&)>& forward){
        if(now<ProfileNext)return;
        if(ProfilePending.empty()){
            if(now>=PublishAt){const auto profile=InventoryMarkService::Call({{"Action","PublicProfile"}});ProfilePending={{"Action","InventoryMarks"},{"ProfileAction","Publish"},{"RequestId",++ProfileSerial},{"CharacterGuid",Character},{"Profile",profile.at("Profile")}};}
            else {
                using namespace RC::Unreal;auto* type=EquipmentDelivery::AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerState"));TArray<Object*> states;
                if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,states,true);if(states.Num()<0||states.Num()>256)throw std::runtime_error("Profile roster bound exceeded");
                std::vector<std::string> peers;for(auto* state:states)if(state&&state->GetWorld()==world&&!state->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))){const auto guid=Key(PlayerStatusRuntime::CharacterGuid(state));if(!guid.empty()&&guid!=Character)peers.push_back(guid);}
                std::sort(peers.begin(),peers.end());peers.erase(std::unique(peers.begin(),peers.end()),peers.end());if(peers.empty()){ProfileNext=now+std::chrono::seconds(2);return;}
                ProfilePending={{"Action","InventoryMarks"},{"ProfileAction","Read"},{"RequestId",++ProfileSerial},{"CharacterGuid",peers[ProfileCursor++%peers.size()]}};
            }ProfileAttempts=0;
        }
        if(ProfileAttempts++>=3){ProfilePending=Json::object();ProfileNext=now+std::chrono::seconds(30);PublishAt=now+std::chrono::seconds(30);return;}
        if(!forward("InventoryMarks",ProfilePending.dump()))throw std::runtime_error("Owned profile bridge is not ready");ProfileNext=now+std::chrono::seconds(2);
    }
public:
    void Reset(){if(World.Get()||!Character.empty()||Generation)try{InventoryMarkService::Call({{"Action","ClearPublicProfiles"}});}catch(...){}World.Reset();Controller.Reset();Character.clear();ProfilePending=Json::object();ProfileAttempts=0;ProfileCursor=0;PublishAt={};ProfileNext={};Pending=Json::object();Offset=0;Version=0;LocalRevision=UINT64_MAX;Attempts=0;Next={};Resync={};}
    void Receive(Object* player,const std::string& payload)noexcept{try{
        if(!player||player->GetWorld()!=World.Get()||EquipmentDelivery::AH::GetObjectRef(player,TEXT("Controller"))!=Controller.Get())return;
        const auto ack=Json::parse(payload);
        if(!ProfilePending.empty()&&ack.value("RequestId",uint64_t{})==ProfilePending.at("RequestId").get<uint64_t>()&&ack.at("CharacterGuid")==ProfilePending.at("CharacterGuid")){
            const bool publish=ProfilePending.at("ProfileAction")=="Publish";if(ack.value("Kind",std::string{})!=(publish?"HelpyProfilePublished1":"HelpyProfile1"))return;
            if(publish)PublishAt=Clock::now()+std::chrono::seconds(15);else InventoryMarkService::Call({{"Action","ReceivePublicProfile"},{"CharacterGuid",ack.at("CharacterGuid")},{"Profile",ack.at("Profile")}});
            ProfilePending=Json::object();ProfileAttempts=0;Next=Clock::now()+std::chrono::milliseconds(500);return;
        }
        if(Pending.empty())return;for(const auto* field:{"CharacterGuid","Version","Offset","Next","Done"})if(ack.at(field)!=Pending.at(field))return;
        Offset=ack.at("Done").get<bool>()?0:ack.at("Next").get<size_t>();
        if(ack.at("Done").get<bool>())Resync=Clock::now()+std::chrono::seconds(15);
        Pending=Json::object();Attempts=0;Next=Clock::now()+std::chrono::milliseconds(500);
    }catch(...){} }
    void Tick(Object* world,uint64_t generation,const std::function<std::string(Object*)>& identify,
        const std::function<bool(const std::string&,const std::string&)>& forward)noexcept{
        const auto now=Clock::now();if(now<Next)return;Next=now+std::chrono::seconds(2);
        try{
            using namespace RC::Unreal;namespace E=EquipmentDelivery;
            if(!world||!forward){Reset();Next=now+std::chrono::seconds(2);return;}
            if(Generation!=generation||World.Get()!=world){Reset();World=WeakObjectHandle(world);Generation=generation;}
            if(Network::Detect(world).Mode!=Network::Role::Client)return;
            auto* type=E::AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));TArray<Object*> controllers;
            if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);if(controllers.Num()>256)throw std::runtime_error("Mark sync controller bound exceeded");
            Object* owner=nullptr;for(auto* candidate:controllers){
                if(!candidate||candidate->GetWorld()!=world||candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
                E::Call local(candidate,TEXT("/Script/Engine.Controller:IsLocalController"),1);local.Invoke();if(!local.Bool())continue;
                if(owner)throw std::runtime_error("Mark sync local player is ambiguous");owner=candidate;
            }
            const auto character=owner?Key(identify(owner)):std::string{};if(character.empty()){Reset();Next=now+std::chrono::seconds(2);return;}
            if(Generation!=generation||World.Get()!=world||Controller.Get()!=owner||Character!=character){Reset();World=WeakObjectHandle(world);Controller=WeakObjectHandle(owner);Character=character;Generation=generation;}
            if(Pending.empty()){
                const auto caps=InventoryMarkService::Call({{"Action","Capabilities"}});if(!caps.value("GameFavorites",false))return;
                const auto revision=caps.at("MarksRevision").get<uint64_t>();if(!Offset&&revision==LocalRevision&&now<Resync){Profiles(world,now,forward);return;}
                Pending=InventoryMarkService::Call({{"Action","ExportMarks"},{"CharacterGuid",Character},{"Offset",Offset},{"Version",Version}});
                Version=Pending.at("Version").get<uint64_t>();LocalRevision=revision;Attempts=0;
            }
            if(Attempts++>=3){Pending=Json::object();Offset=0;LocalRevision=UINT64_MAX;Next=now+std::chrono::seconds(30);throw std::runtime_error("Server did not acknowledge character marks; local marks remain saved");}
            if(!forward("InventoryMarks",Json{{"Action","InventoryMarks"},{"Batch",Pending}}.dump()))throw std::runtime_error("Owned player mark bridge is not ready");
            Next=now+std::chrono::seconds(2);
        }catch(const std::exception& error){if(now>=Warning){Warning=now+std::chrono::seconds(30);Log<RC::LogLevel::Warning>(STR("[HELPY][MARKS][SYNC] {}\n"),RC::to_generic_string(error.what()));}}
    }
};
}
