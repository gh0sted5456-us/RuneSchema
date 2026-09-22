#pragma once

#include <unordered_set>
#include <set>
#include <mutex>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <functional>
#include "SDK/WeakObjectHandle.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/Rotator.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Structs/FBox.h"
#include "nlohmann/json.hpp"
#include "Loader/EventDefinition.h"
#include "Loader/SpawnRadius.h"
#include "Loader/TimeOfDay.h"

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
            nlohmann::json AdditionalDrops=nlohmann::json::array();
            RC::Unreal::FVector Location{};
            RC::Unreal::FVector AuthoredLocation{};
            RC::Unreal::FRotator Rotation{};
            RC::Unreal::FVector Scale{1.0, 1.0, 1.0};
            double DropMultiplier = 1.0;
            double HealthMultiplier = 1.0;
            double DamageMultiplier = 1.0;
            float RemoveRadius = 500.0f;
            nlohmann::json Properties;
            nlohmann::json AuthoredDefinition;
            nlohmann::json CharacterProperties;
            nlohmann::json ComponentProperties;
            nlohmann::json VisualEffect;
            std::string PersistentPlacementKey;
            std::string BuildingDataPath;
            std::string BuildingObjectProperty;
            bool bHasBuildingDataIndex = false;
            int64_t BuildingDataIndex = 0;
            TimeOfDay::Requirement Time = TimeOfDay::Requirement::Any;
            std::string QuestCompleted;
            bool bPersistAfterCondition = true;
            bool bConditionLatched = false;
            bool bSatisfiedByExisting = false;
            double DuplicateRadius = 100.0;
            PS::WeakObjectHandle LiveActor;
            bool bUseNativeRespawn = false;
            bool bBuildingProp = false;
            bool bAllowDeconstruction = false;
            bool bCellActivated = false;
            bool bTimeAllowed = false;
            bool bDeconstructed = false;
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
        std::function<void(RC::Unreal::AActor*,const std::string&)> PublishEventIdentity;
        std::function<void(RC::Unreal::AActor*,const std::string&)> PresentEventState;
        std::function<bool(const std::string&,const std::string&)> ForwardHelpyAuthority;
        std::function<bool(RC::Unreal::UWorld*,const std::string&)> IsQuestCompleted;
        std::string HandleNetworkHelpyAuthority(RC::Unreal::UObject* player,const std::string& action,const std::string& payload);
        bool PresentEventIdentity(RC::Unreal::AActor* actor,const std::string& payload);
        nlohmann::json EventSpawnManifest(const std::string& key) const;
        void ValidateEventSpawn(const std::string& key);
        RC::Unreal::AActor* SpawnEventAI(const std::string& key,RC::Unreal::UWorld* world,const RC::Unreal::FVector& position,bool tool=false,double yaw=0,const std::string& eventKey={},int toolPowerLevel=-1);
        void PumpItemIcons();
        void PumpSpawnTools();
        std::vector<PS::WeakObjectHandle> m_toolActors;
        std::set<std::filesystem::path> m_toolSpawnFiles;
        std::mutex m_toolSpawnFileMutex;
        unsigned long long m_toolSequence=0;
        unsigned long long m_networkToolSequence=0;
        nlohmann::json m_toolPlacements=nlohmann::json::array();
        DragonWildsSpawnLoader();

        ~DragonWildsSpawnLoader() override;

        void LoadPlayerRules(const std::filesystem::path& loaderPath,
            const RC::StringType& modName, bool replaceExisting = false);
        void LoadNameplateDefinitions(const std::filesystem::path& loaderPath,
            const RC::StringType& modName, bool replaceExisting = false);
        void FinalizeNameplateDefinitions();
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
        struct BonusRow { PS::WeakObjectHandle Table; RC::StringType Name; void* Data; bool Rooted; };
        std::vector<BonusRow> m_bonusRows;
        std::map<std::string,RC::StringType> m_bonusRowCache;
        std::map<RC::Unreal::UObject*,PS::WeakObjectHandle> m_bonusApplied;
        struct BonusHandle { PS::WeakObjectHandle Component; RC::StringType Original, Applied; };
        std::vector<BonusHandle> m_bonusHandles;
        void ApplyAdditionalDrops(RC::Unreal::UObject* actor,const nlohmann::json& drops);
        void ClearBonusRows();
        std::map<std::string,Events::SpawnTemplate> m_eventTemplates;
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

        struct PlayerNameplateStateRule {
            std::string GameplayEffect;
            bool Configured = false;
            std::string Icon;
            double Scale = 1.0;
            bool ScaleConfigured = false;
            double InactivitySeconds = 0.0;
            int Priority = 0;
            nlohmann::json ComponentProperties = nlohmann::json::object();
            nlohmann::json WidgetProperties = nlohmann::json::object();
            nlohmann::json TextProperties = nlohmann::json::object();
        };

        struct PlayerNameplateRule {
            bool Configured = false;
            std::string ArchetypeName;
            std::string Mode = "Name";
            std::string Icon;
            double Scale = 1.0;
            double PixelWidth = 64.0;
            double PixelHeight = 64.0;
            bool PixelSizeConfigured = false;
            double Distance = 2500.0;
            double ActivityTimeoutSeconds = 2.0;
            bool ShowSelf = false;
            bool ShowOthers = true;
            std::unordered_map<std::string, PlayerNameplateStateRule> States;
            nlohmann::json Events = nlohmann::json::array();
            nlohmann::json SkillXP = nlohmann::json::object();
            nlohmann::json ComponentProperties = nlohmann::json::object();
            nlohmann::json WidgetProperties = nlohmann::json::object();
            nlohmann::json TextProperties = nlohmann::json::object();
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
            PlayerNameplateRule Nameplate;
            nlohmann::json MapIcon;
            nlohmann::json VisualEffect;
            nlohmann::json PawnProperties = nlohmann::json::object();
            nlohmann::json ComponentProperties = nlohmann::json::object();
            std::string VisualEffectTrigger = "Load";
            double VisualEffectSeconds = 0.0;
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
            PS::WeakObjectHandle Actor;
            RC::Unreal::FGuid SpawnId{};
            std::shared_ptr<SpawnInfo> EventSpawn;
        };
        SpawnInfo* ResolveAIBinding(const LiveAIBinding& binding);

        std::vector<SpawnInfo> m_spawns;
        struct OwnedJsonDocument { RC::StringType ModName; nlohmann::json Document; std::string SourceFile; };
        std::vector<OwnedJsonDocument> m_spawnDocuments;
        std::vector<OwnedJsonDocument> m_playerDocuments;
        std::vector<OwnedJsonDocument> m_nameplateDocuments;
        std::unordered_map<std::string,nlohmann::json> m_nameplateDefinitions;
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
        RC::Unreal::UFunction* m_playerPossessionAckFunction = nullptr;
        int32_t m_playerPossessionAckCallbackId = 0;
        RC::Unreal::UFunction* m_playerPawnStateFunction = nullptr;
        int32_t m_playerPawnStateCallbackId = 0;
        RC::Unreal::UFunction* m_playerTagsChangedFunction = nullptr;
        int32_t m_playerTagsChangedCallbackId = 0;
        RC::Unreal::UFunction* m_playerDamageReceivedFunction = nullptr;
        int32_t m_playerDamageReceivedCallbackId = 0;
        struct PlayerActivityHook {
            RC::Unreal::UFunction* Function = nullptr;
            int32_t CallbackId = 0;
        };
        std::vector<PlayerActivityHook> m_playerActivityHooks;
        std::unordered_set<std::string> m_playerNativeActivityPaths;
        std::unordered_set<std::string> m_playerActivityAttemptedFunctions;
        // Explicit Blueprint event paths are observed through the existing
        // ProcessEvent callback. Names are a cheap first filter; the full
        // path is checked before any parameters are copied.
        std::unordered_set<std::string> m_playerProcessEventActivityPaths;
        std::unordered_set<RC::StringType> m_playerProcessEventActivityNames;
        RC::Unreal::Hook::GlobalCallbackId m_spawnTickCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_worldTeardownCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_protectedBuildingDestroyGuard = RC::Unreal::Hook::ERROR_ID;
        bool m_allowManagedBuildingDestroy = false;
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
        struct AppliedVisual {
            PS::WeakObjectHandle Actor;
            PS::WeakObjectHandle Component;
            std::string Signature;
            AppliedVisual(PS::WeakObjectHandle actor,std::string signature)
                :Actor(actor),Signature(std::move(signature)) {}
            AppliedVisual(PS::WeakObjectHandle actor,PS::WeakObjectHandle component,
                std::string signature):Actor(actor),Component(component),Signature(std::move(signature)) {}
        };
        struct ActiveNameplateState {
            PS::WeakObjectHandle Actor;
            std::string State;
            double RemainingSeconds = 0.0;
            bool Latched = false;
            std::string SourceFunction;
        };
        std::unordered_map<RC::Unreal::UObject*, AppliedVisual> m_visualEffectAppliedActors;
        std::unordered_map<RC::Unreal::UObject*, AppliedVisual> m_nameplateAppliedActors;
        std::unordered_map<RC::Unreal::UObject*, ActiveNameplateState> m_activeNameplateStates;
        double m_nameplateRefreshElapsed = 0.0;
        double m_visualTimerElapsed = 0.0;
        double m_buildingTimeElapsed = 0.0;
        RC::Unreal::Hook::GlobalCallbackId m_activityObserver = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_respawnObserver = RC::Unreal::Hook::ERROR_ID;
        struct PendingRespawn {
            PS::WeakObjectHandle Pawn;
            PS::WeakObjectHandle PlayerState;
            int32_t PlayerId = -1;
            double Seconds = 30.0;
            bool Confirmed = false;
            bool FinishObserved = false;
        };
        std::vector<PendingRespawn> m_pendingRespawns;
        struct ObservedActivity {
            PS::WeakObjectHandle Pawn;
            std::string Function, State;
            nlohmann::json Parameters;
        };
        std::vector<ObservedActivity> m_observedActivities;
        std::unordered_map<std::string,GhostMaterials::Set> m_sharedSpawnVisuals;
        std::vector<PS::WeakObjectHandle> m_rootedVisualEffectMaterials;
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
        void RegisterBuildingProp(SpawnInfo& spawn, const nlohmann::json& value);
        void RegisterRemoveActor(SpawnInfo& spawn, const nlohmann::json& value);
        void ApplyBuildingData(RC::Unreal::AActor* actor,const SpawnInfo& spawn);
        std::pair<bool,std::string> VerifyToolBuildingInstance(RC::Unreal::AActor* actor,RC::Unreal::UObject* building,const SpawnInfo& spawn,RC::Unreal::UWorld* world,bool requireTransient) const;
        std::pair<bool,std::string> VerifyToolBuildingCandidate(RC::Unreal::UObject* building,const SpawnInfo& spawn,RC::Unreal::UWorld* world) const;
        void ReconcileTimedBuildingProps(float deltaSeconds);
        RC::Unreal::UClass* ResolveClass(const RC::StringType& classPath);
        void DumpAIClasses();

        bool SetupWorldReadyHook();
        void SetupAIScaleHook();
        void SetupAIBindingHooks();
        void SetupPlayerJoinHooks();
        void SetupPlayerActivityHooks();
        void ApplyAIScale(RC::Unreal::UObject* character);
        SpawnInfo* ResolveAISpawnForCharacter(RC::Unreal::UObject* character);
        RC::Unreal::UObject* ResolveAINameTextBlock(
            RC::Unreal::UObject* character, bool bossName = false);
        void OnHealthBarTextSet(RC::Unreal::UObject* textBlock);
        void ApplyAIDisplayName(RC::Unreal::UObject* character,
            const std::string& displayName,
            const std::string& bossName = {});
        void RetryPendingAINames(double deltaSeconds);
        bool ApplyAILootRow(RC::Unreal::UObject* character,
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
        bool SetupProtectedBuildingDestroyGuard();
        bool IsProtectedBuildingActor(RC::Unreal::AActor* actor) const;
        bool IsWorldStillLoaded(RC::Unreal::UWorld* world);
        void OnCellShown(RC::Unreal::UObject* cellObject);
        bool HasLiveSpawnedAI(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void DestroyLiveSpawnedAI(RC::Unreal::UWorld* world,const SpawnInfo& spawn);
        void RetireTimedActor(RC::Unreal::AActor* actor);
        void TryProcessSpawns(RC::Unreal::UWorld* world, const std::vector<UECustom::FBox>* bounds, const wchar_t* trigger);
        void ProcessSpawns(RC::Unreal::UWorld* world, const std::vector<UECustom::FBox>* bounds);
        void ProcessAISpawnPointEntry(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void ProcessActorEntry(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        bool HasEquivalentActorNear(RC::Unreal::UWorld* world,const SpawnInfo& spawn);
        void ProcessRemoveActorEntry(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void ResolveGroundedLocation(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void CleanupOrphanedActors(RC::Unreal::UWorld* world);
        RC::Unreal::AActor* FindActorByStableId(RC::Unreal::UWorld* world, const RC::Unreal::FGuid& stableId);
        void CreateSpawn(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void CreateActor(RC::Unreal::UWorld* world, SpawnInfo& spawn);
        void LoadNativeRespawnState();
        bool SaveNativeRespawnState(std::string& error);
        void ApplyPlayerRules();
        void ApplyClientPlayerVisualRules(RC::Unreal::UObject* pawn,
            bool nameplatesOnly = false, const std::string& eventFunction = {},
            const nlohmann::json* eventParameters = nullptr, const std::string& activityState = {});
        void ConfirmPendingRespawn(RC::Unreal::UObject* pawn);
        std::unordered_set<std::string> ActivePlayerEffectData(RC::Unreal::UObject* pawn);
        void RefreshPlayerNameplates(double deltaSeconds);
        void SetPlayerNameplateActivity(RC::Unreal::UObject* source,
            const std::string& state);
        RC::Unreal::UObject* ResolvePlayerPawnFromActivity(
            RC::Unreal::UObject* source);
        std::string ClassifyPlayerAttackActivity(RC::Unreal::UObject* source);
        std::string ClassifyPlayerSpellActivity(RC::Unreal::UObject* source);
        std::string ClassifyPlayerEmoteActivity(RC::Unreal::UObject* source);
        bool ApplyPlayerNameplate(RC::Unreal::UObject* pawn,
            const PlayerNameplateRule& rule, const RC::StringType& context);
        void ApplyPlayerMapIcon(RC::Unreal::UObject* pawn,
            const nlohmann::json& options, const RC::StringType& context);
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
