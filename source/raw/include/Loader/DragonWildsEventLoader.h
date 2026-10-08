#pragma once
#include "Loader/DragonWildsNpcLoader.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Utility/NativeFunctionHook.h"
namespace DragonWilds {
class DragonWildsEventLoader final:public DragonWildsModLoaderBase {
    DragonWildsNpcLoader* npcs;
    RC::Unreal::Hook::GlobalCallbackId observer=RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::UFunction* deathFunction=nullptr;
    RC::Unreal::UFunction* despawnFunction=nullptr;
    RC::Unreal::CallbackId deathHook=0,despawnHook=0;
    void RemoveHooks() {
        if(deathFunction && deathHook)deathFunction->UnregisterHook(deathHook);
        if(despawnFunction && despawnHook)despawnFunction->UnregisterHook(despawnHook);
        deathHook=despawnHook=0;
        if(observer!=RC::Unreal::Hook::ERROR_ID)RC::Unreal::Hook::UnregisterCallback(observer);
        observer=RC::Unreal::Hook::ERROR_ID;
    }
    static RC::Unreal::UFunction* NativeSignal(const TCHAR* path) {
        using namespace RC::Unreal;
        auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
        if(!fn || !(fn->GetFunctionFlags() & FUNC_Native) || fn->GetParmsSize()!=0 || fn->GetReturnProperty())throw std::runtime_error("Event native signal layout unavailable");
        for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))throw std::runtime_error("Event native signal has unexpected arguments");
        return fn;
    }
public:
    explicit DragonWildsEventLoader(DragonWildsNpcLoader* service):DragonWildsModLoaderBase("events"),npcs(service){SetDisplayName(TEXT("Event Loader"));}
    ~DragonWildsEventLoader()override {RemoveHooks();}
protected:
    bool CanInitialize(const EEngineLifecyclePhase& phase)override{return phase==EEngineLifecyclePhase::PostEngineInit;}
    bool OnInitialize()override {
        if(!npcs || !npcs->HasInitialized())return false;
        RC::Unreal::Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("EventDespawnGuard");
        observer=RC::Unreal::Hook::RegisterProcessEventPreCallback([this](auto&,auto* source,auto* function,void*) {
            try{npcs->ObserveEventDespawn(source,function);}catch(...){}
        },options);
        try {
            if(observer==RC::Unreal::Hook::ERROR_ID)throw std::runtime_error("Event despawn observer registration failed");
            deathFunction=NativeSignal(TEXT("/Script/Dominion.DeathComponent:OnHandleDeath"));
            despawnFunction=NativeSignal(TEXT("/Script/Dominion.AiDeathComponent:Despawn"));
            deathHook=PS::RegisterNativePostHook(deathFunction,[this](RC::Unreal::UnrealScriptFunctionCallableContext& context,void*) {
                try{npcs->ObserveEventDeath(context.Context,deathFunction);}catch(...){}
            });
            despawnHook=PS::RegisterNativePreHook(despawnFunction,[this](RC::Unreal::UnrealScriptFunctionCallableContext& context,void*) {
                try{npcs->ObserveEventDespawn(context.Context,despawnFunction);}catch(...){}
            });
            if(!deathHook || !despawnHook)throw std::runtime_error("Event native hook registration failed");
            return true;
        }catch(const std::exception& error){
            RemoveHooks();
            PS::Log<RC::LogLevel::Error>(TEXT("Events disabled: {}.\n"),PS::ToWideSafe(error.what()));
            return false;
        }
    }
    void OnLoad(const std::filesystem::path& path,const RC::StringType& mod,const EEngineLifecyclePhase& phase)override {
        if(phase==EEngineLifecyclePhase::PostEngineInit)PS::JsonHelpers::ParseJsonFilesInPath(path,[&](const nlohmann::json& data){npcs->LoadEvents(data,mod);});
    }
    void OnAutoReload(const RC::StringType& mod,const std::filesystem::path&)override {
        PS::Log<RC::LogLevel::Warning>(TEXT("Event changes in {} require a restart.\n"),mod);
    }
};
}
