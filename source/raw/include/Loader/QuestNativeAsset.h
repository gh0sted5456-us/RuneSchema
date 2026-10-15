#pragma once
#include "Loader/QuestDefinition.h"
#include "Loader/QuestIdentityPolicy.h"
#include "Loader/QuestObjectiveSchema.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include <cstring>

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
        ReflectedContract::Frame(fn);
        if(!library || count!=2 || !ReflectedContract::Input(input,fn)
            || input->GetElementSize()!=sizeof(UObject*) || input->GetArrayDim()!=1 || !input->HasAnyPropertyFlags(CPF_Parm)
            || input->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm) || !result || !result->IsNativeBool()
            || !ReflectedContract::Output(result,fn) || result->GetElementSize()!=1 || result->GetArrayDim()!=1)
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
        const auto baseName=RC::to_generic_string("RuneSchema_Quest_"+definition.PersistenceId);
        auto name=baseName;
        for(unsigned attempt=0;;++attempt) {
            const auto path=RC::StringType(TEXT("/Engine/Transient."))+name;
            auto* previous=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path.c_str(),false);
            if(!previous)break;
            if(previous->IsRootSet()||attempt>=64)throw std::runtime_error("Quest asset name already exists; refusing adoption");
            // Failed construction leaves an unrooted UObject until the next GC.
            // Never adopt it or overwrite another object; retry with a fresh name.
            name=baseName+TEXT("_Retry_")+RC::to_generic_string(std::to_string(attempt+1));
        }
        object=ActorHelper::ConstructTransientObject(type,name);
        auto* slot=FUObjectArray::IndexToObject(object->GetInternalIndex());
        if(!slot || slot->GetUObject()!=object || !EngineValid(object) || slot->IsRootSet())
            throw std::runtime_error("Quest asset root lease unavailable");
        index=object->GetInternalIndex();serial=slot->GetSerialNumber();objectPath=object->GetPathName();object->SetRootSet();
        try {
            const auto field=[&](const TCHAR* key) {
                auto* p=PropertyHelper::GetPropertyByName(type,key);
                if(!p || p->GetArrayDim()!=1)throw std::runtime_error("Quest asset field unavailable: "+RC::to_string(key));
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
            if(!definition.Region.empty()){
                auto* region=field(TEXT("QuestRegion"));UEnum* enumeration=nullptr;
                if(auto* value=CastField<FEnumProperty>(region))enumeration=value->GetEnum();
                else if(auto* value=CastField<FByteProperty>(region))enumeration=value->GetEnum().Get();
                if(!enumeration||enumeration->GetPathName()!=TEXT("/Script/Dominion.EQuestRegion")
                    ||region->GetElementSize()!=1||!ReflectedContract::Fits(region,type->GetPropertiesSize()))
                    throw std::runtime_error("Quest region reflected contract changed");
                const auto& names=enumeration->GetEnumNames();
                if(names.Num()<1||names.Num()>256)throw std::runtime_error("Quest region enum is invalid");
                std::vector<std::pair<std::string,int64_t>> values;
                for(const auto& entry:names)values.emplace_back(RC::to_string(entry.Key.ToString()),entry.Value);
                const auto value=ResolveRegion(definition.Region,values);
                auto* address=region->ContainerPtrToValuePtr<void>(object);
                std::memcpy(address,&value,sizeof(value));
                if(*static_cast<uint8_t*>(address)!=value)throw std::runtime_error("Quest region write was not retained");
            }
            if(PropertyHelper::GetPropertyByName(type,TEXT("CompletedQuestDescription")))
                setText(TEXT("CompletedQuestDescription"),definition.CompletedDescription.empty()?definition.Description:definition.CompletedDescription);
            for(const auto* key:{TEXT("bIsMainQuest"),TEXT("bHideInQuestList"),TEXT("bIsTaskQuest")}) {
                auto* p=CastField<FBoolProperty>(field(key));
                if(!p)throw std::runtime_error("Quest asset boolean type changed");
                p->SetPropertyValue(p->ContainerPtrToValuePtr<void>(object),((hidden||definition.HideInQuestList) && RC::StringType(key)==TEXT("bHideInQuestList"))
                    || (definition.Story && RC::StringType(key)==TEXT("bIsMainQuest"))
                    || (definition.Task && RC::StringType(key)==TEXT("bIsTaskQuest")));
            }
            auto* objectives=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(type,TEXT("ObjectiveTexts")));
            bool structured=false;
            if(!objectives) {
                objectives=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(type,TEXT("Objectives")));
                auto* value=objectives?CastField<FStructProperty>(objectives->GetValueProp()):nullptr;
                auto* schema=value?value->GetStruct().Get():nullptr;
                auto* description=schema?CastField<FTextProperty>(PropertyHelper::GetPropertyByName(schema,TEXT("Description"))):nullptr;
                if(!schema||schema->GetPathName()!=TEXT("/Script/Dominion.QuestObjectiveData")
                    ||!ReflectedContract::Fits(description,schema->GetStructureSize())||value->GetElementSize()!=schema->GetStructureSize())
                    throw std::runtime_error("Quest structured objective schema is unavailable");
                structured=true;
            }
            if(auto* raw=PropertyHelper::GetPropertyByName(type,TEXT("bShouldShowInCompletedQuestList"))) {
                auto* flag=CastField<FBoolProperty>(raw);
                if(!flag||!ReflectedContract::Fits(flag,type->GetPropertiesSize()))throw std::runtime_error("Quest completed-list visibility layout changed");
                flag->SetPropertyValue(flag->ContainerPtrToValuePtr<void>(object),!hidden&&definition.ShowInCompletedQuestList);
            }
            if(!objectives || objectives->GetArrayDim()!=1 || !CastField<FNameProperty>(objectives->GetKeyProp())
                ||(!structured&&!CastField<FTextProperty>(objectives->GetValueProp())))
                throw std::runtime_error("Quest objective map must be FName to FText or QuestObjectiveData");
            auto texts=Json::array({{{"Key",definition.ObjectiveId},{"Value",ObjectivePresentation(definition)}}});
            const auto readyText=definition.AutomaticReward?"Objectives complete. Make room in your inventory for the reward.":"Objectives complete. Return to the quest giver to claim your reward.";
            if(definition.Kill || definition.Acquire)texts.push_back({{"Key","__ready"},{"Value",readyText}});
            if(!definition.Stages.empty()) {
                texts=Json::array();
                for(const auto& [id,entries]:definition.Stages) {
                    std::string text,subobjective,hint,descriptionOverride;
                    const auto append=[](std::string& target,const std::string& value){if(value.empty())return;if(!target.empty())target+='\n';target+=value;};
                    for(const auto& entry:entries){
                        texts.push_back({{"Key",entry.ObjectiveId},{"Value",ObjectivePresentation(entry)}});
                        if(entry.Hidden)continue;
                        append(text,entry.ObjectiveText);append(subobjective,entry.SubobjectiveDescription);
                        append(hint,entry.Hint);append(descriptionOverride,entry.QuestDescriptionOverride);
                    }
                    Json presentation={{"Description",text}};
                    if(!subobjective.empty())presentation["SubobjectiveDescription"]=subobjective;
                    if(!hint.empty())presentation["Hint"]=hint;
                    if(!descriptionOverride.empty()){presentation["bOverrideQuestDescription"]=true;presentation["QuestDescriptionOverride"]=descriptionOverride;}
                    texts.push_back({{"Key",id},{"Value",std::move(presentation)}});
                }
                texts.push_back({{"Key","__ready"},{"Value",readyText}});
            }
            PropertyHelper::CopyJsonValueToContainer(object,objectives,ObjectivePayload(std::move(texts),structured));
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
