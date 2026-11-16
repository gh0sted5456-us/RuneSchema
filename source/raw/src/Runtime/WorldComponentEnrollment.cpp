#include "Runtime/WorldComponentEnrollment.h"
#include "Core/CookedPakRegistryManifest.h"
#include "Runtime/NetworkRoleNotice.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3D.h"
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/World.hpp"
#include "Unreal/FWorldContext.hpp"
#include <stdexcept>
#include <algorithm>
using namespace RC;
using namespace RC::Unreal;
namespace PS {
namespace {
bool Usable(UObject* object){return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad));}
bool Replicated(UObject* object){
    auto* field=CastField<FBoolProperty>(object->GetPropertyByNameInChain(TEXT("bReplicates")));
    if(!field||field->GetArrayDim()!=1)throw std::runtime_error("ActorComponent replication property is unavailable");
    return field->GetPropertyValueInContainer(object);
}
void Warn(const std::string& owner,const std::string& path,const std::string& error){
    const auto detail=owner+" "+path+": "+error;
    if(!StructuredDiagnosticB3D::Report("RS-WORLD-006","Warning","Skipped","World component unavailable",detail,"WorldComponentEnrollment","Application"))
        Log<LogLevel::Warning>(STR("[RS-WORLD-006][WORLD-COMPONENT] {}\n"),ToWideSafe(detail));
}
}
void WorldComponentEnrollment::Stop() noexcept {
    if(m_beginPlay!=Hook::ERROR_ID)Hook::UnregisterCallback(m_beginPlay);
    if(m_loadMap!=Hook::ERROR_ID)Hook::UnregisterCallback(m_loadMap);
    m_beginPlay=m_loadMap=Hook::ERROR_ID;m_entries.clear();m_enteredStates.clear();m_gameStateType.Reset();m_componentType.Reset();m_attaching.clear();
}
void WorldComponentEnrollment::Start(){
    if(m_beginPlay!=Hook::ERROR_ID)return;
    try{
        std::set<std::string> paths;
        for(const auto& [owner,path]:CookedPakRegistryManifest::WorldComponentClasses())
            if(paths.insert(CookedPakRegistryManifest::Detail::Fold(path)).second)m_entries.push_back({owner,path,{}});
        if(m_entries.empty())return;
        m_gameStateType.Assign(DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.GameStateBase")));
        m_componentType.Assign(DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent")));
        if(!m_gameStateType.Get()||!m_componentType.Get())throw std::runtime_error("Native GameState/ActorComponent classes are unavailable");
        Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("RuneSchemaWorldComponents");
        m_beginPlay=Hook::RegisterBeginPlayPostCallback([this](Hook::TCallbackIterationData<void>&,AActor* actor){
            try{
                if(!Usable(actor))return;
                auto* stateType=static_cast<UClass*>(m_gameStateType.Get());
                if(stateType&&actor->IsA(stateType))Attach(actor);
                Enter(actor->GetWorld());
            }catch(const std::exception& error){Warn({},{},error.what());}
        },options);
        if(m_beginPlay==Hook::ERROR_ID)throw std::runtime_error("Native BeginPlay callback registration failed");
        // Some GameState overrides bypass AActor::BeginPlay; completed maps are another native entry point.
        options.HookName=TEXT("RuneSchemaWorldComponentsMapReady");
        m_loadMap=Hook::RegisterLoadMapPostCallback([this](auto& iteration,auto*,FWorldContext& context,auto&&...){
            if(!iteration.GetOriginalFunctionCallResult())return;
            try{Enter(context.GetThisCurrentWorld().Get());}catch(const std::exception& error){Warn({},{},error.what());}
        },options);
        if(m_loadMap==Hook::ERROR_ID)throw std::runtime_error("Native map-load callback registration failed");
        // Dedicated startup may already have created its GameState before enrollment.
        TArray<UObject*> states;UECustom::UObjectGlobals::GetObjectsOfClass(static_cast<UClass*>(m_gameStateType.Get()),states,true,
            static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));
        if(states.Num()<0)throw std::runtime_error("Native GameState roster is invalid");
        for(int i=0;i<states.Num();++i)if(Usable(states[i]))Enter(states[i]->GetWorld());
    }catch(const std::exception& error){Warn({},{},error.what());Stop();}
}
void WorldComponentEnrollment::Enter(UWorld* world){
    if(!Usable(world))return;
    std::erase_if(m_enteredStates,[](const auto& state){return !Usable(state.Get());});
    for(const auto& state:m_enteredStates)if(state.Get()->GetWorld()==world)return;
    const auto path=RC::to_string(world->GetPathName());
    if(!path.starts_with("/Game/Maps/")||!Network::IsGameplayRoleWorld(path))return;
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__GameplayStatics"));
    if(!library)throw std::runtime_error("Native GameplayStatics is unavailable");
    DragonWilds::ActorHelper::FunctionCall current(library,TEXT("/Script/Engine.GameplayStatics:GetGameState"));
    current.Arg(TEXT("WorldContextObject"),world).Invoke();
    auto* state=current.Result<UObject*>();auto* stateType=static_cast<UClass*>(m_gameStateType.Get());
    if(!stateType||!Usable(state)||!state->IsA(stateType)||state->GetWorld()!=world)return;
    const WeakObjectHandle identity(state);if(identity.Get()!=state)return;
    // Cache before attachment: component startup can synchronously begin other actors.
    m_enteredStates.push_back(identity);Attach(static_cast<AActor*>(state));
}
void WorldComponentEnrollment::Attach(AActor* actor){
    auto* stateType=static_cast<UClass*>(m_gameStateType.Get());
    if(!stateType||!Usable(actor)||!actor->IsA(stateType))return;
    auto* world=actor->GetWorld();if(!Usable(world))return;
    const auto worldPath=RC::to_string(world->GetPathName());
    if(!worldPath.starts_with("/Game/Maps/")||!Network::IsGameplayRoleWorld(worldPath))return;
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__GameplayStatics"));
    if(!library)throw std::runtime_error("Native GameplayStatics is unavailable");
    const auto active=[&]{
        DragonWilds::ActorHelper::FunctionCall current(library,TEXT("/Script/Engine.GameplayStatics:GetGameState"));
        current.Arg(TEXT("WorldContextObject"),actor).Invoke();return current.Result<UObject*>()==actor;
    };
    if(!active())return;
    const WeakObjectHandle worldIdentity(world);if(worldIdentity.Get()!=world)return;
    const WeakObjectHandle owner(actor);if(owner.Get()!=actor)return;
    const auto key=std::pair{owner.ObjectIndex,owner.ObjectSerialNumber};if(!m_attaching.insert(key).second)return;
    struct Leave {decltype(m_attaching)& Set;decltype(key) Key;~Leave(){Set.erase(Key);}} leave{m_attaching,key};
    for(auto& entry:m_entries){
        WeakObjectHandle created;
        try{
            if(owner.Get()!=actor||worldIdentity.Get()!=world||!Usable(world)||!Usable(actor)||actor->GetWorld()!=world||!active())return;
            std::erase_if(entry.SeenOwners,[](const auto& previous){return !Usable(previous.Get());});
            if(std::any_of(entry.SeenOwners.begin(),entry.SeenOwners.end(),[&](const auto& previous){return previous.Get()==actor;}))continue;
            auto* type=static_cast<UClass*>(entry.Type.Get());
            if(!type){
                type=DragonWilds::ActorHelper::ResolveClass(ToWideSafe(entry.Path));
                auto* base=static_cast<UClass*>(m_componentType.Get());
                if(!Usable(type)||!base||!type->IsChildOf(base)||type->HasAnyClassFlags(CLASS_Abstract))throw std::runtime_error("Declared class is not a concrete ActorComponent");
                entry.Type.Assign(type);if(entry.Type.Get()!=type)throw std::runtime_error("Component class identity is unavailable");
            }
            if(!Usable(type)||!CookedPakRegistryManifest::Detail::OwnedObjectPath(RC::to_string(type->GetPathName()),entry.Owner))
                throw std::runtime_error("Resolved component class is unavailable or outside its declared owner");
            // NewObject may create a lazy CDO; validate the actual instance before finishing it.
            if(auto* defaults=type->GetClassDefaultObject().Get();defaults&&Replicated(defaults))
                throw std::runtime_error("World components must have nonreplicated cooked defaults");
            if(owner.Get()!=actor||worldIdentity.Get()!=world||!Usable(actor)||actor->GetWorld()!=world)return;
            auto components=actor->GetComponentsByClass(type);
            if(components.Num()<0||components.Num()>1)throw std::runtime_error("Multiple compatible components already exist; none were changed");
            if(components.Num()==1){if(!Usable(components[0])||Replicated(components[0]))throw std::runtime_error("Existing component is unavailable or replicated");entry.SeenOwners.push_back(owner);continue;}
            const FTransform transform{};const bool manual=true,deferred=true;
            DragonWilds::ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
            add.Arg(TEXT("Class"),type).Arg(TEXT("bManualAttachment"),manual).Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),deferred).Invoke();
            auto* component=add.Result<UObject*>();created.Assign(component);
            if(owner.Get()!=actor||!Usable(actor)||actor->GetWorld()!=world||created.Get()!=component||!Usable(component)||!component->IsA(type)||component->GetOuterPrivate()!=actor||Replicated(component))throw std::runtime_error("Native component construction did not return a valid local instance");
            DragonWilds::ActorHelper::FunctionCall getOwner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
            if(getOwner.Result<UObject*>()!=actor)throw std::runtime_error("Constructed component has a different owner");
            component->SetFlags(RF_Transient);
            DragonWilds::ActorHelper::FunctionCall finish(actor,TEXT("/Script/Engine.Actor:FinishAddComponent"));
            finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),manual).Arg(TEXT("RelativeTransform"),transform);
            // A controller may remove itself in BeginPlay; never start it twice on this GameState.
            entry.SeenOwners.push_back(owner);finish.Invoke();
            if(owner.Get()!=actor||worldIdentity.Get()!=world||!Usable(world)||!Usable(actor)||actor->GetWorld()!=world||!active())return;
            if(created.Get()!=component||!Usable(component))continue;
            if(Replicated(component))throw std::runtime_error("Component enabled replication while starting");
            const auto live=actor->GetComponentsByClass(type);
            if(live.Num()==0)continue;
            if(live.Num()!=1||live[0]!=component)throw std::runtime_error("Component ownership became ambiguous during startup");
            Log<LogLevel::Normal>(STR("[WORLD-COMPONENT][MOD:{}] {} attached to {}.\n"),ToWideSafe(entry.Owner),ToWideSafe(entry.Path),actor->GetPathName());
        }catch(const std::exception& error){
            std::string detail=error.what();
            if(auto* component=created.Get();component&&owner.Get()==actor&&worldIdentity.Get()==world&&Usable(world)&&Usable(actor)&&Usable(component)&&component->GetOuterPrivate()==actor)try{
                DragonWilds::ActorHelper::FunctionCall checkOwner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));checkOwner.Invoke();
                if(checkOwner.Result<UObject*>()!=actor)throw std::runtime_error("Component owner changed; rollback left it untouched");
                DragonWilds::ActorHelper::FunctionCall remove(component,TEXT("/Script/Engine.ActorComponent:K2_DestroyComponent"));remove.Arg(TEXT("Object"),actor).Invoke();
            }catch(const std::exception& rollback){detail+="; component rollback failed: "+std::string(rollback.what());}
            Warn(entry.Owner,entry.Path,detail);
        }
    }
}
}
