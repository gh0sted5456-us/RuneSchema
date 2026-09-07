#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "Unreal/FWeakObjectPtr.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/Rotator.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Structs/FBox.h"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class AActor;
    class FArrayProperty;
    class FNumericProperty;
    class UClass;
    class UFunction;
    class UObject;
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
            RC::StringType AIClassPath;
            RC::StringType EntryId;
            std::string DisplayName;
            std::string BossName;
            std::string LootRow;
            RC::Unreal::FVector Location{};
            RC::Unreal::FVector AuthoredLocation{};
            RC::Unreal::FRotator Rotation{};
            RC::Unreal::FVector Scale{1.0, 1.0, 1.0};
            double DropMultiplier = 1.0;
            double HealthMultiplier = 1.0;
            double DamageMultiplier = 1.0;
            float RemoveRadius = 500.0f;
            nlohmann::json Properties;
            nlohmann::json CharacterProperties;
            nlohmann::json ComponentProperties;
            nlohmann::json VisualEffect;
            std::string PersistentPlacementKey;
            bool bUseNativeRespawn = false;
            bool bExistsInWorld = false;
            bool bSpawnFailed = false;
            bool bGroundToSurface = false;
            bool bGroundingResolved = true;
            double GroundZOffset = 0.0;
            double GroundTraceAbove = 5000.0;
            double GroundTraceBelow = 10000.0;
            RC::Unreal::FGuid StableId{};
            RC::Unreal::FGuid LegacyId{};
        };

    public:
        DragonWildsSpawnLoader();

        ~DragonWildsSpawnLoader() override;

        void LoadPlayerRules(const std::filesystem::path& loaderPath,
            const RC::StringType& modName, bool replaceExisting = false);
        void FinalizePlayerRules();
        void ClearAppearanceSources();
        void RegisterAppearanceSource(const std::filesystem::path& modPath,
            const RC::StringType& modName);

    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
        void OnFinalizeLoad(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
    private:
        struct PlayerNumericBaseline {
            RC::Unreal::UObject* Object = nullptr;
            RC::Unreal::FNumericProperty* Property = nullptr;
            double Value = 0.0;
            bool Inverse = false;
        };

        struct PlayerAttributeBaseline {
            RC::Unreal::UObject* Attributes = nullptr;
            RC::Unreal::FArrayProperty* ValuesProperty = nullptr;
            RC::Unreal::UObject* Attribute = nullptr;
            int32_t Index = -1;
            double Value = 0.0;
            bool Valid = false;
        };

        struct NamedPlayerAttributeBaseline {
            std::string Identifier;
            PlayerAttributeBaseline Baseline;
        };

        struct PlayerAttributeMultiplier {
            std::string Identifier;
            double Multiplier = 1.0;
        };

        enum class EPlayerAttributeEditOperation { Set, Add, Multiply };

        struct PlayerAttributeEdit {
            std::string Identifier;
            EPlayerAttributeEditOperation Operation = EPlayerAttributeEditOperation::Set;
            double Value = 0.0;
        };

        struct PlayerAppearanceSelection {
            std::string Field;
            std::string DataTablePath;
            std::string RowName;
            std::string Source;
            std::string FallbackDataTablePath;
            std::string FallbackRowName;
            bool HasFallback = false;
        };

        struct AppearanceSource {
            std::unordered_map<std::string, std::string> Tables;
            std::unordered_map<std::string, std::string> FallbackRows;
        };

        struct AppearanceProvenance {
            std::string PlayerGuid;
            std::string Field;
            std::string OwnerMod;
            std::string Source;
            std::string AppliedDataTablePath;
            std::string AppliedRowName;
            std::string FallbackDataTablePath;
            std::string FallbackRowName;
        };

        struct PlayerAdjustmentState {
            RC::Unreal::UObject* Pawn = nullptr;
            RC::Unreal::FVector BaseScale{1.0, 1.0, 1.0};
            double BaseMaxHealth = 0.0;
            bool HasBaseHealth = false;
            RC::Unreal::UObject* StaminaAttributes = nullptr;
            RC::Unreal::FArrayProperty* StaminaValuesProperty = nullptr;
            RC::Unreal::UObject* MaxStaminaAttribute = nullptr;
            int32_t MaxStaminaIndex = -1;
            double BaseMaxStamina = 0.0;
            bool HasBaseStamina = false;
            PlayerAttributeBaseline HealthAttribute;
            PlayerAttributeBaseline DamageAttribute;
            PlayerAttributeBaseline DefenseAttribute;
            PlayerAttributeBaseline RunSpeedAttribute;
            PlayerAttributeBaseline CarryWeightAttribute;
            PlayerAttributeBaseline PoisonResistanceAttribute;
            PlayerAttributeBaseline StaminaRecoveryAttribute;
            PlayerAttributeBaseline PhysicalAttackAttribute;
            PlayerAttributeBaseline MagicalAttackAttribute;
            PlayerAttributeBaseline RangedAttackAttribute;
            PlayerAttributeBaseline PhysicalDefenseAttribute;
            PlayerAttributeBaseline MagicalDefenseAttribute;
            PlayerAttributeBaseline RangedDefenseAttribute;
            std::vector<NamedPlayerAttributeBaseline> NamedAttributes;
            std::vector<PlayerNumericBaseline> WalkSpeedFields;
            std::vector<PlayerNumericBaseline> RunSpeedFields;
            std::vector<PlayerNumericBaseline> DamageFields;
            std::vector<PlayerNumericBaseline> DefenseFields;
        };

        struct PlayerRule {
            RC::StringType ModName;
            std::vector<std::string> PlayerNames;
            std::vector<std::string> PlayerGuids;
            std::vector<std::size_t> PlayerLoadSlots;
            bool AllPlayers = false;
            bool SetScale = false;
            bool SetHealth = false;
            bool SetMaxHealth = false;
            bool SetDefense = false;
            bool SetDamage = false;
            bool SetStamina = false;
            bool SetMaxStamina = false;
            bool SetWalkSpeed = false;
            bool SetRunSpeed = false;
            bool SetCarryWeight = false;
            bool SetMaxCarryWeight = false;
            bool SetPoisonResistance = false;
            bool SetStaminaRecovery = false;
            bool SetPhysicalAttack = false;
            bool SetMagicalAttack = false;
            bool SetRangedAttack = false;
            bool SetPhysicalDefense = false;
            bool SetMagicalDefense = false;
            bool SetRangedDefense = false;
            std::vector<PlayerAttributeMultiplier> AttributeMultipliers;
            std::vector<PlayerAttributeEdit> Attributes;
            std::vector<PlayerAppearanceSelection> Appearance;
            nlohmann::json VisualEffect;
            double ScaleMultiplier = 1.0;
            double HealthMultiplier = 1.0;
            double MaxHealth = 100.0;
            double DefenseMultiplier = 1.0;
            double DamageMultiplier = 1.0;
            double StaminaMultiplier = 1.0;
            double MaxStamina = 100.0;
            double WalkSpeedMultiplier = 1.0;
            double RunSpeedMultiplier = 1.0;
            double CarryWeightMultiplier = 1.0;
            double MaxCarryWeight = 400.0;
            double PoisonResistanceMultiplier = 1.0;
            double StaminaRecoveryMultiplier = 1.0;
            double PhysicalAttackMultiplier = 1.0;
            double MagicalAttackMultiplier = 1.0;
            double RangedAttackMultiplier = 1.0;
            double PhysicalDefenseMultiplier = 1.0;
            double MagicalDefenseMultiplier = 1.0;
            double RangedDefenseMultiplier = 1.0;
        };

        struct PlayerLoadOrderEntry {
            std::string Key;
            std::string Name;
            std::string Guid;
        };

        struct LiveAIBinding {
            RC::Unreal::FWeakObjectPtr Actor;
            RC::Unreal::FGuid SpawnId{};
        };

        std::vector<SpawnInfo> m_spawns;
        struct OwnedJsonDocument { RC::StringType ModName; nlohmann::json Document; };
        std::vector<OwnedJsonDocument> m_spawnDocuments;
        std::vector<OwnedJsonDocument> m_playerDocuments;
        RC::Unreal::UWorld* m_readyWorld = nullptr;
        RC::Unreal::UWorld* m_pendingWorld = nullptr;
        std::vector<UECustom::FBox> m_pendingCellBounds;
        RC::Unreal::UFunction* m_onLevelShownFunction = nullptr;
        int32_t m_onLevelShownCallbackId = 0;
        RC::Unreal::UFunction* m_aiScaleFunction = nullptr;
        int32_t m_aiScaleCallbackId = 0;
        RC::Unreal::UFunction* m_healthBarSetTextFunction = nullptr;
        int32_t m_healthBarSetTextCallbackId = 0;
        RC::Unreal::UFunction* m_playerPostLoginFunction = nullptr;
        int32_t m_playerPostLoginCallbackId = 0;
        RC::Unreal::UFunction* m_playerClientRestartFunction = nullptr;
        int32_t m_playerClientRestartCallbackId = 0;
        RC::Unreal::UFunction* m_playerPawnStateFunction = nullptr;
        int32_t m_playerPawnStateCallbackId = 0;
        RC::Unreal::Hook::GlobalCallbackId m_spawnTickCallbackId = RC::Unreal::Hook::ERROR_ID;
        bool m_processingSpawns = false;
        std::unordered_set<RC::Unreal::UObject*> m_dropScaledActors;
        std::unordered_set<RC::Unreal::UObject*> m_lootRowConfiguredActors;
        std::unordered_set<RC::Unreal::UObject*> m_lootRowWarningActors;
        std::unordered_set<RC::Unreal::UObject*> m_combatScaledActors;
        std::unordered_set<RC::Unreal::UObject*> m_characterPropertiesAppliedActors;
        std::unordered_set<RC::Unreal::UObject*> m_customNamedAIActors;
        std::unordered_set<RC::Unreal::UObject*> m_customNameWarningActors;
        std::unordered_map<RC::Unreal::UObject*, int> m_aiNameRetryAttempts;
        std::vector<LiveAIBinding> m_liveAIBindings;
        double m_aiNameRetryAccumulator = 0.0;
        bool m_applyingHealthBarName = false;
        std::unordered_set<std::string> m_placedNativeRespawnActors;
        bool m_nativeRespawnStateLoaded = false;
        std::vector<PlayerRule> m_playerRules;
        std::vector<PlayerAdjustmentState> m_playerAdjustments;
        std::unordered_map<std::string, AppearanceSource> m_appearanceSources;
        std::vector<AppearanceProvenance> m_appearanceProvenance;
        bool m_appearanceProvenanceLoaded = false;
        std::unordered_set<std::string> m_reportedPlayerRuleFailures;
        std::unordered_set<std::string> m_reportedPlayerRuleApplications;
        std::unordered_set<std::string> m_reportedAppearanceNoOps;
        struct AppliedVisual { RC::Unreal::FWeakObjectPtr Actor; std::string Signature; };
        std::unordered_map<RC::Unreal::UObject*, AppliedVisual> m_visualEffectAppliedActors;
        std::unordered_map<std::string,GhostMaterials::Set> m_sharedSpawnVisuals;
        std::vector<RC::Unreal::UObject*> m_rootedVisualEffectMaterials;
        std::size_t m_reportedNewSpawns = 0;
        std::size_t m_reportedAlteredSpawns = 0;
        std::size_t m_reportedSpawnErrors = 0;

        void LoadSpawns(const nlohmann::json& data, const RC::StringType& modName);
        void ParsePlayerRulesDocument(const nlohmann::json& data,
            const RC::StringType& modName);
        void RegisterSpawn(const nlohmann::json& value, const RC::StringType& modName);
        void RegisterAISpawnPoint(SpawnInfo& spawn, const nlohmann::json& value);
        void RegisterAISpawnVariants(SpawnInfo& spawn, const nlohmann::json& variants, RC::Unreal::UClass* aiBaseClass);
        void RegisterActor(SpawnInfo& spawn, const nlohmann::json& value);
        void RegisterRemoveActor(SpawnInfo& spawn, const nlohmann::json& value);
        RC::Unreal::UClass* ResolveClass(const RC::StringType& classPath);
        void DumpAIClasses();

        bool SetupWorldReadyHook();
        void SetupAIScaleHook();
        void SetupAIBindingHooks();
        void SetupPlayerJoinHooks();
        void ApplyAIScale(RC::Unreal::UObject* character);
        SpawnInfo* ResolveAISpawnForCharacter(RC::Unreal::UObject* character);
        RC::Unreal::UObject* ResolveAINameTextBlock(
            RC::Unreal::UObject* character, bool bossName = false);
        void OnHealthBarTextSet(RC::Unreal::UObject* textBlock);
        void ApplyAIDisplayName(RC::Unreal::UObject* character,
            const std::string& displayName,
            const std::string& bossName = {});
        void RetryPendingAINames(double deltaSeconds);
        void ApplyAILootRow(RC::Unreal::UObject* character,
            const std::string& lootRow);
        void ApplyCombatMultipliers(RC::Unreal::UObject* character,
            double healthMultiplier, double damageMultiplier);
        void ApplyAIProperties(RC::Unreal::UObject* character,
            const nlohmann::json& characterProperties,
            const nlohmann::json& componentProperties);
        bool ApplyVisualEffect(RC::Unreal::UObject* actor,
            const nlohmann::json& visualEffect,
            const RC::StringType& context);
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
        void ResolveGroundedLocation(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void CleanupOrphanedActors(RC::Unreal::UWorld* world);
        RC::Unreal::AActor* FindActorByStableId(RC::Unreal::UWorld* world, const RC::Unreal::FGuid& stableId);
        void CreateSpawn(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void CreateActor(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void LoadNativeRespawnState();
        bool SaveNativeRespawnState(std::string& error);
        void ApplyPlayerRules();
        void ApplyClientPlayerVisualRules(RC::Unreal::UObject* pawn);
        bool AdjustRuntimePlayerRule(const std::string& targetPlayerName,
            const PlayerRule& rule, std::string& result,
            const std::string& targetPlayerGuid = {});
        RC::Unreal::UObject* FindPlayerControllerByName(
            const RC::StringType& playerName, bool& ambiguous);
        RC::Unreal::UObject* FindLocalPlayerController();
        RC::Unreal::UObject* FindPlayerControllerByGuid(
            const std::string& playerGuid, bool& ambiguous);
        std::string GetPlayerControllerName(RC::Unreal::UObject* controller);
        std::string GetPlayerCharacterGuid(RC::Unreal::UObject* controller);
        std::vector<std::string> GetConnectedPlayerNames();
        std::vector<PlayerLoadOrderEntry> GetConnectedPlayersInLoadOrder();
        void LoadAppearanceProvenance();
        bool SaveAppearanceProvenance(std::string& error);
        void ReconcileAppearanceFallbacks(
            const std::unordered_map<std::string, std::string>& activeOwners);
        bool ReadPlayerAppearance(RC::Unreal::UObject* pawn,
            const std::string& field, std::string& dataTablePath,
            std::string& rowName, RC::Unreal::UObject** customization,
            std::string& error);
        bool WritePlayerAppearance(RC::Unreal::UObject* pawn,
            const std::string& field, const std::string& dataTablePath,
            const std::string& rowName, bool& changed,
            RC::Unreal::UObject** customization, std::string& error);
        static std::filesystem::path GetAppearanceProvenancePath();
        static std::filesystem::path GetNativeRespawnStatePath();
    };
}
