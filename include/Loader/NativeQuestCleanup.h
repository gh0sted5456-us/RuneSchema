#pragma once
#include "Loader/QuestSaveOwnership.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Core/HAL/UnrealMemory.hpp"
#include <filesystem>
#include <cstring>

namespace DragonWilds::Quests {
using namespace RC::Unreal;
class NativeQuestCleanup {
    struct ArrayBuffer {
        FArrayProperty* Property;
        void* Data;
        explicit ArrayBuffer(FArrayProperty* p):Property(p),Data(FMemory::Malloc(p->GetElementSize(),p->GetMinAlignment())) {
            if(!Data)throw std::bad_alloc();Property->InitializeValue(Data);
        }
        ~ArrayBuffer(){Property->DestroyValue(Data);FMemory::Free(Data);}
        ArrayBuffer(const ArrayBuffer&)=delete;
        FScriptArray& Array(){return *static_cast<FScriptArray*>(Data);}
        void Add(void* source) {
            auto* inner=Property->GetInner();
            const auto index=Array().Add(1,inner->GetElementSize(),inner->GetMinAlignment());
            auto* destination=static_cast<uint8_t*>(Array().GetData())+index*inner->GetElementSize();
            inner->InitializeValue(destination);inner->CopyCompleteValue(destination,source);
        }
    };
    template<class Field>static Field* Required(UScriptStruct* type,const TCHAR* name,int offset,int size) {
        auto* field=CastField<Field>(PropertyHelper::GetPropertyByName(type,name));
        if(!field || field->GetOffset_Internal()!=offset || field->GetElementSize()!=size || field->GetArrayDim()!=1)
            throw std::runtime_error("Quest cleanup field layout changed");
        return field;
    }
    static FArrayProperty* ArrayProperty(UObject* owner,const TCHAR* name,const TCHAR* innerName,int size) {
        auto* p=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(owner->GetClassPrivate(),name));
        auto* inner=p?CastField<FStructProperty>(p->GetInner()):nullptr;
        if(!p || p->GetElementSize()!=sizeof(FScriptArray) || p->GetArrayDim()!=1 || !inner || !inner->GetStruct()
            || inner->GetStruct()->GetFName()!=FName(innerName,FNAME_Add) || (size && inner->GetElementSize()!=size)
            || inner->GetElementSize()!=inner->GetStruct()->GetStructureSize())throw std::runtime_error("Quest cleanup array layout changed");
        return p;
    }
    static void CheckArray(FScriptArray& array) {
        if(array.Num()<0 || array.Num()>4096 || (array.Num() && !array.GetData()))throw std::runtime_error("Quest cleanup array bounds invalid");
    }
public:
    static size_t Run(UObject* controller,const std::filesystem::path& mods) {
        if(!controller || !controller->GetWorld())return 0;
        ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())return 0;
        auto* component=ActorHelper::GetObjectRef(controller,TEXT("QuestProgressComponent"));
        if(!component || component->GetOuterPrivate()!=controller)return 0;
        if(!std::filesystem::is_directory(mods))throw std::runtime_error("Mod root unavailable; cleanup refused");
        // Complete a readable directory enumeration before interpreting absence.
        for(const auto& entry:std::filesystem::directory_iterator(mods)){(void)entry.path();}
        auto* property=ArrayProperty(component,TEXT("Quests"),TEXT("QuestProgress"),56);
        auto* inner=CastField<FStructProperty>(property->GetInner());auto* type=inner->GetStruct().Get();
        auto* dataField=Required<FObjectPropertyBase>(type,TEXT("Data"),0,8);
        auto* integers=Required<FArrayProperty>(type,TEXT("QuestInts"),24,16);
        auto* integerInner=CastField<FStructProperty>(integers->GetInner());
        if(!integerInner || !integerInner->GetStruct() || integerInner->GetStruct()->GetFName()!=FName(TEXT("QuestInt"),FNAME_Add) || integerInner->GetElementSize()<12 || integerInner->GetElementSize()>64
            || integerInner->GetElementSize()!=integerInner->GetStruct()->GetStructureSize())
            throw std::runtime_error("Quest integer layout changed");
        auto* integerName=Required<FNameProperty>(integerInner->GetStruct().Get(),TEXT("QuestIntName"),0,8);
        auto* integerValue=Required<FIntProperty>(integerInner->GetStruct().Get(),TEXT("QuestIntValue"),8,4);
        auto& live=*property->ContainerPtrToValuePtr<FScriptArray>(component);CheckArray(live);
        ArrayBuffer retained(property),original(property);property->CopyCompleteValue(original.Data,&live);
        std::set<UObject*> removedAssets;std::set<std::string> removedIds,locations,allIds;
        for(int i=0;i<live.Num();++i) {
            auto* row=static_cast<uint8_t*>(live.GetData())+i*56;
            auto& ints=*integers->ContainerPtrToValuePtr<FScriptArray>(row);CheckArray(ints);
            SaveJson savedInts=SaveJson::array();std::string id;std::set<std::string> rowLocations;
            for(int j=0;j<ints.Num();++j) {
                auto* value=static_cast<uint8_t*>(ints.GetData())+j*integerInner->GetElementSize();
                const auto name=RC::to_string(integerName->GetPropertyValue(integerName->ContainerPtrToValuePtr<void>(value)).ToString());
                const auto number=integerValue->GetPropertyValue(integerValue->ContainerPtrToValuePtr<void>(value));
                savedInts.push_back({{"QuestVariableName",name},{"QuestVariableValue",number}});
                if(name.starts_with("RuneSchema.Identity:"))id=name.substr(20);
                if(name.starts_with("RuneSchema.Location:") && number==OwnershipVersion)rowLocations.insert(name.substr(20));
            }
            const auto owner=OwnedBy({{"QuestId",id},{"QuestInts",savedInts}});
            if(owner.empty()){retained.Add(row);continue;}
            if(!allIds.insert(id).second)throw std::runtime_error("Duplicate owned quest identity; cleanup refused");
            std::error_code error;
            const bool present=std::filesystem::exists(mods/std::filesystem::path(RC::to_generic_string(owner)),error);
            if(error)throw std::runtime_error("Cannot establish mod absence; cleanup refused");
            if(present){retained.Add(row);continue;}
            for(const auto& variable:savedInts)if(variable.at("QuestVariableName")=="RuneSchema.Phase"
                && (variable.at("QuestVariableValue")==2 || variable.at("QuestVariableValue")==3))
                throw std::runtime_error("Removed mod has an unresolved quest exchange; records retained for recovery");
            for(const auto& variable:savedInts)if(variable.at("QuestVariableName").get<std::string>().starts_with("RuneSchema.Flag:") && variable.at("QuestVariableValue")==1)
                throw std::runtime_error("Removed mod has an unresolved dialogue reward; records retained for recovery");
            UObject* asset=nullptr;std::memcpy(&asset,dataField->ContainerPtrToValuePtr<void>(row),sizeof(asset));
            if(asset) {
                auto* identity=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(asset->GetClassPrivate(),TEXT("PersistenceID")));
                if(!identity || identity->GetArrayDim()!=1)throw std::runtime_error("Owned quest asset identity unavailable");
                const auto& text=identity->GetPropertyValue(identity->ContainerPtrToValuePtr<void>(asset));
                const auto& chars=text.GetCharArray();
                if(chars.Num()<2 || chars.Num()>1025 || !chars.GetData() || chars.GetData()[chars.Num()-1]!=0
                    || RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1))!=id)throw std::runtime_error("Owned quest asset identity mismatch; cleanup refused");
            }
            if(asset)removedAssets.insert(asset);
            removedIds.insert(id);locations.insert(rowLocations.begin(),rowLocations.end());
        }
        if(removedIds.empty())return 0;
        auto* visibility=ArrayProperty(component,TEXT("VisibilityTrackers"),TEXT("LocationVisibilityTracker"),0);
        auto* visibilityType=CastField<FStructProperty>(visibility->GetInner())->GetStruct().Get();
        const auto visibilityStride=visibility->GetInner()->GetElementSize();
        if(visibilityStride<9 || visibilityStride>64)throw std::runtime_error("Quest visibility stride invalid");
        auto* locationName=Required<FNameProperty>(visibilityType,TEXT("LocationName"),0,8);
        auto& liveVisibility=*visibility->ContainerPtrToValuePtr<FScriptArray>(component);CheckArray(liveVisibility);
        ArrayBuffer retainedVisibility(visibility),oldVisibility(visibility);visibility->CopyCompleteValue(oldVisibility.Data,&liveVisibility);
        for(int i=0;i<liveVisibility.Num();++i) {
            auto* row=static_cast<uint8_t*>(liveVisibility.GetData())+i*visibilityStride;
            const auto name=RC::to_string(locationName->GetPropertyValue(locationName->ContainerPtrToValuePtr<void>(row)).ToString());
            if(!locations.contains(name))retainedVisibility.Add(row);
        }
        auto* tracked=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("TrackedSecondaryQuest")));
        if(!tracked || tracked->GetElementSize()!=sizeof(UObject*) || tracked->GetArrayDim()!=1)throw std::runtime_error("Tracked quest layout changed");
        UObject* oldTracked=nullptr;auto* trackedPtr=tracked->ContainerPtrToValuePtr<void>(component);std::memcpy(&oldTracked,trackedPtr,sizeof(oldTracked));
        auto* notify=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.QuestProgressComponent:OnQuestsUpdated"),false);
        if(!notify || notify->GetParmsSize()!=17)throw std::runtime_error("Quest cleanup notification unavailable");
        size_t parameters=0;FArrayProperty* updated=nullptr;FBoolProperty* loaded=nullptr;
        for(auto* p:TFieldRange<FProperty>(notify,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)) {
            ++parameters;if(p->GetFName()==FName(TEXT("UpdatedQuests"),FNAME_Add))updated=CastField<FArrayProperty>(p);
            if(p->GetFName()==FName(TEXT("bFromLoadedState"),FNAME_Add))loaded=CastField<FBoolProperty>(p);
        }
        auto* updatedInner=updated?CastField<FStructProperty>(updated->GetInner()):nullptr;
        if(parameters!=2 || !updated || updated->GetOffset_Internal()!=0 || updated->GetElementSize()!=16 || !updatedInner || updatedInner->GetStruct()!=inner->GetStruct()
            || !loaded || loaded->GetOffset_Internal()!=16 || loaded->GetElementSize()!=1 || !loaded->IsNativeBool())throw std::runtime_error("Quest cleanup notification layout changed");
        try {
            property->CopyCompleteValue(&live,retained.Data);
            visibility->CopyCompleteValue(&liveVisibility,retainedVisibility.Data);
            if(removedAssets.contains(oldTracked)){UObject* none=nullptr;std::memcpy(trackedPtr,&none,sizeof(none));}
            ActorHelper::FunctionCall(component,notify->GetPathName()).Arg(TEXT("UpdatedQuests"),retained.Array()).Arg(TEXT("bFromLoadedState"),false).Invoke();
        }catch(...) {
            property->CopyCompleteValue(&live,original.Data);visibility->CopyCompleteValue(&liveVisibility,oldVisibility.Data);
            std::memcpy(trackedPtr,&oldTracked,sizeof(oldTracked));throw;
        }
        return removedIds.size();
    }
};
}
