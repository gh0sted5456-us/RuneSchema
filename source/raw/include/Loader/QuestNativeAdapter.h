#pragma once
#include "Loader/QuestNativeContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include <vector>

namespace DragonWilds::QuestNative {
// Short-lived, game-thread interface. The caller must verify authority and a
// registered quest identity before mutation; this class does not register assets.
class Adapter {
    RC::Unreal::UObject* controller;
    RC::Unreal::UObject* component;
    RC::Unreal::UObject* quest;
    void ChangeRestartState(const char* expected,const char* next) const {
        using namespace RC::Unreal;
        CheckOwner();
        ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>() || !quest->GetOuterPrivate()
            || quest->GetOuterPrivate()->GetPathName()!=TEXT("/Engine/Transient")
            || quest->GetName().find(TEXT("RuneSchema_Quest_"))!=0)
            throw std::runtime_error("Quest restart requires an authoritative RuneSchema quest");
        auto* array=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("Quests")));
        auto* inner=array?CastField<FStructProperty>(array->GetInner()):nullptr;
        auto* type=inner?inner->GetStruct().Get():nullptr;
        if(!array || array->GetArrayDim()!=1 || !inner || inner->GetArrayDim()!=1 || !type
            || type->GetPathName()!=TEXT("/Script/Dominion.QuestProgress"))
            throw std::runtime_error("Quest restart progress layout changed");
        auto* data=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Data")));
        auto* state=PropertyHelper::GetPropertyByName(type,TEXT("State"));
        UEnum* enumeration=nullptr;
        if(auto* value=CastField<FEnumProperty>(state))enumeration=value->GetEnum();
        else if(auto* byte=CastField<FByteProperty>(state))enumeration=byte->GetEnum().Get();
        if(!data || data->GetArrayDim()!=1 || data->GetElementSize()!=sizeof(UObject*)
            || !data->GetPropertyClass().Get() || !quest->IsA(data->GetPropertyClass().Get())
            || !state || state->GetArrayDim()!=1 || state->GetElementSize()!=1 || !enumeration
            || enumeration->GetPathName()!=TEXT("/Script/Dominion.EQuestState"))
            throw std::runtime_error("Quest restart state layout changed");
        const auto valueFor=[&](const char* name) {
            int found=-1;
            const auto& names=enumeration->GetEnumNames();
            if(names.Num()<1 || names.Num()>64)throw std::runtime_error("Quest state enum count invalid");
            for(const auto& entry:names) {
                auto text=RC::to_string(entry.Key.ToString());const auto colon=text.rfind("::");
                if(colon!=text.npos)text=text.substr(colon+2);
                if(text==name) {
                    if(found!=-1 || entry.Value<0 || entry.Value>255)throw std::runtime_error("Quest state enum ambiguous");
                    found=static_cast<int>(entry.Value);
                }
            }
            if(found<0)throw std::runtime_error("Quest state enum value unavailable");
            return static_cast<uint8_t>(found);
        };
        const auto before=valueFor(expected),after=valueFor(next);
        auto* raw=array->ContainerPtrToValuePtr<FScriptArray>(component);
        if(raw->Num()<1 || raw->Num()>4096)throw std::runtime_error("Quest restart registry count invalid");
        UECustom::FScriptArrayHelper view(raw,array);
        void* matched=nullptr;
        view.ForEachElement([&](void* entry) {
            if(data->GetObjectPropertyValue(data->ContainerPtrToValuePtr<void>(entry))!=quest)return;
            if(matched)throw std::runtime_error("Quest restart identity is duplicated");
            matched=entry;
        });
        if(!matched)throw std::runtime_error("Quest restart identity is absent");
        auto* address=state->ContainerPtrToValuePtr<uint8_t>(matched);
        if(*address!=before)throw std::runtime_error("Quest restart state changed before transition");
        state->CopyCompleteValue(address,&after);
        if(*address!=after)throw std::runtime_error("Quest restart state write failed");
    }
    void CheckOwner() const {
        if(!controller || !component || !quest || !controller->GetWorld()
            || component->GetOuterPrivate()!=controller || component->GetWorld()!=controller->GetWorld()
            || ActorHelper::GetObjectRef(controller,TEXT("QuestProgressComponent"))!=component)
            throw std::runtime_error("Quest component is not owned by the current controller");
        const auto invalid=static_cast<RC::Unreal::EObjectFlags>(RC::Unreal::RF_ClassDefaultObject|RC::Unreal::RF_ArchetypeObject|RC::Unreal::RF_BeginDestroyed|RC::Unreal::RF_FinishDestroyed);
        if(controller->HasAnyFlags(invalid) || component->HasAnyFlags(invalid) || quest->HasAnyFlags(invalid))
            throw std::runtime_error("Quest interface cannot use template or destroyed objects");
        auto* componentClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestProgressComponent"));
        auto* questClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestData"));
        if(!componentClass || !questClass || !component->IsA(componentClass) || !quest->IsA(questClass))
            throw std::runtime_error("Quest object types do not match native contracts");
    }
    RC::Unreal::UFunction* Function(Method method) const {
        using namespace RC::Unreal;
        CheckOwner();
        const auto expected=Get(method);
        const auto path=RC::to_generic_string("/Script/Dominion.QuestProgressComponent:"+std::string(expected.Name));
        auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path.c_str(),false);
        if(!function)throw std::runtime_error("Native quest function unavailable");
        std::vector<std::string> names;
        std::vector<Field> fields;
        names.reserve(8);fields.reserve(8);
        for(auto* p:TFieldRange<FProperty>(function,EFieldIterationFlags::Default)) {
            if(!p->HasAnyPropertyFlags(CPF_Parm))continue;
            if(names.size()>=8 || p->GetArrayDim()!=1)throw std::runtime_error("Unexpected quest parameter layout");
            Kind kind;
            if(auto* object=CastField<FObjectPropertyBase>(p)) {
                if(!object->GetPropertyClass().Get() || !quest->IsA(object->GetPropertyClass().Get()))
                    throw std::runtime_error("Quest parameter object class changed");
                kind=Kind::Quest;
            } else if(auto* boolean=CastField<FBoolProperty>(p)) {
                if(!boolean->IsNativeBool() || boolean->GetByteOffset()!=0)throw std::runtime_error("Quest bool layout changed");
                kind=Kind::Boolean;
            } else if(CastField<FNameProperty>(p))kind=Kind::Name;
            else if(CastField<FIntProperty>(p))kind=Kind::Integer;
            else {
                UEnum* enumeration=nullptr;
                if(auto* value=CastField<FEnumProperty>(p))enumeration=value->GetEnum();
                else if(auto* byte=CastField<FByteProperty>(p))enumeration=byte->GetEnum().Get();
                if(!enumeration || enumeration->GetFName()!=FName(TEXT("EQuestState"),FNAME_Add))throw std::runtime_error("Quest state enum changed");
                kind=Kind::State;
            }
            names.push_back(RC::to_string(p->GetName()));
            fields.push_back({names.back(),p->GetOffset_Internal(),p->GetElementSize(),kind,function->GetReturnProperty()==p});
        }
        Validate(expected,function->GetParmsSize(),fields);
        return function;
    }
