#pragma once

#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/Blueprint/DragonWildsBlueprintMod.h"
#include "Unreal/NameTypes.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UObjectArray.hpp"
#include "safetyhook.hpp"
#include <unordered_map>
#include <vector>

namespace UECustom {
    class UBlueprintGeneratedClass;
}

namespace DragonWilds {
    class DragonWildsBlueprintModLoader : public DragonWildsModLoaderBase {
    public:
        DragonWildsBlueprintModLoader();

        ~DragonWildsBlueprintModLoader();
    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
    private:
        std::unordered_map<RC::Unreal::FName, std::vector<DragonWildsBlueprintMod>> m_modsMap;

        bool HookPostLoad();
        bool HookPostInitComponents();

        void LoadSafe(const nlohmann::json& data);

        void LoadUnsafe(const nlohmann::json& data);

        std::vector<DragonWildsBlueprintMod>& GetModsForBlueprint(const RC::Unreal::FName& name);

        void ModifyObject(RC::Unreal::UObject* object);

        void ApplyMod(const DragonWildsBlueprintMod& mod, RC::Unreal::UObject* object);

        void ApplyData(const nlohmann::json& data, RC::Unreal::UObject* object, bool resolveWidgetTemplates = false);

        RC::Unreal::UObject* FindWidgetTemplate(RC::Unreal::UClass* objectClass, const RC::StringType& widgetName);

        void HandleInheritableComponent(UECustom::UBlueprintGeneratedClass* bpClass, const RC::StringType& componentName, const nlohmann::json& componentData);

        void HandleNodeComponent(UECustom::UBlueprintGeneratedClass* bpClass, const RC::StringType& componentName, const nlohmann::json& componentData);

        void ModifyComponent(RC::Unreal::UObject* component, const nlohmann::json& componentData);
    private:
        static inline SafetyHookInline PostLoadHook;
        static inline std::function<void(RC::Unreal::UClass*)> PostLoadCallback = nullptr;
        static void PostLoad(RC::Unreal::UClass* self);

        static inline SafetyHookInline PostInitComponentsHook;
        static inline std::function<void(RC::Unreal::AActor*)> PostInitComponentsCallback = nullptr;
        static void PostInitComponents(RC::Unreal::AActor* self);
    };
}
