#pragma once
#include "SDK/Helper/PlayerVitals.h"
#include "Core/EquipmentTransfer.h"
#include <array>
#include <set>
#include <vector>
#include <cstring>
#include <type_traits>
namespace PS::EquipmentDelivery {
using namespace RC::Unreal;namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
using Guid=EquipmentTransfer::Guid;
// A short-lived, reflected frame owns all native return containers. Never copy
// an engine TArray into a C++ container with memcpy or retain its allocation.
class Call {
    UObject* self;UFunction* fn;std::vector<uint8_t> bytes;std::vector<FProperty*> initialized;
public:
    Call(UObject* object,const TCHAR* path,size_t count):self(object){
        fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
        auto* owner=fn?fn->GetOuterPrivate():nullptr;
        if(!self||!fn||!owner||!owner->IsA<UClass>()||!self->IsA(static_cast<UClass*>(owner))
            ||!fn->HasAnyFunctionFlags(FUNC_Native)||fn->HasAnyFunctionFlags(FUNC_Net)||C::Count(fn)!=count)throw std::runtime_error("Native equipment function contract unavailable");
        bytes.resize(fn->GetParmsSize());
        try{for(auto* field:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm)){
            if(!C::Fits(field,fn->GetParmsSize()))throw std::runtime_error("Equipment parameter extent changed");
            field->InitializeValue_InContainer(bytes.data());initialized.push_back(field);
        }}catch(...){Destroy();throw;}
    }
    ~Call(){Destroy();}Call(const Call&)=delete;Call& operator=(const Call&)=delete;
    void Destroy(){for(auto it=initialized.rbegin();it!=initialized.rend();++it)try{(*it)->DestroyValue_InContainer(bytes.data());}catch(...){}initialized.clear();}
    FProperty* Field(const TCHAR* name){auto* p=fn->FindProperty(FName(name,FNAME_Find));if(!p||!C::Fits(p,fn->GetParmsSize())||!p->HasAnyPropertyFlags(CPF_Parm))throw std::runtime_error("Equipment parameter changed");return p;}
    FProperty* Return(){auto* p=fn->GetReturnProperty();if(!C::Output(p,fn))throw std::runtime_error("Equipment return changed");return p;}
    Call& Object(const TCHAR* name,UObject* value){auto* p=CastField<FObjectProperty>(Field(name));if(!p||!C::Input(p,fn)||p->GetSize()!=sizeof(UObject*)||!p->GetPropertyClass()||(value&&!value->IsA(p->GetPropertyClass().Get())))throw std::runtime_error("Equipment object input changed");std::memcpy(p->ContainerPtrToValuePtr<void>(bytes.data()),&value,sizeof(value));return *this;}
    Call& Integer(const TCHAR* name,int value){auto* p=CastField<FIntProperty>(Field(name));if(!p||!C::Input(p,fn))throw std::runtime_error("Equipment integer input changed");*p->ContainerPtrToValuePtr<int32_t>(bytes.data())=value;return *this;}
    Call& Struct(const TCHAR* name,const TCHAR* path,const nlohmann::json& value){auto* p=CastField<FStructProperty>(Field(name));if(!p||!C::Input(p,fn)||!p->GetStruct()||p->GetStruct()->GetPathName()!=path)throw std::runtime_error("Native struct parameter changed");DragonWilds::PropertyHelper::CopyJsonValueToContainer(bytes.data(),p,value);return *this;}
    void Invoke(){self->ProcessEvent(fn,bytes.data());}
    bool Bool(){auto* p=CastField<FBoolProperty>(Return());if(!p)throw std::runtime_error("Equipment bool return changed");return p->GetPropertyValueInContainer(bytes.data());}
    int Integer(){auto* p=CastField<FIntProperty>(Return());if(!p)throw std::runtime_error("Equipment integer result changed");return *p->ContainerPtrToValuePtr<int32_t>(bytes.data());}
    UObject* Object(){auto* p=CastField<FObjectProperty>(Return());if(!p)throw std::runtime_error("Equipment object return changed");auto* value=p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(bytes.data()));if(value&&(!p->GetPropertyClass()||!value->IsA(p->GetPropertyClass().Get())))throw std::runtime_error("Equipment object result type changed");return value;}
    Guid Identity(){auto* p=CastField<FStructProperty>(Return());if(!p||!p->GetStruct()||p->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.Guid")||p->GetSize()!=sizeof(Guid))throw std::runtime_error("Equipment GUID return changed");Guid id{};std::memcpy(id.data(),p->ContainerPtrToValuePtr<void>(bytes.data()),sizeof(id));if(id==Guid{})throw std::runtime_error("Equipment has no instance GUID");return id;}
    template<class T>std::vector<T> Array(){auto* p=CastField<FArrayProperty>(Return());if(!p||!p->GetInner()||p->GetSize()!=sizeof(FScriptArray)||p->GetInner()->GetSize()!=sizeof(T))throw std::runtime_error("Equipment array return changed");
        if constexpr(std::is_same_v<T,UObject*>){auto* inner=CastField<FObjectProperty>(p->GetInner());auto* item=AH::ResolveClass(TEXT("/Script/Dominion.Item"));if(!inner||!item||inner->GetPropertyClass().Get()!=item)throw std::runtime_error("Equipment item array changed");}
        else if(!CastField<FIntProperty>(p->GetInner()))throw std::runtime_error("Equipment slot array changed");
        auto* a=p->ContainerPtrToValuePtr<FScriptArray>(bytes.data());if(!a||a->Num()<0||a->Num()>1024||(a->Num()&&!a->GetData()))throw std::runtime_error("Equipment array exceeds bounds");if(!a->Num())return {};auto* data=static_cast<T*>(a->GetData());return {data,data+a->Num()};}
};
inline Guid Identity(UObject* item){Call c(item,TEXT("/Script/Dominion.Item:GetGuid"),1);c.Invoke();return c.Identity();}
inline std::vector<UObject*> Slots(UObject* inventory){Call c(inventory,TEXT("/Script/Dominion.InventoryComponent:GetItemSlots"),1);c.Invoke();return c.Array<UObject*>();}
inline UObject* Data(UObject* item){Call c(item,TEXT("/Script/Dominion.Item:BP_GetItemData"),1);c.Invoke();return c.Object();}
inline UObject* Owned(UObject* controller,const TCHAR* field,const TCHAR* path){auto* type=AH::ResolveClass(path);auto* value=AH::GetObjectRef(controller,field);if(!type||!value||!value->IsA(type)||value->GetOuterPrivate()!=controller)throw std::runtime_error("Authoritative equipment component ownership unavailable");return value;}
inline bool Matches(UObject* item,const std::string& path){if(!item)return false;auto* type=AH::ResolveClass(TEXT("/Script/Dominion.Equipment"));if(!type||!item->IsA(type))return false;auto* data=Data(item);return data&&RC::to_string(data->GetPathName())==path;}
inline std::set<Guid> Capture(UObject* controller,const std::string& path){
    auto* inventory=Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));std::set<Guid> ids;
    for(auto* item:Slots(inventory))if(Matches(item,path)&&!ids.insert(Identity(item)).second)throw std::runtime_error("Duplicate equipment instance identity");return ids;
}
inline nlohmann::json Equip(UObject* controller,const std::string& path,int count,const std::set<Guid>& before){
    bool attempted=false;try{
        if(count!=1)return {{"State","Skipped"},{"Message","Delivered; equip supports one equipment instance per item type."}};
        if(!controller||!controller->GetWorld())throw std::runtime_error("Player left the authoritative world");
        auto* world=controller->GetWorld();auto* pawn=AH::GetObjectRef(controller,TEXT("Pawn"));if(!pawn||pawn->GetWorld()!=world)throw std::runtime_error("Current equipment character is unavailable");
        auto* bag=Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));
        auto* loadout=Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent"));
        auto* mover=Owned(controller,TEXT("InventoryController"),TEXT("/Script/Dominion.InventoryController"));
        const auto items=Slots(bag);std::vector<EquipmentTransfer::Instance> matches;
        for(size_t i=0;i<items.size();++i)if(Matches(items[i],path))matches.push_back({Identity(items[i]),static_cast<int>(i)});
        const auto candidate=EquipmentTransfer::NewlyDelivered(before,matches);
        if(!candidate)return {{"State","Skipped"},{"Message","Delivered; no unambiguous new equipment instance (non-equipment items are never used or consumed)."}};
        const int source=candidate->Slot;const auto id=candidate->Id;auto* chosen=items[source];
        Call allowed(loadout,TEXT("/Script/Dominion.InventoryComponent:IsItemAllowed"),2);allowed.Object(TEXT("Item"),chosen).Invoke();
        Call capacity(loadout,TEXT("/Script/Dominion.InventoryComponent:CanAddItem"),2);capacity.Object(TEXT("Item"),chosen).Invoke();
        if(!allowed.Bool()||!capacity.Bool())throw std::runtime_error("Native loadout rejected this equipment");
        Call candidates(loadout,TEXT("/Script/Dominion.LoadoutComponent:GetSlotIndexesForItem"),2);candidates.Object(TEXT("Item"),chosen).Invoke();
        const auto indexes=candidates.Array<int32_t>();const auto targetItems=Slots(loadout);int target=-1;
        for(int slot:indexes)if(slot>=0&&static_cast<size_t>(slot)<targetItems.size()&&!targetItems[slot]){target=slot;break;}
        if(target<0)throw std::runtime_error("No empty native equipment slot; existing gear was not replaced");
        // Read back immediately before the single move. No swaps, arbitrary
        // RPCs, UseItem calls or notification setters are used.
        const auto checkBag=Slots(bag),checkLoadout=Slots(loadout);
        if(static_cast<size_t>(source)>=checkBag.size()||checkBag[source]!=chosen||Identity(chosen)!=id||static_cast<size_t>(target)>=checkLoadout.size()||checkLoadout[target])throw std::runtime_error("Equipment slots changed before transfer");
        Call sourceSlot(bag,TEXT("/Script/Dominion.InventoryComponent:GetSlotForItem"),2);sourceSlot.Object(TEXT("Item"),chosen).Invoke();
        Call targetItem(loadout,TEXT("/Script/Dominion.InventoryComponent:GetItemFromSlot"),2);targetItem.Integer(TEXT("SlotIndex"),target).Invoke();
        if(sourceSlot.Integer()!=source||targetItem.Object())throw std::runtime_error("Native slot lookup disagrees with equipment snapshot");
        if(controller->GetWorld()!=world||AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn
            ||AH::GetObjectRef(controller,TEXT("InventoryComponent"))!=bag||AH::GetObjectRef(controller,TEXT("LoadoutComponent"))!=loadout
            ||AH::GetObjectRef(controller,TEXT("InventoryController"))!=mover)throw std::runtime_error("Player world, character or equipment ownership changed");
        Call move(mover,TEXT("/Script/Dominion.InventoryController:MoveItemBetweenInventories"),5);
        move.Object(TEXT("SourceInventory"),bag).Integer(TEXT("SourceSlotIndex"),source).Object(TEXT("TargetInventory"),loadout).Integer(TEXT("TargetSlotIndex"),target);
        (void)move.Bool(); // Validate the reflected result before a mutating call.
        attempted=true;move.Invoke();const bool accepted=move.Bool();const auto equipped=Slots(loadout);bool remains=false;
        for(auto* item:Slots(bag))if(item&&Matches(item,path)&&Identity(item)==id)remains=true;
        if(controller->GetWorld()!=world||AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn)throw std::runtime_error("Character changed during equip; inspect loadout without repeating delivery");
        if(static_cast<size_t>(target)>=equipped.size()||!equipped[target]||!Matches(equipped[target],path)||!EquipmentTransfer::Confirmed(accepted,id,Identity(equipped[target]),remains))throw std::runtime_error("Native equip result unconfirmed; inspect loadout. Do not re-grant.");
        return {{"State","Confirmed"},{"Message","Delivered and exact new equipment instance verified in its native loadout slot."}};
    }catch(const std::exception& e){return {{"State",attempted?"Unconfirmed":"Skipped"},{"Message",e.what()}};}
    catch(...){return {{"State",attempted?"Unconfirmed":"Skipped"},{"Message","Equipment check failed; delivery is not retried."}};}
}
}
