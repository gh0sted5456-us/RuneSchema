#pragma once
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Helpers/String.hpp"
#include <nlohmann/json.hpp>
namespace DragonWilds {
// Read-only metadata, not permission to invoke an equipment command or bypass
// native discovery. No UObject is created, no function is called, no save is read.
inline nlohmann::json InspectNativeUnlockEquipContracts() {
    using namespace RC;using namespace RC::Unreal;
    auto report=nlohmann::json{{"ReportVersion",2},{"AutoEquipSupported",false},{"ReadOnly",true},{"Functions",nlohmann::json::array()},
        {"Reason","Equip requires an inventory-instance command and verified native slot/result contract; signature discovery alone does not authorize invocation."}};
    for(const auto* owner:{"PlayerEquipmentComponent","EquipmentInventoryComponent","InventoryComponent","InventoryController","InventoryUIAPI","LoadoutComponent","AutoEquipComponent","EquipmentAutoEquipper","Item","Equipment","WearableEquipmentItem","JournalComponent","JournalSubsystem","ProgressComponent"})
        for(const auto* name:{"CanEquip","CanEquipWithReason","EquipWearableItem","RequestItemEquipOnServer","FindSlotFromEquipment","GetCurrentEquipment",
            "GetEquipmentFromSlot","GetSlotIndexForItem","GetSlotIndexesForItem","GetSlotIndexForSlot","GetSlotForSlotIndex","IsEquipped",
            "MoveItemBetweenInventories","Server_MoveItemBetweenInventories","MoveItemBetweenInventoriesAnySlot","MoveItem","CanAddItem","IsItemAllowed","GetSlotForItem","GetItemFromSlot","GetItemSlots","GetGuid","BP_GetItemData","BP_GetEquipmentData",
            "HandleInternalDragAndDrop","HandleInternalUseItem","UseItemFromInventory","Server_UseItemFromInventory",
            "IsJournalEntryUnlocked","UnlockJournalEntry","UnlockJournalEntriesByPredicate","UnlockRecipes","UnlockBuildings","NotifyItemFirstPickup"}) {
            const auto path=RC::to_generic_string(std::string("/Script/Dominion.")+owner+":"+name);
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path.c_str(),false);
            if(!function)continue;
            auto fields=nlohmann::json::array();
            for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm)) {
                if(fields.size()>=32)break;
                std::string type;try{type=PropertyHelper::GetPropertyTypeAsUTF8String(field);}catch(...){type=RC::to_string(field->GetClass().GetName());}
                fields.push_back({{"Name",RC::to_string(field->GetName())},{"Type",type},{"Offset",field->GetOffset_Internal()},
                    {"Size",field->GetElementSize()},{"ArrayDim",field->GetArrayDim()},{"Return",field->HasAnyPropertyFlags(CPF_ReturnParm)},
                    {"Out",field->HasAnyPropertyFlags(CPF_OutParm)}});
            }
            report["Functions"].push_back({{"Path",RC::to_string(function->GetPathName())},{"FunctionFlags",static_cast<uint64_t>(function->GetFunctionFlags())},{"ParmsSize",function->GetParmsSize()},{"Parameters",std::move(fields)}});
        }
    report["Classes"]=nlohmann::json::array();
    for(const auto* name:{"DominionPlayerController","InventoryController","LoadoutComponent","InventoryComponent","EquipmentInventoryComponent","AutoEquipComponent","Item","Equipment"}){
        const auto path=RC::to_generic_string(std::string("/Script/Dominion.")+name);
        auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,path.c_str(),false);if(!type)continue;
        auto fields=nlohmann::json::array();
        for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)){
            if(fields.size()>=256)break;std::string propertyType;try{propertyType=PropertyHelper::GetPropertyTypeAsUTF8String(field);}catch(...){propertyType=RC::to_string(field->GetClass().GetName());}
            fields.push_back({{"Name",RC::to_string(field->GetName())},{"Type",propertyType},{"Offset",field->GetOffset_Internal()},{"Size",field->GetElementSize()},{"ArrayDim",field->GetArrayDim()}});
        }
        report["Classes"].push_back({{"Path",RC::to_string(type->GetPathName())},{"PropertiesSize",type->GetPropertiesSize()},{"Fields",std::move(fields)}});
    }
    report["Structs"]=nlohmann::json::array();
    for(const auto* name:{"JournalEntryUnlockCondition","InventoryItem","InventoryItemSlot","EquipmentSlot","InventorySlotChange","ComponentReference"}) {
        const auto path=RC::to_generic_string(std::string("/Script/Dominion.")+name);
        auto* structure=UECustom::UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr,nullptr,path.c_str(),false);if(!structure)continue;
        auto fields=nlohmann::json::array();
        for(auto* field:TFieldRange<FProperty>(structure,EFieldIterationFlags::Default)) {
            if(fields.size()>=64)break;
            std::string type;try{type=PropertyHelper::GetPropertyTypeAsUTF8String(field);}catch(...){type=RC::to_string(field->GetClass().GetName());}
            fields.push_back({{"Name",RC::to_string(field->GetName())},{"Type",type},{"Offset",field->GetOffset_Internal()},{"Size",field->GetElementSize()}});
        }
        report["Structs"].push_back({{"Path",RC::to_string(structure->GetPathName())},{"Fields",std::move(fields)}});
    }
    return report;
}
}