public:
    Adapter(RC::Unreal::UObject* owner,RC::Unreal::UObject* data):controller(owner),
        component(owner?ActorHelper::GetObjectRef(owner,TEXT("QuestProgressComponent")):nullptr),quest(data){CheckOwner();}
    void ResetForDefinitionChange() const {
        using namespace RC::Unreal;
        Function(Method::State);Function(Method::Objective);Function(Method::GetInt);Function(Method::SetInt);
        auto current=RC::to_string(StateName());const auto colon=current.rfind("::");if(colon!=current.npos)current=current.substr(colon+2);
        if(current!="Ungiven"&&current!="Given"&&current!="Complete")throw std::runtime_error("Changed quest has an unsupported native state");
        if(current!="Ungiven")ChangeRestartState(current.c_str(),"Ungiven");
        auto* array=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("Quests")));
        auto* inner=array?CastField<FStructProperty>(array->GetInner()):nullptr;auto* type=inner?inner->GetStruct().Get():nullptr;
        auto* data=type?CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("Data"))):nullptr;
        auto* integers=type?CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(type,TEXT("QuestInts"))):nullptr;
        auto* integerType=integers?CastField<FStructProperty>(integers->GetInner()):nullptr;
        if(!array||array->GetArrayDim()!=1||!inner||!type||type->GetPathName()!=TEXT("/Script/Dominion.QuestProgress")
            ||!data||data->GetOffset_Internal()!=0||data->GetElementSize()!=sizeof(UObject*)
            ||!integers||integers->GetOffset_Internal()!=24||integers->GetElementSize()!=sizeof(FScriptArray)
            ||!integerType||!integerType->GetStruct()||integerType->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.QuestInt"))
            throw std::runtime_error("Changed quest reset layout changed");
        auto* raw=array->ContainerPtrToValuePtr<FScriptArray>(component);
        if(raw->Num()<1||raw->Num()>4096)throw std::runtime_error("Changed quest reset registry count invalid");
        UECustom::FScriptArrayHelper rows(raw,array);void* matched=nullptr;
        rows.ForEachElement([&](void* entry){if(data->GetObjectPropertyValue(data->ContainerPtrToValuePtr<void>(entry))==quest){if(matched)throw std::runtime_error("Changed quest identity is duplicated");matched=entry;}});
        if(!matched)throw std::runtime_error("Changed quest identity is absent");
        auto* values=integers->ContainerPtrToValuePtr<FScriptArray>(matched);
        if(values->Num()<0||values->Num()>4096)throw std::runtime_error("Changed quest counter count invalid");
        UECustom::FScriptArrayHelper counters(values,integers);counters.Empty();
        SetObjective(FName(TEXT("__definition_reset"),FNAME_Add));
        NotifyRecovery();
    }
    void ValidateAll() const {
        for(const auto method:{Method::Give,Method::Complete,Method::Initialize,Method::IsInitialized,Method::State,Method::Objective})Function(method);
    }
    void SetGiven(bool silent) const {Mutate(Method::Give,silent);}
    template<class Prepare> bool RestartCompleted(Prepare prepare) const {
        // Only the owned state byte is reset. Native GiveQuest performs notification;
        // receipt timestamps, inventory and all unrelated quest entries stay intact.
        Function(Method::Give);Function(Method::State);
        ChangeRestartState("Complete","Ungiven");
        try {
            // Rebuild every counter/objective while the quest is hidden, then
            // publish one coherent update. This prevents the client briefly
            // presenting the previous completed run before the new stage.
            prepare();
            SetGiven(true);
        }catch(...) {
            auto name=RC::to_string(StateName());const auto colon=name.rfind("::");
            if(colon!=name.npos)name=name.substr(colon+2);
            if(name=="Ungiven")ChangeRestartState("Ungiven","Complete");
            throw;
        }
        auto name=RC::to_string(StateName());const auto colon=name.rfind("::");
        if(colon!=name.npos)name=name.substr(colon+2);
        if(name=="Ungiven")ChangeRestartState("Ungiven","Complete");
        if(name=="Given")NotifyRecovery();
        return name=="Given";
    }
    void SetComplete(bool silent) const {Mutate(Method::Complete,silent);}
    void NotifyRecovery(bool invoke=true) const {
        using namespace RC::Unreal;
        CheckOwner();
        auto* array=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("Quests")));
        auto* inner=array?CastField<FStructProperty>(array->GetInner()):nullptr;
        // This must enter through the native owning-client RPC. Calling the
        // similarly shaped OnQuestsUpdated delegate only updates the local
        // component; on a listen host it never reaches the host's client HUD,
        // and on a dedicated server it never reaches the owning connection.
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.QuestProgressComponent:Client_OnQuestsUpdated"),false);
        if(!array || !inner || !fn || fn->GetParmsSize()!=17)throw std::runtime_error("Quest control notification unavailable");
        size_t count=0;FArrayProperty* updated=nullptr;FBoolProperty* loaded=nullptr;
        for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)) {
            ++count;if(p->GetFName()==FName(TEXT("UpdatedQuests"),FNAME_Add))updated=CastField<FArrayProperty>(p);
            if(p->GetFName()==FName(TEXT("bFromLoadedState"),FNAME_Add))loaded=CastField<FBoolProperty>(p);
        }
        auto* item=updated?CastField<FStructProperty>(updated->GetInner()):nullptr;
        if(count!=2 || !fn->HasAnyFunctionFlags(FUNC_Net) || !fn->HasAnyFunctionFlags(FUNC_NetClient)
            || !updated || updated->GetOffset_Internal()!=0 || updated->GetElementSize()!=16 || !item || item->GetStruct()!=inner->GetStruct()
            || !loaded || loaded->GetOffset_Internal()!=16 || loaded->GetElementSize()!=1 || !loaded->IsNativeBool())throw std::runtime_error("Quest control notification layout changed");
        auto* values=array->ContainerPtrToValuePtr<FScriptArray>(component);
        if(values->Num()<1 || values->Num()>4096)throw std::runtime_error("Quest control registry count invalid");
        if(invoke)ActorHelper::FunctionCall(component,fn)
            .ArrayArg(TEXT("UpdatedQuests"),array,values)
            .Arg(TEXT("bFromLoadedState"),false).Invoke();
    }
    void CancelForRecovery() const {NotifyRecovery(false);ChangeRestartState("Given","Ungiven");}
    template<class Prepare> void RestartForRecovery(Prepare prepare,const RC::Unreal::FName& objective) const {
        Function(Method::Give);Function(Method::State);Function(Method::Objective);
        auto state=RC::to_string(StateName());const auto colon=state.rfind("::");
        if(colon!=state.npos)state=state.substr(colon+2);
        if(state!="Given" && state!="Complete" && state!="Ungiven")throw std::runtime_error("Unsupported quest recovery state");
        // Even Ungiven passes the same authority, owned-asset and layout checks.
        ChangeRestartState(state.c_str(),"Ungiven");
        prepare();SetObjective(objective);SetGiven(false);
    }
    void Mutate(Method method,bool silent) const {
        if(method!=Method::Give && method!=Method::Complete)throw std::runtime_error("Invalid quest mutation");
        auto* fn=Function(method);
        ActorHelper::FunctionCall(component,fn->GetPathName()).Arg(TEXT("QuestData"),quest).Arg(TEXT("bSilent"),silent).Invoke();
    }
    bool IsInitialized() const {
        auto* fn=Function(Method::IsInitialized);
        ActorHelper::FunctionCall call(component,fn->GetPathName());call.Arg(TEXT("QuestData"),quest).Invoke();return call.Result<bool>();
    }
    void Initialize() const {
        auto* fn=Function(Method::Initialize);
        ActorHelper::FunctionCall(component,fn->GetPathName()).Arg(TEXT("QuestData"),quest).Invoke();
        if(!IsInitialized())throw std::runtime_error("Native quest initialization was not confirmed");
    }
    void SetObjective(const RC::Unreal::FName& name) const {
        auto* fn=Function(Method::Objective);
        ActorHelper::FunctionCall(component,fn->GetPathName()).Arg(TEXT("QuestData"),quest).Arg(TEXT("ObjectiveName"),name).Invoke();
    }
    void ValidateCounters() const {Function(Method::GetInt);Function(Method::SetInt);}
    int32_t GetInt(const RC::Unreal::FName& name) const {
        auto* fn=Function(Method::GetInt);
        ActorHelper::FunctionCall call(component,fn->GetPathName());
        call.Arg(TEXT("QuestData"),quest).Arg(TEXT("QuestIntName"),name).Invoke();
        return call.Result<int32_t>();
    }
    void SetInt(const RC::Unreal::FName& name,int32_t value) const {
        auto* fn=Function(Method::SetInt);
        ActorHelper::FunctionCall(component,fn->GetPathName()).Arg(TEXT("QuestData"),quest)
            .Arg(TEXT("QuestIntName"),name).Arg(TEXT("IntValue"),value).Invoke();
        if(GetInt(name)!=value)throw std::runtime_error("Native quest counter did not match after update");
    }
    RC::StringType StateName() const {
        using namespace RC::Unreal;
        auto* fn=Function(Method::State);
        auto* result=fn->GetReturnProperty();
        UEnum* enumeration=nullptr;
        if(auto* value=CastField<FEnumProperty>(result))enumeration=value->GetEnum();
        else if(auto* byte=CastField<FByteProperty>(result))enumeration=byte->GetEnum().Get();
        ActorHelper::FunctionCall call(component,fn->GetPathName());call.Arg(TEXT("QuestData"),quest).Invoke();
        return enumeration->GetNameByValue(call.Result<uint8_t>()).ToString();
    }
};
}
