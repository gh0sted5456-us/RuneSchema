#include "Runtime/ItemDestroyService.h"
#include "Runtime/ItemSnapshotService.h"
#include "Core/ItemDestroy.h"
#include "SDK/Helper/EquipmentDelivery.h"
#include "SDK/Helper/PlayerStatus.h"
#include "SDK/WeakObjectHandle.h"
#include "Runtime/NetworkContext.h"
#include "Runtime/NetworkRole.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Unreal/UnrealInitializer.hpp"
#include <array>
#include <cstring>
#include <format>
#include <memory>
namespace PS::ItemDestroyService {
namespace {
namespace D=ItemDestroy;namespace S=ItemSnapshot;namespace E=EquipmentDelivery;namespace AH=DragonWilds::ActorHelper;using namespace RC::Unreal;
D::Requests Receipts;
thread_local bool Executing=false;
std::string GuidText(const E::Guid& id){return std::format("{:08X}{:08X}{:08X}{:08X}",id[0],id[1],id[2],id[3]);}
int Count(UObject* item){E::Call count(item,TEXT("/Script/Dominion.Item:GetStackSize"),1);count.Invoke();const auto n=count.Integer();if(n<1||n>1000000)throw std::runtime_error("The live stack count is unavailable.");return n;}
struct Prepared{
    D::Request Request;ItemSnapshotService::LiveItem Live;
    UObject* World{};UObject* Pawn{};UObject* Mover{};
    WeakObjectHandle ControllerHandle,WorldHandle,PawnHandle,MoverHandle;
    // The character's Inventory, Loadout and Personal, plus for Scope Owned the named component.
    std::array<UObject*,4> Inventories{};std::array<WeakObjectHandle,4> InventoryHandles;size_t Used=3;
    bool OwnedBy(UObject* component)const{
        auto* type=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));if(!type)return false;
        for(auto* outer=component->GetOuterPrivate();outer;outer=outer->GetOuterPrivate())if(outer->IsA(type))return outer==Live.Controller;
        return false;
    }
    std::unique_ptr<E::Call> Remove;
    nlohmann::json Snapshot;int64_t BeforeTotal{};
    explicit Prepared(const D::Request& request):Request(request){
        Live=ItemSnapshotService::Resolve(Request.Exact);
        if(!Live.Error.is_null())throw std::runtime_error(Live.Error.value("Error",std::string("Exact item unavailable.")));
        World=Live.Controller->GetWorld();Pawn=AH::GetObjectRef(Live.Controller,TEXT("Pawn"));
        if(!Pawn||Pawn->GetWorld()!=World)throw std::runtime_error("The active character is unavailable.");
        ControllerHandle.Assign(Live.Controller);WorldHandle.Assign(World);PawnHandle.Assign(Pawn);
        const std::array<const TCHAR*,3> names{TEXT("InventoryComponent"),TEXT("LoadoutComponent"),TEXT("PersonalInventoryComponent")};
        const std::array<const TCHAR*,3> types{TEXT("/Script/Dominion.InventoryComponent"),TEXT("/Script/Dominion.LoadoutComponent"),TEXT("/Script/Dominion.PersonalInventoryComponent")};
        for(size_t i=0;i<3;++i){Inventories[i]=E::Owned(Live.Controller,names[i],types[i]);InventoryHandles[i].Assign(Inventories[i]);}
        if(Request.Exact.Where==S::Scope::Owned){
            for(size_t i=0;i<3;++i)if(Inventories[i]==Live.Container)throw std::runtime_error("Name the character's own containers by Scope, not as Owned.");
            Inventories[3]=Live.Container;InventoryHandles[3].Assign(Live.Container);Used=4;
        }
        Mover=E::Owned(Live.Controller,TEXT("InventoryController"),TEXT("/Script/Dominion.InventoryController"));MoverHandle.Assign(Mover);
        if(Count(Live.Item)!=Request.Count)throw std::runtime_error("Destroy requires the whole current stack.");
        size_t identities=0;
        for(size_t i=0;i<Used;++i)for(auto* item:E::Slots(Inventories[i]))if(item&&GuidText(E::Identity(item))==Request.Exact.Instance){
            if(i==1)throw std::runtime_error("Equipped items cannot be destroyed.");++identities;}
        if(identities!=1)throw std::runtime_error("Native item identity is ambiguous.");
        Snapshot=ItemSnapshotService::Read(Request.Exact);D::CheckSnapshot(Request,Snapshot);BeforeTotal=Total();
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.InventoryController:DestroyItemFromInventory"),false);
        if(!fn||fn->GetReturnProperty())throw std::runtime_error("Native destroy signature changed.");
        Remove=std::make_unique<E::Call>(Mover,TEXT("/Script/Dominion.InventoryController:DestroyItemFromInventory"),3);
        Remove->Object(TEXT("Inventory"),Live.Container).Integer(TEXT("SlotIndex"),Request.Exact.Slot).Object(TEXT("Instigator"),Live.Controller);
        Validate();
    }
    void Validate(){
        if(ControllerHandle.Get()!=Live.Controller||WorldHandle.Get()!=World||PawnHandle.Get()!=Pawn||MoverHandle.Get()!=Mover
            ||Live.Controller->GetWorld()!=World||AH::GetObjectRef(Live.Controller,TEXT("Pawn"))!=Pawn||AH::GetObjectRef(Live.Controller,TEXT("InventoryController"))!=Mover
            ||!Network::OwnsGameplay(Network::Detect(Live.Controller).Mode)
            ||S::NormalizeGuid(PlayerStatusRuntime::CharacterGuid(AH::GetObjectRef(Live.Controller,TEXT("PlayerState"))))!=Request.Exact.Player)
            throw std::runtime_error("The native character/world changed.");
        const std::array<const TCHAR*,3> names{TEXT("InventoryComponent"),TEXT("LoadoutComponent"),TEXT("PersonalInventoryComponent")};
        for(size_t i=0;i<3;++i)if(InventoryHandles[i].Get()!=Inventories[i]||AH::GetObjectRef(Live.Controller,names[i])!=Inventories[i])throw std::runtime_error("Native inventory ownership changed.");
        if(Used==4&&(InventoryHandles[3].Get()!=Inventories[3]||Inventories[3]->GetWorld()!=World||!OwnedBy(Inventories[3])))throw std::runtime_error("Native inventory ownership changed.");
    }
    int64_t Total(){int64_t n=0;for(size_t i=0;i<Used;++i)for(auto* item:E::Slots(Inventories[i]))if(item&&E::Data(item)==Live.Data)n+=Count(item);return n;}
    void Invoke(){
        Validate();const auto exact=ItemSnapshotService::Resolve(Request.Exact);
        if(!exact.Error.is_null()||exact.Controller!=Live.Controller||exact.Container!=Live.Container||exact.Item!=Live.Item||Count(exact.Item)!=Request.Count)
            throw std::runtime_error("The exact item changed before native destroy.");
        const auto fresh=ItemSnapshotService::Read(Request.Exact);
        if(!fresh.value("Ok",false)||fresh.at("Digest")!=Snapshot.at("Digest"))throw std::runtime_error("Native item state changed before destroy.");
        Remove->Invoke();
    }
    D::State Verify(){
        Validate();size_t remains=0;
        for(size_t i=0;i<Used;++i)for(auto* item:E::Slots(Inventories[i]))if(item&&GuidText(E::Identity(item))==Request.Exact.Instance)++remains;
        if(!remains)return BeforeTotal-Total()==Request.Count?D::State::Confirmed:D::State::Unknown;
        if(remains!=1)return D::State::Unknown;
        const auto retained=ItemSnapshotService::Read(Request.Exact);
        return retained.value("Ok",false)&&retained.at("Digest")==Snapshot.at("Digest")&&Total()==BeforeTotal?D::State::Retained:D::State::Unknown;
    }
};
}
int32_t Call(const char* caller,bool permitted,const char* request,char* response,uint32_t capacity,uint32_t* size){
    if(!caller||!request||!size||(!response&&capacity)||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
    if(!permitted)return RS_PLUGIN_INVALID_ARGUMENT;
    // A size probe performs no native lookup, prepares no call and reserves no request.
    if(!response||capacity<D::MinCapacity){*size=static_cast<uint32_t>(D::MinCapacity);return RS_PLUGIN_BUFFER_TOO_SMALL;}
    if(Executing)return RS_PLUGIN_FAILED;
    nlohmann::json reply;
    if(!IsInGameThread())reply=D::Refusal("Thread","Native destroy must run on the game thread.");
    else{
        struct Guard{Guard(){Executing=true;}~Guard(){Executing=false;}} guard;
        try{
            const auto parsed=D::Parse(nlohmann::json::parse(request));std::unique_ptr<Prepared> operation;
            reply=Receipts.Run(caller,permitted,parsed,capacity,[&]{operation=std::make_unique<Prepared>(parsed);return operation->Snapshot;},[&]{operation->Invoke();},[&]{return operation->Verify();});
        }catch(const D::BufferRequired& needed){*size=static_cast<uint32_t>(needed.Size);return RS_PLUGIN_BUFFER_TOO_SMALL;}
        catch(const std::exception& error){reply=D::Refusal("Request",error.what());}
        catch(...){return RS_PLUGIN_FAILED;}
    }
    const auto text=reply.dump();*size=static_cast<uint32_t>(text.size()+1);
    if(*size>capacity)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
}
}
