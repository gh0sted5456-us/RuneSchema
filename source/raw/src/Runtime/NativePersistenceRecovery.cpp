#include "Runtime/NativePersistenceRecovery.h"
#include "Core/NativePersistencePolicy.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/World.hpp"
#include "Unreal/NameTypes.hpp"
#include "Helpers/String.hpp"
#include <set>
#include <stdexcept>
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Runtime/Storefront.h"
namespace PS::NativePersistenceRecovery {
using namespace RC::Unreal;
namespace {
NativePersistencePolicy::Lease sessionLease;
std::optional<NativePersistencePolicy::Outcome> sessionOutcome;
DWORD sessionThread=0;
UFunction* Function(const TCHAR* path){
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    DragonWilds::ReflectedContract::Frame(function);return function;
}
UObject* Library(){return UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetSystemLibrary"),false);}
void Check(UFunction* function,bool getter){
    std::set<RC::StringType> seen;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::IncludeDeprecated))if(field->HasAnyPropertyFlags(CPF_Parm)){
        const auto name=field->GetName();
        if(!seen.insert(name).second)throw std::runtime_error("Duplicate console parameter");
        const bool returned=field->HasAnyPropertyFlags(CPF_ReturnParm);
        const bool text=(getter&&(name==TEXT("VariableName")||name==TEXT("ReturnValue")))||(!getter&&name==TEXT("Command"));
        const bool object=!getter&&(name==TEXT("WorldContextObject")||name==TEXT("SpecificPlayer"));
        if(!DragonWilds::ReflectedContract::Fits(field,function->GetParmsSize())
            ||(returned?!DragonWilds::ReflectedContract::Output(field,function):!DragonWilds::ReflectedContract::Input(field,function))
            ||(text&&(!CastField<FStrProperty>(field)||field->GetElementSize()!=sizeof(FString)))
            ||(object&&(!CastField<FObjectPropertyBase>(field)||field->GetElementSize()!=sizeof(UObject*)))
            ||(!text&&!object)||returned!=(getter&&name==TEXT("ReturnValue")))
            throw std::runtime_error("Console function layout is unverified");
    }
    const std::set<RC::StringType> expected=getter?std::set<RC::StringType>{TEXT("VariableName"),TEXT("ReturnValue")}
        :std::set<RC::StringType>{TEXT("WorldContextObject"),TEXT("Command"),TEXT("SpecificPlayer")};
    if(seen!=expected)throw std::runtime_error("Console function parameters changed");
}
}
std::optional<bool> ReadStrictMode(){
    auto* function=Function(TEXT("/Script/Engine.KismetSystemLibrary:GetConsoleVariableStringValue"));Check(function,true);
    DragonWilds::ActorHelper::FunctionCall call(Library(),function);
    call.StringArg(TEXT("VariableName"),NativePersistencePolicy::Variable);call.Invoke();
    FString value;call.MoveResult(&value,sizeof(value));
    return NativePersistencePolicy::Parse(RC::to_string(RC::StringType(*value)));
}
void WriteStrictMode(UObject* context,bool strict){
    if(!context||context->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))||!context->GetWorld())
        throw std::runtime_error("Native recovery requires a live world context");
    auto* function=Function(TEXT("/Script/Engine.KismetSystemLibrary:ExecuteConsoleCommand"));Check(function,false);
    DragonWilds::ActorHelper::FunctionCall call(Library(),function);
    call.Arg(TEXT("WorldContextObject"),context).StringArg(TEXT("Command"),std::string(NativePersistencePolicy::Variable)+(strict?" 1":" 0"))
        .Arg(TEXT("SpecificPlayer"),static_cast<UObject*>(nullptr));call.Invoke();
}
void EnsureSessionPolicy(UObject* context) noexcept{
    const auto& launch=PSConfig::Get()->GetSettings().quickLaunch;
    const bool requested=NativePersistencePolicy::SessionRequested(Storefront::IsDedicatedServer(),
        PSConfig::Get()->GetSettings().persistence.allowCorruptCharacterEntry,launch.mode);
    if(!requested||!context)return;
    if(sessionThread&&sessionThread!=GetCurrentThreadId())return;
    sessionThread=GetCurrentThreadId();
    const auto outcome=NativePersistencePolicy::Hold(sessionLease,true,ReadStrictMode,
        [context](bool strict){WriteStrictMode(context,strict);});
    if(sessionOutcome==outcome)return;sessionOutcome=outcome;
    try{
        if(outcome==NativePersistencePolicy::Outcome::Applied||outcome==NativePersistencePolicy::Outcome::AlreadyTolerant)
            Log<RC::LogLevel::Normal>(STR("[SAVE-ENTRY][SESSION-POLICY][TOLERANT] dom.PersistenceStrictMode=0 verified; retained across loads for this session. Pruning remains independent; native acceptance follows Save Bypass. No save repair or world-entry result is implied.\n"));
        else Log<RC::LogLevel::Warning>(STR("[SAVE-ENTRY][SESSION-POLICY][UNAVAILABLE] Native tolerant mode could not be verified. Loading and pruning continue without a success claim.\n"));
    }catch(...){}
}
void ReleaseSessionPolicy(UObject* context) noexcept{
    // Reflected console writes must stay on the owning game thread, even at shutdown.
    if(sessionLease.RestoreNeeded&&sessionThread!=GetCurrentThreadId()){
        try{Log<RC::LogLevel::Warning>(STR("[SAVE-ENTRY][SESSION-POLICY][RESTORE-UNVERIFIED] Shutdown is not on the policy's game thread; native strictness was not changed.\n"));}catch(...){}
        return;
    }
    const auto result=NativePersistencePolicy::Release(sessionLease,ReadStrictMode,
        [context](bool strict){WriteStrictMode(context,strict);});
    try{if(result==NativePersistencePolicy::Outcome::RestoreFailed)
        Log<RC::LogLevel::Warning>(STR("[SAVE-ENTRY][SESSION-POLICY][RESTORE-UNVERIFIED] Original strict mode could not be verified during shutdown.\n"));}catch(...){}
    sessionOutcome.reset();sessionThread=0;
}
}
