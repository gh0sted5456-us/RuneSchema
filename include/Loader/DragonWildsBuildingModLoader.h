#pragma once

#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class AGameModeBase;
    class UClass;
    class UObject;
}

namespace DragonWilds {
    class DragonWildsBuildingModLoader final : public DragonWildsModLoaderBase {
    public:
        DragonWildsBuildingModLoader();
        ~DragonWildsBuildingModLoader() override = default;
        void ActivateWorldRegistration();

    protected:
        void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName,
            const EEngineLifecyclePhase& engineLifecyclePhase) override;
        void OnAutoReload(const RC::StringType& modName,
            const std::filesystem::path& modFilePath) override;
        bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override;
        bool OnInitialize() override;

    private:
        struct Placement {
            RC::Unreal::int32 PageIndex = 0;
            RC::StringType Collection;
        };

        struct BuildingDefinition {
            RC::StringType Owner;
            RC::StringType Key;
            RC::StringType AssetPath;
            nlohmann::json Properties;
            nlohmann::json Requirements;
            Placement Target;
            bool Unlock = true;
        };

        struct LoadResult {
            int Loaded = 0;
            int Errors = 0;
        };

        struct RegistryStringMapEntry {
            RC::Unreal::FString Key;
            RC::Unreal::UObject* Value = nullptr;
        };

        struct NativeRegistrySnapshot {
            RC::Unreal::UObject* Subsystem = nullptr;
            std::vector<RC::Unreal::UObject*> NetIdToData;
            std::vector<std::pair<RC::Unreal::UObject*, RC::Unreal::uint16>> DataToNetIdMap;
            std::vector<RegistryStringMapEntry> PersistenceIDToDataMap;
            std::vector<RegistryStringMapEntry> InternalNameToDataMap;
            std::unordered_map<RC::Unreal::UObject*, RC::Unreal::int32> BuildingPieceDataIndices;
        };

        void ReadDefinitions(const nlohmann::json& data, const RC::StringType& modName);
        void ApplyDefinitions();
        RC::Unreal::UObject* LoadBuilding(
            const BuildingDefinition& definition, LoadResult& result);
        void ApplyProperties(RC::Unreal::UObject* building,
            const BuildingDefinition& definition, LoadResult& result);
        bool ApplyRequirements(RC::Unreal::UObject* building,
            const BuildingDefinition& definition);
        bool EnsureStabilityProfile(RC::Unreal::UObject* building);
        bool AddPersistenceIdentity(RC::Unreal::UObject* building);
        bool AddToMenu(RC::Unreal::UObject* building, const Placement& placement);
        void RegisterHooks();
        void PrepareWorldState(RC::Unreal::AGameModeBase* gameMode);
        bool ResolveWorldRegistryPath(RC::Unreal::AGameModeBase* gameMode);
        bool ProtectWorldRegistry(RC::Unreal::UObject* subsystem);
        RC::Unreal::UObject* CreateRetiredBuilding(
            const nlohmann::json& record, RC::Unreal::int32 historicalIndex);
        bool CaptureNativeRegistry(RC::Unreal::UObject* subsystem);
        bool RestoreNativeRegistry();
        void ClearWorldRegistryState();
        void ApplyUnlocks(RC::Unreal::UObject* progressComponent);
        RC::Unreal::UObject* FindProgressComponent() const;
        RC::Unreal::UObject* FindBuildingSubsystem(
            RC::Unreal::UObject* worldContext = nullptr) const;
        RC::Unreal::UObject* LoadObject(const RC::StringType& path) const;
        static RC::StringType Identity(
            const RC::StringType& owner, const RC::StringType& key);

        RC::Unreal::UClass* m_buildingPieceClass = nullptr;
        RC::Unreal::UClass* m_buildingPieceSubsystemClass = nullptr;
        RC::Unreal::UClass* m_progressComponentClass = nullptr;
        RC::Unreal::UObject* m_catalogue = nullptr;

        std::vector<BuildingDefinition> m_definitions;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*> m_buildings;
        std::unordered_set<RC::StringType> m_applied;
        std::unordered_set<RC::StringType> m_unlocks;
        std::vector<RC::Unreal::UObject*> m_retiredBuildings;
        NativeRegistrySnapshot m_nativeRegistrySnapshot;
        std::filesystem::path m_worldManifestPath;
        bool m_hooksRegistered = false;
    };
}
