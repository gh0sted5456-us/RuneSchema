#pragma once
#include "SDK/WeakObjectHandle.h"

#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/Blueprint/DragonWildsBlueprintMod.h"
#include "Unreal/NameTypes.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UObjectArray.hpp"
#include "safetyhook.hpp"
#include "Loader/Spawn/GhostMaterials.h"
#include <functional>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace UECustom {
    class UBlueprintGeneratedClass;
}

namespace DragonWilds {
    struct RuntimeWidgetRule {
        RC::Unreal::FName OwnerClass;
        RC::StringType WidgetPath;
        nlohmann::json Data;
        RC::StringType ModName;
    };

    class DragonWildsBlueprintModLoader : public DragonWildsModLoaderBase {
    public:
        DragonWildsBlueprintModLoader();

        ~DragonWildsBlueprintModLoader();

        // Share the PostInitializeComponents detour with spawn modifiers.
        static void SetActorInitializedObserver(
            std::function<void(RC::Unreal::AActor*)> observer);

        // Additional runtime systems can observe actor initialization without
        // replacing the spawn loader's existing observer.
        static uint64_t RegisterActorInitializedObserver(
            std::function<void(RC::Unreal::AActor*)> observer);
        static void UnregisterActorInitializedObserver(uint64_t observerId);
    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
        void OnFinalizeLoad(const EEngineLifecyclePhase& phase) override final;
    private:
        std::unordered_map<RC::Unreal::FName, std::vector<DragonWildsBlueprintMod>> m_modsMap;
        std::vector<DragonWildsBlueprintMod> m_blueprintPatches;
        std::vector<nlohmann::json> m_pendingBlueprintPatches;
        std::vector<nlohmann::json> m_pathBlueprintPatches;
        std::vector<RuntimeWidgetRule> m_runtimeWidgetRules;
        std::unordered_set<std::string> m_reportedRuntimeWidgetFailures;
        std::unordered_set<std::string> m_runtimeWidgetActiveRules;
        std::unordered_set<std::string> m_runtimeWidgetCompletedRules;
        std::vector<PS::WeakObjectHandle> m_ghostRoots;
        std::unordered_map<std::string, GhostMaterials::Set> m_ghostMaterials;
        RC::Unreal::Hook::GlobalCallbackId m_worldTeardownCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_runtimeWidgetCallbackId = RC::Unreal::Hook::ERROR_ID;
        void ApplyBlueprintVisualEffect(RC::Unreal::AActor* actor);
        void ClearWorldVisualEffects();
        void ClearRuntimeWidgetState();
        void ApplyDeferredPatches(RC::Unreal::UObject* object);
        void RegisterRuntimeWidgetRules(
            const std::string& identity,
            const nlohmann::json& runtimeWidgets,
            const RC::StringType& modName);
        void ObserveRuntimeWidgetEvent(
            RC::Unreal::UObject* source,
            RC::Unreal::UFunction* function);
        RC::Unreal::UObject* ResolveRuntimeWidgetPath(
            RC::Unreal::UObject* owner,
            const RC::StringType& widgetPath);
        bool RuntimeWidgetPathContains(
            RC::Unreal::UObject* owner,
            RC::Unreal::UObject* candidate,
            const RC::StringType& widgetPath);
        RC::Unreal::UObject* FindRuntimeWidgetOwner(
            RC::Unreal::UObject* source,
            const RC::Unreal::FName& ownerClass);
        RC::Unreal::UObject* ResolveRuntimeWidgetCallTarget(
            RC::Unreal::UObject* owner,
            RC::Unreal::UObject* widget,
            const std::string& targetPath);
        bool RuntimeWidgetRuleMatchesEvent(
            const RuntimeWidgetRule& rule,
            RC::Unreal::UFunction* function);
        std::string RuntimeWidgetRuleKey(
            RC::Unreal::UObject* owner,
            const RuntimeWidgetRule& rule) const;
        void ApplyRuntimeWidgetRule(
            RC::Unreal::UObject* owner,
            const RuntimeWidgetRule& rule);
        void ApplyRuntimeWidgetCalls(
            RC::Unreal::UObject* owner,
            RC::Unreal::UObject* widget,
            const RuntimeWidgetRule& rule);
        void ApplyRuntimeWidgetActivation(
            RC::Unreal::UObject* widget,
            const RuntimeWidgetRule& rule);
        void ApplyRuntimeWidgetBinding(
            RC::Unreal::UObject* owner,
            RC::Unreal::UObject* widget,
            const RuntimeWidgetRule& rule);

        bool HookPostLoad();
        bool HookPostInitComponents();
        void ResetHooks();

        void LoadSafe(const nlohmann::json& data, const RC::StringType& modName);

        void LoadUnsafe(const nlohmann::json& data);

        void ModifyObject(RC::Unreal::UObject* object);

        void ApplyMod(const DragonWildsBlueprintMod& mod, RC::Unreal::UObject* object);

        void ApplyData(const nlohmann::json& data, RC::Unreal::UObject* object, bool resolveWidgetTemplates = false);

        RC::Unreal::UObject* FindWidgetTemplate(RC::Unreal::UClass* objectClass, const RC::StringType& widgetName);

        void HandleInheritableComponent(UECustom::UBlueprintGeneratedClass* bpClass, const RC::StringType& componentName, const nlohmann::json& componentData);

        void HandleNodeComponent(UECustom::UBlueprintGeneratedClass* bpClass, const RC::StringType& componentName, const nlohmann::json& componentData);

        void ModifyComponent(RC::Unreal::UObject* component, const nlohmann::json& componentData);
    private:
        static inline SafetyHookInline PostLoadHook;
        static inline std::atomic<bool> HooksReady{false};
        static inline std::function<void(RC::Unreal::UClass*)> PostLoadCallback = nullptr;
        static void PostLoad(RC::Unreal::UClass* self);

        static inline SafetyHookInline PostInitComponentsHook;
        static inline std::function<void(RC::Unreal::AActor*)> PostInitComponentsCallback = nullptr;
        static inline std::function<void(RC::Unreal::AActor*)> ActorInitializedObserver = nullptr;
        static inline uint64_t NextActorObserverId = 1;
        static inline std::unordered_map<uint64_t, std::function<void(RC::Unreal::AActor*)>> ActorInitializedObservers;
        static void PostInitComponents(RC::Unreal::AActor* self);
    };
}
