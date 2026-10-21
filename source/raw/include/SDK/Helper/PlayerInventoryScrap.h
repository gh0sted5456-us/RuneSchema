#pragma once
#include "SDK/Helper/PlayerGearActions.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "Runtime/PluginHost.h"
#include "SDK/Helper/NativeDeleteObserver.h"
#include "Loader/AssetMetadataRegistry.h"
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
namespace PS::PlayerInventoryScrap {
using namespace RC::Unreal;namespace E=EquipmentDelivery;namespace C=DragonWilds::ReflectedContract;using Json=nlohmann::json;
inline FProperty* Field(UObject* object,const TCHAR* name){
    auto* p=object?DragonWilds::PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name):nullptr;
    if(!object||!C::Fits(p,object->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Native crafting field unavailable");return p;
}
inline Json Side(UObject* recipe,const TCHAR* name){
    auto* p=CastField<FArrayProperty>(Field(recipe,name));auto* inner=p?CastField<FStructProperty>(p->GetInner()):nullptr;
    auto* type=inner?inner->GetStruct().Get():nullptr;
    if(!p||!type||type->GetPathName()!=TEXT("/Script/Dominion.ItemDataContainer")||inner->GetOffset_Internal()!=0)throw std::runtime_error("Native recipe item layout changed");
    auto* data=CastField<FObjectProperty>(type->FindProperty(FName(TEXT("ItemData"),FNAME_Find)));
    auto* count=CastField<FIntProperty>(type->FindProperty(FName(TEXT("Count"),FNAME_Find)));
    auto* itemType=E::AH::ResolveClass(TEXT("/Script/Dominion.ItemData"));
    if(!C::Fits(data,type->GetPropertiesSize())||!C::Fits(count,type->GetPropertiesSize())||!itemType)throw std::runtime_error("Native recipe item fields changed");
    auto* rows=p->ContainerPtrToValuePtr<FScriptArray>(recipe);
    if(rows->Num()<0||rows->Num()>32||(rows->Num()&&!rows->GetData()))throw std::runtime_error("Native recipe item count exceeds bound");
    Json result=Json::array();for(int i=0;i<rows->Num();++i){
        auto* address=static_cast<uint8_t*>(rows->GetData())+i*inner->GetSize();auto* item=data->GetObjectPropertyValue(data->ContainerPtrToValuePtr<void>(address));
        const auto n=*count->ContainerPtrToValuePtr<int32_t>(address);
        if(!item||!item->IsA(itemType)||n<1||n>1000000)throw std::runtime_error("Native recipe contains an unresolved item/count");
        result.push_back({{"Path",RC::to_string(item->GetPathName())},{"Count",n}});
    }return result;
}
inline bool Deterministic(UObject* recipe){
    auto* chance=CastField<FFloatProperty>(Field(recipe,TEXT("ChanceToProduceExtraItems")));
    auto* multiplier=CastField<FFloatProperty>(Field(recipe,TEXT("CraftQuantityMultiplier")));
    auto* extras=CastField<FArrayProperty>(Field(recipe,TEXT("ExtraItemsCreated")));
    auto* alternative=CastField<FStructProperty>(Field(recipe,TEXT("AlternativeItemCreated")));
    auto* attribute=CastField<FObjectProperty>(Field(recipe,TEXT("AlternativeItemChanceAttribute")));
    if(!chance||!multiplier||!extras||!alternative||!alternative->GetStruct()||!attribute)return false;
    auto* item=CastField<FObjectProperty>(alternative->GetStruct()->FindProperty(FName(TEXT("ItemData"),FNAME_Find)));
    if(!C::Fits(item,alternative->GetStruct()->GetPropertiesSize()))return false;
    return *chance->ContainerPtrToValuePtr<float>(recipe)==0.f&&*multiplier->ContainerPtrToValuePtr<float>(recipe)==1.f
        &&extras->ContainerPtrToValuePtr<FScriptArray>(recipe)->Num()==0&&!attribute->GetObjectPropertyValue(attribute->ContainerPtrToValuePtr<void>(recipe))
        &&!item->GetObjectPropertyValue(item->ContainerPtrToValuePtr<void>(alternative->ContainerPtrToValuePtr<void>(recipe)));
}
inline Json Recipes(UObject* controller,const std::string& itemPath){
    auto* type=E::AH::ResolveClass(TEXT("/Script/Dominion.RecipeSubsystem"));auto* recipeType=E::AH::ResolveClass(TEXT("/Script/Dominion.RecipeData"));
    if(!type||!recipeType)throw std::runtime_error("Native recipe registry unavailable");
    TArray<UObject*> instances;UECustom::UObjectGlobals::GetObjectsOfClass(type,instances,true);
    if(instances.Num()<0||instances.Num()>32)throw std::runtime_error("Recipe registry instance bound exceeded");
    UObject* selected=nullptr;for(auto* instance:instances)if(instance&&instance->GetWorld()==controller->GetWorld()
        &&!instance->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))){
        if(selected&&selected!=instance)throw std::runtime_error("Current native recipe registry is ambiguous");selected=instance;
    }
    auto* map=selected?CastField<FMapProperty>(Field(selected,TEXT("PersistenceIDToDataMap"))):nullptr;
    auto* value=map?CastField<FObjectProperty>(map->GetValueProp()):nullptr;
    if(!map||!value||value->GetOffset_Internal()!=0||!CastField<FStrProperty>(map->GetKeyProp()))throw std::runtime_error("Native recipe registry layout changed");
    UECustom::FScriptMapHelper entries(map,map->ContainerPtrToValuePtr<void>(selected));Json result=Json::array();std::set<UObject*> seen;size_t inspected=0;
    entries.ForEachPair([&](void*,void* address){
        if(++inspected>32768)throw std::runtime_error("Native recipe registry exceeds bound");auto* recipe=value->GetObjectPropertyValue(address);
        if(!recipe||!recipe->IsA(recipeType))throw std::runtime_error("Native recipe registry is incomplete");if(!seen.insert(recipe).second)return;
        const auto outputs=Side(recipe,TEXT("ItemsCreated"));bool matches=false;for(const auto& output:outputs)if(output.at("Path")==itemPath)matches=true;
        if(matches){if(result.size()>=256)throw std::runtime_error("Too many crafting refunds match this item");
            result.push_back({{"Path",RC::to_string(recipe->GetPathName())},{"Inputs",Side(recipe,TEXT("ItemsConsumed"))},{"Outputs",outputs},{"Deterministic",Deterministic(recipe)}});}
    });
    if(!inspected)throw std::runtime_error("Native recipe registry is still empty");return result;
}
inline Json Preview(UObject* controller,const Json& request){
    auto* bag=E::Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));
    const auto path=request.at("Path").get<std::string>();auto* item=PlayerGearActions::Exact(bag,request.at("Slot"),request.at("Instance"),path);
    E::Call count(item,TEXT("/Script/Dominion.Item:GetStackSize"),1);count.Invoke();const auto current=count.Integer();
    const auto quantity=request.at("Quantity");if(!quantity.is_number_integer()||quantity<1||quantity>current||current>1000000)throw std::runtime_error("Current native item quantity changed");
    bool equipped=false;auto* loadout=E::Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent"));
    for(auto* other:E::Slots(loadout))if(other&&E::Identity(other)==E::Identity(item))equipped=true;
    const Json row={{"Path",path},{"Instance",E::Identity(item)},{"PlayerGuid",request.at("PlayerGuid")},{"Count",current},{"Equipped",equipped}};
    const Json message={{"Action","ScrapPlan"},{"Item",row},{"Quantity",quantity},{"Recipes",Recipes(controller,path)}};
    const auto result=Json::parse(PluginHost::Call("RuneSchema.Core","helpy.inventory",message.dump()));
    if(result.contains("Error")||!result.contains("Plan"))throw std::runtime_error("Helpy crafting refund policy is unavailable");
    auto plan=result.at("Plan");plan["Slot"]=request.at("Slot");plan["ExpectedCount"]=current;plan["Generation"]=request.at("Generation");
    return {{"State","Preview"},{"Plan",plan},{"Status","Crafting refund calculated from the native registry; no item consumed or delivered."}};
}
inline Json Service(const Json& request){auto reply=Json::parse(PluginHost::Call("RuneSchema.Core","helpy.inventory",request.dump()));if(reply.contains("Error"))throw std::runtime_error(reply.at("Error").get<std::string>());return reply;}
inline Json Commit(UObject* controller,const Json& request,const std::function<bool()>& current){
    bool prepared=false,deleted=false,observed=false;Json drops=Json::array();std::string token;
    const auto character=request.at("PlayerGuid").get<std::string>();
    try{
        if(!current())throw std::runtime_error("Scrap target changed before validation");
        const auto plan=Preview(controller,request).at("Plan");
        if(!request.contains("ExpectedPlan")||request.at("ExpectedPlan")!=plan)throw std::runtime_error("Crafting refund changed; inspect the stack again");
        const int quantity=plan.at("Quantity");if(quantity!=plan.at("ExpectedCount"))throw std::runtime_error("Scrap deletes one complete native stack; split a stack in-game first");
        WeakObjectHandle owner(controller);auto* world=controller->GetWorld();WeakObjectHandle worldHandle(world);
        auto* pawn=E::AH::GetObjectRef(controller,TEXT("Pawn"));WeakObjectHandle pawnHandle(pawn);
        if(!pawn||!pawn->IsA<AActor>()||pawn->GetWorld()!=world)throw std::runtime_error("Current native character is unavailable");
        std::array<UObject*,3> inventories{};size_t index=0;for(const auto* name:{TEXT("InventoryComponent"),TEXT("LoadoutComponent"),TEXT("PersonalInventoryComponent")}){
            const auto* type=index==1?TEXT("/Script/Dominion.LoadoutComponent"):index==2?TEXT("/Script/Dominion.PersonalInventoryComponent"):TEXT("/Script/Dominion.InventoryComponent");
            if(E::AH::GetObjectRef(controller,name))inventories[index]=E::Owned(controller,name,type);++index;
        }
        auto* bag=inventories[0];auto* mover=E::Owned(controller,TEXT("InventoryController"),TEXT("/Script/Dominion.InventoryController"));
        auto* item=PlayerGearActions::Exact(bag,request.at("Slot"),request.at("Instance"),plan.at("Path"));const auto id=E::Identity(item);
        const auto valid=[&]{return owner.Get()==controller&&worldHandle.Get()==world&&pawnHandle.Get()==pawn&&current()
            &&controller->GetWorld()==world&&E::AH::GetObjectRef(controller,TEXT("Pawn"))==pawn
            &&E::AH::GetObjectRef(controller,TEXT("InventoryComponent"))==inventories[0]&&E::AH::GetObjectRef(controller,TEXT("LoadoutComponent"))==inventories[1]
            &&E::AH::GetObjectRef(controller,TEXT("PersonalInventoryComponent"))==inventories[2]&&E::AH::GetObjectRef(controller,TEXT("InventoryController"))==mover;};
        auto* helperClass=E::AH::ResolveClass(TEXT("/Script/Dominion.ItemHelperLibrary"));auto* helper=helperClass?helperClass->GetClassDefaultObject().Get():nullptr;
        auto* dropClass=E::AH::ResolveClass(TEXT("/Game/Gameplay/WorldItems/BP_RuntimeSpawnedWorldItem.BP_RuntimeSpawnedWorldItem_C"));
        auto* nativeDrop=E::AH::ResolveClass(TEXT("/Script/Dominion.WorldItem"));
        if(!helper||!dropClass||!nativeDrop||!dropClass->IsChildOf(nativeDrop)||E::AH::IsAbstract(dropClass))throw std::runtime_error("Native world-drop class is unavailable");
        const auto location=E::AH::GetActorLocation(static_cast<AActor*>(pawn));
        if(!std::isfinite(location.X())||!std::isfinite(location.Y())||!std::isfinite(location.Z()))throw std::runtime_error("Native refund location is invalid");
        struct Drop {UObject* Data;int Count;std::unique_ptr<E::Call> Call;};std::vector<Drop> refunds;
        for(const auto& refund:plan.at("Refunds")){
            auto* data=E::AH::ResolveObject(RC::to_generic_string(refund.at("Path").get<std::string>()));auto* type=E::AH::ResolveClass(TEXT("/Script/Dominion.ItemData"));
            if(!data||!type||!data->IsA(type)||AssetMetadata::IsIncomplete(data))throw std::runtime_error("Crafting component is not fully registered");
            E::Call maximum(data,TEXT("/Script/Dominion.ItemData:GetMaxStackSize"),1);maximum.Invoke();const auto limit=maximum.Integer();
            if(limit<1||limit>1000000)throw std::runtime_error("Native refund stack limit is unavailable");
            int64_t remaining=refund.at("Count");if(remaining<1||remaining>1000000||refunds.size()+size_t((remaining+limit-1)/limit)>128)throw std::runtime_error("Refund exceeds the 128-drop bound");
            while(remaining){const int n=int(std::min<int64_t>(remaining,limit));remaining-=n;
                auto call=std::make_unique<E::Call>(helper,TEXT("/Script/Dominion.ItemHelperLibrary:SpawnAndLaunchItem_Sync"),4);
                call->Object(TEXT("WorldContextObject"),controller).Struct(TEXT("ItemParameter"),TEXT("/Script/Dominion.ItemSpawnParameters"),{
                    {"ItemClass",RC::to_string(dropClass->GetPathName())},{"bCreateItem",true},{"bMagnetize",false},{"SpawnedItemData",refund.at("Path")},{"Count",n},
                    {"Transform",{{"Translation",{{"X",location.X()},{"Y",location.Y()},{"Z",location.Z()+60.0}}},{"Rotation",{{"X",0.0},{"Y",0.0},{"Z",0.0},{"W",1.0}}},{"Scale3D",{{"X",1.0},{"Y",1.0},{"Z",1.0}}}}},
                    {"OwnerController",RC::to_string(controller->GetPathName())},{"PlayerControllerThatDroppedItem",RC::to_string(controller->GetPathName())},
                    {"bAddToWorldItemRuntimeCache",true},{"bSpawnOnlyForController",false},{"bMagnetizeToInstigatorOnSpawn",false},{"bSkipFloorSafetyCheck",false}});
                (void)call->Object();refunds.push_back({data,n,std::move(call)});
            }
        }
        E::Call remove(mover,TEXT("/Script/Dominion.InventoryController:DestroyItemFromInventory"),3);
        remove.Object(TEXT("Inventory"),bag).Integer(TEXT("SlotIndex"),request.at("Slot")).Object(TEXT("Instigator"),controller);
        auto* data=E::Data(item);auto* persistence=CastField<FStrProperty>(Field(data,TEXT("PersistenceID")));
        if(!persistence)throw std::runtime_error("Item persistence identity is unavailable");const auto persistent=RC::to_string(RC::StringType(*persistence->GetPropertyValue(persistence->ContainerPtrToValuePtr<void>(data))));
        std::string instance;for(auto word:id){char part[9]{};std::snprintf(part,sizeof(part),"%08x",word);instance+=part;}
        const Json record={{"AssetPath",plan.at("Path")},{"PersistenceID",persistent},{"InstanceGuid",instance},{"Count",quantity},
            {"NativeItem",{{"ItemData",persistent},{"GUID",instance},{"Count",quantity}}}};
        static uint64_t sequence=0;token=std::to_string(GetCurrentProcessId())+"-scrap-"+std::to_string(GetTickCount64())+"-"+std::to_string(++sequence);
        E::Call currentCount(item,TEXT("/Script/Dominion.Item:GetStackSize"),1);currentCount.Invoke();
        if(!valid()||currentCount.Integer()!=quantity||PlayerGearActions::Exact(bag,request.at("Slot"),request.at("Instance"),plan.at("Path"))!=item)throw std::runtime_error("Inventory changed before Scrap");
        if(!Service({{"Action","BeginScrap"},{"CharacterGuid",character},{"CaptureId",token},{"Item",record},{"Plan",plan}}).value("Prepared",false))throw std::runtime_error("Scrap intent was not committed");prepared=true;
        if(!valid())throw std::runtime_error("Player changed before native deletion");
        {NativeDeleteObserver::ScrapScope scope(controller,id);remove.Invoke();}
        if(!valid())throw std::runtime_error("Player changed during native deletion; no refunds attempted");
        bool present=false;for(auto* inventory:inventories)if(inventory)for(auto* entry:E::Slots(inventory))if(entry&&E::Identity(entry)==id)present=true;
        deleted=!present;Service({{"Action","ObserveScrap"},{"CharacterGuid",character},{"CaptureId",token},{"Result",deleted?"Removed":"Retained"}});observed=true;
        if(!deleted)return {{"State","Rejected"},{"Confirmed",false},{"Status","Native deletion retained the item; no materials refunded."}};
        for(auto& refund:refunds){
            if(!valid())throw std::runtime_error("Player changed while refunding; remaining drops not attempted");
            refund.Call->Invoke();auto* actor=refund.Call->Object();
            if(!actor||!actor->IsA(nativeDrop)||actor->GetWorld()!=world||E::AH::GetObjectRef(actor,TEXT("ItemData"))!=refund.Data)throw std::runtime_error("Native refund drop was not confirmed; no retry");
            E::Call count(actor,TEXT("/Script/Dominion.WorldItem:GetStackSize"),1);count.Invoke();
            if(count.Integer()!=refund.Count)throw std::runtime_error("Native refund drop quantity was not confirmed; no retry");
            drops.push_back({{"Path",RC::to_string(refund.Data->GetPathName())},{"Count",refund.Count},{"Actor",RC::to_string(actor->GetPathName())}});
        }
        Service({{"Action","FinishScrap"},{"CharacterGuid",character},{"CaptureId",token},{"Confirmed",true},{"Drops",drops}});
        return {{"State","Confirmed"},{"Confirmed",true},{"Drops",drops},{"Status","Scrapped with native deletion; crafting components dropped beside the player."}};
    }catch(const std::exception& error){
        if(prepared)try{if(observed&&deleted)Service({{"Action","FinishScrap"},{"CharacterGuid",character},{"CaptureId",token},{"Confirmed",false},{"Drops",drops}});
            else if(!observed)Service({{"Action","ObserveScrap"},{"CharacterGuid",character},{"CaptureId",token},{"Result","Unknown"}});}catch(...){}
        return {{"State",prepared?"Unconfirmed":"Rejected"},{"Confirmed",false},{"Drops",drops},{"Status",std::string(error.what())+(prepared?" Inspect inventory and drops; no automatic retry.":" No native deletion was attempted.")}};
    }
}
}
