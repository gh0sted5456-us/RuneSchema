#pragma once

#include <atomic>
#include "Generator/HelpyNodes.h"
#include <functional>
#include "Loader/QuestService.h"
#include "Loader/EventRuntime.h"
#include "Loader/QuestKillCredit.h"
#include "Loader/QuestAcquisition.h"
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/VendorSpawnGate.h"
#include "Loader/VendorPolicy.h"
#include "Loader/VendorIdentity.h"
#include "Loader/VendorTraceBudget.h"
#include "Loader/NpcCatalog.h"
#include "Loader/TimeOfDay.h"
#include "Loader/VendorCategoryGate.h"
#include "Loader/DialogueDefinition.h"
#include "Loader/DialogueProgress.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/Hooks.hpp"

namespace RC::Unreal {
    class AActor;
    class UClass;
    class UObject;
    class UDataTable;
    class UFunction;
    class UWorld;
    class UnrealScriptFunctionCallableContext;
}

namespace DragonWilds {
    class DragonWildsRecipeModLoader;
    // Owns neutral NPC actors and optional merchant attachments.
    class DragonWildsNpcLoader final : public DragonWildsModLoaderBase {
    public:
        explicit DragonWildsNpcLoader(DragonWildsRecipeModLoader* recipes);
        ~DragonWildsNpcLoader() override;
        void LoadVendors(const nlohmann::json& data, const RC::StringType& modName);
        void LoadDialogues(const nlohmann::json& data,const RC::StringType& modName);
        void LoadQuests(const nlohmann::json& data,const RC::StringType& mod){m_quests.Load(RC::to_string(mod),data);}
        Events::Runtime& EventService(){return m_events;}
        void PublishEventIdentity(RC::Unreal::AActor* actor,const std::string& payload);
        std::function<void(const std::string&,const std::string&)> PublishWorldState;
        std::function<bool(RC::Unreal::AActor*,const std::string&)> PresentEventIdentity;
        std::function<bool(RC::Unreal::UObject*,const std::string&)> OpenLore;
        void ObserveEventDespawn(RC::Unreal::UObject* source,RC::Unreal::UFunction* function);
        void ObserveEventDeath(RC::Unreal::UObject* source,RC::Unreal::UFunction* function);
        void ObserveClientEvent(RC::Unreal::AActor* actor,const std::string& eventKey){m_events.ObserveClientReplica(actor,eventKey);}
        void LoadEvents(const nlohmann::json& data,const RC::StringType& mod){m_events.Load(RC::to_string(mod),data);}
        void PrepareQuests(RC::Unreal::UObject* context){m_quests.Prepare(context);}
        std::string HandleNetworkQuestControl(RC::Unreal::UObject* player,const std::string& quest,
            const std::string& action,const std::string& payload);
        bool IsQuestCompleted(RC::Unreal::UWorld* world,const std::string& quest) const;
        void HandleNetworkNotification(RC::Unreal::UObject* player,const std::string& channel,
            const std::string& entity,const std::string& payload);
        void MigrateSavedProgress(RC::Unreal::UObject* controller);
        // Game-thread authoring facade. No second NPC manager or global actor scan.
        inline static DragonWildsNpcLoader* HelpyInstance=nullptr;
        nlohmann::json HelpyDefinitions();
        nlohmann::json SpawnHelpyNpc(const nlohmann::json& request,RC::Unreal::UWorld* world,RC::Unreal::AActor* player);
        std::size_t DismissHelpyNpcs(RC::Unreal::UWorld* world=nullptr);
        std::size_t HelpyTemporaryCount()const {return m_helpyNpcs.size();}

