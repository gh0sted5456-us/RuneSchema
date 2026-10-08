#pragma once
#include "Runtime/NetworkContext.h"
#include "Runtime/NetworkRoleNotice.h"
#include "Utility/Logging.h"
#include "Unreal/Hooks.hpp"
namespace PS::Network {
class RoleMonitor {
    RoleNotice notices;
    RC::Unreal::Hook::GlobalCallbackId worldLoaded=RC::Unreal::Hook::ERROR_ID;
    void WorldLoaded() {
        Context context;
        RC::StringType world;
        try {
            context=Detect();
            if(!context.World || context.Mode==Role::Unknown)return;
            world=context.World->GetPathName();
            if(!IsGameplayRoleWorld(RC::to_string(world)))return;
        } catch(...) {return;}
        if(!notices.Observe(context.Mode,RC::to_string(world),true))return;
        RC::Output::send<RC::LogLevel::Normal>(STR("[RuneSchema] Network mode: {} | authority={} | local presentation={} | world={}\n"),
            PS::ToWideSafe(std::string(Label(context.Mode)).c_str()),
            OwnsGameplay(context.Mode)?STR("yes"):STR("no"),
            HasLocalPresentation(context.Mode)?STR("yes"):STR("no"),world);
    }
public:
    void Stop() {
        if(worldLoaded!=RC::Unreal::Hook::ERROR_ID)RC::Unreal::Hook::UnregisterCallback(worldLoaded);
        worldLoaded=RC::Unreal::Hook::ERROR_ID;
    }
    void Start() {
        using namespace RC::Unreal;
        if(worldLoaded!=Hook::ERROR_ID)return;
        Hook::FCallbackOptions options{};
        options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("NetworkRoleGameplayWorldLoaded");
        worldLoaded=Hook::RegisterInitGameStatePostCallback([this](Hook::TCallbackIterationData<void>&,AGameModeBase*) {
            WorldLoaded();
        },options);
        if(worldLoaded==Hook::ERROR_ID)PS::Log<RC::LogLevel::Warning>(STR("World-loaded network notice unavailable; gameplay role checks are unchanged.\n"));
    }
};
}
