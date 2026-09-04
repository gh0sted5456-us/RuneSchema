#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Unreal/NameTypes.hpp"

namespace RC::Unreal {
    class UObject;
    class UClass;
    class FString;
}

namespace DragonWilds {
    class DragonWildsDataRegistrar {
    public:
        void Initialize();

    private:
        std::vector<std::pair<RC::Unreal::UClass*, RC::Unreal::UClass*>> m_bindings;
        bool m_initialized = false;
        bool m_savesCleaned = false;

        bool ResolveBindings();
        void InstallHooks();
        void RegisterAll();
        void RegisterMissing(RC::Unreal::UClass* dataClass, RC::Unreal::UObject* subsystem);
        int32_t EnsureNetworkIdentity(
            RC::Unreal::UObject* dataAsset, RC::Unreal::UObject* subsystem);
        RC::Unreal::UObject* FindSubsystemInstance(RC::Unreal::UClass* subsystemClass);
        bool InsertIntoMap(RC::Unreal::UObject* subsystem, const RC::StringType& mapName,
            const RC::Unreal::FString& key, RC::Unreal::UObject* value);

        void CleanSaves();
        bool ReadKnownIds(RC::Unreal::UClass* subsystemClass, std::unordered_set<std::string>& outIds);
        bool CleanCharacterSave(const std::filesystem::path& savePath,
            const std::unordered_set<std::string>& knownItems, const std::unordered_set<std::string>& knownRecipes);
    };
}
