#pragma once
#include "Loader/QuestRegistryPlan.h"
#include "Loader/QuestNetworkId.h"
#include "Loader/QuestObjectReference.h"
#include "Loader/ItemIdentity.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "Unreal/Core/HAL/UnrealMemory.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include <memory>
#include <cstring>
#include <vector>

namespace DragonWilds::QuestRegistry {
// Call only on the game thread while the selected game instance is stable.
// This does not create assets, select a global subsystem or modify save files.
class NativeRegistry {
    using UObject=RC::Unreal::UObject;
    using FProperty=RC::Unreal::FProperty;
    using FMapProperty=RC::Unreal::FMapProperty;
    using FArrayProperty=RC::Unreal::FArrayProperty;
    using FObjectPropertyBase=RC::Unreal::FObjectPropertyBase;
    using FString=RC::Unreal::FString;
    struct NativeValue {
        FProperty* Property;
        void* Data;
        explicit NativeValue(FProperty* property):Property(property),Data(RC::Unreal::FMemory::Malloc(property->GetSize(),property->GetMinAlignment())) {
            if(!Data)throw std::bad_alloc();
            try {Property->InitializeValue(Data);}catch(...){RC::Unreal::FMemory::Free(Data);throw;}
        }
        NativeValue(const NativeValue&)=delete;
        ~NativeValue(){Property->DestroyValue(Data);RC::Unreal::FMemory::Free(Data);}
    };
    struct Stage {
        FProperty* Property;
        void* Original;
        void* Data;
        Stage(FProperty* property,UObject* owner):Property(property),Original(property->ContainerPtrToValuePtr<void>(owner)),Data(nullptr) {
            if(property->GetArrayDim()!=1 || property->GetSize()<=0 || property->GetSize()>256)
                throw std::runtime_error("Quest registry container layout changed");
            Data=RC::Unreal::FMemory::Malloc(property->GetSize(),property->GetMinAlignment());
            if(!Data)throw std::bad_alloc();
            property->InitializeValue(Data);
            try {property->CopyCompleteValue(Data,Original);}catch(...){property->DestroyValue(Data);RC::Unreal::FMemory::Free(Data);throw;}
        }
        Stage(const Stage&)=delete;
        ~Stage(){Property->DestroyValue(Data);RC::Unreal::FMemory::Free(Data);}
        void Publish() noexcept {
            auto* old=static_cast<unsigned char*>(Original);auto* next=static_cast<unsigned char*>(Data);
            for(int i=0;i<Property->GetSize();++i)std::swap(old[i],next[i]);
        }
    };
    static FObjectPropertyBase* ObjectProperty(FProperty* property,UObject* quest) {
        using namespace RC::Unreal;
        auto* object=CastField<FObjectPropertyBase>(property);
        if(!object || property->GetArrayDim()!=1 || property->GetElementSize()!=sizeof(UObject*)
            || !object->GetPropertyClass().Get() || !quest->IsA(object->GetPropertyClass().Get()))
            throw std::runtime_error("Quest registry object property changed");
        ValidateObjectReferenceLayout(RC::to_string(property->GetClass().GetName()),property->GetElementSize(),property->GetArrayDim(),sizeof(UObject*));
        return object;
    }
    static void CopyObjectReference(FObjectPropertyBase* property,void* destination,UObject* quest) {
        CopyResolvedReference(quest,destination,
            [&](void* target,const void* source){property->CopyCompleteValue(target,source);},
            [&](void* target){return property->GetObjectPropertyValue(target);});
    }
    static void CheckMap(FMapProperty* property,void* data) {
        if(!property || !property->GetKeyProp() || !property->GetValueProp())throw std::runtime_error("Quest registry map missing");
        auto* map=static_cast<RC::Unreal::FScriptMap*>(data);
        if(map->Num()<0 || map->Num()>65535 || map->GetMaxIndex()<0 || map->GetMaxIndex()>131072)
            throw std::runtime_error("Quest registry map exceeds bounds");
    }
    static void Insert(FMapProperty* property,void* data,void* key,void* value) {
        using namespace RC::Unreal;
        CheckMap(property,data);
        auto* map=static_cast<FScriptMap*>(data);
        auto* kp=property->GetKeyProp();auto* vp=property->GetValueProp();
        const auto layout=FScriptMap::GetScriptLayout(kp->GetSize(),kp->GetMinAlignment(),vp->GetSize(),vp->GetMinAlignment());
        bool found=false;
        UECustom::FScriptMapHelper helper(property,data);
        helper.ForEachPair([&](void* existingKey,void* existingValue){
            if(kp->Identical(existingKey,key)) {
                if(found || !vp->Identical(existingValue,value))throw std::runtime_error("Quest registry identity is already occupied");
                found=true;
            }
        });
        if(found)return;
        if(map->Num()>=65535)throw std::runtime_error("Quest registry map is full");
        const auto index=map->AddUninitialized(layout);
        auto* pair=static_cast<uint8*>(map->GetData(index,layout));
        kp->InitializeValue(pair);vp->InitializeValue(pair+layout.ValueOffset);
        kp->CopyCompleteValue(pair,key);vp->CopyCompleteValue(pair+layout.ValueOffset,value);
        helper.Rehash();
    }
    static void StringEntry(FMapProperty* property,void* data,const FString& key,UObject* quest) {
        using namespace RC::Unreal;
        if(!CastField<FStrProperty>(property->GetKeyProp()) || property->GetKeyProp()->GetArrayDim()!=1)
            throw std::runtime_error("Quest registry requires FString keys");
        auto* object=ObjectProperty(property->GetValueProp(),quest);
        NativeValue value(object);
        CopyObjectReference(object,value.Data,quest);
        Insert(property,data,const_cast<FString*>(&key),value.Data);
    }
public:
    static uint16_t Register(UObject* subsystem,UObject* gameInstance,UObject* quest) {
        return RegisterAsset(subsystem,gameInstance,quest,false);
    }
    static uint16_t RegisterJournal(UObject* subsystem,UObject* gameInstance,UObject* entry) {
        return RegisterAsset(subsystem,gameInstance,entry,true);
    }
private:
    static uint16_t RegisterAsset(UObject* subsystem,UObject* gameInstance,UObject* quest,bool journal) {
        using namespace RC::Unreal;
        auto* type=ActorHelper::ResolveClass(journal?TEXT("/Script/Dominion.JournalSubsystem"):TEXT("/Script/Dominion.QuestDataSubsystem"));
        auto* instanceType=ActorHelper::ResolveClass(TEXT("/Script/Engine.GameInstance"));
        auto* questType=ActorHelper::ResolveClass(journal?TEXT("/Script/Dominion.JournalEntryData"):TEXT("/Script/Dominion.QuestData"));
        if(!subsystem || !gameInstance || !quest || !type || !instanceType || !questType
            || !subsystem->IsA(type) || !gameInstance->IsA(instanceType) || !quest->IsA(questType)
            || subsystem->GetOuterPrivate()!=gameInstance)
            throw std::runtime_error("Quest registration owner/type mismatch");
        const auto invalid=static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization);
        if(subsystem->HasAnyFlags(invalid) || gameInstance->HasAnyFlags(invalid) || quest->HasAnyFlags(invalid))
            throw std::runtime_error("Quest registration object is not ready");
        const auto identity=[&](const TCHAR* name) {
            auto* p=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(quest->GetClassPrivate(),name));
            if(!p || p->GetArrayDim()!=1)throw std::runtime_error("Quest identity field changed");
            const auto& text=p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(quest));
            const auto& chars=text.GetCharArray();
            if(chars.Num()<=1 || !chars.GetData())throw std::runtime_error("Quest identity is empty: "+RC::to_string(name)+" on "+RC::to_string(quest->GetPathName()));
            if(chars.Num()>1025 || chars.GetData()[chars.Num()-1]!=0)throw std::runtime_error("Quest identity string is malformed");
            return RC::StringType(chars.GetData(),chars.Num()-1);
        };
        const auto persistenceText=identity(TEXT("PersistenceID")),internalText=identity(TEXT("InternalName"));
        const FString persistence(persistenceText.c_str()),internal(internalText.c_str());
        if(!journal && !IsCanonicalPersistenceId(RC::to_string(RC::StringType(*persistence))))throw std::runtime_error("Invalid quest persistence ID");
        const FString asset(quest->GetName().c_str());
        const auto map=[&](const TCHAR* name) {
            auto* p=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(),name));
            if(!p)throw std::runtime_error("Quest registry map unavailable");
            CheckMap(p,p->ContainerPtrToValuePtr<void>(subsystem));return p;
        };
        auto* ids=map(TEXT("PersistenceIDToDataMap"));auto* names=map(TEXT("InternalNameToDataMap"));
        auto* assets=map(TEXT("AssetNameToDataMap"));auto* reverse=map(TEXT("DataToNetIdMap"));
        auto* array=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(),TEXT("NetIdToData")));
        if(!array)throw std::runtime_error("Quest network array unavailable");
        auto* inner=ObjectProperty(array->GetInner(),quest);
        auto* reverseKey=ObjectProperty(reverse->GetKeyProp(),quest);
        auto* reverseValue=reverse->GetValueProp();
        if(!reverseValue)throw std::runtime_error("Quest network ID property is missing");
        FProperty* idField=reverseValue;
        if(auto* wrapper=CastField<FStructProperty>(reverseValue)) {
            auto* type=wrapper->GetStruct().Get();
            if(!type)throw std::runtime_error("Quest network ID wrapper type is missing");
            std::vector<NetworkIdField> fields;
            for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
                if(fields.size()>=2)throw std::runtime_error("Quest network ID wrapper has extra fields");
                fields.push_back({RC::to_string(field->GetName()),RC::to_string(field->GetClass().GetName()),
                    field->GetElementSize(),field->GetOffset_Internal(),field->GetArrayDim()});
                idField=field;
            }
            ValidateNetworkIdWrapper(RC::to_string(type->GetPathName()),reverseValue->GetElementSize(),reverseValue->GetArrayDim(),fields);
            if(type->GetPropertiesSize()!=2)throw std::runtime_error("Quest network ID struct size changed");
        }
        const NetworkIdLayout idLayout(RC::to_string(idField->GetClass().GetName()),
            idField->GetElementSize(),idField->GetArrayDim());
        auto* rawArray=array->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        if(rawArray->Num()<0 || rawArray->Num()>65535)throw std::runtime_error("Quest network array exceeds bounds");
        std::vector<uintptr_t> slots;
        UECustom::FScriptArrayHelper arrayView(rawArray,array);
        arrayView.ForEachElement([&](void* value){slots.push_back(reinterpret_cast<uintptr_t>(inner->GetObjectPropertyValue(value)));});
        std::optional<uint16_t> oldId;
        UECustom::FScriptMapHelper reverseView(reverse,reverse->ContainerPtrToValuePtr<void>(subsystem));
        reverseView.ForEachPair([&](void* key,void* value){if(reverseKey->GetObjectPropertyValue(key)==quest) {
            if(oldId)throw std::runtime_error("Duplicate quest reverse key");
            oldId=idLayout.Read(value);
        }});
        const auto plan=NetworkPlan(reinterpret_cast<uintptr_t>(quest),slots,oldId);
        Stage idStage(ids,subsystem),nameStage(names,subsystem),assetStage(assets,subsystem),reverseStage(reverse,subsystem),arrayStage(array,subsystem);
        StringEntry(ids,idStage.Data,persistence,quest);
        StringEntry(names,nameStage.Data,persistence,quest);StringEntry(names,nameStage.Data,internal,quest);
        StringEntry(assets,assetStage.Data,asset,quest);
        if(plan.Append) {
            UECustom::FScriptArrayHelper stagedArray(static_cast<FScriptArray*>(arrayStage.Data),array);
            NativeValue value(inner),key(reverseKey);
            CopyObjectReference(inner,value.Data,quest);stagedArray.Add(value.Data);
            CopyObjectReference(reverseKey,key.Data,quest);
            NativeValue id(reverseValue);
            idLayout.Write(id.Data,plan.Id);Insert(reverse,reverseStage.Data,key.Data,id.Data);
        }
        // No allocations or callbacks after publication starts. Stage destructors
        // release the old containers, not the newly published entries.
        idStage.Publish();nameStage.Publish();assetStage.Publish();reverseStage.Publish();arrayStage.Publish();
        return plan.Id;
    }
};
}
