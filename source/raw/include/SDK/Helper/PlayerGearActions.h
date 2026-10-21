#pragma once
#include "SDK/Helper/EquipmentDelivery.h"
#include "Generator/CharacterPreview.h"
#include "SDK/WeakObjectHandle.h"
#include <Unreal/World.hpp>
namespace PS::PlayerGearActions {
namespace E=EquipmentDelivery;using namespace RC::Unreal;using Json=nlohmann::json;
inline UObject* Exact(UObject* inventory,int slot,const Json& identity,const std::string& path){
    const auto slots=E::Slots(inventory);if(slot<0||size_t(slot)>=slots.size()||!slots[slot])throw std::runtime_error("Selected item slot changed; refresh before retrying");auto* item=slots[slot];
    auto* data=E::Data(item);if(!data||!identity.is_array()||identity.size()!=4||identity.get<E::Guid>()!=E::Identity(item)||RC::to_string(data->GetPathName())!=path)throw std::runtime_error("Exact item identity changed; no action attempted");return item;
}
inline Json Restore(UObject* controller,const Json& request){
    WeakObjectHandle owner(controller);auto* world=controller->GetWorld();WeakObjectHandle worldHandle(world);auto* pawn=E::AH::GetObjectRef(controller,TEXT("Pawn"));WeakObjectHandle pawnHandle(pawn);
    auto* loadout=E::Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent"));const int slot=request.at("Slot");const auto id=request.at("Instance");const auto path=request.at("Path").get<std::string>();
    auto* item=Exact(loadout,slot,id,path);E::Call maximum(item,TEXT("/Script/Dominion.Item:GetMaxDurability"),1),current(item,TEXT("/Script/Dominion.Item:GetDurability"),1);maximum.Invoke();current.Invoke();const int limit=maximum.Integer(),before=current.Integer();
    if(limit<1||limit>1000000000||before<0||before>limit)throw std::runtime_error("Native durability range unavailable");if(before==limit)return {{"Confirmed",true},{"Changed",false},{"Status","Item already has full durability."}};
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.Item:SetDurability"),false);
    if(!function||E::C::Count(function)!=1||function->GetReturnProperty())throw std::runtime_error("Native durability setter is unavailable; no durability was written");
    FProperty* input=nullptr;for(auto* p:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))input=p;
    if(!CastField<FIntProperty>(input)||!E::C::Input(input,function))throw std::runtime_error("Native durability setter layout changed");
    E::Call set(item,TEXT("/Script/Dominion.Item:SetDurability"),1);set.Integer(input->GetName().c_str(),limit);
    if(owner.Get()!=controller||worldHandle.Get()!=world||pawnHandle.Get()!=pawn||controller->GetWorld()!=world||E::AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn||E::AH::GetObjectRef(controller,TEXT("LoadoutComponent"))!=loadout||Exact(loadout,slot,id,path)!=item)throw std::runtime_error("Equipment changed before restore");set.Invoke();
    if(owner.Get()!=controller||worldHandle.Get()!=world||pawnHandle.Get()!=pawn||controller->GetWorld()!=world||E::AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn||E::AH::GetObjectRef(controller,TEXT("LoadoutComponent"))!=loadout)throw std::runtime_error("Character changed during restore; inspect equipment without retrying");
    current.Invoke();const bool confirmed=Exact(loadout,slot,id,path)==item&&current.Integer()==limit;
    return {{"Confirmed",confirmed},{"Changed",true},{"Status",confirmed?"Exact equipped item durability restored and verified.":"Durability restore unconfirmed; inspect item before retrying."}};
}
inline Json Change(UObject* controller,const Json& request){
    WeakObjectHandle owner(controller);auto* world=controller->GetWorld();WeakObjectHandle worldHandle(world);auto* pawn=E::AH::GetObjectRef(controller,TEXT("Pawn"));WeakObjectHandle pawnHandle(pawn);
    auto* bag=E::Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));auto* loadout=E::Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent"));auto* mover=E::Owned(controller,TEXT("InventoryController"),TEXT("/Script/Dominion.InventoryController"));
    const int source=request.at("SourceSlot"),target=request.at("Slot");const auto id=request.at("Instance");const auto path=request.at("Path").get<std::string>();auto* chosen=Exact(bag,source,id,path);
    if(CharacterPreview::LoadoutSlot(target)!=request.at("Location").get<std::string>())throw std::runtime_error("Native loadout location changed");
    E::Call candidates(loadout,TEXT("/Script/Dominion.LoadoutComponent:GetSlotIndexesForItem"),2);candidates.Object(TEXT("Item"),chosen).Invoke();const auto indexes=candidates.Array<int32_t>();if(std::find(indexes.begin(),indexes.end(),target)==indexes.end())throw std::runtime_error("Native loadout rejected this item's slot");
    const auto previous=E::Slots(loadout);if(target<0||size_t(target)>=previous.size())throw std::runtime_error("Loadout slot is unavailable");auto* old=previous[target];const auto oldId=old?E::Identity(old):E::Guid{};
    if(oldId!=request.at("ExpectedTarget").get<E::Guid>())throw std::runtime_error("Equipped item changed; refresh before swapping");
    E::Call move(mover,TEXT("/Script/Dominion.InventoryController:MoveItemBetweenInventories"),5);move.Object(TEXT("SourceInventory"),bag).Integer(TEXT("SourceSlotIndex"),source).Object(TEXT("TargetInventory"),loadout).Integer(TEXT("TargetSlotIndex"),target);(void)move.Bool();
    const auto currentSlots=E::Slots(loadout);
    if(owner.Get()!=controller||worldHandle.Get()!=world||pawnHandle.Get()!=pawn||controller->GetWorld()!=world||E::AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn
        ||E::AH::GetObjectRef(controller,TEXT("InventoryComponent"))!=bag||E::AH::GetObjectRef(controller,TEXT("LoadoutComponent"))!=loadout||E::AH::GetObjectRef(controller,TEXT("InventoryController"))!=mover
        ||Exact(bag,source,id,path)!=chosen||size_t(target)>=currentSlots.size()||currentSlots[target]!=old)throw std::runtime_error("Inventory changed before native gear move");
    // One native move only. Do not pre-delete gear, grant copies, force slot
    // pointers, chain moves or retry a rejected/unconfirmed native operation.
    move.Invoke();const bool accepted=move.Bool();
    if(owner.Get()!=controller||worldHandle.Get()!=world||pawnHandle.Get()!=pawn||controller->GetWorld()!=world||E::AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn
        ||E::AH::GetObjectRef(controller,TEXT("InventoryComponent"))!=bag||E::AH::GetObjectRef(controller,TEXT("LoadoutComponent"))!=loadout||E::AH::GetObjectRef(controller,TEXT("InventoryController"))!=mover)throw std::runtime_error("Character or component ownership changed during gear move; inspect without retrying");
    const auto after=E::Slots(loadout);bool remains=false,oldRetained=!old;size_t chosenCount=0,oldCount=0;
    for(auto* item:E::Slots(bag))if(item){const auto found=E::Identity(item);if(found==id.get<E::Guid>())remains=true;if(old&&found==oldId){oldRetained=true;++oldCount;}}
    for(auto* item:after)if(item&&E::Identity(item)==id.get<E::Guid>())++chosenCount;
    const bool confirmed=accepted&&size_t(target)<after.size()&&after[target]&&E::Identity(after[target])==id.get<E::Guid>()&&!remains&&chosenCount==1&&oldRetained&&(!old||oldCount==1);
    return {{"Confirmed",confirmed},{"Status",confirmed?"Native gear change verified; previous gear retained in inventory.":"Native gear change rejected or unconfirmed. Inspect both panes; no grant or retry was attempted."}};
}
}
