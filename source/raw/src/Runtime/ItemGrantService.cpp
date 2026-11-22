#include "Runtime/ItemGrantService.h"
#include "Core/ItemGrant.h"
#include "Loader/AssetMetadataRegistry.h"
#include "Loader/WorldSaveIdentity.h"
#include "Runtime/HelpyDeliveryGate.h"
#include "Runtime/NetworkContext.h"
#include "Runtime/NetworkRole.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/EquipmentDelivery.h"
#include "SDK/Helper/PlayerStatus.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/UnrealInitializer.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include <cstring>
#include <initializer_list>
#include <memory>
#include <tuple>
// runeschema.item.grant: the same native calls as Helpy's own item grant
// (InventoryComponent:CanAddItemByData, AddItemByData, and GetNumItemsByData before and after),
// aimed at one inventory component the caller names: a connected character's Inventory or
// Personal container, one exact component that character's controller owns, or a world
// container such as a chest.
namespace PS::ItemGrantService {
namespace {
using namespace RC::Unreal;
namespace G=ItemGrant;namespace S=ItemSnapshot;namespace E=EquipmentDelivery;namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
G::Requests Receipts;
thread_local bool Executing=false;
constexpr const TCHAR* CountPath=TEXT("/Script/Dominion.InventoryComponent:GetNumItemsByData");
constexpr const TCHAR* ReadyPath=TEXT("/Script/Dominion.InventoryComponent:CanAddItemByData");
constexpr const TCHAR* GivePath=TEXT("/Script/Dominion.InventoryComponent:AddItemByData");

// The native parameter layout Helpy's grant checks, so a changed game refuses instead of misreading.
void Validate(const TCHAR* path,std::initializer_list<std::tuple<const TCHAR*,int32_t,int32_t>> fields){
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    C::Frame(function);
    size_t count=0;for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
    if(count!=fields.size())throw std::runtime_error("Native inventory command parameter count changed.");
    for(const auto& [name,offset,width]:fields){
        auto* field=function->FindProperty(FName(name,FNAME_Find));
        if(!C::Fits(field,function->GetParmsSize())||!field->HasAnyPropertyFlags(CPF_Parm)
            ||(RC::StringType(name)!=TEXT("GameplayTags")&&field->GetElementSize()!=width))
            throw std::runtime_error("Native inventory command parameter layout changed.");
    }
}
UObject* Controller(const std::string& character){
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerController"));
    if(!type)throw std::runtime_error("The game's player controller type is unavailable.");
    UObject* found=nullptr;TArray<UObject*> controllers;
    UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
    for(auto* candidate:controllers){
        if(!candidate||candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))||!candidate->GetWorld())continue;
        if(S::NormalizeGuid(PlayerStatusRuntime::CharacterGuid(AH::GetObjectRef(candidate,TEXT("PlayerState"))))!=character)continue;
        if(found)throw std::runtime_error("More than one controller has that character GUID.");
        found=candidate;
    }
    if(!found)throw std::runtime_error("That character is not connected.");
    return found;
}
bool IsLocal(UObject* controller){
    AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();return local.Result<bool>();
}
bool Live(UObject* object){
    return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))&&object->GetWorld();
}
// The first player controller in the component's outer chain, or null.
UObject* OwningController(UObject* component,UClass* controllerType){
    for(auto* outer=component->GetOuterPrivate();outer;outer=outer->GetOuterPrivate())if(outer->IsA(controllerType))return outer;
    return nullptr;
}
// A component inside a player's state or a player's pawn also belongs to that player.
bool PlayerOwned(UObject* component,UClass* controllerType){
    if(OwningController(component,controllerType))return true;
    auto* stateType=AH::ResolveClass(TEXT("/Script/Engine.PlayerState"));
    auto* pawnType=AH::ResolveClass(TEXT("/Script/Engine.Pawn"));
    if(!stateType||!pawnType)return true; // Cannot rule it out: refuse.
    for(auto* outer=component->GetOuterPrivate();outer;outer=outer->GetOuterPrivate()){
        if(outer->IsA(stateType))return true;
        if(outer->IsA(pawnType)){auto* controller=AH::GetObjectRef(outer,TEXT("Controller"));if(controller&&controller->IsA(controllerType))return true;}
    }
    return false;
}
struct Prepared{
    G::Request Request;UObject* Inventory{};UObject* Item{};WeakObjectHandle InventoryHandle;
    UObject* Owner{};WeakObjectHandle OwnerHandle;
    Prepared(const std::string& caller,const PluginServerRequest::Serving* serving,const G::Request& request):Request(request){
        auto* inventoryType=AH::ResolveClass(TEXT("/Script/Dominion.InventoryComponent"));
        auto* itemType=AH::ResolveClass(TEXT("/Script/Dominion.ItemData"));
        auto* controllerType=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
        if(!inventoryType||!itemType||!controllerType)throw std::runtime_error("The game's inventory types are unavailable.");
        if(Request.To.Is==G::Kind::Character){
            auto* controller=Controller(Request.To.Character);
            if(!Network::OwnsGameplay(Network::Detect(controller).Mode))throw std::runtime_error("Items are granted only where the game is authoritative.");
            // Bound to the sender's own request, or a character local to this game (ItemGrant::Gate).
            const auto gate=G::Gate(caller,serving,Request.To.Character,IsLocal(controller));
            if(Request.To.Owned){
                // One exact inventory component that this character's controller owns, in its world.
                auto* loadoutType=AH::ResolveClass(TEXT("/Script/Dominion.LoadoutComponent"));
                if(!loadoutType)throw std::runtime_error("The game's loadout type is unavailable; nothing was granted.");
                WeakObjectHandle handle;handle.ObjectIndex=Request.To.Index;handle.ObjectSerialNumber=Request.To.Serial;
                Inventory=handle.Get();
                if(!Live(Inventory)||!Inventory->IsA(inventoryType))throw std::runtime_error("The container is not a live inventory.");
                if(Inventory->IsA(loadoutType))throw std::runtime_error("Equipped items cannot be granted into; name a storage container.");
                if(Inventory->GetWorld()!=controller->GetWorld())throw std::runtime_error("The container is not in that character's world.");
                if(OwningController(Inventory,controllerType)!=controller)throw std::runtime_error("That container does not belong to this character.");
            }else Inventory=Request.To.Where==S::Scope::Personal
                ?E::Owned(controller,TEXT("PersonalInventoryComponent"),TEXT("/Script/Dominion.PersonalInventoryComponent"))
                :E::Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));
            // A grant to a character is a delivery: the player's own switch and every mod's pauses
            // apply, except the caller's own pause when bound to the sender's request.
            std::string world;try{world=DragonWilds::WorldSaveIdentity::Guid(controller,"Helpy delivery flags");}catch(...){}
            HelpyDeliveryGate::Require(Request.To.Character,world,gate.Exempt);
            Owner=controller;OwnerHandle.Assign(controller);
        }else{
            WeakObjectHandle handle;handle.ObjectIndex=Request.To.Index;handle.ObjectSerialNumber=Request.To.Serial;
            Inventory=handle.Get();
            if(!Inventory||!Inventory->IsA(inventoryType)||Inventory->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))||!Inventory->GetWorld())
                throw std::runtime_error("The container is not a live inventory in a world.");
            // A player's own containers are reachable only as a Character target, where delivery flags apply.
            if(PlayerOwned(Inventory,controllerType))throw std::runtime_error("Name a player's container by its character, not by object identity.");
            if(!Network::OwnsGameplay(Network::Detect(Inventory->GetWorld()).Mode))throw std::runtime_error("Items are granted only where the game is authoritative.");
        }
        if(!Inventory||!Inventory->IsA(inventoryType))throw std::runtime_error("The target inventory is unavailable.");
        InventoryHandle.Assign(Inventory);
        Item=AH::ResolveObject(RC::to_generic_string(Request.Item));
        if(!Item||!Item->IsA(itemType))throw std::runtime_error("That item definition is unavailable or is not ItemData.");
        if(AssetMetadata::IsIncomplete(Item))throw std::runtime_error("That item's files are incomplete; nothing was granted.");
        Validate(CountPath,{{TEXT("ItemData"),0,8},{TEXT("ReturnValue"),8,4}});
        Validate(ReadyPath,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("ReturnValue"),12,1}});
        Validate(GivePath,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("DurabilityPercentage"),12,4},{TEXT("GameplayTags"),16,32},{TEXT("ReturnValue"),48,1}});
    }
    // The same component, and for a character the same controller, still holding that character.
    void Recheck(){
        if(InventoryHandle.Get()!=Inventory||!Live(Inventory))throw std::runtime_error("The target inventory changed.");
        if(Request.To.Is!=G::Kind::Character)return;
        if(OwnerHandle.Get()!=Owner||!Live(Owner)||Inventory->GetWorld()!=Owner->GetWorld()
            ||S::NormalizeGuid(PlayerStatusRuntime::CharacterGuid(AH::GetObjectRef(Owner,TEXT("PlayerState"))))!=Request.To.Character)
            throw std::runtime_error("The target character changed.");
        if(Request.To.Owned){
            auto* controllerType=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
            if(!controllerType||OwningController(Inventory,controllerType)!=Owner)throw std::runtime_error("The container's owner changed.");
        }
    }
    int64_t Count(){
        Recheck();
        AH::FunctionCall count(Inventory,CountPath);count.Arg(TEXT("ItemData"),Item).Invoke();return count.Result<int32_t>();
    }
    int64_t Before(){
        const auto before=Count();
        AH::FunctionCall ready(Inventory,ReadyPath);ready.Arg(TEXT("ItemData"),Item).Arg(TEXT("Count"),static_cast<int32_t>(Request.Count)).Invoke();
        if(!ready.Result<bool>())throw std::runtime_error("The target inventory cannot accept that quantity.");
        return before;
    }
    void Invoke(){
        Recheck();
        AH::FunctionCall give(Inventory,GivePath);
        give.Arg(TEXT("ItemData"),Item).Arg(TEXT("Count"),static_cast<int32_t>(Request.Count)).Arg(TEXT("DurabilityPercentage"),1.0f).JsonArg(TEXT("GameplayTags"),nlohmann::json::object());
        give.Invoke();
        if(!give.Result<bool>())throw std::runtime_error("The game refused the grant.");
    }
};
}
int32_t Call(const char* caller,bool permitted,const PluginServerRequest::Serving* serving,const char* request,char* response,uint32_t capacity,uint32_t* size){
    if(!caller||!request||!size||(!response&&capacity)||capacity>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_INVALID_ARGUMENT;
    if(!permitted)return RS_PLUGIN_INVALID_ARGUMENT;
    // A size probe looks nothing up and reserves nothing.
    if(!response||capacity<G::MinCapacity){*size=static_cast<uint32_t>(G::MinCapacity);return RS_PLUGIN_BUFFER_TOO_SMALL;}
    if(Executing)return RS_PLUGIN_FAILED;
    nlohmann::json reply;
    if(!IsInGameThread())reply=G::Refusal("Thread","Native grants must run on the game thread.");
    else{
        struct Guard{Guard(){Executing=true;}~Guard(){Executing=false;}} guard;
        try{
            const auto parsed=G::Parse(nlohmann::json::parse(request));std::unique_ptr<Prepared> operation;
            reply=Receipts.Run(caller,permitted,parsed,capacity,
                [&]{operation=std::make_unique<Prepared>(caller,serving,parsed);return operation->Before();},
                [&]{operation->Invoke();},[&]{return operation->Count();});
        }catch(const G::BufferRequired& needed){*size=static_cast<uint32_t>(needed.Size);return RS_PLUGIN_BUFFER_TOO_SMALL;}
        catch(const std::exception& error){reply=G::Refusal("Request",error.what());}
        catch(...){return RS_PLUGIN_FAILED;}
    }
    const auto text=reply.dump();*size=static_cast<uint32_t>(text.size()+1);
    if(*size>capacity)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
}
}
