#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "Unreal/Hooks.hpp"
#include "Unreal/Rotator.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "Loader/DragonWildsModLoaderBase.h"
#include "SDK/Structs/FBox.h"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class AActor;
    class UClass;
    class UFunction;
    class UWorld;
}

namespace UECustom {
    class UWorldPartitionRuntimeLevelStreamingCell;
}

namespace DragonWilds {
    class DragonWildsSpawnLoader : public DragonWildsModLoaderBase {
        enum class ESpawnEntryType {
            AISpawnPoint,
            Actor,
            RemoveActor,
        };

        struct SpawnInfo {
            ESpawnEntryType Type = ESpawnEntryType::AISpawnPoint;
            RC::StringType ModName;
            RC::StringType ClassPath;
            RC::StringType EntryId;
            RC::Unreal::FVector Location{};
            RC::Unreal::FRotator Rotation{};
            RC::Unreal::FVector Scale{1.0, 1.0, 1.0};
            double DropMultiplier = 1.0;
            float RemoveRadius = 500.0f;
            nlohmann::json Properties;
            bool bExistsInWorld = false;
            bool bSpawnFailed = false;
            RC::Unreal::FGuid StableId{};
            RC::Unreal::FGuid LegacyId{};
        };

    public:
        DragonWildsSpawnLoader();

        ~DragonWildsSpawnLoader() override;

        // /players is owned by the 0.6.2 spawn/runtime loader because it
        // already owns the world-ready hook and recurring engine tick.
        void LoadPlayerRules(const std::filesystem::path& loaderPath,
            const RC::StringType& modName, bool replaceExisting = false);

    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
    private:
        struct PlayerRule {
            RC::StringType ModName;
            std::vector<std::string> Names;
            std::vector<std::string> Guids;
            std::vector<std::size_t> LoadSlots;
            bool AllPlayers = false;
            nlohmann::json Adjustments;
        };

        std::vector<SpawnInfo> m_spawns;
        RC::Unreal::UWorld* m_readyWorld = nullptr;
        RC::Unreal::UWorld* m_pendingWorld = nullptr;
        std::vector<UECustom::FBox> m_pendingCellBounds;
        RC::Unreal::UFunction* m_onLevelShownFunction = nullptr;
        int32_t m_onLevelShownCallbackId = 0;
        RC::Unreal::UFunction* m_aiScaleFunction = nullptr;
        int32_t m_aiScaleCallbackId = 0;
        RC::Unreal::Hook::GlobalCallbackId m_spawnTickCallbackId = RC::Unreal::Hook::ERROR_ID;
        bool m_processingSpawns = false;
        std::unordered_set<RC::Unreal::UObject*> m_dropScaledActors;
        std::vector<PlayerRule> m_playerRules;
        std::unordered_map<RC::Unreal::UObject*, RC::Unreal::FVector> m_playerBaseScales;
        std::unordered_set<std::string> m_appliedPlayerRules;
        std::vector<std::string> m_playerLoadOrder;
        double m_playerRuleTickAccumulator = 0.0;

        void LoadSpawns(const nlohmann::json& data, const RC::StringType& modName);
        void RegisterSpawn(const nlohmann::json& value, const RC::StringType& modName);
        void RegisterAISpawnPoint(SpawnInfo& spawn, const nlohmann::json& value);
        void RegisterAISpawnVariants(SpawnInfo& spawn, const nlohmann::json& variants, RC::Unreal::UClass* aiBaseClass);
        void RegisterActor(SpawnInfo& spawn, const nlohmann::json& value);
        void RegisterRemoveActor(SpawnInfo& spawn, const nlohmann::json& value);
        RC::Unreal::UClass* ResolveClass(const RC::StringType& classPath);
        void DumpAIClasses();

        bool SetupWorldReadyHook();
        void SetupAIScaleHook();
        void ApplyAIScale(RC::Unreal::UObject* character);
        void ApplyDropMultiplier(RC::Unreal::UObject* actor, double multiplier);
        int ApplyDropMultiplierToObject(RC::Unreal::UObject* object, double multiplier);
        bool SetupSpawnTick();
        bool IsWorldStillLoaded(RC::Unreal::UWorld* world);
        void OnCellShown(RC::Unreal::UObject* cellObject);
        bool HasLiveSpawnedAI(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void TryProcessSpawns(RC::Unreal::UWorld* world, const std::vector<UECustom::FBox>* bounds, const wchar_t* trigger);
        void ProcessSpawns(RC::Unreal::UWorld* world, const std::vector<UECustom::FBox>* bounds);
        void ProcessAISpawnPointEntry(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void ProcessActorEntry(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void ProcessRemoveActorEntry(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void CleanupOrphanedActors(RC::Unreal::UWorld* world);
        RC::Unreal::AActor* FindActorByStableId(RC::Unreal::UWorld* world, const RC::Unreal::FGuid& stableId);
        void CreateSpawn(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void CreateActor(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void ApplyPlayerRules();
        bool ApplyPlayerRule(RC::Unreal::UObject* controller,
            const PlayerRule& rule, std::string& result);
        std::string GetPlayerName(RC::Unreal::UObject* controller) const;
        std::string GetPlayerGuid(RC::Unreal::UObject* controller) const;
    };
}