    protected:
        void OnLoad(const std::filesystem::path& loaderPath,
            const RC::StringType& modName,
            const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        void OnAutoReload(const RC::StringType& modName,
            const std::filesystem::path& modFilePath) override final;
        bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        bool OnInitialize() override final;
        void OnFinalizeLoad(const EEngineLifecyclePhase& engineLifecyclePhase) override final;

    private:
        struct VendorDefinition {
            std::string Id;
            std::string LoaderId;
            nlohmann::json HelpySource;
            bool HelpyTemporary=false;
            bool Human=false;
            bool Multiplayer=true;
            TimeOfDay::Requirement Time=TimeOfDay::Requirement::Any;
            std::string NetworkActorName;
            std::string NetworkGameplayFingerprint;
            bool Resource=false;
            nlohmann::json Appearance=nlohmann::json::object();
            nlohmann::json Equipment=nlohmann::json::object();
            HumanPose::Selection Pose;
            bool HasDialoguePose=false;
            HumanPose::Selection DialoguePose;
            nlohmann::json Ghost=nlohmann::json::object();
            nlohmann::json VisualEffect=nlohmann::json::object();
            nlohmann::json Markers=nlohmann::json::object();
            std::string StoreOwner;
            std::string DialogueKey;
            std::string LoreEntry;
            std::string QuestKey;
            std::string RequiredFlag;
            std::string LockedDialogueKey;
            std::string ModName;
            std::string DisplayName;
            std::string BaseActorClassPath = "/Game/Gameplay/NPCs/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C";
            std::string VisualMeshPath;
            bool HideMesh=false;
            bool HideName=false;
            std::string VisualSourcePath;
            std::string IdleAnimationPath;
            std::string DataTablePath;
            std::string RowName;
            std::string RowHandleProperty = "CraftingDataRowHandle";
            std::string InteractionComponentClass = "/Script/Dominion.InteractionComponent";
            std::string VendorComponentClass = "/Script/Dominion.CraftingStationComponent";
            std::string MerchantName;
            std::string VendorHeaderImage;
            bool Repairable=false;
            bool Masterworkable=false;
            std::string WidgetType = "Crafting";
            std::string ItemsProperty;
            nlohmann::json InteractionProperties = nlohmann::json::object();
            nlohmann::json VendorProperties = nlohmann::json::object();
            nlohmann::json Items = nlohmann::json::array();
            std::vector<VendorCategoryGate::Rule> CategoryRules;
            nlohmann::json Materials = nlohmann::json::array();
            double TargetLocation[3]{};
            bool GroundToSurface=false;
            double GroundOffset=0;
            double SpawnRotation[3]{};
            double SpawnScale[3]{ 1.0, 1.0, 1.0 };
            bool HasTargetLocation = false;
            bool InlineMerchant = false;
            bool EnableCollision = true;
            std::string MeshCollision = "Native";
            RC::Unreal::UClass* BaseActorClass = nullptr;
            RC::Unreal::UClass* InteractionClass = nullptr;
            RC::Unreal::UClass* VendorClass = nullptr;
            bool Enabled = true;
            VendorSpawnGate SpawnGate;
            VendorIdentity::Words PersistentId{};
            RC::Unreal::UWorld* CellReadyWorld = nullptr; // compared only; cleared on travel
            VendorPolicy::VendorStage Stage = VendorPolicy::VendorStage::Visual;
        };

        struct HelpyNpcLease {
            PS::WeakObjectHandle Actor,World;
            std::string Key;
            PS::HelpyNodes::Lease Lifetime;
            std::string AppliedKey{};
            RC::StringType ObjectPath{};
        };
        std::vector<HelpyNpcLease> m_helpyNpcs;
        bool m_helpySpawning=false;
        std::size_t m_helpyCreated=0;
        RC::Unreal::AActor* FindHelpyNpc(RC::Unreal::UWorld* world,const VendorDefinition& definition) const;
        void PumpHelpyNpcs();
        void RetireHelpyNpc(HelpyNpcLease& lease);
        std::string HelpyTemporaryReason(const VendorDefinition& definition);
        nlohmann::json HelpyNpcDocument(const VendorDefinition& source,const std::string& id);
        std::vector<VendorDefinition> m_definitions;
        NpcCatalog m_catalog;
        std::unordered_map<std::string,Dialogue::Definition> m_dialogues;
        Quests::Service m_quests;
        Quests::KillCredits m_killCredits;
        std::map<std::string,Quests::AcquisitionBaseline> m_acquisitionBaselines;
        // Client-only late-join recovery: one native current-objective
        // presentation per already-active quest and world.
        std::unordered_set<std::string> m_joinQuestPresented;
        bool m_observingAcquisition=false;
        void ObserveQuestInventory(RC::Unreal::UObject* controller,bool credit);
        void OnQuestInventoryChanged(RC::Unreal::UObject* source,RC::Unreal::UFunction* function);
        struct PendingQuestRefresh { PS::WeakObjectHandle Controller; std::string Quest; std::string Toast; double Due=0; };
        std::vector<PendingQuestRefresh> m_pendingQuestRefreshes;
        void QueueQuestRefresh(RC::Unreal::UObject* controller,const std::string& quest,double delaySeconds=0,std::string toast={});
        void PumpQuestRefreshes();
        void PumpQuestStatus();
        uint64_t m_killCreditEpoch=0;
        void OnQuestDamageCredit(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
        Events::Runtime m_events;
        double m_questLocationElapsed=0;
        unsigned m_questLocationChecks=0;
        void PumpQuestLocations(float deltaSeconds);
        struct PendingQuestLocationRefresh {
            PS::WeakObjectHandle Controller;
            double Elapsed=0;
            unsigned Attempts=0;
        };
        std::vector<PendingQuestLocationRefresh> m_pendingQuestLocationRefreshes;
        void QueueQuestLocationRefresh(RC::Unreal::UObject* controller);
        void PumpQuestLocationRefreshes(float deltaSeconds);
        void ReconcileQuestLocations(RC::Unreal::UObject* controller);
        struct DialogueGraphLease {
            RC::Unreal::UObject* Graph=nullptr; int32_t Index=-1; int32_t Serial=0;
            std::vector<VendorIdentity::Words> Nodes;
            RC::Unreal::UObject* PlayerToken=nullptr;
            int32_t PlayerIndex=-1,PlayerSerial=0;
        };
        std::unordered_map<std::string,DialogueGraphLease> m_dialogueGraphs;
        uint64_t m_dialogueRevision=0;
        RC::Unreal::UObject* BuildDialogueGraph(const VendorDefinition& definition,bool completed,const std::string& character,RC::Unreal::UObject* controller);
        void OpenDialogue(RC::Unreal::AActor* actor,RC::Unreal::AActor* player,const VendorDefinition& definition);
        RC::Unreal::UObject* ConfigureDialogueParticipant(RC::Unreal::AActor* actor,const VendorDefinition& definition);
        void ReleaseDialogueGraphs();
        void RetireDialoguePlayer(RC::Unreal::UObject* player);
        void PrepareClientDialogueTask(RC::Unreal::UObject* participant,void* parameters);
        RC::Unreal::UFunction* m_clientDialogueFunction=nullptr;
        int32_t m_clientDialogueHookId=0;
        RC::Unreal::Hook::GlobalCallbackId m_dialogueTransportCallbackId=RC::Unreal::Hook::ERROR_ID;
        void SanitizeDialogueRpc(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
        bool m_preparingClientDialogue=false;
        bool m_dialogueRequirementReady=false;
        uint8_t m_dialogueHiddenResult=0;
        void OnDialogueRequirement(RC::Unreal::UnrealScriptFunctionCallableContext& context);
        void ConfigureNpcMarkers(RC::Unreal::AActor* actor,const VendorDefinition& definition);
        struct DialogueCompletionBinding {
            RC::Unreal::UObject* Graph=nullptr;
            RC::Unreal::UObject* Response=nullptr;
            std::string Flag,Item,SuccessText;
            int32_t Count=1;
            std::string Store;
            std::string QuestKey,QuestAction,QuestEntry;
            std::string EventKey,EventAction;
            std::string NpcGoAwayKey;
            bool EndsDialogue=false;
            nlohmann::json Cue=nlohmann::json::object();
            nlohmann::json VisualEffectAction=nlohmann::json::object();
            std::string GateQuest;
            std::string GateTime;
            nlohmann::json GateStates=nlohmann::json::array();
            nlohmann::json GateConditions=nlohmann::json::object();
        };
        bool DialogueGateAllows(const DialogueCompletionBinding& action,RC::Unreal::UObject* controller);
        std::string RunQuestAction(const DialogueCompletionBinding& action,RC::Unreal::UObject* controller,const std::function<bool()>& current);
        std::unordered_map<RC::Unreal::UObject*,DialogueCompletionBinding> m_dialogueCompletions;
        struct DialogueSession {
            RC::Unreal::UObject* Instance=nullptr;
            RC::Unreal::UObject* Graph=nullptr;
            RC::Unreal::UObject* Player=nullptr;
            std::string Character;
            std::string NpcKey;
            RC::Unreal::AActor* Npc=nullptr;
            int32_t NpcIndex=-1,NpcSerial=0;
            bool ShopPending=false;
            double ShopDeadline=0,ShopCheck=0;
            int32_t PlayerIndex=-1,PlayerSerial=0;
            int32_t InstanceIndex=-1,InstanceSerial=0;
            bool Handling=false;
            bool DialoguePoseApplied=false;
        };
        std::unordered_map<RC::Unreal::UObject*,std::shared_ptr<DialogueSession>> m_dialogueSessions;
        RC::Unreal::AActor* ResolveDialoguePlayer(const DialogueSession& session);
        void NpcGoAway(const std::string& key,const std::string& reason);
        void PlayDialogueCue(RC::Unreal::AActor* actor,const nlohmann::json& cue);
        void ApplyDialogueVisualEffect(RC::Unreal::AActor* actor,const nlohmann::json& action);
        void PublishDialogueCue(RC::Unreal::AActor* actor,const nlohmann::json& cue);
        void ApplyNetworkDialogueCue(RC::Unreal::AActor* actor,const std::string& payload);
        void PumpDialoguePoseRestores();
        struct PendingDialoguePoseRestore {
            PS::WeakObjectHandle Actor;
            nlohmann::json Cue;
            double Deadline=0;
        };
        std::vector<PendingDialoguePoseRestore> m_pendingDialoguePoseRestores;
        std::unordered_map<RC::Unreal::UObject*,uint64_t> m_serverDialogueCueRevisions;
        std::unordered_map<std::string,uint64_t> m_clientDialogueCueRevisions;
        bool m_applyingNetworkDialogueCue=false;
        void PumpDialogueShop();
        void OpenNpcShop(RC::Unreal::AActor* actor,RC::Unreal::AActor* player,const VendorDefinition& value);
        void OnDialogueTask(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
        DragonWildsRecipeModLoader* m_recipes = nullptr;
        std::unordered_set<std::string> m_applied;
        std::unordered_set<RC::Unreal::UObject*> m_createdComponents;
        std::unordered_set<std::string> m_warnings;
        std::unordered_set<std::string> m_errors;

        struct CreatedMerchantRow {
            RC::Unreal::UDataTable* Table = nullptr; // held by m_tableLeases
            std::string RowName;
            std::string Owner;
            const void* RowAddress = nullptr;
        };
        std::vector<CreatedMerchantRow> m_createdRows;
        // Only roots acquired by this loader are released at shutdown.
        struct TableLease {
            RC::Unreal::UDataTable* Table = nullptr;
            int32_t Index = -1;
            bool AddedRoot = false;
        };
        std::vector<TableLease> m_tableLeases;
        bool HasLiveTableLease(RC::Unreal::UDataTable* table) const;

        struct SpawnedVendor {
            std::string Key;
            PS::WeakObjectHandle Actor;
        };
        std::vector<SpawnedVendor> m_spawnedVendors;
        struct RetiredVendor {
            const RC::Unreal::UObject* Address=nullptr;
            int32_t Index=-1;
            int32_t Serial=0;
            RC::StringType Path;
        };
        std::vector<RetiredVendor> m_retiredVendors;
        struct PendingNpcCleanup {
            RC::Unreal::AActor* Actor=nullptr;
            int32_t Index=-1;
            bool AddedRoot=false;
            std::string RespawnDefinition;
        };
        std::vector<PendingNpcCleanup> m_pendingNpcCleanup;
        double m_npcCleanupElapsed=0;
        void QueueNpcCleanup(RC::Unreal::AActor* actor,const std::string& respawnDefinition={});
        void PumpNpcCleanup(double deltaSeconds);
        bool m_npcTimeDirty=false;
        double m_npcTimeElapsed=0;
        void ReconcileNpcTimeOfDay();
        bool NpcTimeAllows(RC::Unreal::UObject* context,const VendorDefinition& definition) const;
        struct NamedTarget {
            RC::Unreal::UObject* Token=nullptr; // compared against live callback objects only
            std::string Name;
        };
        std::unordered_map<RC::StringType,NamedTarget> m_npcNames;
        void TrackNpcName(RC::Unreal::UObject* target,const VendorDefinition& definition);
        void OnNpcNameQuery(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
        void ConfigureNpcInteraction(RC::Unreal::AActor* actor,const VendorDefinition& definition);

        struct MerchantInteractionBinding {
            std::string DefinitionKey;
            PS::WeakObjectHandle Actor;
            PS::WeakObjectHandle Interaction;
            PS::WeakObjectHandle Station;
            // Comparison tokens only: never dereferenced or used to open a shop.
            // They let diagnostics observe events even when a weak serial is zero.
            RC::Unreal::UObject* ActorToken = nullptr;
            RC::Unreal::UObject* InteractionToken = nullptr;
            RC::Unreal::UObject* StationToken = nullptr;
        };
        std::vector<MerchantInteractionBinding> m_merchantBindings;
        VendorTraceBudget m_interactionTraceBudget;
        nlohmann::json m_interactionTrace = nlohmann::json::array();
        void TraceMerchantEvent(RC::Unreal::UObject* source,
            RC::Unreal::UFunction* function, void* parameters);

        RC::Unreal::Hook::GlobalCallbackId m_tickCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_worldTeardownCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_interactionCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::UFunction* m_cellShownFunction = nullptr;
        int32_t m_cellShownHookId = 0;
        uint64_t m_worldGeneration = 0;
        std::atomic<uint32_t> m_gameThreadId{0};
        bool m_handlingInteraction = false;
        struct PendingLoreRequest {
            PS::WeakObjectHandle Controller;
            std::string Entry;
            uint64_t WorldGeneration=0;
            float RemainingSeconds=5.0f;
            float RetryElapsed=0.0f;
        };
        std::vector<PendingLoreRequest> m_pendingLoreRequests;
        void QueueLoreRequest(RC::Unreal::UObject* controller,const std::string& entry);
        void PumpLoreRequests(float deltaSeconds);
        double m_scanElapsed = 0.0;
        VendorScanBudget m_scanBudget;
        bool m_spawning = false;
        nlohmann::json m_vendorReport = nlohmann::json::object();
        void WriteVendorStatus() noexcept;
        void WriteVendorInteractionTrace() noexcept;
        void RecordPhase(const VendorDefinition& definition, const char* phase,
            RC::Unreal::AActor* actor = nullptr, const nlohmann::json& details = nlohmann::json::object());

        void LoadDefinitions(const nlohmann::json& data, const RC::StringType& modName);
        void LoadDefinition(const nlohmann::json& data, const RC::StringType& modName,
            const std::string& fallbackId = {}, const std::string& storeOwner = {});

        void ScanLoadedActors();
        void QueueClientReplica(RC::Unreal::AActor* actor);
        bool PrepareClientReplica(RC::Unreal::AActor* actor);
        VendorDefinition* FindClientDefinition(RC::Unreal::AActor* actor);
        std::string NetworkDefinitionFingerprint(RC::Unreal::UObject* context,const VendorDefinition& definition);
        std::unordered_map<std::string,std::string> m_networkDialogueFingerprints;
        void ApplyNetworkIdentity(RC::Unreal::AActor* actor,const VendorDefinition& definition);
        void PumpClientReplicas(float deltaSeconds);
        struct PendingClientReplica { RC::StringType Path; unsigned Attempts=0; };
        std::vector<PendingClientReplica> m_pendingClientReplicas;
        uint64_t m_actorObserverId=0;
        RC::Unreal::UFunction* m_clientShopFunction=nullptr;
        int32_t m_clientShopHookId=0;
        std::vector<std::pair<RC::Unreal::UFunction*,int32_t>> m_questDeliveryHooks;
        void LogQuestStage(RC::Unreal::UObject* controller,const std::string& quest,size_t stage);
        void OnQuestTextQuery(RC::Unreal::UnrealScriptFunctionCallableContext& context,RC::Unreal::UFunction* function);
        void TryAutomaticQuests(RC::Unreal::UObject* controller);
        bool m_completingAutomaticQuests=false;
        double m_clientReplicaElapsed=0;
        bool m_preparingClientReplica=false;
        void TrySpawnConfiguredVendors(RC::Unreal::UWorld* world);
        RC::Unreal::AActor* SpawnVisualVendor(RC::Unreal::UWorld* world,
            VendorDefinition& definition);
        bool ApplyVendorVisuals(RC::Unreal::AActor* actor,
            VendorDefinition& definition);
        void ApplyHumanVisuals(RC::Unreal::AActor* actor, VendorDefinition& definition);
        void ApplyNpcVisualEffect(RC::Unreal::UObject* visual,const VendorDefinition& definition);
        RC::Unreal::UObject* FindMeshComponent(RC::Unreal::UObject* actor) const;
        RC::Unreal::AActor* FindVisualSourceActor(
            const VendorDefinition& definition) const;
        RC::Unreal::UClass* ResolveVisualSourceClass(
            const VendorDefinition& definition) const;
        void ApplyVendorDisplayName(RC::Unreal::AActor* actor,
            const VendorDefinition& definition);
        void ApplyVendorMaterials(RC::Unreal::UObject* mesh,
            const VendorDefinition& definition) const;
        void ReleaseVendorTracking();
        void OnVendorCellShown(RC::Unreal::UObject* cell);
        RC::Unreal::AActor* FindPersistentVendor(RC::Unreal::UWorld* world,
            const VendorDefinition& definition) const;
        void SetPersistentVendorId(RC::Unreal::AActor* actor,
            const VendorDefinition& definition) const;
        bool ApplyVendor(RC::Unreal::AActor* actor, VendorDefinition& definition);
        void BindMerchantInteraction(RC::Unreal::AActor* actor,
            RC::Unreal::UObject* interaction, RC::Unreal::UObject* station,
            const VendorDefinition& definition);
        void OnMerchantInteraction(RC::Unreal::UObject* source,
            RC::Unreal::UFunction* function, void* parameters);
        RC::Unreal::UObject* FindCraftingApi(RC::Unreal::UWorld* world, nlohmann::json& diagnostics) const;
        uint8_t ResolveWidgetType(RC::Unreal::UFunction* function,
            const VendorDefinition& definition, nlohmann::json& details) const;
        void SetVendorVisible(RC::Unreal::AActor* actor, const VendorDefinition& definition);
        void ApplyResourceVisuals(RC::Unreal::AActor* actor, VendorDefinition& definition);
        bool ResolveDefinitionClasses(VendorDefinition& definition);
        RC::Unreal::UWorld* FindLoadedWorld() const;
        void RemoveCreatedMerchantRows();

        RC::Unreal::UObject* EnsureComponent(RC::Unreal::AActor* actor,
            RC::Unreal::UClass* componentClass);
        void ApplyComponentProperties(RC::Unreal::UObject* component,
            const nlohmann::json& properties, const VendorDefinition& definition);
        void ApplyVendorRow(RC::Unreal::UObject* component,
            const VendorDefinition& definition);
        void CreateInlineMerchantRow(const VendorDefinition& definition,
            const nlohmann::json* visibleItems=nullptr);
        nlohmann::json VisibleVendorItems(const VendorDefinition& definition,
            RC::Unreal::UObject* controller);
        void RegisterComponent(RC::Unreal::AActor* actor,
            RC::Unreal::UObject* component,
            const VendorDefinition& definition);

        bool WarnOnce(const std::string& key, const RC::StringType& message);
        bool ErrorOnce(const std::string& key, const RC::StringType& message);
        static std::string ActorKey(RC::Unreal::AActor* actor,
            const VendorDefinition& definition);
    };
}
