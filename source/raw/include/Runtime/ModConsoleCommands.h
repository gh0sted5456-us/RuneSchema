#pragma once
#include "Core/CookedPakRegistryManifest.h"
#include "Runtime/ConsoleMenu.h"
#include "Runtime/NetworkRoleNotice.h"
#include "Unreal/AActor.hpp"
#include "Unreal/World.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"

namespace PS::Console {
inline std::string DispatchMod(RC::Unreal::ULocalPlayer* player,const ModRequest& request){
    using namespace RC::Unreal;
    namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
    const auto usable=[](UObject* object){return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad));};
    if(!IsInGameThread()||Storefront::IsDedicatedServer()||!usable(player))throw std::runtime_error("Mod commands require a local authoritative gameplay host");
    const auto reference=[&](UObject* owner,const TCHAR* name){
        auto* field=usable(owner)?CastField<FObjectPropertyBase>(owner->GetPropertyByNameInChain(name)):nullptr;
        if(!field||!C::Fits(field,owner->GetClassPrivate()->GetPropertiesSize())||field->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Console object contract is unavailable");
        auto* result=AH::GetObjectRef(owner,name);if(!usable(result))throw std::runtime_error("Console object is unavailable");return result;
    };
    auto* controller=reference(player,TEXT("PlayerController"));
    auto* controllerType=AH::ResolveClass(TEXT("/Script/Engine.PlayerController"));
    if(!controllerType||!controller->IsA(controllerType)||reference(controller,TEXT("Player"))!=player)throw std::runtime_error("Console player identity changed");
    AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
    AH::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!local.Result<bool>()||!authority.Result<bool>())throw std::runtime_error("Remote clients cannot run mod host commands; no request was forwarded");
    auto* world=controller->GetWorld();
    if(!usable(world))throw std::runtime_error("Gameplay world is unavailable");
    const auto worldPath=RC::to_string(world->GetPathName());
    if(!worldPath.starts_with("/Game/Maps/")||!Network::IsGameplayRoleWorld(worldPath))throw std::runtime_error("Mod commands are unavailable outside gameplay");
    std::string path;
    const auto owner=RC::to_string(RC::StringType(request.Owner)),id=RC::to_string(RC::StringType(request.Id));
    const auto fold=CookedPakRegistryManifest::Detail::Fold;
    for(const auto& entry:CookedPakRegistryManifest::ConsoleCommandDefinitions())if(fold(entry.Owner)==fold(owner)&&fold(entry.Id)==fold(id)){
        if(!path.empty()&&fold(path)!=fold(entry.Class))throw std::runtime_error("Conflicting mod command declarations; restart after correcting the mod");
        path=entry.Class;
    }
    if(path.empty())throw std::runtime_error("This command is not declared by an enabled mod");
    auto* type=AH::ResolveClass(ToWideSafe(path));auto* base=AH::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
    if(!usable(type)||!base||!type->IsChildOf(base)||type->HasAnyClassFlags(CLASS_Abstract)||RC::to_string(type->GetPathName())!=path)
        throw std::runtime_error("Declared command component class is unavailable");
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__GameplayStatics"));
    if(!library)throw std::runtime_error("GameplayStatics is unavailable");
    AH::FunctionCall getState(library,TEXT("/Script/Engine.GameplayStatics:GetGameState"));getState.Arg(TEXT("WorldContextObject"),world).Invoke();
    auto* state=getState.Result<UObject*>();auto* stateType=AH::ResolveClass(TEXT("/Script/Engine.GameStateBase"));
    if(!usable(state)||!stateType||!state->IsA(stateType)||state->GetWorld()!=world)throw std::runtime_error("Active GameState is unavailable");
    AH::FunctionCall stateAuthority(state,TEXT("/Script/Engine.Actor:HasAuthority"));stateAuthority.Invoke();
    if(!stateAuthority.Result<bool>())throw std::runtime_error("Active world is not authoritative");
    const auto components=static_cast<AActor*>(state)->GetComponentsByClass(type);
    if(components.Num()!=1)throw std::runtime_error("Expected exactly one existing command component; none were created");
    auto* component=components[0];
    if(!usable(component)||component->GetClassPrivate()!=type||component->GetOuterPrivate()!=state||component->GetWorld()!=world)throw std::runtime_error("Command component identity changed");
    AH::FunctionCall getOwner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
    if(getOwner.Result<UObject*>()!=state)throw std::runtime_error("Command component has a different owner");
    auto* function=component->GetFunctionByNameInChain(TEXT("RuneSchemaConsoleCommand"));
    if(!function||function->GetOuterPrivate()!=type||function->GetReturnProperty()
        ||function->HasAnyFunctionFlags(FUNC_Net|FUNC_Native|FUNC_Exec|FUNC_BlueprintPure)||C::Count(function)!=4)
        throw std::runtime_error("Mod command handler must be a local impure Blueprint function with the declared four pins");
    const auto field=[&](const TCHAR* name){auto* p=function->FindProperty(FName(name,FNAME_Find));if(!C::Fits(p,function->GetParmsSize())||!p->HasAnyPropertyFlags(CPF_Parm)||p->HasAnyPropertyFlags(CPF_ReturnParm))throw std::runtime_error("Mod command parameter layout changed");return p;};
    auto* pc=CastField<FObjectPropertyBase>(field(TEXT("PlayerController")));
    auto* args=CastField<FArrayProperty>(field(TEXT("Arguments")));
    auto* success=CastField<FBoolProperty>(field(TEXT("Success")));
    auto* reply=CastField<FStrProperty>(field(TEXT("Reply")));
    const auto output=[](FProperty* p){return p&&p->HasAnyPropertyFlags(CPF_OutParm)&&!p->HasAnyPropertyFlags(CPF_ReferenceParm|CPF_ConstParm);};
    const auto input=[](FProperty* p){return p&&(!p->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||p->HasAllPropertyFlags(CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm));};
    if(!pc||pc->GetPropertyClass().Get()!=controllerType||pc->GetElementSize()!=sizeof(UObject*)||!input(pc)
        ||!args||!CastField<FStrProperty>(args->GetInner())||!input(args)||!output(success)||!output(reply)||reply->GetElementSize()!=sizeof(FString))
        throw std::runtime_error("Mod command parameter types or directions changed");
    nlohmann::json arguments=nlohmann::json::array();for(size_t i=0;i<request.Count;++i)arguments.push_back(RC::to_string(RC::StringType(request.Arguments[i])));
    const WeakObjectHandle playerIdentity(player),controllerIdentity(controller),stateIdentity(state),componentIdentity(component);
    AH::FunctionCall invoke(component,function);invoke.Arg(TEXT("PlayerController"),controller).JsonArg(TEXT("Arguments"),arguments)
        .InitializeOutput(TEXT("Success")).InitializeOutput(TEXT("Reply"));
    if(playerIdentity.Get()!=player||controllerIdentity.Get()!=controller||stateIdentity.Get()!=state||componentIdentity.Get()!=component)throw std::runtime_error("Command owner changed before dispatch");
    invoke.Invoke();const auto text=invoke.StringOutput(TEXT("Reply"),1024);
    return std::string(invoke.BooleanOutput(TEXT("Success"))?"Mod command completed: ":"Mod command rejected: ")+(text.empty()?"No reply provided.":text);
}
}
