#pragma once
#include "Loader/DragonWildsNpcLoader.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Unreal/Hooks.hpp"
#include "Runtime/NetworkContext.h"
#include "Runtime/NetworkRoleNotice.h"
namespace DragonWilds {
class DragonWildsQuestLoader final : public DragonWildsModLoaderBase {
    DragonWildsNpcLoader* npcs;
    RC::Unreal::Hook::GlobalCallbackId hook=RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId readyHook=RC::Unreal::Hook::ERROR_ID;
public:
    explicit DragonWildsQuestLoader(DragonWildsNpcLoader* service):DragonWildsModLoaderBase("quests"),npcs(service){SetDisplayName(TEXT("Quest Loader"));}
    ~DragonWildsQuestLoader() override {
        if(hook!=RC::Unreal::Hook::ERROR_ID)RC::Unreal::Hook::UnregisterCallback(hook);
        if(readyHook!=RC::Unreal::Hook::ERROR_ID)RC::Unreal::Hook::UnregisterCallback(readyHook);
    }
protected:
    bool CanInitialize(const EEngineLifecyclePhase& phase) override {return phase==EEngineLifecyclePhase::PostEngineInit;}
    bool OnInitialize() override {
        if(!npcs || !npcs->HasInitialized())return false;
        RC::Unreal::Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("QuestRegistryBeforeGameState");
        hook=RC::Unreal::Hook::RegisterInitGameStatePreCallback([this](auto&,auto* mode) {
            try {npcs->PrepareQuests(reinterpret_cast<RC::Unreal::UObject*>(mode));}
            catch(const std::exception& error){PS::Log<RC::LogLevel::Error>(STR("Quest registry preparation failed: {}\n"),PS::ToWideSafe(error.what()));}
        },options);
        options.HookName=TEXT("QuestRegistryGameStateReady");
        readyHook=RC::Unreal::Hook::RegisterInitGameStatePostCallback([this](auto&,auto* mode) {
            try {
                const auto network=PS::Network::Detect();
                if(!network.World || network.Mode==PS::Network::Role::Unknown
                    || !PS::Network::IsGameplayRoleWorld(RC::to_string(network.World->GetPathName())))return;
                npcs->PrepareQuests(reinterpret_cast<RC::Unreal::UObject*>(mode));
            }catch(const std::exception& error){PS::Log<RC::LogLevel::Error>(STR("World quest registry preparation failed: {}\n"),PS::ToWideSafe(error.what()));}
        },options);
        return hook!=RC::Unreal::Hook::ERROR_ID && readyHook!=RC::Unreal::Hook::ERROR_ID;
    }
    void OnLoad(const std::filesystem::path& path,const RC::StringType& mod,const EEngineLifecyclePhase& phase) override {
        if(phase==EEngineLifecyclePhase::PostEngineInit)
            PS::JsonHelpers::ParseJsonFilesInPath(path,[&](const nlohmann::json& data){npcs->LoadQuests(data,mod);});
    }
    void OnAutoReload(const RC::StringType& mod,const std::filesystem::path&) override {
        PS::Log<RC::LogLevel::Warning>(TEXT("Quest changes in {} require a restart.\n"),mod);
    }
};
}
