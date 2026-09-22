#pragma once
#include "Loader/QuestDefinition.h"
#include "Loader/QuestIdentityPolicy.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"

namespace DragonWilds::Quests {
// Root lease for a newly created quest asset. Registry references may outlive
// this lease; never clear another object's root based only on a reused pointer.
class NativeAsset {
    RC::Unreal::UObject* object=nullptr;
    int32_t index=-1;
    mutable int32_t serial=0;
    RC::StringType objectPath;
    bool registered=false;
    static bool EngineValid(RC::Unreal::UObject* value) {
        using namespace RC::Unreal;
        const auto* path=TEXT("/Script/Engine.KismetSystemLibrary:IsValid");
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
        auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetSystemLibrary"),false);
        auto* input=fn?CastField<FObjectPropertyBase>(fn->FindProperty(FName(TEXT("Object"),FNAME_Find))):nullptr;
        auto* result=fn?CastField<FBoolProperty>(fn->GetReturnProperty()):nullptr;
        size_t count=0;if(fn)for(auto* field:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
        if(!library || !fn || count!=2 || fn->GetParmsSize()!=9 || !input || input->GetOffset_Internal()!=0
            || input->GetElementSize()!=sizeof(UObject*) || input->GetArrayDim()!=1 || !input->HasAnyPropertyFlags(CPF_Parm)
            || input->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm) || !result || !result->IsNativeBool()
            || result->GetOffset_Internal()!=8 || result->GetElementSize()!=1 || result->GetArrayDim()!=1)
            throw std::runtime_error("Quest asset engine-validity contract changed");
        ActorHelper::FunctionCall call(library,path);call.Arg(TEXT("Object"),value).Invoke();return call.Result<bool>();
    }
    void Release() noexcept {
        if(!object || index<0)return;
        auto* slot=RC::Unreal::FUObjectArray::IndexToObject(index);
        try {
            if(slot && slot->GetUObject()==object && RootedQuestLeaseMatches(true,object->GetPathName()==objectPath,
                slot->IsRootSet(),!object->HasAnyFlags(static_cast<RC::Unreal::EObjectFlags>(RC::Unreal::RF_BeginDestroyed|RC::Unreal::RF_FinishDestroyed)),serial,slot->GetSerialNumber()))
                object->ClearRootSet();
        }catch(...) {}
        object=nullptr;
    }
public:
    NativeAsset(const NativeAsset&)=delete;
    NativeAsset& operator=(const NativeAsset&)=delete;
    explicit NativeAsset(const Definition& definition,bool hidden=false) {
        using namespace RC::Unreal;
        if(!IsCanonicalPersistenceId(definition.PersistenceId))throw std::runtime_error("Quest asset identity is invalid");
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestData"));
        if(!type)throw std::runtime_error("QuestData class unavailable");
        const auto name=RC::to_generic_string("RuneSchema_Quest_"+definition.PersistenceId);
        const auto path=RC::StringType(TEXT("/Engine/Transient."))+name;
        if(UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path.c_str(),false))
            throw std::runtime_error("Quest asset name already exists; refusing adoption");
        object=ActorHelper::ConstructTransientObject(type,name);
        auto* slot=FUObjectArray::IndexToObject(object->GetInternalIndex());
        if(!slot || slot->GetUObject()!=object || !EngineValid(object) || slot->IsRootSet())
            throw std::runtime_error("Quest asset root lease unavailable");
        index=object->GetInternalIndex();serial=slot->GetSerialNumber();objectPath=object->GetPathName();object->SetRootSet();
        try {
            const auto field=[&](const TCHAR* key) {
                auto* p=PropertyHelper::GetPropertyByName(type,key);
                if(!p || p->GetArrayDim()!=1)throw std::runtime_error("Quest asset field unavailable");
                return p;
            };
            const auto setString=[&](const TCHAR* key,const std::string& value) {
                auto* p=CastField<FStrProperty>(field(key));
                if(!p)throw std::runtime_error("Quest asset string type changed");
                const FString text(RC::to_generic_string(value).c_str());
                p->CopyCompleteValue(p->ContainerPtrToValuePtr<void>(object),&text);
                const auto& copied=p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(object));
                const auto& chars=copied.GetCharArray();
                if(chars.Num()<=1 || !chars.GetData() || chars.Num()>1025
                    || RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1))!=value)
                    throw std::runtime_error("Quest identity copy failed: "+RC::to_string(key));
            };
            const auto setText=[&](const TCHAR* key,const std::string& value) {
                auto* p=CastField<FTextProperty>(field(key));
                if(!p)throw std::runtime_error("Quest asset text type changed");
                PropertyHelper::CopyJsonValueToContainer(object,p,value);
            };
            setString(TEXT("PersistenceID"),definition.PersistenceId);setString(TEXT("InternalName"),definition.Key);
            setText(TEXT("QuestName"),definition.Title);setText(TEXT("QuestDescription"),definition.Description);
            for(const auto* key:{TEXT("bIsMainQuest"),TEXT("bHideInQuestList"),TEXT("bIsTaskQuest")}) {
                auto* p=CastField<FBoolProperty>(field(key));
                if(!p)throw std::runtime_error("Quest asset boolean type changed");
                p->SetPropertyValue(p->ContainerPtrToValuePtr<void>(object),(hidden && RC::StringType(key)==TEXT("bHideInQuestList"))
                    || (definition.Story && RC::StringType(key)==TEXT("bIsMainQuest"))
                    || (definition.Task && RC::StringType(key)==TEXT("bIsTaskQuest")));
            }
            auto* objectives=CastField<FMapProperty>(field(TEXT("ObjectiveTexts")));
            if(!objectives || !CastField<FNameProperty>(objectives->GetKeyProp()) || !CastField<FTextProperty>(objectives->GetValueProp()))
                throw std::runtime_error("Quest objective map must be FName to FText");
            auto texts=Json::array({{{"Key",definition.ObjectiveId},{"Value",definition.ObjectiveText}}});
            const auto readyText=definition.AutomaticReward?"Objectives complete. Make room in your inventory for the reward.":"Objectives complete. Return to the quest giver to claim your reward.";
            if(definition.Kill || definition.Acquire)texts.push_back({{"Key","__ready"},{"Value",readyText}});
            if(!definition.Stages.empty()) {
                texts=Json::array();
                for(const auto& [id,entries]:definition.Stages) {
                    std::string text;
                    for(const auto& entry:entries){
                        texts.push_back({{"Key",entry.ObjectiveId},{"Value",entry.ObjectiveText}});
                        if(entry.Hidden)continue;if(!text.empty())text+="\n";text+=entry.ObjectiveText;
                    }
                    texts.push_back({{"Key",id},{"Value",text}});
                }
                texts.push_back({{"Key","__ready"},{"Value",readyText}});
            }
            PropertyHelper::CopyJsonValueToContainer(object,objectives,texts);
        } catch(...) {Release();throw;}
    }
    ~NativeAsset(){Release();}
    void MarkRegistered() noexcept {registered=true;}
    void EnsureIdentity(const Definition& definition) {
        using namespace RC::Unreal;
        auto* owner=Get();
        for(const auto& [name,expected]:{std::pair{TEXT("PersistenceID"),definition.PersistenceId},std::pair{TEXT("InternalName"),definition.Key}}) {
            auto* p=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(owner->GetClassPrivate(),name));
            if(!p || p->GetArrayDim()!=1)throw std::runtime_error("Quest identity field layout changed");
            const auto& text=p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(owner));
            const auto& chars=text.GetCharArray();
            if(chars.Num()<0 || chars.Num()>1025 || (chars.Num() && (!chars.GetData() || chars.GetData()[chars.Num()-1]!=0)))
                throw std::runtime_error("Quest identity string is malformed");
            const auto actual=chars.Num()>1?RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1)):std::string{};
            if(RestoreUnregisteredIdentity(actual,expected,registered)) {
                const FString replacement(RC::to_generic_string(expected).c_str());
                p->CopyCompleteValue(p->ContainerPtrToValuePtr<void>(owner),&replacement);
            }
        }
    }
    RC::Unreal::UObject* Get() const {
        auto* slot=index<0?nullptr:RC::Unreal::FUObjectArray::IndexToObject(index);
        const auto fail=[&](const char* reason) {
            return std::runtime_error("Quest asset lease failed for "+RC::to_string(objectPath)+": "+reason
                +" (saved serial="+std::to_string(serial)+", current serial="+std::to_string(slot?slot->GetSerialNumber():-1)+")");
        };
        if(!slot || slot->GetUObject()!=object)throw fail("object slot changed");
        if(!slot->IsRootSet())throw fail("owned root was removed");
        if(object->GetPathName()!=objectPath)throw fail("asset path changed");
        if(!RootedQuestLeaseMatches(true,true,true,true,serial,slot->GetSerialNumber()))throw fail("object generation changed");
        if(!EngineValid(object))throw fail("engine reports destroyed/invalid asset");
        slot=RC::Unreal::FUObjectArray::IndexToObject(index);
        if(!slot || slot->GetUObject()!=object || !RootedQuestLeaseMatches(true,object->GetPathName()==objectPath,
            slot->IsRootSet(),true,serial,slot->GetSerialNumber()))throw fail("lease changed during validation");
        // Zero is unassigned; a continuously rooted asset may receive its first serial later.
        serial=slot->GetSerialNumber();
        return object;
    }
};
}
