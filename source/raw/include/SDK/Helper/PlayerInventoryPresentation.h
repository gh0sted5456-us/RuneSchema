#pragma once
#include "SDK/Helper/EquipmentDelivery.h"
#include "Generator/AssetSearch.h"
#include "Core/CharacterWorkbench.h"
namespace PS::PlayerInventoryPresentation {
inline nlohmann::json ReadInventory(RC::Unreal::UObject* inventory,const nlohmann::json& request){
    using Json=nlohmann::json;namespace E=EquipmentDelivery;
    const bool personal=request.value("Personal",false);const auto offset=request.value("Offset",0);if(offset<0||offset>1024)throw std::runtime_error("Inventory page is invalid");
    const auto slots=E::Slots(inventory);Json rows=Json::array();const auto end=std::min(slots.size(),size_t(offset)+32);
    for(size_t i=offset;i<end;++i){auto* item=slots[i];if(!item)continue;
        try{auto* data=E::Data(item);if(!data)throw std::runtime_error("Item data unavailable");const auto path=RC::to_string(data->GetPathName());auto row=AssetSearch::ItemMetadata(path);
            E::Call stack(item,TEXT("/Script/Dominion.Item:GetStackSize"),1);stack.Invoke();const auto count=stack.Integer();if(count<0||count>1000000)throw std::runtime_error("Native stack count is invalid");
            row["Path"]=path;row["Slot"]=i;row["Count"]=count;row["Instance"]=E::Identity(item);row["NativeCategory"]=row.value("Tags",std::string{});
            row["Group"]=personal?"Personal storage":i<8?"Quick slots":CharacterWorkbench::InventoryGroup(row);rows.push_back(std::move(row));
        }catch(const std::exception& error){rows.push_back({{"Slot",i},{"Name","Unreadable native slot"},{"Reason",error.what()},{"Available",false}});}
    }
    return {{"Items",rows},{"Offset",offset},{"NextOffset",end},{"HasMore",end<slots.size()},{"Slots",slots.size()},{"Personal",personal},{"ObservedUnixMs",PlayerVitals::Now()},{"ReadOnly",true}};
}
inline nlohmann::json Read(RC::Unreal::UObject* controller,const nlohmann::json& request){
    auto result=ReadInventory(EquipmentDelivery::Owned(controller,request.value("Personal",false)?TEXT("PersonalInventoryComponent"):TEXT("InventoryComponent"),request.value("Personal",false)?TEXT("/Script/Dominion.PersonalInventoryComponent"):TEXT("/Script/Dominion.InventoryComponent")),request);
    if(!request.value("Personal",false)&&request.value("Offset",0)==0)try{
        const auto equipment=ReadInventory(EquipmentDelivery::Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent")),{{"Offset",0},{"Personal",false}});
        if(equipment.value("HasMore",false))throw std::runtime_error("Native equipment snapshot exceeds its 32-slot bound");
        result["Loadout"]=equipment.at("Items");for(auto& row:result["Loadout"]){row["Group"]="Equipped";row["Equipped"]=true;}
    }catch(const std::exception& error){result["LoadoutError"]=error.what();}
    return result;
}
inline bool StorageClass(RC::Unreal::UClass* type){
    auto* building=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.BaseBuildingActor"));return type&&building&&type->IsChildOf(building);
}
inline RC::Unreal::UObject* Storage(RC::Unreal::UObject* actor){
    using namespace RC::Unreal;if(!actor||!StorageClass(actor->GetClassPrivate()))return nullptr;auto* inventory=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.InventoryComponent"));UObject* found=nullptr;size_t count=0;
    if(!inventory||!actor->IsA<AActor>())return nullptr;
    const auto components=static_cast<AActor*>(actor)->GetComponentsByClass(inventory);if(components.Num()<0||components.Num()>32)return nullptr;
    for(auto* value:components){if(!value||value->GetOuterPrivate()!=actor||value->GetWorld()!=actor->GetWorld()||!value->IsA(inventory)||value->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed)))continue;if(found&&found!=value)return nullptr;found=value;}
    for(auto* field:TFieldRange<FProperty>(actor->GetClassPrivate(),EFieldIterationFlags::IncludeSuper)){
        if(++count>256)break;auto* ref=CastField<FObjectProperty>(field);if(!ref||ref->GetSize()!=sizeof(UObject*)||!DragonWilds::ReflectedContract::Fits(ref,actor->GetClassPrivate()->GetPropertiesSize())||!ref->GetPropertyClass()||!ref->GetPropertyClass()->IsChildOf(inventory))continue;
        auto* value=ref->GetObjectPropertyValue(ref->ContainerPtrToValuePtr<void>(actor));if(!value||value->GetOuterPrivate()!=actor||value->GetWorld()!=actor->GetWorld()||!value->IsA(inventory)||value->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        if(found&&found!=value)return nullptr;found=value;
    }return found;
}
}
