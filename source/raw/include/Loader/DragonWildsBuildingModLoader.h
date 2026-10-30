#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Loader/DragonWildsModLoaderBase.h"
#include "SDK/WeakObjectHandle.h"
#include "Runtime/WorldBuildingRegistryValidation.h"
#include "Core/BuildingNetworkGuard.h"
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
        ~DragonWildsBuildingModLoader() override;
        void ActivateWorldRegistration();
        PS::BuildingNetworkGuard::Registry NetworkIdentity(RC::Unreal::UObject* context);
        nlohmann::json NetworkIdentityManifest();
        std::vector<std::string> PlanNetworkOrder(const std::vector<std::size_t>& order);
        void SetNetworkOrder(std::vector<std::string> order) noexcept;
        void ImportCookedDefinitions();
        std::vector<PS::WorldBuildingRegistryValidation::Expected> CleanupExpectations();
        void RejectCleanup(const std::string& reason);
        std::function<void(const nlohmann::json&,const RC::StringType&)> ImportPlacements;
        nlohmann::json PlanToolImport(const nlohmann::json& data);

    protected:
        void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName,
            const EEngineLifecyclePhase& engineLifecyclePhase) override;
        void OnAutoReload(const RC::StringType& modName,
            const std::filesystem::path& modFilePath) override;
        bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override;
        bool OnInitialize() override;
        void OnFinalizeLoad(const EEngineLifecyclePhase& phase) override;

    private:
        struct Placement {
            RC::Unreal::int32 PageIndex = 0;
            RC::StringType Collection;
        };

        struct BuildingDefinition {
            std::string SourceFile;
            RC::StringType Owner;
            RC::StringType Key;
            RC::StringType AssetPath;
            nlohmann::json Properties;
            nlohmann::json Overrides;
            nlohmann::json Requirements;
            std::vector<Placement> Targets;
            bool InheritSourcePlacement = true;
            bool Unlock = true;
            bool UnlockAuthored = false;
            bool Clone = false;
            bool Declared = false;
            std::string DeclaredPersistenceID;
            std::string DeclaredInternalName;
            bool DeclaredInternalNameAsserted = false;
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

        void ReadDefinitions(const nlohmann::json& data, const RC::StringType& modName, const std::string& sourceFile={}, nlohmann::json* plan=nullptr);
        void ApplyPatch(const nlohmann::json& patch, const RC::StringType& modName,
            const std::string& sourceFile);
        void ApplyDefinitions();
        RC::Unreal::UObject* LoadBuilding(
            const BuildingDefinition& definition, LoadResult& result,
            RC::Unreal::UObject** sourceOut = nullptr);
        bool ApplyProperties(RC::Unreal::UObject* building,
            const BuildingDefinition& definition, LoadResult& result);
        bool ValidateBuildableActor(RC::Unreal::UObject* source,
            const BuildingDefinition& definition);
        bool ApplyRequirements(RC::Unreal::UObject* building,
            const BuildingDefinition& definition);
        bool ApplyOverrides(RC::Unreal::UObject* building,
            const BuildingDefinition& definition, LoadResult& result);
        bool EnsureStabilityProfile(RC::Unreal::UObject* building,bool allowFallback=true);
        bool AddPersistenceIdentity(RC::Unreal::UObject* building);
        std::vector<Placement> FindSourcePlacements(RC::Unreal::UObject* source) const;
        bool AddToMenu(RC::Unreal::UObject* building, const Placement& placement);
        void DiscardUncommittedClone(RC::Unreal::UObject* building);
        void RegisterHooks();
        bool PrepareWorldState(RC::Unreal::UObject* worldContext);
        bool EnsureWorldState(RC::Unreal::UObject* worldContext);
        bool TryWorldRecovery(RC::Unreal::UObject* worldContext,
            const char* source);
        void ScheduleWorldRecovery(RC::Unreal::UObject* worldContext);
        void RetryWorldRecovery(float deltaSeconds);
        bool ProtectWorldRegistry(RC::Unreal::UObject* subsystem);
        bool RefreshBuildingReferencesForWorld();
        bool RefreshBuildingCatalogueForWorld();
        RC::Unreal::UObject* GetValidBuilding(const RC::StringType& identity);
        void RememberBuilding(const RC::StringType& identity, RC::Unreal::UObject* object);
        bool CaptureNativeRegistry(RC::Unreal::UObject* subsystem);
        bool RestoreNativeRegistry();
        void ClearWorldRegistryState();
        bool ApplyUnlocks(RC::Unreal::UObject* progressComponent);
        size_t ApplyUnlocksToWorld(
            RC::Unreal::UObject* worldContext = nullptr,
            bool reportDeferred = true);
        void NotifyBuildingUnlocks(RC::Unreal::UObject* progressComponent,
            const std::vector<RC::Unreal::UObject*>& buildings) const;
        RC::Unreal::UObject* FindBuildingSubsystem(
            RC::Unreal::UObject* worldContext = nullptr) const;
        RC::Unreal::UObject* LoadObject(const RC::StringType& path) const;
        RC::Unreal::UObject* CloneBuilding(RC::Unreal::UObject* source,
            const RC::StringType& owner, const RC::StringType& key);
        static RC::StringType Identity(
            const RC::StringType& owner, const RC::StringType& key);

        RC::Unreal::UClass* m_buildingPieceClass = nullptr;
        RC::Unreal::UClass* m_buildingPieceSubsystemClass = nullptr;
        RC::Unreal::UClass* m_progressComponentClass = nullptr;
        RC::Unreal::UObject* m_catalogue = nullptr;

        std::vector<BuildingDefinition> m_definitions;
        std::string m_cleanupDiscoveryFailure;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*> m_buildings;
        std::unordered_map<RC::StringType, int32_t> m_buildingIndices;
        std::unordered_map<RC::StringType, PS::WeakObjectHandle> m_buildingHandles;
        std::unordered_set<RC::StringType> m_applied;
        std::unordered_set<RC::StringType> m_unlocks;
        std::unordered_set<std::string> m_ownedProfileRows;
        std::vector<RC::Unreal::UObject*> m_createdBuildings;
        NativeRegistrySnapshot m_nativeRegistrySnapshot;
        PS::WeakObjectHandle m_networkWorld;
        PS::BuildingNetworkGuard::Registry m_networkIdentity;
        std::size_t m_networkNativeCount=0;
        std::vector<std::string> m_networkOrder;
        PS::WeakObjectHandle m_networkPreviousContext;
        bool m_hooksRegistered = false;
        bool m_cookedImported = false;
        RC::Unreal::Hook::GlobalCallbackId m_initGameStateCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_unlockGameStateCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_recoveryTickCallbackId = RC::Unreal::Hook::ERROR_ID;
        PS::WeakObjectHandle m_registeredWorldContext;
        PS::WeakObjectHandle m_pendingWorldContext;
        float m_recoveryElapsed = 0.0f;
        float m_recoveryInterval = 0.0f;
        bool m_worldRegistryReady = false;
        std::string m_lastRecoveryFailure;
    };
}
