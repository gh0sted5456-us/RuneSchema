#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <chrono>
#include <cctype>
#include <format>
#include <initializer_list>
#include <string_view>
#include <Windows.h>
#include "SDK/WeakObjectHandle.h"

#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Loader/DialogueGraphLifecycle.h"
#include "Loader/DialogueGraphIdentity.h"
#include "Loader/QuestDamageCredit.h"
#include "Loader/NativeQuestReceipt.h"
#include "Loader/NativeDialogueSave.h"
#include "Loader/NativeQuestCleanup.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/NameTypes.hpp"
#include "Unreal/Transform.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UnrealFlags.hpp"
#include "Unreal/World.hpp"
#include "Runtime/NetworkContext.h"
#include "Loader/NpcNetwork.h"
#include "Loader/TimeOfDayRuntime.h"
#include "Loader/NiagaraAttachment.h"
#include "Loader/QuestInspectorModel.h"
#include "Loader/NpcIdentityPayload.h"
#include "Loader/EventIdentity.h"
#include "Loader/QuestProgressText.h"
#include "Loader/QuestHandIns.h"
#include "Loader/NativeShopContract.h"
#include "Generator/AppearanceResolver.h"
#include "Generator/SaveViewer.h"
#include "Generator/ToolRequest.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Utility/BuildInfo.h"
#include "Loader/VendorAcknowledgement.h"
#include "Utility/NativeFunctionHook.h"
#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "Loader/DragonWildsNpcLoader.h"
#include "Loader/DefinitionRegistry.h"
#include "Loader/VirtualDefinitionId.h"
#include "Loader/HelpyNpcGuards.h"
#include "Runtime/AuthoredFile.h"
#include <exception>
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/DragonWildsRecipeModLoader.h"
#include "Loader/VendorOffers.h"
#include "Loader/VendorCategoryText.h"
#include "Loader/VendorCategoryGroups.h"
#include "Loader/NpcPlacement.h"
#include "Loader/NpcComponentSelection.h"
#include "Loader/NpcCollisionArguments.h"
#include "Loader/QuestNativeAdapter.h"
#include "Loader/QuestProgress.h"
#include "Loader/PinnedObjectIdentity.h"
#include "Loader/GameplayTestContext.h"
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {

    bool DragonWildsNpcLoader::IsQuestCompleted(UWorld* world,const std::string& quest) const
    {
        if(!world || quest.empty() || !m_quests.HasAsset(quest))return false;
        auto* controllerType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));
        if(!controllerType)return false;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerType,controllers,true);
        if(controllers.Num()<0 || controllers.Num()>256)throw std::runtime_error("Quest condition player roster exceeds the safe bound");
        for(auto* controller:controllers)try {
            if(!controller || controller->GetWorld()!=world || !IsGameplayQuestController(controller))continue;
            const QuestNative::Adapter native(controller,m_quests.Asset(quest));
            auto state=RC::to_string(native.StateName());
            if(const auto colon=state.rfind("::");colon!=state.npos)state=state.substr(colon+2);
            if(state=="Complete")return true;
        }catch(...) {}
        return false;
    }
    namespace {
        FStrProperty* IdentityField(UClass* type);
        UClass* RequireIdentityClass();
        std::string ReadIdentity(UObject* component);
    }
#include "NpcNativeShopRefresh.inl"
#include "NpcMeshSetter.inl"
#include "HumanNpc.inl"
#include "ResourceNpc.inl"
#include "NpcMarkers.inl"
#include "NpcInteraction.inl"
#include "NpcDialogue.inl"
#include "NpcQuestActions.inl"
#include "NpcQuestAcquisition.inl"
#include "NpcQuestPresentation.inl"
#include "NpcQuestCompletion.inl"
    namespace {
        constexpr std::string_view DefaultInteractionComponent =
            "/Script/Dominion.InteractionComponent";
        constexpr std::string_view DefaultVendorComponent =
            "/Script/Dominion.CraftingStationComponent";
        constexpr std::string_view DefaultVendorDataTable =
            "/Game/Gameplay/World/Vendors/DT_VendorDataTable.DT_VendorDataTable";

        std::string ReadString(const nlohmann::json& data,
            std::initializer_list<const char*> names,
            const std::string& fallback = {})
        {
            for (const auto* name : names)
            {
                if (!data.contains(name))
                {
                    continue;
                }
                if (!data.at(name).is_string())
                {
                    throw std::runtime_error(std::format("Vendor field '{}' must be a string", name));
                }
                return data.at(name).get<std::string>();
            }
            return fallback;
        }

        bool ReadBool(const nlohmann::json& data, const char* name, bool fallback)
        {
            if (!data.contains(name))
            {
                return fallback;
            }
            if (!data.at(name).is_boolean())
            {
                throw std::runtime_error(std::format("Vendor field '{}' must be a boolean", name));
            }
            return data.at(name).get<bool>();
        }

        void ReadVector3(const nlohmann::json& data,
            const char* name, double (&outValue)[3], bool allowUniform)
        {
            if (!data.contains(name))
            {
                return;
            }

            const auto& value = data.at(name);
            if (allowUniform && value.is_number())
            {
                const auto uniform = value.get<double>();
                if (!std::isfinite(uniform))
                {
                    throw std::runtime_error(std::format(
                        "Vendor field '{}' must contain only finite numbers", name));
                }
                outValue[0] = uniform;
                outValue[1] = uniform;
                outValue[2] = uniform;
                return;
            }

            if (value.is_object())
            {
                const bool rotation = std::string_view(name) == "Rotation";
                const std::array<const char*, 3> axes = rotation
                    ? std::array<const char*, 3>{"Pitch", "Yaw", "Roll"}
                    : std::array<const char*, 3>{"X", "Y", "Z"};
                for (size_t index = 0; index < axes.size(); ++index)
                {
                    if (!value.contains(axes[index]) || !value.at(axes[index]).is_number())
                        throw std::runtime_error(std::format(
                            "Vendor field '{}' requires numeric {}, {} and {} fields",
                            name, axes[0], axes[1], axes[2]));
                    outValue[index] = value.at(axes[index]).get<double>();
                    if (!std::isfinite(outValue[index]))
                        throw std::runtime_error(std::format(
                            "Vendor field '{}' must contain only finite numbers", name));
                }
                return;
            }

            if (std::string_view(name) == "Rotation")
                throw std::runtime_error(
                    "Vendor field 'Rotation' requires numeric Pitch, Yaw and Roll fields");

            if (!value.is_array() || value.size() != 3
                || !value[0].is_number() || !value[1].is_number()
                || !value[2].is_number())
            {
                throw std::runtime_error(std::format(
                    "Vendor field '{}' must be a number or an array of three finite numbers",
                    name));
            }

            for (size_t index = 0; index < 3; ++index)
            {
                outValue[index] = value[index].get<double>();
                if (!std::isfinite(outValue[index]))
                {
                    throw std::runtime_error(std::format(
                        "Vendor field '{}' must contain only finite numbers", name));
                }
            }
        }

        nlohmann::json ReadObject(const nlohmann::json& data, const char* name)
        {
            if (!data.contains(name))
            {
                return nlohmann::json::object();
            }
            if (!data.at(name).is_object())
            {
                throw std::runtime_error(std::format("Vendor field '{}' must be an object", name));
            }
            return data.at(name);
        }


    }

    namespace {
        FStrProperty* IdentityField(UClass* type) {
            auto* field=type?CastField<FStrProperty>(PropertyHelper::GetPropertyByName(type,TEXT("IdentityPayload"))):nullptr;
            if(!field || field->GetArrayDim()!=1 || field->GetSize()!=sizeof(FString)
                || !field->HasAnyPropertyFlags(CPF_Net) || !field->HasAnyPropertyFlags(CPF_RepNotify))
                throw std::runtime_error("RuneSchema identity component needs IdentityPayload String with RepNotify");
            return field;
        }
        UClass* RequireIdentityClass() {
            auto* type=ActorHelper::ResolveClass(NpcIdentity::ClassPath);
            if(!type)type=ActorHelper::ResolveClass(NpcIdentity::LegacyClassPath);
            auto* base=ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
            if(!type || !base || !type->IsChildOf(base) || ActorHelper::IsAbstract(type))
                throw std::runtime_error("RuneSchema multiplayer support component unavailable: mount /RuneSchema/Networking/BPC_RuneSchemaIdentity on server and client");
            (void)IdentityField(type);return type;
        }
        std::string ReadIdentity(UObject* component) {
            auto* field=IdentityField(component->GetClassPrivate());
            const auto& chars=field->ContainerPtrToValuePtr<FString>(component)->GetCharArray();
            if(chars.Num()==0)return {};
            if(chars.Num()<0 || chars.Num()>NpcIdentity::MaxPayload+1 || !chars.GetData()
                || chars.GetData()[chars.Num()-1]!=0)throw std::runtime_error("NPC identity string layout invalid");
            return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
        }
        bool IsNpcObjectUsable(UObject* object)
        {
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
                TEXT("/Script/Engine.KismetSystemLibrary:IsValid"),false);
            auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,
                TEXT("/Script/Engine.Default__KismetSystemLibrary"),false);
            auto* returned=function?CastField<FBoolProperty>(function->GetReturnProperty()):nullptr;
            FObjectPropertyBase* input=nullptr;
            unsigned count=0;
            if(function)for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
                if(field->HasAnyPropertyFlags(CPF_Parm)) {
                    ++count;
                    if(field->GetFName()==FName(TEXT("Object"),FNAME_Add))input=CastField<FObjectPropertyBase>(field);
                }
            if(!library || !function || count!=2 || function->GetParmsSize()!=9
                || !returned || !returned->IsNativeBool() || returned->GetOffset_Internal()!=8
                || returned->GetSize()!=1 || returned->GetArrayDim()!=1
                || !returned->HasAnyPropertyFlags(CPF_ReturnParm)
                || !input || input->GetOffset_Internal()!=0 || input->GetSize()!=sizeof(UObject*)
                || input->GetArrayDim()!=1 || input->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm))
                throw std::runtime_error("NPC retirement validity function layout unavailable");
            ActorHelper::FunctionCall call(library,TEXT("/Script/Engine.KismetSystemLibrary:IsValid"));
            call.Arg(TEXT("Object"),object).Invoke();
            return call.Result<bool>();
        }
    }

#include "NpcCleanup.inl"
#include "HelpyNpcAuthoring.inl"
    DragonWildsNpcLoader::DragonWildsNpcLoader(DragonWildsRecipeModLoader* recipes)
        : DragonWildsModLoaderBase("npc"), m_recipes(recipes)
    {
        SetDisplayName(TEXT("NPC Loader"));
    }

    DragonWildsNpcLoader::~DragonWildsNpcLoader()
    {
        if(HelpyInstance==this)HelpyInstance=nullptr;
        // Shutdown stays on the existing loader lifecycle. Native lifespan is an
        // additional fallback; do not call engine APIs from another thread.
        if(m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId()) {
            try {DismissHelpyNpcs();PumpNpcCleanup(1.0);}catch(...) {}
        }
        DragonWildsBlueprintModLoader::UnregisterActorInitializedObserver(m_actorObserverId);
        if(m_clientShopFunction && m_clientShopHookId)m_clientShopFunction->UnregisterHook(m_clientShopHookId);
        if(m_clientDialogueFunction && m_clientDialogueHookId)m_clientDialogueFunction->UnregisterHook(m_clientDialogueHookId);
        if(m_dialogueTransportCallbackId!=Hook::ERROR_ID)Hook::UnregisterCallback(m_dialogueTransportCallbackId);
        for(const auto& [function,id]:m_questDeliveryHooks)if(function && id)function->UnregisterHook(id);
        if (m_cellShownFunction && m_cellShownHookId)
            m_cellShownFunction->UnregisterHook(m_cellShownHookId);
        if (m_interactionCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_interactionCallbackId);
        }
        if (m_worldTeardownCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_worldTeardownCallbackId);
        }
        if (m_tickCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_tickCallbackId);
        }
        ReleaseVendorTracking();
        ReleaseDialogueGraphs();
        RemoveCreatedMerchantRows();
    }

    bool DragonWildsNpcLoader::CanInitialize(
        const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit;
    }

    void DragonWildsNpcLoader::WriteVendorStatus() noexcept
    {
        try {
            const auto& settings=PS::PSConfig::Get()->GetSettings();
            if(!settings.advancedRuntime||!settings.npcDiagnostics.statusExport)return;
            PS::ConfigFiles::Write(PS::HostServices::ExportsDirectory() / "VendorStatus.json",m_vendorReport.dump(2)+"\n");
        } catch(...) {
            try { WarnOnce("npc-status-export",TEXT("NPC status export failed; gameplay is unaffected.")); } catch(...) {}
        }
    }

    void DragonWildsNpcLoader::WriteVendorInteractionTrace() noexcept
    {
        try {
            const auto& settings=PS::PSConfig::Get()->GetSettings();
            if(!settings.advancedRuntime||!settings.npcDiagnostics.interactionTraceExport)return;
            PS::ConfigFiles::Write(PS::HostServices::ExportsDirectory() / "VendorInteractionTrace.json",
                nlohmann::json({{"Build",PS::BuildInfo::Name},{"StartedUnixSeconds",m_vendorReport["StartedUnixSeconds"]},
                    {"Limit",64},{"Events",m_interactionTrace}}).dump(2)+"\n");
        } catch(...) {
            try { WarnOnce("npc-interaction-export",TEXT("NPC interaction export failed; gameplay is unaffected.")); } catch(...) {}
        }
    }

    bool DragonWildsNpcLoader::OnInitialize()
    {
        m_events.Report=[](const std::string& message){PS::Log<LogLevel::Normal>(STR("Events: {}.\n"),PS::ToWideSafe(message.c_str()));};
        m_events.ApplyActorCue=[this](AActor* actor,const nlohmann::json& cue){PlayDialogueCue(actor,cue);};
        m_events.SetArea=[this](UObject* controller,const Events::Definition& event,bool show){return m_quests.SetEventArea(controller,event,show);};
        m_events.Notify=[](UObject* controller,const std::string& message,Events::Scope scope) {
            using namespace RC::Unreal;
            if(!controller || !controller->GetWorld() || message.empty())return;
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
                TEXT("/Script/Dominion.DominionPlayerController:Client_DisplayTextNotification"),false);
            auto* tag=function?CastField<FStructProperty>(function->FindProperty(FName(TEXT("NotificationTag"),FNAME_Find))):nullptr;
            auto* text=function?CastField<FTextProperty>(function->FindProperty(FName(TEXT("Text"),FNAME_Find))):nullptr;
            size_t count=0;if(function)for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
            if(!function || function->GetParmsSize()!=24 || function->GetReturnProperty() || count!=2
                || !tag || tag->GetOffset_Internal()!=0 || tag->GetElementSize()!=sizeof(FName) || tag->GetArrayDim()!=1
                || !tag->GetStruct().Get() || tag->GetStruct().Get()->GetPathName()!=TEXT("/Script/GameplayTags.GameplayTag")
                || !text || text->GetOffset_Internal()!=8 || text->GetElementSize()!=16 || text->GetArrayDim()!=1)
                throw std::runtime_error("Native event notification layout changed");
            const auto deliver=[&](UObject* recipient) {
                if(!recipient || recipient->GetWorld()!=controller->GetWorld())return;
                std::array<uint8_t,24> params{};
                text->InitializeValue_InContainer(params.data());
                try {
                    PropertyHelper::CopyJsonValueToContainer(params.data(),text,message);
                    recipient->ProcessEvent(function,params.data());
                    text->DestroyValue_InContainer(params.data());
                } catch(...) {
                    try{text->DestroyValue_InContainer(params.data());}catch(...) {}
                    throw;
                }
            };
            if(scope==Events::Scope::Participant){deliver(controller);return;}
            auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
            if(!type)throw std::runtime_error("Player controller class unavailable for scoped event message");
            TArray<UObject*> controllers;UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
            for(auto* recipient:controllers)if(recipient && !recipient->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))deliver(recipient);
        };
        m_vendorReport = {{"Build", PS::BuildInfo::Name},
            {"Phase", "Merchant.WaitingForWorld"}, {"VendorSpawningEnabled", true},
            {"InteractionBindingEnabled", true},
            {"StartedUnixSeconds", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}};
        WriteVendorStatus();
        WriteVendorInteractionTrace();
        // Use the same post-cell-load boundary as /spawns' persistent resources.
        m_cellShownFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.WorldPartitionRuntimeLevelStreamingCell:OnLevelShown"));
        m_cellShownHookId = PS::RegisterNativePostHook(m_cellShownFunction,
            [this](UnrealScriptFunctionCallableContext& context, void*) {
                try { OnVendorCellShown(context.Context); } catch (...) {}
            });
        if (!m_cellShownHookId) {
            m_vendorReport["Phase"] = "Merchant.CellHookFailed";
            WriteVendorStatus();
            return false; // Never create a duplicate ahead of save/cell restoration.
        }
        Hook::FCallbackOptions options{};
        m_vendorReport["DispatchRoute"] = "PlayerInteractionManager.Multicast_AcknowledgeInteractionRequest";
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("RuneSchemaMerchantInteraction");
        m_interactionCallbackId = Hook::RegisterProcessEventPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UObject* source,
                UFunction* function, void* parameters) {
                // The observed acknowledgement is the sole shop dispatcher.
                try {
                    static const FName onRepIdentity(TEXT("OnRep_IdentityPayload"),FNAME_Add);
                    static const FName receiveBeginPlay(TEXT("ReceiveBeginPlay"),FNAME_Add);
                    static const FName receiveEndPlay(TEXT("ReceiveEndPlay"),FNAME_Add);
                    static const FName questsUpdated(TEXT("OnQuestsUpdated"),FNAME_Add);
                    if(function && m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId()) {
                        // FModel/RSDW 1.0.0.2 shows that the actual roadside
                        // torch/campfire path is InGameTimeSensorComponent,
                        // whose Blueprint bindings dispatch these enter/exit
                        // events.  Observe those as well as the native world
                        // clock transition when a build exposes it.
                        static const FName enterTimeFrame(TEXT("OnEnterTimeFrameDynamic_Event"),FNAME_Add);
                        static const FName exitTimeFrame(TEXT("OnExitTimeFrameDynamic_Event"),FNAME_Add);
                        if(function==m_timeStateFunction || function->GetFName()==enterTimeFrame
                            || function->GetFName()==exitTimeFrame)m_npcTimeDirty=true;
                    }
                    if(source && function && source->GetClassPrivate()->GetPathName()==NpcIdentity::ClassPath
                        && (function->GetFName()==onRepIdentity || function->GetFName()==receiveBeginPlay)
                        && m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId()) {
                        auto* owner=source->GetOuterPrivate();
                        if(owner && owner->IsA<AActor>())QueueClientReplica(static_cast<AActor*>(owner));
                    }
                    OnNpcNameQuery(source,function,parameters);
                    if(source && function && function->GetFName()==receiveEndPlay
                        && function->GetPathName()==TEXT("/Script/Engine.Actor:ReceiveEndPlay")
                        && m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId()) {
                        RetireDialoguePlayer(source);
                    }
                    OnDialogueTask(source,function,parameters);
                    ObserveEventDeath(source,function);
                    OnQuestDamageCredit(source,function,parameters);
                    OnQuestInventoryChanged(source,function);
                    if(source && function && parameters && function->GetFName()==questsUpdated
                        && function->GetPathName()==TEXT("/Script/Dominion.QuestProgressComponent:OnQuestsUpdated")
                        && function->GetParmsSize()==17
                        && m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId()) {
                        try {
                            auto* owner=source->GetOuterPrivate();
                            if(!IsGameplayQuestController(owner))return;
                            if(static_cast<const uint8_t*>(parameters)[16]==1) {
                            const auto removed=Quests::NativeQuestCleanup::Run(owner,std::filesystem::path(PS::HostServices::WorkingDirectory())/"Mods/RuneSchema/mods");
                            if(removed)PS::Log<LogLevel::Normal>(STR("Quests: removed {} absent-mod records after player progress loaded.\n"),removed);
                            MigrateSavedProgress(owner);
                            }
                            if(m_quests.NetworkManifest().empty())PrepareQuests(owner);
                            ReconcileQuestLocations(owner);
                            QueueQuestLocationRefresh(owner);
                        }catch(const std::exception& error){ErrorOnce("quest-save-cleanup",PS::ToWideSafe(error.what()));}
                    }
                    OnMerchantInteraction(source, function, parameters);
                }
                catch (...) {
                    // No exception may cross the global ProcessEvent hook.
                }
            }, options);
        if (m_interactionCallbackId == Hook::ERROR_ID)
        {
            m_vendorReport["Phase"] = "Interaction.HookFailed";
            WriteVendorStatus();
            PS::Log<LogLevel::Error>(
                STR("Unable to register the merchant interaction callback; vendors will not spawn.\n"));
            return false;
        }

        m_actorObserverId=DragonWildsBlueprintModLoader::RegisterActorInitializedObserver(
            [this](AActor* actor){try{QueueClientReplica(actor);}catch(...) {}});
        m_clientShopFunction=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerController:Client_ToggleCraftingMenu"));
        m_clientShopHookId=PS::RegisterNativePreHook(m_clientShopFunction,
            [this](UnrealScriptFunctionCallableContext& context,void*){
                auto* source=context.Context;
                auto* function=m_clientShopFunction;
                auto* parameters=context.TheStack.Locals();
                if(!source || !function || !parameters || m_preparingClientReplica
                    || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
                try {
                    if(function->GetFName()!=FName(TEXT("Client_ToggleCraftingMenu"),FNAME_Add)
                        || function->GetPathName()!=TEXT("/Script/Dominion.DominionPlayerController:Client_ToggleCraftingMenu"))return;
                    FObjectPropertyBase* parameter=nullptr;
                    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
                        if(field->GetFName()==FName(TEXT("CraftingStationComponent"),FNAME_Add))parameter=CastField<FObjectPropertyBase>(field);
                    if(function->GetParmsSize()!=sizeof(UObject*) || !parameter
                        || parameter->GetOffset_Internal()!=0 || parameter->GetSize()!=sizeof(UObject*)
                        || parameter->GetArrayDim()!=1 || !parameter->HasAnyPropertyFlags(CPF_Parm)
                        || parameter->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm))return;
                    UObject* station=nullptr;std::memcpy(&station,parameters,sizeof(station));
                    if(!station || !station->IsA(parameter->GetPropertyClass().Get()))return;
                    auto* outer=station->GetOuterPrivate();
                    auto* actor=outer && outer->IsA<AActor>()?static_cast<AActor*>(outer):nullptr;
                    if(!actor || actor->GetWorld()!=source->GetWorld())return;
                    if(PS::Network::Detect(actor).Mode!=PS::Network::Role::Client)return;
                    if(!PrepareClientReplica(actor)) {
                        if(FindClientDefinition(actor))
                            ErrorOnce("client-merchant-not-ready",TEXT("Client merchant presentation was not ready at native menu entry."));
                        return;
                    }
                    if(auto* matched=FindClientDefinition(actor);
                        matched && matched->Stage==VendorPolicy::VendorStage::Merchant) {
                        const auto& definition=*matched;
                        m_activeVendorStation=PS::WeakObject(station);
                        m_activeVendorController=PS::WeakObject(source);
                        m_activeVendorDefinition=definition.ModName+":"+definition.Id;
                        nlohmann::json visible=nlohmann::json::array();
                        if(definition.InlineMerchant && definition.ItemsProperty.empty()) {
                            if(!m_recipes)throw std::runtime_error("Client merchant recipe service is unavailable");
                            visible=VisibleVendorItems(definition,source);
                            CreateInlineMerchantRow(definition,&visible);
                        }
                        // Reapply the row handle and rebuild the native station
                        // cache before Client_ToggleCraftingMenu constructs its
                        // tab bar. This is required for Repair and Ascend flags
                        // to paint on remote clients, even when the shop itself
                        // was already generated from a non-inline data row.
                        ApplyVendorRow(station,definition);
                        RefreshClientShop(station);
                        PrepareClientShopTabAssets(station);
                        PrepareClientShopBanner(station,definition.VendorHeaderImage);
                        if(definition.InlineMerchant && definition.ItemsProperty.empty()) {
                            const auto owner=definition.StoreOwner.empty()?VendorOffers::Owner(definition.ModName,definition.Id):definition.StoreOwner;
                            const auto availability=m_recipes->PrepareStoreForPlayer(owner,visible,source);
                            PS::Log<LogLevel::Verbose>(STR("Client merchant '{}' recipe availability verified before native menu entry: {}\n"),
                                PS::ToWideSafe(definition.Id.c_str()),PS::ToWideSafe(availability.dump().c_str()));
                        }
                    }
                }catch(const std::exception& error){ErrorOnce("client-merchant-presentation",PS::ToWideSafe(error.what()));}
                catch(...) {}
            });
        if(!m_clientShopHookId)return false;
        m_clientDialogueFunction=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
            TEXT("/Script/CommonConversationRuntime.ConversationParticipantComponent:ClientExecuteTaskAndSideEffects"));
        m_clientDialogueHookId=PS::RegisterNativePreHook(m_clientDialogueFunction,
            [this](UnrealScriptFunctionCallableContext& context,void*) {
                if(m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId() || m_preparingClientDialogue)return;
                try {PrepareClientDialogueTask(context.Context,context.TheStack.Locals());}
                catch(const std::exception& error){ErrorOnce("client-dialogue-graph",PS::ToWideSafe(error.what()));}
            });
        if(!m_clientDialogueHookId)return false;
        options.HookName=TEXT("RuneSchemaDialogueRpcSanitizer");
        m_dialogueTransportCallbackId=Hook::RegisterProcessEventPreCallback(
            [this](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void* parameters) {
                try {SanitizeDialogueRpc(source,function,parameters);} catch(const std::exception& error) {
                    ErrorOnce("dialogue-rpc-sanitize",PS::ToWideSafe(error.what()));
                } catch(...) {}
            },options);
        if(m_dialogueTransportCallbackId==Hook::ERROR_ID)return false;
        {
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
                TEXT("/Script/CommonConversationRuntime.ConversationTaskNode:IsRequirementSatisfied"),false);
            auto* input=function?CastField<FStructProperty>(function->FindProperty(FName(TEXT("Context"),FNAME_Find))):nullptr;
            auto* result=function?function->GetReturnProperty():nullptr;
            UEnum* enumeration=nullptr;
            if(auto* value=CastField<FEnumProperty>(result))enumeration=value->GetEnum();
            else if(auto* value=CastField<FByteProperty>(result))enumeration=value->GetEnum().Get();
            if(function && function->GetParmsSize()==57 && input && input->GetOffset_Internal()==0 && input->GetElementSize()==56
                && input->GetArrayDim()==1 && input->GetStruct().Get() && input->GetStruct()->GetPathName()==TEXT("/Script/CommonConversationRuntime.ConversationContext")
                && result && result->GetOffset_Internal()==56 && result->GetElementSize()==1 && result->GetArrayDim()==1 && enumeration) {
                for(int value=0;value<256;++value) {
                    if(enumeration->GetNameByValue(value).ToString()!=TEXT("EConversationRequirementResult::FailedAndHidden"))continue;
                    m_dialogueHiddenResult=static_cast<uint8_t>(value);
                    const auto id=PS::RegisterNativePostHook(function,[this](UnrealScriptFunctionCallableContext& context,void*){OnDialogueRequirement(context);});
                    if(id){m_questDeliveryHooks.emplace_back(function,id);m_dialogueRequirementReady=true;}
                    break;
                }
            }
            if(!m_dialogueRequirementReady)WarnOnce("dialogue-requirements",TEXT("Native dialogue requirement contract unavailable; gated dialogues will not open."));
        }
        for(const auto& [name,size]:{std::pair{TEXT("Client_OnQuestUpdated"),114},
            std::pair{TEXT("Client_OnQuestsUpdated"),17},std::pair{TEXT("Client_NotifyTrackedQuestLoaded"),8}}) {
            const auto path=RC::StringType(TEXT("/Script/Dominion.QuestProgressComponent:"))+name;
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path.c_str(),false);
            if(!function || function->GetParmsSize()!=size)throw std::runtime_error("Native quest delivery contract changed");
            const auto id=PS::RegisterNativePostHook(function,[this](UnrealScriptFunctionCallableContext& context,void*) {
                auto* source=context.Context;
                if(!source || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;
                try {
                    auto* owner=source->GetOuterPrivate();
                    if(!IsGameplayQuestController(owner)
                        || ActorHelper::GetObjectRef(owner,TEXT("QuestProgressComponent"))!=source)return;
                    if(m_quests.NetworkManifest().empty())PrepareQuests(owner);
                    ReconcileQuestLocations(owner);
                    QueueQuestLocationRefresh(owner);
                }catch(const std::exception& error){ErrorOnce("quest-client-delivery",PS::ToWideSafe(error.what()));}
            });
            if(!id)throw std::runtime_error("Native quest delivery hook unavailable");
            m_questDeliveryHooks.emplace_back(function,id);
        }
        for(const auto* name:{TEXT("GetCurrentObjectiveText"),TEXT("GetObjectiveText")}) {
            const auto path=RC::StringType(TEXT("/Script/Dominion.QuestProgressComponent:"))+name;
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path.c_str(),false);
            const auto id=PS::RegisterNativePostHook(function,[this,function](UnrealScriptFunctionCallableContext& context,void*){OnQuestTextQuery(context,function);});
            if(id)m_questDeliveryHooks.emplace_back(function,id);
            else WarnOnce("quest-text-hook:"+RC::to_string(name),TEXT("Quest progress text hook unavailable; native objective wording retained."));
        }
        // The game's phase-change function can be a Blueprint/reflected event,
        // not a native UFunction. Observe it through the ProcessEvent callback
        // above; RegisterNativePostHook rejects non-native functions.
        m_timeStateFunction=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
            TEXT("/Script/Dominion.InGameTimeActor:OnChangeTimeOfDayState"),false);
        if(m_interactionCallbackId==Hook::ERROR_ID)
            WarnOnce("time-state-hook",TEXT("Game time transition observer unavailable; using the one-second replicated-state fallback."));
        else if(!m_timeStateFunction)
            PS::Log<LogLevel::Normal>(STR("Time of day: using the game's InGameTimeSensor enter/exit events with replicated-state verification.\n"));

        options.HookName = TEXT("RuneSchemaVendorScan");
        m_tickCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float deltaSeconds, bool) {
                if (!m_gameThreadId.load(std::memory_order_relaxed))
                {
                    m_gameThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
                }
                PumpDialogueShop();
                PumpDialoguePoseRestores();
                PumpLoreRequests(deltaSeconds);
                PumpQuestRefreshes();
                PumpQuestStatus();
                PumpQuestLocationRefreshes(deltaSeconds);
                PS::SaveViewer::PumpNpcExport();
                PumpClientReplicas(deltaSeconds);
                PumpHelpyNpcs();
                PumpNpcCleanup(deltaSeconds);
                ReconcileNpcScales(deltaSeconds);
                // The current game build no longer exposes a reflected
                // day/night transition function.  Re-read its native actor
                // fields at a bounded cadence, and only when a definition
                // actually uses time gating.  This is one check per second,
                // not an actor/world scan or a permanent background timer.
                if(std::ranges::any_of(m_definitions,[](const auto& definition){
                    return !definition.HelpyTemporary && definition.Enabled
                        && (definition.Time!=TimeOfDay::Requirement::Any
                            || std::ranges::any_of(definition.CategoryRules,[](const auto& rule){return rule.Time!=TimeOfDay::Requirement::Any;})
                            || std::ranges::any_of(definition.Items,[](const auto& item){return item.contains("TimeOfDay");}));
                })) {
                    m_npcTimeElapsed+=deltaSeconds;
                    if(m_npcTimeElapsed>=1.0) {
                        m_npcTimeElapsed=0;
                        m_npcTimeDirty=true;
                    }
                } else m_npcTimeElapsed=0;
                if(m_npcTimeDirty)ReconcileNpcTimeOfDay();
                if(m_events.NeedsTick())try {m_events.Tick(deltaSeconds);}
                    catch(const std::exception& error){try{m_events.Reset();}catch(...){}ErrorOnce("event-tick",PS::ToWideSafe(error.what()));}
                PumpQuestLocations(deltaSeconds);
                if (m_definitions.empty() || m_scanBudget.Exhausted()) return;
                if (std::none_of(m_definitions.begin(), m_definitions.end(),
                    [](const auto& definition) { return !definition.HelpyTemporary && definition.Enabled && definition.SpawnGate.Pending(); }))
                {
                    return;
                }
                m_scanElapsed += deltaSeconds;
                if (m_scanElapsed < 0.5) return;

                m_scanElapsed = 0.0;
                try
                {
                    ScanLoadedActors();
                }
                catch (const std::exception& error)
                {
                    m_vendorReport["Phase"] = "Merchant.ScanFailed";
                    m_vendorReport["Error"] = error.what();
                    WriteVendorStatus();
                    PS::Log<LogLevel::Error>(
                        STR("Vendor scan failed: {}\n"),
                        PS::ToWideSafe(error.what()));
                }
            }, options);

        if (m_tickCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(
                STR("Unable to register the vendor scan callback; /vendors will not attach components.\n"));
            return false;
        }

        options.HookName = TEXT("RuneSchemaVendorWorldTeardown");
        m_worldTeardownCallbackId = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                // Retire only our session NPCs before dropping world bindings.
                // Cleanup failures retain their owned retry records.
                try {DismissHelpyNpcs();PumpNpcCleanup(1.0);}catch(...) {}
                m_merchantBindings.clear();
                m_pendingLoreRequests.clear();
                m_pendingQuestRefreshes.clear();
                PS::QuestStatusRequests::Clear();
                ++m_worldGeneration;
                try {m_events.Reset();}catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("Event travel cleanup failed: {}\n"),PS::ToWideSafe(error.what()));}
                m_quests.ClearLocations([this](const std::string& key,const char* error){
                    ErrorOnce("quest-marker-cleanup:"+key,RC::to_generic_string(std::string("Quest marker cleanup: ")+error));
                });
                m_questLocationElapsed=0;m_questLocationChecks=0;
                m_pendingQuestLocationRefreshes.clear();
                m_joinQuestPresented.clear();
                m_acquisitionBaselines.clear();
                m_interactionTraceBudget.Reset();
                m_interactionTrace = nlohmann::json::array();
                try {
                    WriteVendorInteractionTrace();
                } catch (...) { /* Optional trace I/O cannot interrupt map cleanup. */ }
                ReleaseVendorTracking();
                ReleaseDialogueGraphs();
                // DataTable assets/rows can survive travel. Keep ownership and
                // roots until shutdown, rather than forgetting still-live rows.
                m_applied.clear();
                m_createdComponents.clear();
                m_errors.clear();
                m_scanElapsed = 0.0;
                m_scanBudget.ResetForMap();
                m_npcTimeDirty=false;
                m_npcTimeElapsed=0;
                m_npcScaleElapsed=0;
                m_lastObservedTime=TimeOfDay::Requirement::Any;
                m_activeVendorStation={};
                m_activeVendorController={};
                m_activeVendorDefinition.clear();
                m_vendorReport["Phase"] = m_definitions.empty()
                    ? "Merchant.NoDefinitions" : "Merchant.WaitingForWorld";
                m_vendorReport["ScanPasses"] = 0;
                m_vendorReport.erase("ScanBudgetExhausted");
                for (const auto& definition : m_definitions)
                    m_vendorReport.erase(definition.ModName + ":" + definition.Id);
                m_vendorReport.erase("World");
                m_vendorReport.erase("ActorCount");
                WriteVendorStatus();
                for (auto& definition : m_definitions)
                {
                    definition.BaseActorClass = nullptr;
                    definition.InteractionClass = nullptr;
                    definition.VendorClass = nullptr;
                    definition.CellReadyWorld = nullptr;
                    definition.SpawnGate.ResetForMap();
                }
            }, options);

        if (m_worldTeardownCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Warning>(
                STR("Vendor world teardown reset could not be registered; restart between worlds if needed.\n"));
        }

        HelpyInstance=this;
        return true;
    }

    void DragonWildsNpcLoader::OnLoad(
        const std::filesystem::path& loaderPath,
        const RC::StringType& modName,
        const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::PostEngineInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPathIsolated(loaderPath,
            [&](const nlohmann::json& data) {
                const auto& entries=data.is_object() && data.contains("Npcs") ? data.at("Npcs") : data;
                const auto add=[&](const nlohmann::json& entry) {
                    try {auto pending=m_catalog;pending.AddNpc(RC::to_string(modName),entry);m_catalog=std::move(pending);}
                    catch(const std::exception& error) {
                        const auto id=entry.is_object()?entry.value("Id",std::string("<missing Id>")):std::string("<non-object>");
                        PS::Log<LogLevel::Error>(STR("NPC '{}:{}' rejected: {}\n"),
                            modName,RC::to_generic_string(id),PS::ToWideSafe(error.what()));
                    }
                    catch(...) {PS::Log<LogLevel::Error>(STR("NPC definition in '{}' rejected: unknown error.\n"),modName);}
                };
                if(entries.is_array())for(const auto& entry:entries)add(entry);else add(entries);
            },
            [&](const std::filesystem::path& file,const std::string& error) {
                PS::Log<LogLevel::Error>(STR("NPC file '{}' in '{}' was rejected; other files will continue: {}\n"),
                    file.filename().native(),modName,PS::ToWideSafe(error.c_str()));
            });
    }

    void DragonWildsNpcLoader::OnAutoReload(
        const RC::StringType& modName,
        const std::filesystem::path& modFilePath)
    {
        PS::Log<LogLevel::Warning>(
            STR("Vendor definition changes in {} require a restart; existing component attachments are not hot-reloaded.\n"),
            modName);
        (void)modFilePath;
    }

    void DragonWildsNpcLoader::OnFinalizeLoad(
        const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            std::size_t rejected=0;
            const auto report=[&](const std::string& key,const std::string& error) {
                ++rejected;PS::Log<LogLevel::Error>(STR("NPC '{}' disabled: {}\n"),
                    RC::to_generic_string(key),PS::ToWideSafe(error.c_str()));
            };
            const auto resolved=m_catalog.ResolveIsolated(
                m_recipes ? m_recipes->StoreOffers() : std::vector<NpcCatalog::StoreOffer>{},report);
            for(const auto& npc:resolved)try {LoadDefinition(npc.Data,RC::to_generic_string(npc.Mod),{},npc.StoreOwner);}
            catch(const std::exception& error){report(npc.Data.value("LoaderID",npc.Mod+":<unknown>"),error.what());}
            catch(...){report(npc.Data.value("LoaderID",npc.Mod+":<unknown>"),"unknown native-definition error");}
            m_vendorReport["DefinitionCount"] = m_definitions.size();
            m_vendorReport["EnabledDefinitionCount"] = std::count_if(
                m_definitions.begin(), m_definitions.end(),
                [](const auto& definition) { return definition.Enabled; });
            m_vendorReport["Phase"] = m_definitions.empty()
                ? "Merchant.NoDefinitions" : "Merchant.WaitingForWorld";
            WriteVendorStatus();
            PS::Log<LogLevel::Normal>(
                STR("Loaded {} NPC/legacy vendor definition(s); {} rejected definition(s) were isolated.\n"),
                m_definitions.size(),rejected);
        }
    }

    void DragonWildsNpcLoader::LoadVendors(const nlohmann::json& data,const RC::StringType& modName)
    {
        if(data.is_array()) {
            size_t index=0;
            for(const auto& entry:data) {
                const auto id=entry.is_object()?entry.value("Id",std::string("#")+std::to_string(index)):
                    std::string("#")+std::to_string(index);
                try {LoadVendors(entry,modName);}
                catch(const std::exception& error) {
                    PS::Log<LogLevel::Error>(STR("[DEGRADED][LOADER:vendors][MOD:{}][RECORD:{}] Store record disabled; remaining records continue: {}.\n"),
                        modName,PS::ToWideSafe(id.c_str()),PS::ToWideSafe(error.what()));
                }
                catch(...) {
                    PS::Log<LogLevel::Error>(STR("[DEGRADED][LOADER:vendors][MOD:{}][RECORD:{}] Store record disabled by an unknown error; remaining records continue.\n"),
                        modName,PS::ToWideSafe(id.c_str()));
                }
                ++index;
            }
            return;
        }
        if(data.is_object() && data.contains("Vendors")) { LoadVendors(data.at("Vendors"),modName); return; }
        if(data.is_object() && data.contains("Id") && data.contains("Items") && !data.contains("Location")) {
            m_catalog.AddStore(RC::to_string(modName),data);
            return;
        }
        throw std::runtime_error("/vendors accepts storefront records only. Move actor, Location, dialogue, gating, and spawning fields to /npc and reference this shop with VendorID");
    }

    void DragonWildsNpcLoader::LoadDefinitions(
        const nlohmann::json& data,
        const RC::StringType& modName)
    {
        if (data.is_array())
        {
            for (const auto& entry : data)
            {
                LoadDefinition(entry, modName);
            }
            return;
        }

        if (!data.is_object())
        {
            throw std::runtime_error("Vendor definition root must be an object or array");
        }

        if (data.contains("Vendors"))
        {
            LoadDefinitions(data.at("Vendors"), modName);
            return;
        }

        if (data.contains("Mesh") || data.contains("VisualMesh")
            || data.contains("SkeletalMesh") || data.contains("VisualSource")
            || data.contains("BaseActor") || data.contains("BaseActorClass")
            || data.contains("Actor")
            || data.contains("Items") || data.contains("Location"))
        {
            LoadDefinition(data, modName);
            return;
        }

        // Also accept a map keyed by a stable definition ID.
        for (const auto& [key, value] : data.items())
        {
            if (!key.starts_with("$") && value.is_object())
            {
                LoadDefinition(value, modName, key);
            }
        }
    }

    void DragonWildsNpcLoader::LoadDefinition(
        const nlohmann::json& data,
        const RC::StringType& modName,
        const std::string& fallbackId, const std::string& storeOwner)
    {
        if (!data.is_object())
        {
            throw std::runtime_error("Each vendor definition must be an object");
        }

        VendorDefinition definition;
        definition.HelpySource=data;
        HumanNpc::Validate(data);
        definition.HideMesh=data.value("HideMesh",false);
        definition.HideName=data.value("HideName",false);
        if(data.contains("VisualEffect")) {
            NpcVisualEffect::Validate(data.at("VisualEffect"));
            definition.VisualEffect=data.at("VisualEffect");
        }
        NpcMarkers::Validate(data);
        for(const auto* key:{"Map","OverheadIcon"})if(data.contains(key))definition.Markers[key]=data.at(key);
        definition.Human=HumanNpc::IsHuman(data);
        definition.Multiplayer=ReadBool(data,"Multiplayer",true);
        if(data.contains("TimeOfDay")) {
            if(!data.at("TimeOfDay").is_string())throw std::runtime_error("TimeOfDay must be Any, Day, or Night");
            definition.Time=TimeOfDay::Parse(data.at("TimeOfDay").get<std::string>());
        }
        definition.NetworkActorName=NpcNetwork::ActorName(RC::to_string(modName)+":"+data.dump());
        definition.NetworkGameplayFingerprint=NpcNetwork::GameplayFingerprint(RC::to_string(modName),data);
        definition.Resource=HumanNpc::UsesStaticMesh(data);
        if(definition.Human) {
            definition.Appearance=data.at("Appearance");
            definition.Equipment=data.value("Equipment",nlohmann::json::object());
            definition.HideWeapon=data.value("HideWeapon",false);
            if(data.contains("Pose"))definition.Pose=HumanPose::Parse(data.at("Pose"));
            if(data.contains("DialoguePose")){definition.DialoguePose=HumanPose::Parse(data.at("DialoguePose"));definition.HasDialoguePose=true;}
            definition.Ghost=data.value("Ghost",nlohmann::json::object());
        }
        definition.StoreOwner=storeOwner;
        definition.LoaderId=data.value("LoaderID",NpcCatalog::Key(RC::to_string(modName),data.at("Id").get<std::string>()));
        if(data.contains("RequiresFlag")) {
            definition.RequiredFlag=Dialogue::Reference(RC::to_string(modName),data.at("RequiresFlag"));
            definition.LockedDialogueKey=Dialogue::Reference(RC::to_string(modName),data.at("LockedDialogueID"));
            if(!m_dialogues.contains(definition.LockedDialogueKey))throw std::runtime_error("Store lock dialogue is missing or disabled: "+definition.LockedDialogueKey);
            for(const auto& key:m_dialogues.at(definition.LockedDialogueKey).Quests)m_quests.Find("_",key);
            for(const auto& key:m_dialogues.at(definition.LockedDialogueKey).Events)m_events.Require(key);
        }
        if(data.contains("DialogueID")) {
            definition.DialogueKey=Dialogue::Reference(RC::to_string(modName),data.at("DialogueID"));
            if(!m_dialogues.contains(definition.DialogueKey))throw std::runtime_error("NPC dialogue is missing or disabled: "+definition.DialogueKey);
            Dialogue::ValidateNpcStore(m_dialogues.at(definition.DialogueKey),storeOwner);
            for(const auto& key:m_dialogues.at(definition.DialogueKey).Quests)m_quests.Find("_",key);
            for(const auto& key:m_dialogues.at(definition.DialogueKey).Events)m_events.Require(key);
        }
        definition.LoreEntry=data.value("LoreEntry",std::string{});
        if(data.contains("QuestID")) {
            definition.QuestKey=data.at("QuestID").get<std::string>();
            (void)m_quests.Find("_",definition.QuestKey);
        }
        definition.Stage = VendorPolicy::Stage(data);
        definition.ModName = RC::to_string(modName);
        // Id is an internal stable key. Name/DisplayName are presentation
        // fields and must not accidentally become the duplicate-prevention
        // key when a vendor is renamed.
        definition.Id = ReadString(data, {"Id"}, fallbackId);
        definition.DisplayName = ReadString(data,
            {"DisplayName", "Name", "VendorName"}, fallbackId);
        definition.BaseActorClassPath = ReadString(data,
            {"BaseActor", "BaseActorClass", "ProxyClass"},
            definition.BaseActorClassPath);
        definition.VisualMeshPath = ReadString(data,
            {"Mesh", "VisualMesh", "SkeletalMesh", "VisualMeshPath"});
        definition.VisualSourcePath = ReadString(data,
            {"VisualSource", "SourceActor", "VisualSourceActor", "Actor"});
        if (data.contains("AnimationClass") || data.contains("AnimClass"))
            throw std::runtime_error("Neutral vendors use IdleAnimation, not AI animation Blueprint classes");
        definition.IdleAnimationPath = ReadString(data,
            {"IdleAnimation", "IdleAnim", "IdleAnimationAsset"});
        definition.DataTablePath = ReadString(data,
            {"DataTable", "VendorDataTable"}, std::string(DefaultVendorDataTable));
        definition.RowName = ReadString(data,
            {"RowName", "Row", "VendorRow"});
        definition.RowHandleProperty = ReadString(data,
            {"RowHandleProperty"}, definition.RowHandleProperty);
        definition.InteractionComponentClass = ReadString(data,
            {"InteractionComponentClass"}, std::string(DefaultInteractionComponent));
        definition.VendorComponentClass = ReadString(data,
            {"VendorComponentClass", "CraftingComponentClass"},
            std::string(DefaultVendorComponent));
        definition.MerchantName = ReadString(data,
            {"MerchantName", "VendorName", "DisplayName"},
            definition.DisplayName.empty() ? definition.Id : definition.DisplayName);
        definition.VendorHeaderImage = NpcCatalog::HeaderImage(data);
        if(!data.contains("Items") && !data.contains("VendorHeaderImage"))definition.VendorHeaderImage.clear();
        if(!definition.VendorHeaderImage.empty() && !data.contains("Items"))
            throw std::runtime_error("VendorHeaderImage requires an owned inline merchant row (Items)");
        definition.Repairable = ReadBool(data, "Repairable", false);
        definition.Masterworkable = ReadBool(data, "Masterworkable", false);
        definition.WidgetType = ReadString(data,
            {"WidgetType", "ShopWidgetType"}, definition.WidgetType);
        definition.ItemsProperty = ReadString(data,
            {"ItemsProperty", "VendorItemsProperty"});
        definition.Enabled = ReadBool(data, "Enabled", true);
        definition.EnableCollision = ReadBool(data, "EnableCollision", true);
        definition.MeshCollision = data.value("MeshCollision",std::string(HumanNpc::IsHuman(data)?"Native":"Pawn"));

        if(data.contains("Location")) {
            const auto placement=NpcPlacement::Parse(data["Location"]);
            std::copy(placement.Position.begin(),placement.Position.end(),definition.TargetLocation);
            definition.HasTargetLocation=true;
            definition.GroundToSurface=placement.Ground;
            definition.GroundOffset=placement.Offset;
        }
        ReadVector3(data, "Rotation", definition.SpawnRotation, false);
        ReadVector3(data, "Scale", definition.SpawnScale, true);
        for (const auto value : definition.SpawnScale)
        {
            if (!std::isfinite(value) || value < 0.01 || value > 100.0)
            {
                throw std::runtime_error(
                    "Vendor field 'Scale' must contain only finite numbers from 0.01 to 100");
            }
        }
        if (data.contains("Items"))
        {
            if (!data.at("Items").is_array())
            {
                throw std::runtime_error("Vendor field 'Items' must be an array");
            }
            definition.Items = data.at("Items");
            if(definition.Items.size()>128)throw std::runtime_error("Vendor Items exceeds 128 offers");
            if(definition.ItemsProperty.empty()) {
                nlohmann::json valid=nlohmann::json::array();size_t offerIndex=0;
                for(const auto& item:definition.Items) {
                    const auto slot=item.is_object()?item.value("_RecipeSlot",std::to_string(offerIndex)):std::to_string(offerIndex);
                    try {(void)VendorOffers::Properties(item);(void)VendorOffers::Category(item);valid.push_back(item);}
                    catch(const std::exception& error) {
                        PS::Log<LogLevel::Warning>(STR("[DEGRADED][LOADER:vendors][MOD:{}][VENDOR:{}][OFFER:{}] Offer disabled; remaining offers continue: {}.\n"),
                            modName,PS::ToWideSafe(definition.Id.c_str()),PS::ToWideSafe(slot.c_str()),PS::ToWideSafe(error.what()));
                    }
                    ++offerIndex;
                }
                definition.Items=std::move(valid);
            }
            for(auto& item:definition.Items)if(item.contains("QuestCompleted"))
                item["QuestCompleted"]=Dialogue::Reference(definition.ModName,item.at("QuestCompleted"));
            definition.InlineMerchant = true;
        }
        if(data.contains("CategoryRules"))
            definition.CategoryRules=VendorCategoryGate::Parse(data.at("CategoryRules"));
        for(auto& rule:definition.CategoryRules)if(!rule.CompletedQuest.empty())
            rule.CompletedQuest=Dialogue::Reference(definition.ModName,rule.CompletedQuest);
        std::erase_if(definition.CategoryRules,[&](const auto& rule){
            const bool missing=!std::ranges::any_of(definition.Items,[&](const auto& item){return VendorOffers::Category(item)==rule.Category;});
            if(missing)PS::Log<LogLevel::Warning>(STR("[DEGRADED][LOADER:vendors][MOD:{}][VENDOR:{}][CATEGORY:{}] Category rule disabled because no valid offers remain.\n"),
                modName,PS::ToWideSafe(definition.Id.c_str()),PS::ToWideSafe(rule.Category.c_str()));
            return missing;
        });

        if (data.contains("Materials"))
        {
            if (!data.at("Materials").is_array())
            {
                throw std::runtime_error("Vendor field 'Materials' must be an array");
            }
            for (const auto& material : data.at("Materials"))
            {
                if (!material.is_string())
                {
                    throw std::runtime_error(
                        "Vendor field 'Materials' must contain only asset paths");
                }
            }
            definition.Materials = data.at("Materials");
        }

        if (definition.Id.empty())
        {
            definition.Id = !definition.VisualSourcePath.empty()
                ? definition.VisualSourcePath : definition.VisualMeshPath;
        }
        if (definition.Id.empty())
        {
            definition.Id = std::format("vendor_{}", m_definitions.size() + 1);
        }
        if (definition.MerchantName.empty())
        {
            definition.MerchantName = definition.DisplayName.empty()
                ? definition.Id : definition.DisplayName;
        }
        if (definition.DisplayName.empty())
        {
            definition.DisplayName = definition.MerchantName;
        }
        if (!definition.Human && definition.VisualMeshPath.empty()
            && definition.VisualSourcePath.empty())
        {
            throw std::runtime_error(std::format(
                "Vendor '{}' requires Mesh/VisualMesh or VisualSource", definition.Id));
        }
        if (!definition.HasTargetLocation)
        {
            throw std::runtime_error(std::format(
                "Vendor '{}' requires Location to place its actor", definition.Id));
        }
        if (definition.Stage == VendorPolicy::VendorStage::Merchant
            && (definition.DataTablePath.empty() || definition.RowName.empty()))
        {
            if (!definition.InlineMerchant)
            {
                throw std::runtime_error(std::format(
                    "Vendor '{}' requires both DataTable and RowName", definition.Id));
            }
            if (definition.RowName.empty())
            {
                definition.RowName = "RSVendor_"+VendorOffers::Identity(
                    definition.StoreOwner.empty() ? VendorOffers::Owner(definition.ModName,definition.Id) : definition.StoreOwner,"row");
            }
        }
        if (definition.InteractionComponentClass.empty()
            || definition.VendorComponentClass.empty())
        {
            throw std::runtime_error(std::format(
                "Vendor '{}' requires both component classes", definition.Id));
        }

        definition.InteractionProperties = ReadObject(data, "InteractionProperties");
        definition.VendorProperties = ReadObject(data, "VendorProperties");
        if (data.contains("Properties"))
        {
            auto properties = ReadObject(data, "Properties");
            if (definition.VendorProperties.empty())
            {
                definition.VendorProperties = std::move(properties);
            }
        }

        if(std::any_of(m_definitions.begin(),m_definitions.end(),[&](const auto& other){
            return other.ModName==definition.ModName && other.Id==definition.Id;
        }))throw std::runtime_error("Duplicate vendor Id within mod: "+definition.Id);
        definition.PersistentId = VendorIdentity::ForOwner(
            VendorOffers::Owner(definition.ModName, definition.Id));
        if (std::any_of(m_definitions.begin(), m_definitions.end(), [&](const auto& other) {
            return other.PersistentId == definition.PersistentId;
        })) throw std::runtime_error("Vendor persistent identity collision; choose another Id");
        m_definitions.push_back(std::move(definition));
    }

    bool DragonWildsNpcLoader::ResolveDefinitionClasses(
        VendorDefinition& definition)
    {
        definition.BaseActorClass = ActorHelper::ResolveClass(
            RC::to_generic_string(definition.BaseActorClassPath));
        if (!definition.BaseActorClass
            || !ActorHelper::IsActorClass(definition.BaseActorClass)
            || ActorHelper::IsAbstract(definition.BaseActorClass))
        {
            WarnOnce("base-class:" + definition.ModName + ":" + definition.Id,
                RC::to_generic_string(std::format(
                    "Vendor '{}' could not resolve usable BaseActor '{}'; it will be retried.",
                    definition.Id, definition.BaseActorClassPath)));
            return false;
        }

        if (definition.Stage == VendorPolicy::VendorStage::Visual) return true;
        definition.InteractionClass = ActorHelper::ResolveClass(
            RC::to_generic_string(definition.InteractionComponentClass));
        if (!definition.InteractionClass)
        {
            WarnOnce("interaction-class:" + definition.ModName + ":" + definition.Id,
                RC::to_generic_string(std::format(
                    "Vendor '{}' could not resolve InteractionComponentClass '{}'.",
                    definition.Id, definition.InteractionComponentClass)));
            return false;
        }

        if (definition.Stage == VendorPolicy::VendorStage::Interaction) {
            auto* type = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Engine.ActorComponent"));
            if (!type || !definition.InteractionClass->IsChildOf(type))
                throw std::runtime_error("Interaction class is not an ActorComponent");
            return true;
        }
        definition.VendorClass = ActorHelper::ResolveClass(
            RC::to_generic_string(definition.VendorComponentClass));
        if (!definition.VendorClass)
        {
            WarnOnce("vendor-class:" + definition.ModName + ":" + definition.Id,
                RC::to_generic_string(std::format(
                    "Vendor '{}' could not resolve VendorComponentClass '{}'.",
                    definition.Id, definition.VendorComponentClass)));
            return false;
        }

        auto* actorComponentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.ActorComponent"));
        if (!actorComponentClass
            || !definition.InteractionClass->IsChildOf(actorComponentClass)
            || !definition.VendorClass->IsChildOf(actorComponentClass))
        {
            throw std::runtime_error(std::format(
                "Vendor '{}' specified a class that is not an ActorComponent",
                definition.Id));
        }

        return true;
    }

    void DragonWildsNpcLoader::ObserveEventDespawn(UObject* source,UFunction* function)
    {
        if(m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId() && m_events.Active())m_events.OnDespawn(source,function);
    }

    void DragonWildsNpcLoader::ObserveEventDeath(UObject* source,UFunction* function)
    {
        if(m_gameThreadId.load(std::memory_order_relaxed)==GetCurrentThreadId() && m_events.Active())m_events.OnDeath(source,function);
    }

    void DragonWildsNpcLoader::OnQuestDamageCredit(UObject* source,UFunction* function,void* parameters)
    {
        if(m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId() || (!m_events.Active() && !m_quests.HasKills()) || !function)return;
        static const FName damage(TEXT("BP_OnDamageReceived"),FNAME_Add),point(TEXT("BP_OnPointDamageReceived"),FNAME_Add);
        const auto name=function->GetNamePrivate();if(name!=damage && name!=point)return;
        try {
            const auto death=Quests::ReadFatalDamage(source,function,parameters);
            if(!death.Victim || !death.Player)return;
            const auto eventKey=m_events.EventForActor(death.Victim);
            const auto spawnKey=m_events.SpawnForActor(death.Victim);
            m_events.ConfirmVictim(death.Victim);
            if(!m_quests.HasKills())return;
            auto* controller=ActorHelper::GetObjectRef(death.Player,TEXT("Controller"));
            if(!controller || ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=death.Player)return;
            const auto character=DialogueCharacter(controller);
            const auto epoch=m_worldGeneration+1;
            if(m_killCreditEpoch!=epoch){m_killCredits.BeginWorld(epoch);m_killCreditEpoch=epoch;}
            auto* slot=FUObjectArray::IndexToObject(death.Victim->GetInternalIndex());
            if(!slot || slot->GetUObject()!=death.Victim || !slot->IsValid(false) || slot->GetSerialNumber()<=0)throw std::runtime_error("Killed actor serial identity unavailable");
            Quests::KillSignal signal{epoch,death.Victim->GetInternalIndex(),slot->GetSerialNumber(),character,RC::to_string(death.Victim->GetClassPrivate()->GetPathName()),eventKey,{},true,true};
            const auto victimPosition=ActorHelper::GetActorLocation(static_cast<AActor*>(death.Victim));
            signal.Position=std::array<double,3>{victimPosition.X(),victimPosition.Y(),victimPosition.Z()};
            signal.SpawnKey=spawnKey;
            for(auto* base=death.Victim->GetClassPrivate()->GetSuperStruct();base;base=base->GetSuperStruct())signal.VictimBaseClasses.push_back(RC::to_string(base->GetPathName()));
            const auto progress=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
            m_quests.ForEachKill([&](const std::string& key,const Quests::Definition& quest,const nlohmann::json& document){
                try {
                    const QuestNative::Adapter native(controller,m_quests.Asset(key));
                    if(!native.IsInitialized())return;
                    const Quests::NativeReceipt receipt(native,quest,document,progress,character);
                    if(receipt.Phase()!=1)return;
                    auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
                    if(!quest.Stages.empty()) {
                        auto staged=Quests::StageProgress(native,quest,receipt.Run());
                        const auto active=staged.ActiveStage();
                        if(active==quest.Stages.size() || state!="Given")return;
                        if(m_killCredits.StageForSignal(key,signal,active)!=active)return;
                        bool changed=false;
                        for(const auto& objective:quest.Stages[active].second) {
                            if(!objective.Kill)continue;
                            const auto counter=Quests::StageCounter(objective);
                            const auto result=m_killCredits.Apply(objective,signal,character,true,[&]{return native.GetInt(counter);},[&](int value){native.SetInt(counter,value);});
                            if(result==Quests::KillResult::Counted && objective.AnnounceProgress)try {
                                m_events.Notify(controller,Quests::ObjectiveProgressText(objective,native.GetInt(counter),receipt.Run()),Events::Scope::Participant);
                            }catch(const std::exception& error){ErrorOnce("quest-progress-message:"+key+":"+objective.ObjectiveId,PS::ToWideSafe(error.what()));}
                            changed|=result==Quests::KillResult::Counted;
                        }
                        if(changed) {
                            Quests::SetStageText(native,quest,staged.ActiveStage());
                            if(staged.ActiveStage()>active)LogQuestStage(controller,key,active+1);
                            if(staged.Complete())receipt.MarkObjectiveSatisfied();
                            QueueQuestRefresh(controller,key);
                        }
                        return;
                    }
                    const FName counter(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add);
                    const auto result=m_killCredits.Apply(quest,signal,character,state=="Given",[&]{return native.GetInt(counter);},[&](int value){native.SetInt(counter,value);});
                    if(result==Quests::KillResult::Counted) {
                        if(quest.AnnounceProgress)try {
                            m_events.Notify(controller,Quests::ObjectiveProgressText(quest,native.GetInt(counter),receipt.Run()),Events::Scope::Participant);
                        }catch(const std::exception& error){ErrorOnce("quest-progress-message:"+key,PS::ToWideSafe(error.what()));}
                        if(native.GetInt(counter)==quest.Required.Count) {
                            receipt.MarkObjectiveSatisfied();
                            native.SetObjective(FName(TEXT("__ready"),FNAME_Add));
                            LogQuestStage(controller,key,1);
                        }
                        QueueQuestRefresh(controller,key);
                        PS::Log<LogLevel::Verbose>(STR("Quest '{}': confirmed kills {}/{}.\n"),RC::to_generic_string(key),native.GetInt(counter),quest.Required.Count);
                    }
                }catch(const std::exception& error){ErrorOnce("quest-kill:"+key,PS::ToWideSafe(error.what()));}
            });
        }catch(const std::exception& error){ErrorOnce("quest-kill-route",PS::ToWideSafe(error.what()));}
    }

    void DragonWildsNpcLoader::QueueQuestRefresh(UObject* controller,const std::string& quest,double delaySeconds,std::string toast)
    {
        if(!controller || quest.empty())return;
        const auto due=double(GetTickCount64())/1000.0+std::max(0.0,delaySeconds);
        const auto found=std::find_if(m_pendingQuestRefreshes.begin(),m_pendingQuestRefreshes.end(),
            [&](const auto& value){return value.Controller.Get()==controller && value.Quest==quest;});
        if(found==m_pendingQuestRefreshes.end())m_pendingQuestRefreshes.push_back({PS::WeakObject(controller),quest,std::move(toast),due,0});
        else {found->Due=std::max(found->Due,due);if(!toast.empty()){found->Toast=std::move(toast);found->ToastAttempts=0;}}
    }

    void DragonWildsNpcLoader::PumpQuestRefreshes()
    {
        if(m_pendingQuestRefreshes.empty())return;
        const auto now=double(GetTickCount64())/1000.0;
        std::vector<PendingQuestRefresh> pending;
        for(auto iterator=m_pendingQuestRefreshes.begin();iterator!=m_pendingQuestRefreshes.end();)
            if(iterator->Due<=now){pending.push_back(std::move(*iterator));iterator=m_pendingQuestRefreshes.erase(iterator);}
            else ++iterator;
        for(auto& request:pending) {
            auto* controller=request.Controller.Get();
            if(!IsGameplayQuestController(controller))continue;
            try {
                const QuestNative::Adapter native(controller,m_quests.Asset(request.Quest));
                if(native.IsInitialized())native.NotifyRecovery();
                ReconcileQuestLocations(controller);
            }catch(const std::exception& error){ErrorOnce("quest-refresh:"+request.Quest,PS::ToWideSafe(error.what()));}
            // Presentation is independent of the native refresh. Standalone
            // can reject a client-RPC-shaped refresh while still owning a
            // perfectly valid local HUD and accepted quest.
            if(!request.Toast.empty())try {
                // Client_DisplayTextNotification is an owning-client RPC on
                // authority and a local HUD call in standalone. GiveQuest(false)
                // remains the native primary quest banner; this is the reliable
                // initial-accept fallback after dialogue has released the HUD.
                m_events.Notify(controller,request.Toast,Events::Scope::Participant);
            } catch(const std::exception& error) {
                if(++request.ToastAttempts<4 && IsGameplayQuestController(controller)) {
                    request.Due=now+0.5;
                    m_pendingQuestRefreshes.push_back(std::move(request));
                } else ErrorOnce("quest-toast:"+request.Quest,PS::ToWideSafe(error.what()));
            }
        }
    }

    void DragonWildsNpcLoader::PumpQuestStatus()
    {
        PS::QuestStatusRequests::Available=true;
        auto request=PS::QuestStatusRequests::Take();
        if(!request)return;
        nlohmann::json players=nlohmann::json::array();
        try {
            const auto requestAction=request->value("Action",std::string{});std::string controlStatus;
            if(requestAction!="Status"&&requestAction!="Control")throw std::runtime_error("Unknown quest-status action");
            auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));
            if(!type)throw std::runtime_error("Player controller class unavailable; enter a world first");
            TArray<UObject*> controllers;UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
            if(requestAction=="Control") {
                const auto player=request->value("Player",std::string{}),quest=request->value("Quest",std::string{}),control=request->value("Control",std::string{});
                if(player.empty()||quest.empty()||(control!="start"&&control!="repeat"&&control!="abandon"&&control!="reset"))throw std::runtime_error("Invalid quest control request");
                UObject* target=nullptr;for(auto* candidate:controllers)if(IsGameplayQuestController(candidate)&&RC::to_string(candidate->GetPathName())==player){target=candidate;break;}
                if(!target)throw std::runtime_error("Selected gameplay player is no longer available");
                DialogueCompletionBinding binding;binding.QuestKey=quest;binding.QuestAction=(control=="abandon"||control=="reset")?"Abandon":"Accept";
                const auto current=[&]{return IsGameplayQuestController(target);};controlStatus=RunQuestAction(binding,target,current);
                if(control=="reset"&&controlStatus.starts_with("Quest abandoned")){binding.QuestAction="Accept";controlStatus=RunQuestAction(binding,target,current);}
            }
            for(auto* controller:controllers) {
                if(!IsGameplayQuestController(controller))continue;
                std::string playerName="Player";
                if(auto* state=ActorHelper::GetObjectRef(controller,TEXT("PlayerState")))try {
                    ActorHelper::FunctionCall call(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));call.Invoke();
                    const auto value=call.Result<FString>();const auto& chars=value.GetCharArray();
                    if(chars.Num()>1 && chars.Num()<256 && chars.GetData())playerName=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
                }catch(...) {}
                nlohmann::json quests=nlohmann::json::array();
                m_quests.ForEachVisible([&](const std::string& key,const Quests::Definition& quest,const nlohmann::json&){
                    nlohmann::json row={{"Key",key},{"Title",quest.Title},{"Objectives",nlohmann::json::array()},{"Run",0},{"Repeatable",quest.Repeat.Enabled},{"RepeatReady",false}};
                    try {
                        const QuestNative::Adapter native(controller,m_quests.Asset(key));
                        if(!native.IsInitialized())row["State"]="Uninitialized";
                        else {
                            auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");
                            if(colon!=state.npos)state=state.substr(colon+2);row["State"]=state;
                            row["Run"]=native.GetInt(FName(TEXT("RuneSchema.Run"),FNAME_Add));
                            if(state=="Complete"&&quest.Repeat.Enabled) {
                                const auto read=[&](const std::string& field){return native.GetInt(FName(RC::to_generic_string(field).c_str(),FNAME_Add));};
                                const auto time=[&](const std::string& field)->int64_t{const auto high=read(field+"Hi");if(high<0)throw std::runtime_error("Invalid saved quest timestamp");return (int64_t(high)<<32)|std::bit_cast<uint32_t>(read(field+"Lo"));};
                                row["RepeatReady"]=Quests::RepeatReady(quest.Repeat,time("RuneSchema.Completed"),Quests::ReceiptJournal::EpochNow(),time("RuneSchema.Clock"));
                            }
                            if(quest.Stages.empty())row["Objectives"].push_back({{"Id",quest.ObjectiveId},{"Text",quest.ObjectiveText},
                                {"Count",native.GetInt(FName(RC::to_generic_string(quest.ObjectiveId).c_str(),FNAME_Add))},{"Required",quest.Required.Count},{"Hidden",quest.Hidden}});
                            else for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)
                                row["Objectives"].push_back({{"Id",stage+":"+objective.ObjectiveId},{"Text",objective.ObjectiveText},
                                    {"Count",native.GetInt(Quests::StageCounter(objective))},{"Required",objective.Required.Count},{"Hidden",objective.Hidden}});
                        }
                    }catch(const std::exception& error){row["State"]="Unavailable";row["Error"]=error.what();}
                    quests.push_back(std::move(row));
                });
                players.push_back({{"Name",playerName},{"Path",RC::to_string(controller->GetPathName())},{"Quests",std::move(quests)}});
            }
            PS::QuestStatusRequests::Publish({{"Players",players},{"Status",controlStatus.empty()?std::format("Read native RuneSchema quest state for {} gameplay player(s).",players.size()):controlStatus}});
        }catch(const std::exception& error){PS::QuestStatusRequests::Publish({{"Players",players},{"Status",error.what()}});}
    }

    std::string DragonWildsNpcLoader::HandleNetworkQuestControl(UObject* player,const std::string& quest,
        const std::string& action,const std::string& payload)
    {
        if(!player || quest.empty())throw std::runtime_error("Quest control requires an owned player and quest ID");
        const auto request=nlohmann::json::parse(payload);
        if(!request.is_object() || !request.empty())
            throw std::runtime_error("Quest control payload currently accepts an empty object only");
        if(action!="start"&&action!="repeat"&&action!="abandon"&&action!="reset")
            throw std::runtime_error("Unsupported quest control action");
        ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())throw std::runtime_error("Quest control reached a non-authority player");

        UObject* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
        if(!controller) {
            ActorHelper::FunctionCall get(player,TEXT("/Script/Engine.Pawn:GetController"));get.Invoke();
            controller=get.Result<UObject*>();
        }
        if(!IsGameplayQuestController(controller))
            throw std::runtime_error("Quest control has no gameplay player controller");
        (void)m_quests.Find("_",quest);

        DialogueCompletionBinding binding;
        binding.QuestKey=quest;
        binding.QuestAction=(action=="abandon"||action=="reset")?"Abandon":"Accept";
        const auto current=[&]{return IsGameplayQuestController(controller);};
        auto result=RunQuestAction(binding,controller,current);
        if(action=="reset"&&result.starts_with("Quest abandoned")) {
            binding.QuestAction="Accept";
            result=RunQuestAction(binding,controller,current);
        }
        QueueQuestRefresh(controller,quest);
        QueueQuestLocationRefresh(controller);
        return result;
    }

    void DragonWildsNpcLoader::HandleNetworkNotification(UObject* player,const std::string& channel,
        const std::string& entity,const std::string& payload)
    {
        if(channel!="quest.state"||!player)return;
        const auto message=nlohmann::json::parse(payload);
        if(!message.is_object())throw std::runtime_error("Quest-state notification payload is invalid");
        const auto kind=message.value("kind",std::string{});
        if(kind!="changed"&&kind!="resync")
            throw std::runtime_error("Quest-state notification payload is invalid");
        UObject* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
        if(!controller) {
            ActorHelper::FunctionCall get(player,TEXT("/Script/Engine.Pawn:GetController"));get.Invoke();
            controller=get.Result<UObject*>();
        }
        if(!IsGameplayQuestController(controller))
            throw std::runtime_error("Quest-state notification has no gameplay player controller");
        if(entity!="all"&&!entity.empty())QueueQuestRefresh(controller,entity,0.0);
        QueueQuestLocationRefresh(controller);
        ReconcileQuestLocations(controller);
    }

    void DragonWildsNpcLoader::LogQuestStage(UObject* controller,const std::string& quest,size_t stage) {
        RC::StringType name=TEXT("Unknown player");
        try {
            if(auto* state=ActorHelper::GetObjectRef(controller,TEXT("PlayerState"))) {
                ActorHelper::FunctionCall call(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));call.Invoke();
                const auto value=call.Result<FString>();
                if(value.GetCharArray().Num()>1 && value.GetCharArray().Num()<=256)name=RC::StringType(*value);
            }
        }catch(...) {}
        RC::Output::send<LogLevel::Normal>(STR("[RuneSchema] Player '{}' | Quest '{}' | Stage {} complete.\n"),name,RC::to_generic_string(quest),stage);
    }

    void DragonWildsNpcLoader::ReconcileQuestLocations(UObject* controller)
    {
        if(!IsGameplayQuestController(controller))return;
        try {TryAutomaticQuests(controller);}catch(const std::exception& error){ErrorOnce("quest-auto-completion",PS::ToWideSafe(error.what()));}
        try {m_quests.ReconcileLocations(controller,[this](const std::string& key,const char* error){
            ErrorOnce("quest-marker:"+key,RC::to_generic_string("Quest marker '"+key+"': "+error));
        });}catch(const std::exception& error){
            ErrorOnce("quest-marker-reconcile",RC::to_generic_string(std::string("Quest marker reconciliation: ")+error.what()));
        }
    }

    void DragonWildsNpcLoader::QueueQuestLocationRefresh(UObject* controller)
    {
        if(!IsGameplayQuestController(controller))return;
        ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
        if(!local.Result<bool>())return;
        auto found=std::find_if(m_pendingQuestLocationRefreshes.begin(),m_pendingQuestLocationRefreshes.end(),
            [&](const auto& pending){return pending.Controller.Get()==controller;});
        if(found==m_pendingQuestLocationRefreshes.end())
            m_pendingQuestLocationRefreshes.push_back({PS::WeakObject(controller),0,60});
        else {found->Elapsed=0;found->Attempts=60;}
    }

    void DragonWildsNpcLoader::PumpQuestLocationRefreshes(float deltaSeconds)
    {
        for(auto entry=m_pendingQuestLocationRefreshes.begin();entry!=m_pendingQuestLocationRefreshes.end();) {
            auto* controller=entry->Controller.Get();
            if(!controller || !entry->Attempts) {entry=m_pendingQuestLocationRefreshes.erase(entry);continue;}
            entry->Elapsed+=deltaSeconds;
            if(entry->Elapsed<0.5){++entry;continue;}
            entry->Elapsed=0;--entry->Attempts;
            try {
                if(m_quests.NetworkManifest().empty())PrepareQuests(controller);
                ReconcileQuestLocations(controller);
            }catch(const std::exception& error){ErrorOnce("quest-marker-delivery-retry",PS::ToWideSafe(error.what()));}
            if(!entry->Attempts)entry=m_pendingQuestLocationRefreshes.erase(entry);else ++entry;
        }
    }

    void DragonWildsNpcLoader::MigrateSavedProgress(UObject* controller) {
        if(!IsGameplayQuestController(controller))return;
        ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())return;
        const auto character=DialogueCharacter(controller);
        const auto legacy=PS::HostServices::ProgressDirectory()/"quests"/(character+".json");
        PrepareQuests(controller);
        ObserveQuestInventory(controller,false);
        m_quests.ForEachVisible([&](const std::string& key,const Quests::Definition& quest,const nlohmann::json& document){
            try {
                const QuestNative::Adapter native(controller,m_quests.Asset(key));
                if(native.IsInitialized()){const Quests::NativeReceipt receipt(native,quest,document,legacy,character);}
            }catch(const std::exception& error){ErrorOnce("quest-save-import:"+key,PS::ToWideSafe(error.what()));}
        });
        m_quests.ForEachDialogueMod([&](const std::string& mod,const std::string& key){
            try {const DialogueSave::State state(controller,m_quests.Asset(key),mod,DialogueProgressPath(character),character);}
            catch(const std::exception& error){ErrorOnce("dialogue-save-import:"+key,PS::ToWideSafe(error.what()));}
        });
    }

    void DragonWildsNpcLoader::PumpQuestLocations(float deltaSeconds)
    {
        // A joining client can receive its replicated quest state well after
        // the pawn and streamed world are ready. Keep this low-frequency
        // reconciliation alive long enough to observe that initial state.
        if(m_questLocationChecks>=300)return;
        m_questLocationElapsed+=deltaSeconds;
        if(m_questLocationElapsed<1.0)return;
        m_questLocationElapsed=0;
        auto* pawn=FindLocalGameplayTestPawn();
        if(!pawn)return;
        try {
            auto* controller=ActorHelper::GetObjectRef(pawn,TEXT("Controller"));
            if(!controller || !ActorHelper::GetObjectRef(controller,TEXT("QuestProgressComponent")))return;
            ++m_questLocationChecks;
            if(m_questLocationChecks==1) {
                const auto removed=Quests::NativeQuestCleanup::Run(controller,std::filesystem::path(PS::HostServices::WorkingDirectory())/"Mods/RuneSchema/mods");
                if(removed)PS::Log<LogLevel::Normal>(STR("Quests: removed {} owned records for absent mods from the active player; changes follow the normal game save.\n"),removed);
                MigrateSavedProgress(controller);
            }
            PrepareQuests(controller);
            ReconcileQuestLocations(controller);
            ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
            if(!authority.Result<bool>())m_quests.ForEachVisible([&](const std::string& key,const Quests::Definition&,const nlohmann::json&){
                if(m_joinQuestPresented.contains(key))return;
                const QuestNative::Adapter native(controller,m_quests.Asset(key));
                auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
                if(state!="Given")return;
                // This is the same native recovery presentation used after a
                // normal objective update. It does not alter quest progress.
                native.NotifyRecovery();m_joinQuestPresented.insert(key);
            });
        }catch(const std::exception& error){
            ErrorOnce("quest-marker-startup",RC::to_generic_string(std::string("Quest marker startup: ")+error.what()));
        }
    }

    void DragonWildsNpcLoader::QueueClientReplica(AActor* actor)
    {
        if(!actor || actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))return;
        if(std::none_of(m_definitions.begin(),m_definitions.end(),[&](const auto& d){return d.Enabled && d.Multiplayer
            && (actor->GetClassPrivate()->GetPathName()==RC::to_generic_string(d.BaseActorClassPath));})) {
            if(!PresentEventIdentity)return;
            auto* identity=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,NpcIdentity::ClassPath,false);
            if(!identity || actor->GetComponentsByClass(identity).Num()!=1)return;
        }
        const auto path=actor->GetPathName();
        if(std::any_of(m_pendingClientReplicas.begin(),m_pendingClientReplicas.end(),[&](const auto& p){return p.Path==path;}))return;
        if(m_pendingClientReplicas.size()>=256) {
            WarnOnce("client-npc-queue-limit",TEXT("NPC client arrival queue is full; interaction can retry identity setup."));return;
        }
        m_pendingClientReplicas.push_back({path,0});
    }

    void DragonWildsNpcLoader::HandleNetworkWorldState(const std::string& payload)
    {
        try {
            const auto snapshot=nlohmann::json::parse(payload);
            if(snapshot.value("kind",std::string{})!="RuneSchemaWorldSnapshot"
                || snapshot.value("version",0)!=1 || !snapshot.contains("instances")
                || !snapshot["instances"].is_array())return;
            for(const auto& record:snapshot["instances"]) {
                if(!record.is_object() || record.value("lifecycle",std::string("active"))!="active")continue;
                const auto kind=record.value("kind",std::string{});
                if(kind!="npc" && kind!="event")continue;
                const auto path=RC::to_generic_string(record.value("actor",std::string{}));
                if(path.empty())continue;
                if(auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path.c_str(),false);
                    object && object->IsA<AActor>()) QueueClientReplica(static_cast<AActor*>(object));
            }
        } catch(const std::exception& error) {
            const auto detail=std::string("NPC world-state presentation ignored: ")+error.what();
            ErrorOnce("world-state-npc",PS::ToWideSafe(detail.c_str()));
        }
    }

    void DragonWildsNpcLoader::PumpClientReplicas(float deltaSeconds)
    {
        if(m_pendingClientReplicas.empty())return;
        m_clientReplicaElapsed+=deltaSeconds;
        if(m_clientReplicaElapsed<0.5)return;
        m_clientReplicaElapsed=0;
        auto pending=std::move(m_pendingClientReplicas);
        m_pendingClientReplicas.clear();
        for(auto& entry:pending) {
            try {
                auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,entry.Path.c_str(),false);
                if(!object || !object->IsA<AActor>() || object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))continue;
                auto* actor=static_cast<AActor*>(object);
                ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
                if(authority.Result<bool>())continue;
                auto* ai=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
                auto* identityType=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,NpcIdentity::ClassPath,false);
                const bool toolResource=identityType && actor->GetComponentsByClass(identityType).Num()==1
                    && nlohmann::json::parse(ReadIdentity(actor->GetComponentsByClass(identityType)[0])).value("kind",std::string{})=="tool-resource";
                if(((ai && actor->IsA(ai)) || toolResource) && PresentEventIdentity) {
                    auto* type=RequireIdentityClass();
                    const auto components=actor->GetComponentsByClass(type);
                    if(components.Num()!=1)throw std::runtime_error("Event actor needs exactly one identity component");
                    if(PresentEventIdentity(actor,ReadIdentity(components[0])))continue;
                } else if(PrepareClientReplica(actor))continue;
            }catch(const std::exception& error){ErrorOnce("client-npc:"+RC::to_string(entry.Path),PS::ToWideSafe(error.what()));continue;}
            catch(...) {continue;}
            if(++entry.Attempts<NpcNetwork::ClientArrivalAttempts)m_pendingClientReplicas.push_back(entry);
            // Unmarked vanilla NPCs also pass the class filter; leave them untouched.
        }
    }

    bool DragonWildsNpcLoader::PrepareClientReplica(AActor* actor)
    {
        if(!actor || m_preparingClientReplica || actor->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return false;
        m_preparingClientReplica=true;
        struct Guard {bool& Active;~Guard(){Active=false;}} guard{m_preparingClientReplica};
        auto* matched=FindClientDefinition(actor);
        if(!matched)return false;
        auto& definition=*matched;
        if(!NpcNetwork::Supported(definition.Multiplayer,definition.Human,definition.Resource,
            definition.Stage==VendorPolicy::VendorStage::Merchant,!definition.DialogueKey.empty(),!definition.RequiredFlag.empty(),!definition.LoreEntry.empty(),definition.Stage==VendorPolicy::VendorStage::Visual))return false;
        if(PS::Network::Detect(actor).Mode!=PS::Network::Role::Client)return false;
        ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(authority.Result<bool>() || !ResolveDefinitionClasses(definition) || !actor->IsA(definition.BaseActorClass))return false;
        const auto interactions=definition.InteractionClass?actor->GetComponentsByClass(definition.InteractionClass):TArray<UObject*>{};
        const bool merchant=definition.Stage==VendorPolicy::VendorStage::Merchant;
        const auto stations=merchant?actor->GetComponentsByClass(definition.VendorClass):TArray<UObject*>{};
        const bool visual=definition.Stage==VendorPolicy::VendorStage::Visual;
        if(visual && m_applied.contains(ActorKey(actor,definition))){ApplyNetworkDialogueCue(actor,ReadIdentity(actor->GetComponentsByClass(RequireIdentityClass())[0]));return true;}
        if(!visual && !NpcNetwork::ClientComponentsReady(interactions.Num(),stations.Num(),merchant))return false;
        const auto key=definition.ModName+":"+definition.Id;
        for(const auto& binding:m_merchantBindings)if(!visual && binding.DefinitionKey==key) {
            // Compare only against freshly resolved live objects. This UE4SS build
            // does not allocate a serial when constructing FWeakObjectPtr.
            if(binding.ActorToken==actor && binding.InteractionToken==interactions[0]
                && binding.StationToken==(merchant?stations[0]:nullptr)) {
                // OnRep_IdentityPayload re-enters through this already-configured
                // path for dialogue poses/emotes. Consume the newer cue revision
                // before returning; otherwise only the authority sees the cue.
                ApplyNetworkDialogueCue(actor,ReadIdentity(actor->GetComponentsByClass(RequireIdentityClass())[0]));
                return true;
            }
            if(auto* existing=FindPersistentVendor(actor->GetWorld(),definition);existing && existing!=actor)
                throw std::runtime_error("Multiple live client NPCs match one definition");
        }
        std::erase_if(m_merchantBindings,[&](const auto& binding){return binding.DefinitionKey==key;});
        m_applied.erase(ActorKey(actor,definition));
        if(definition.InlineMerchant)CreateInlineMerchantRow(definition);
        SetPersistentVendorId(actor,definition);
        actor->SetActorScale3D(FVector(definition.SpawnScale[0],definition.SpawnScale[1],definition.SpawnScale[2]));
        ApplyVendorDisplayName(actor,definition);
        if(!ApplyVendorVisuals(actor,definition))return false;
        SetVendorVisible(actor,definition);
        if(definition.Human)ApplyHumanVisuals(actor,definition);
        ConfigureNpcInteraction(actor,definition);
        if(!definition.DialogueKey.empty() || !definition.LockedDialogueKey.empty())ConfigureDialogueParticipant(actor,definition);
        ConfigureNpcMarkers(actor,definition);
        if(visual)m_applied.insert(ActorKey(actor,definition));
        else if(!ApplyVendor(actor,definition))throw std::runtime_error("Replicated NPC merchant setup failed");
        std::erase_if(m_spawnedVendors,[&](const auto& entry){return entry.Key==key;});
        m_spawnedVendors.push_back({key,PS::WeakObject(actor)});
        ApplyNetworkDialogueCue(actor,ReadIdentity(actor->GetComponentsByClass(RequireIdentityClass())[0]));
        definition.SpawnGate.Begin();
        return true;
    }

    DragonWildsNpcLoader::VendorDefinition* DragonWildsNpcLoader::FindClientDefinition(AActor* actor)
    {
        if(!actor || !IsNpcObjectUsable(actor))return nullptr;
        auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,NpcIdentity::ClassPath,false);
        if(!type)return nullptr;
        const auto components=actor->GetComponentsByClass(type);
        if(components.Num()==0)return nullptr;
        if(components.Num()!=1)throw std::runtime_error("Multiple RuneSchema identity components on client NPC");
        const auto text=ReadIdentity(components[0]);
        if(text.empty())return nullptr;
        const auto identity=NpcIdentity::Decode(text);
        VendorDefinition* found=nullptr;
        for(auto& definition:m_definitions)if(definition.ModName==identity.Mod && definition.Id==identity.Npc) {
            if(found)throw std::runtime_error("Duplicate local RuneSchema NPC definition");
            found=&definition;
        }
        if(!found || !found->Enabled || !found->Multiplayer)
            throw std::runtime_error("Server NPC identity has no enabled multiplayer definition on this client");
        if(NetworkDefinitionFingerprint(actor,*found)!=identity.Fingerprint)
            throw std::runtime_error("Server/client NPC or dialogue/quest registry mismatch (server="+identity.Fingerprint+", client="+NetworkDefinitionFingerprint(actor,*found)+"); presentation refused");
        return found;
    }

    std::string DragonWildsNpcLoader::NetworkDefinitionFingerprint(UObject* context,const VendorDefinition& definition) {
        if(definition.DialogueKey.empty() && definition.LockedDialogueKey.empty())return definition.NetworkGameplayFingerprint;
        const auto key=definition.ModName+":"+definition.Id;
        if(const auto found=m_networkDialogueFingerprints.find(key);found!=m_networkDialogueFingerprints.end())return found->second;
        nlohmann::json graphs=nlohmann::json::object();
        for(const auto& id:{definition.DialogueKey,definition.LockedDialogueKey}) {
            if(id.empty())continue;
            const auto& graph=m_dialogues.at(id);
            nlohmann::json events=nlohmann::json::object();
            for(const auto& event:graph.Events)events[event]=m_events.NetworkManifest(event);
            for(const auto& questKey:graph.Quests) {
                const auto& quest=m_quests.Find("_",questKey);
                const auto include=[&](const auto& objective) {
                    if(objective.Kill && !objective.Kill->EventKey.empty()) {
                        const auto& event=objective.Kill->EventKey;
                        events[event]=m_events.NetworkManifest(event);
                        if(!objective.Kill->SpawnKey.empty())m_events.RequireSpawn(event,objective.Kill->SpawnKey);
                    }
                };
                include(quest);
                for(const auto& [stage,objectives]:quest.Stages)for(const auto& objective:objectives)include(objective);
            }
            graphs[id]=events.empty()?graph.Data:nlohmann::json{{"dialogue",graph.Data},{"events",events}};
        }
        PrepareQuests(context);
        auto fingerprint=NpcNetwork::DialogueFingerprint(definition.NetworkGameplayFingerprint,graphs,m_quests.NetworkManifest());
        m_networkDialogueFingerprints.emplace(key,fingerprint);
        return fingerprint;
    }

    void DragonWildsNpcLoader::PublishEventIdentity(AActor* actor,const std::string& payload)
    {
        if(!actor || m_gameThreadId.load(std::memory_order_relaxed)!=GetCurrentThreadId())
            throw std::runtime_error("Event identity must be assigned on the game thread");
        const auto identity=Events::DecodeIdentity(payload);
        const bool resource=identity.at("kind")=="tool-resource";
        if(resource)(void)Events::ToolIdentity(identity);
        auto* ai=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
        if(!resource && (!ai || !actor->IsA(ai)))throw std::runtime_error("Event identity requires an AI actor");
        ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())throw std::runtime_error("Only authority can publish event identity");
        auto* type=RequireIdentityClass();
        if(actor->GetComponentsByClass(type).Num()!=0)throw std::runtime_error("Event actor already has a RuneSchema identity");
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetReplicateMovement")).Arg(TEXT("bInReplicateMovement"),true).Invoke();
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetReplicates")).Arg(TEXT("bInReplicates"),true).Invoke();
        auto* component=EnsureComponent(actor,type);
        PropertyHelper::CopyJsonValueToContainer(component,IdentityField(type),payload);
        if(ReadIdentity(component)!=payload)throw std::runtime_error("Event identity write verification failed");
        VendorDefinition label;label.Id="event-ai";
        RegisterComponent(actor,component,label);
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled")).Arg(TEXT("bEnabled"),false).Invoke();
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated")).Arg(TEXT("ShouldReplicate"),true).Invoke();
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:FlushNetDormancy")).Invoke();
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:ForceNetUpdate")).Invoke();
        if(PublishWorldState) {
            const auto path=RC::to_string(actor->GetPathName());const auto id="event:"+std::to_string(std::hash<std::string>{}(path));
            PublishWorldState(id,nlohmann::json{{"kind","event"},{"actor",path},{"identity",identity},{"lifecycle","active"}}.dump());
        }
    }

    void DragonWildsNpcLoader::ApplyNetworkIdentity(AActor* actor,const VendorDefinition& definition)
    {
        auto* type=RequireIdentityClass();
        if(actor->GetComponentsByClass(type).Num()>1)throw std::runtime_error("Multiple RuneSchema identity components on server NPC");
        ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())throw std::runtime_error("Only authority can assign NPC identity");
        auto* component=EnsureComponent(actor,type);
        const auto payload=NpcIdentity::Encode(definition.ModName,definition.Id,NetworkDefinitionFingerprint(actor,definition));
        PropertyHelper::CopyJsonValueToContainer(component,IdentityField(type),payload);
        if(ReadIdentity(component)!=payload)throw std::runtime_error("NPC identity write verification failed");
        RegisterComponent(actor,component,definition);
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled"))
            .Arg(TEXT("bEnabled"),false).Invoke();
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated"))
            .Arg(TEXT("ShouldReplicate"),true).Invoke();
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:FlushNetDormancy")).Invoke();
        ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:ForceNetUpdate")).Invoke();
        if(PublishWorldState) {
            const auto path=RC::to_string(actor->GetPathName());const auto id=definition.ModName+":"+definition.Id+":"+std::to_string(std::hash<std::string>{}(path));
            PublishWorldState(id,nlohmann::json{{"kind","npc"},{"mod",definition.ModName},{"definition",definition.Id},{"fingerprint",NetworkDefinitionFingerprint(actor,definition)},{"actor",path},{"lifecycle","active"},{"timeOfDay",TimeOfDay::Name(definition.Time)}}.dump());
        }
    }

    void DragonWildsNpcLoader::ScanLoadedActors()
    {
        auto* world = FindLoadedWorld();
        if (world && std::none_of(m_definitions.begin(), m_definitions.end(),
            [&](const auto& definition) {
                return !definition.HelpyTemporary && definition.Enabled && definition.SpawnGate.Pending()
                    && definition.CellReadyWorld == world;
            })) {
            if (m_vendorReport.value("Phase", std::string{}) != "Merchant.WaitingForCell") {
                m_vendorReport["Phase"] = "Merchant.WaitingForCell";
                WriteVendorStatus();
            }
            return;
        }
        if (m_scanBudget.Begin(world != nullptr))
        {
            // Menu/loading time must not exhaust the bounded asset-ready scans.
            m_vendorReport["ScanPasses"] = m_scanBudget.Passes();
            m_vendorReport["Phase"] = "Merchant.Scanning";
            m_vendorReport["World"] = RC::to_string(world->GetPathName());
            TrySpawnConfiguredVendors(world);
            const auto pending = std::count_if(m_definitions.begin(), m_definitions.end(),
                [](const auto& definition) { return !definition.HelpyTemporary && definition.Enabled && definition.SpawnGate.Pending(); });
            m_vendorReport["PendingDefinitionCount"] = pending;
            if (!pending) m_vendorReport["Phase"] = "Merchant.AttemptsFinished";
            if (m_scanBudget.Exhausted()) {
                m_vendorReport["ScanBudgetExhausted"] = true;
                if (pending) m_vendorReport["Phase"] = "Merchant.ScanBudgetExhausted";
            }
            WriteVendorStatus();
        }
    }

    void DragonWildsNpcLoader::TrySpawnConfiguredVendors(UWorld* world)
    {
        if (!world || m_spawning)
        {
            return;
        }
        // Only the bounded timer invokes spawning; guard any synchronous reentry.
        m_spawning = true;
        struct SpawnGuard {
            bool& Active;
            ~SpawnGuard() { Active = false; }
        } guard{m_spawning};

        const auto network=PS::Network::Detect(world);
        if(network.Mode==PS::Network::Role::Unknown)return;
        const bool multiplayer=network.Mode!=PS::Network::Role::Standalone;
        for (auto& definition : m_definitions)
        {
            if (definition.HelpyTemporary || !definition.Enabled || !definition.SpawnGate.Pending()
                || definition.CellReadyWorld != world)
            {
                continue;
            }

            const auto key = definition.ModName + ":" + definition.Id;
            if(network.Mode!=PS::Network::Role::Client && definition.Time!=TimeOfDay::Requirement::Any) {
                try { if(!NpcTimeAllows(world,definition))continue; }
                catch(const std::exception& error) {
                    WarnOnce("npc-time-wait:"+key,RC::to_generic_string(std::string("NPC '")+key+"' is waiting for the native time-of-day actor: "+error.what()));
                    continue;
                }
            }
            if(multiplayer && !NpcNetwork::Supported(definition.Multiplayer,definition.Human,definition.Resource,
                definition.Stage==VendorPolicy::VendorStage::Merchant,!definition.DialogueKey.empty(),!definition.RequiredFlag.empty(),!definition.LoreEntry.empty(),definition.Stage==VendorPolicy::VendorStage::Visual)) {
                WarnOnce("network-unsupported:"+key,RC::to_generic_string("NPC '"+key+"' requires Multiplayer=true for multiplayer; skipped."));
                definition.SpawnGate.Begin();continue;
            }
            if(network.Mode==PS::Network::Role::Client) {
                if(!ResolveDefinitionClasses(definition))continue;
                TArray<UObject*> candidates;
                UECustom::UObjectGlobals::GetObjectsOfClass(definition.BaseActorClass,candidates,true,
                    static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));
                for(auto* candidate:candidates)if(candidate && candidate->GetWorld()==world)
                    QueueClientReplica(static_cast<AActor*>(candidate));
                continue;
            }
            const auto alreadySpawned = std::find_if(
                m_spawnedVendors.begin(), m_spawnedVendors.end(),
                [&](const auto& entry) { return entry.Key == key; });
            if (alreadySpawned != m_spawnedVendors.end())
            {
                continue;
            }

            try
            {
                if (!definition.Human && definition.VisualMeshPath.empty()
                    && !ResolveVisualSourceClass(definition)
                    && !FindVisualSourceActor(definition))
                {
                    WarnOnce("visual-source-wait:" + key,
                        RC::to_generic_string(std::format(
                            "Vendor '{}' is waiting for VisualSource '{}' to load before spawning its proxy.",
                            definition.Id, definition.VisualSourcePath)));
                    continue;
                }

                // Readiness may retry within the timer budget, but actor
                // construction and row mutation get one attempt per map.
                if (!ResolveDefinitionClasses(definition)) continue;
                if (!definition.SpawnGate.Begin()) continue;
                if (definition.Stage == VendorPolicy::VendorStage::Merchant && definition.InlineMerchant) {
                    RecordPhase(definition, "MerchantRow.Before");
                    CreateInlineMerchantRow(definition);
                    RecordPhase(definition, "MerchantRow.Ready");
                }
                if (auto* actor = SpawnVisualVendor(world, definition))
                {
                    m_spawnedVendors.push_back({ key, PS::WeakObject(actor) });
                }
                else
                {
                    ErrorOnce("spawn:" + key, RC::to_generic_string(std::format(
                        "Vendor '{}' stopped after a failed proxy/attachment attempt; no retry until next map or restart.", definition.Id)));
                }
            }
            catch (const std::exception& error)
            {
                ErrorOnce("spawn:" + key,
                    RC::to_generic_string(std::format(
                        "Vendor '{}' stopped for this map before completing its proxy: {}",
                        definition.Id, error.what())));
                definition.SpawnGate.Begin();
            }
            catch (...)
            {
                definition.SpawnGate.Begin();
                ErrorOnce("spawn:" + key, RC::to_generic_string(std::format(
                    "Vendor '{}' stopped for this map after an unknown failure.", definition.Id)));
            }
        }
    }

    AActor* DragonWildsNpcLoader::SpawnVisualVendor(
        UWorld* world,
        VendorDefinition& definition)
    {
        // Raw component bookkeeping is valid only within this synchronous spawn.
        struct ComponentsGuard {
            std::unordered_set<UObject*>& Components;
            ~ComponentsGuard() { Components.clear(); }
        } componentsGuard{m_createdComponents};
        if (!ResolveDefinitionClasses(definition))
        {
            return nullptr;
        }

        const auto network=PS::Network::Detect(world);
        if(!PS::Network::OwnsGameplay(network.Mode))throw std::runtime_error("NPC creation requires server authority");
        const bool multiplayer=network.Mode!=PS::Network::Role::Standalone;

        if(multiplayer) {
            (void)RequireIdentityClass();
            (void)NpcIdentity::Encode(definition.ModName,definition.Id,NetworkDefinitionFingerprint(world,definition));
        }
        auto* actor = definition.HelpyTemporary?nullptr:FindPersistentVendor(world, definition);
        bool restored = actor != nullptr;
        const auto retire=[&] {
            if(!actor)return;
            const auto index=actor->GetInternalIndex();
            auto* slot=index>=0?FUObjectArray::IndexToObject(index):nullptr;
            if(!slot || slot->GetUObject()!=actor)
                throw std::runtime_error("Cannot verify NPC retirement identity");
            const auto serial=slot->GetSerialNumber();
            const auto path=actor->GetPathName();
            // Validate through the engine, not UE4SS's older internal GC flags.
            if(IsNpcObjectUsable(actor))ActorHelper::DestroyActor(actor);
            auto* after=FUObjectArray::IndexToObject(index);
            const bool sameSlot=after && after->GetUObject()==actor && after->GetSerialNumber()==serial;
            const bool usable=sameSlot && IsNpcObjectUsable(actor);
            if(!VendorIdentity::RetirementConfirmed(sameSlot,usable))
                throw std::runtime_error("NPC remains usable after destruction; replacement cancelled to prevent duplicate identity");
            if(std::none_of(m_retiredVendors.begin(),m_retiredVendors.end(),[&](const auto& old) {
                return VendorIdentity::RetiredInstanceMatches(old.Address,actor,old.Index,index,old.Serial,serial,old.Path==path);
            }))m_retiredVendors.push_back({actor,index,serial,path});
        };
        const auto quarantine=[&] {
            if(!actor)return;
            const auto attempt=[&](const char* operation,const auto& action) {
                try { action(); } catch(const std::exception& error) {
                    ErrorOnce("npc-quarantine:"+definition.ModName+":"+definition.Id+":"+operation,
                        RC::to_generic_string(std::string("Failed NPC cleanup ")+operation+": "+error.what()));
                }
            };
            attempt("collision",[&] {
                ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorEnableCollision"))
                    .Arg(TEXT("bNewActorEnableCollision"),false).Invoke();
            });
            attempt("visibility",[&] {
                ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorHiddenInGame"))
                    .Arg(TEXT("bNewHidden"),true).Invoke();
            });
            attempt("components",[&] {
                auto* primitive=ActorHelper::ResolveClass(TEXT("/Script/Engine.PrimitiveComponent"));
                if(!primitive)throw std::runtime_error("PrimitiveComponent class unavailable");
                for(auto* component:actor->GetComponentsByClass(primitive))if(component) {
                    attempt("component-collision",[&] {
                        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionEnabled"))
                            .Arg(TEXT("NewType"),uint8_t(0)).Invoke();
                    });
                }
            });
            for(const auto* path:{TEXT("/Script/Dominion.InteractionComponent"),
                TEXT("/Script/Engine.ChildActorComponent"),TEXT("/Script/Engine.BillboardComponent"),
                TEXT("/Script/MinimapPlugin.MapIconComponent")}) {
                attempt("transient-components",[&] {
                    auto* type=ActorHelper::ResolveClass(path);
                    if(!type)throw std::runtime_error("NPC component class unavailable");
                    for(auto* component:actor->GetComponentsByClass(type))if(component) {
                        std::erase_if(m_npcNames,[&](const auto& entry){return entry.second.Token==component;});
                        ActorHelper::DestroyComponent(component);
                    }
                });
            }
            std::erase_if(m_merchantBindings,[&](const auto& binding){return binding.Actor.Get()==actor;});
            std::erase_if(m_npcNames,[&](const auto& entry){return entry.second.Token==actor;});
            m_applied.erase(ActorKey(actor,definition));
        };
        try {
        if(definition.GroundToSurface) {
            FVector impact{};std::string error;
            std::vector<AActor*> ignored;
            if(actor)ignored.push_back(actor);
            if(!UECustom::UKismetSystemLibrary::LineTraceGround(world,
                FVector(definition.TargetLocation[0],definition.TargetLocation[1],5000.0),
                FVector(definition.TargetLocation[0],definition.TargetLocation[1],-10000.0),ignored,impact,error))
                throw std::runtime_error(error.empty()?"NPC Location.Z $ found no blocking surface":"NPC ground trace failed: "+error);
            definition.TargetLocation[2]=impact.Z()+definition.GroundOffset;
            RecordPhase(definition,"Placement.Grounded",nullptr,{{"GroundZ",impact.Z()},{"PlacedZ",definition.TargetLocation[2]}});
        }
        if(actor) {
                const auto readTransform=[&](const TCHAR* path,bool rotation) {
                    auto* getter=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path);
                    auto* result=getter?CastField<FStructProperty>(getter->GetReturnProperty()):nullptr;
                    if(!result || result->GetSize()!=sizeof(double)*3 || !result->GetStruct()
                        || result->GetOffset_Internal()<0 || result->GetOffset_Internal()+result->GetSize()>getter->GetParmsSize())
                        throw std::runtime_error("NPC transform getter layout unavailable");
                    for(auto* parameter:TFieldRange<FProperty>(getter,EFieldIterationFlags::Default))
                        if(parameter->HasAnyPropertyFlags(CPF_Parm) && parameter!=result)
                            throw std::runtime_error("NPC transform getter has unexpected inputs");
                    size_t axisIndex=0;
                    for(const auto* axis:rotation?std::array<const TCHAR*,3>{TEXT("Pitch"),TEXT("Yaw"),TEXT("Roll")}
                        :std::array<const TCHAR*,3>{TEXT("X"),TEXT("Y"),TEXT("Z")}) {
                        auto* coordinate=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(result->GetStruct().Get(),axis));
                        if(!coordinate || !coordinate->IsFloatingPoint() || coordinate->GetSize()!=sizeof(double)
                            || coordinate->GetOffset_Internal()!=axisIndex++*sizeof(double))
                            throw std::runtime_error("NPC transform getter coordinates changed");
                    }
                    ActorHelper::FunctionCall call(actor,path);call.Invoke();
                    return call.Result<std::array<double,3>>();
                };
                auto location=readTransform(TEXT("/Script/Engine.Actor:K2_GetActorLocation"),false);
                auto rotation=readTransform(TEXT("/Script/Engine.Actor:K2_GetActorRotation"),true);
            const bool changed=!NpcPlacement::Matches(definition.TargetLocation,location)
                || !NpcPlacement::Matches(definition.SpawnRotation,rotation,true);
            const auto previousName=RC::to_string(actor->GetName());
            const bool identityChanged=NpcNetwork::NeedsIdentityReplacement(multiplayer,previousName,definition.NetworkActorName);
            if(changed || identityChanged) {
                RecordPhase(definition,"Placement.ReplacementRequired",actor,
                    {{"PreviousLocation",location},{"Location",definition.TargetLocation},
                     {"PlacementChanged",changed},{"NetworkIdentityReplacement",identityChanged},
                     {"PreviousName",previousName},{"ExpectedNetworkName",definition.NetworkActorName}});
                PS::Log<LogLevel::Normal>(STR("NPC '{}:{}' replacing saved proxy (placement changed={}, network identity changed={}).\n"),
                    RC::to_generic_string(definition.ModName),RC::to_generic_string(definition.Id),changed,identityChanged);
                // Match /spawns relocation: retire the owned saved actor before
                // constructing its replacement with the same stable identity.
                quarantine();
                retire();
                actor=nullptr;
                restored=false;
                RecordPhase(definition,"Placement.Replacing",nullptr,
                    {{"PreviousLocation",location},{"Location",definition.TargetLocation},
                     {"NetworkIdentityReplacement",identityChanged},{"PreviousName",previousName},
                     {"ExpectedNetworkName",definition.NetworkActorName}});
                if(identityChanged)PS::Log<LogLevel::Normal>(STR("NPC '{}:{}' replacing saved actor '{}' with network identity '{}'; persistence GUID retained.\n"),
                    RC::to_generic_string(definition.ModName),RC::to_generic_string(definition.Id),
                    RC::to_generic_string(previousName),RC::to_generic_string(definition.NetworkActorName));
            }
        }
        if (!actor) {
        RecordPhase(definition, "Spawn.Before");
        actor = ActorHelper::SpawnActor(
            world,
            definition.BaseActorClass,
            FVector(definition.TargetLocation[0], definition.TargetLocation[1],
                definition.TargetLocation[2]),
            FRotator(definition.SpawnRotation[0], definition.SpawnRotation[1],
                definition.SpawnRotation[2]),
            [&](AActor* pending) {
                actor=pending; // Retain cleanup ownership if deferred initialization throws.
                if(multiplayer && (!pending->Rename(RC::to_generic_string(definition.NetworkActorName).c_str())
                    || RC::to_string(pending->GetName())!=definition.NetworkActorName))
                    throw std::runtime_error("Stable network NPC name could not be assigned exactly");
                if(definition.HelpyTemporary)HelpyNpcGuards::Exclude(pending);
                else SetPersistentVendorId(pending, definition);
                RecordPhase(definition, "Spawn.Allocated", pending);
                for (const auto* field : {TEXT("AutoPossessAI"), TEXT("AutoPossessPlayer")}) {
                    if (auto* property = PropertyHelper::GetPropertyByName(pending->GetClassPrivate(), field))
                        PropertyHelper::CopyJsonValueToContainer(pending, property, "Disabled");
                }
                if (auto* property = PropertyHelper::GetPropertyByName(pending->GetClassPrivate(), TEXT("AIControllerClass")))
                    PropertyHelper::CopyJsonValueToContainer(pending, property, nullptr);
                // Populate native components before FinishSpawningActor runs
                // construction/BeginPlay, then verify visuals again afterward.
                pending->SetActorScale3D(FVector(definition.SpawnScale[0], definition.SpawnScale[1], definition.SpawnScale[2]));
                ApplyVendorDisplayName(pending, definition);
                if(!definition.Human && !definition.Resource)ApplyVendorVisuals(pending, definition);
                SetVendorVisible(pending,definition);
            },
            ESpawnActorScaleMethod::OverrideRootScale);
        }

        if (!actor) throw std::runtime_error("SpawnActor returned null");
        if(definition.HelpyTemporary)HelpyNpcGuards::VerifyExcluded(actor);
        RecordPhase(definition, restored ? "Persistence.Reused" : "Spawn.Finished", actor,
            {{"RestoredActor", restored}, {"PersistentGuidWords", definition.PersistentId}});

            actor->SetActorScale3D(FVector(
                definition.SpawnScale[0], definition.SpawnScale[1],
                definition.SpawnScale[2]));
            ApplyVendorDisplayName(actor, definition);
            ApplyVendorVisuals(actor, definition);
            SetVendorVisible(actor,definition);
            if(definition.Human && PS::Network::Detect(actor).Mode!=PS::Network::Role::DedicatedServer)ApplyHumanVisuals(actor,definition);
            // The default Character collision is a fallback only for true.
            // An explicit false must not silently retain collision.
            {
                try
                {
                    ActorHelper::FunctionCall enableCollision(actor,
                        TEXT("/Script/Engine.Actor:SetActorEnableCollision"));
                    enableCollision.Arg(TEXT("bNewActorEnableCollision"), definition.EnableCollision).Invoke();
                }
                catch (const std::exception&)
                {
                    if (!definition.EnableCollision)
                        throw;
                    // Character's capsule collision is enabled by default.
                    // Keep that native default if the optional reflected
                    // setter is unavailable in this game build.
                }
            }

            if (definition.Stage != VendorPolicy::VendorStage::Visual && !ApplyVendor(actor, definition))
            {
                throw std::runtime_error("NPC interaction setup failed");
            }
            ConfigureNpcInteraction(actor,definition);
            if(!definition.DialogueKey.empty() || !definition.LockedDialogueKey.empty())ConfigureDialogueParticipant(actor,definition);
            if(PS::Network::HasLocalPresentation(network.Mode))ConfigureNpcMarkers(actor,definition);
            if(multiplayer) {
                ApplyNetworkIdentity(actor,definition);
                if(RC::to_string(actor->GetName())!=definition.NetworkActorName)
                    throw std::runtime_error("NPC network identity changed during setup; replication cancelled");
                for(auto* type:{definition.InteractionClass,definition.VendorClass}) {
                    if(definition.Stage==VendorPolicy::VendorStage::Visual)continue;
                    if(!type && definition.Stage==VendorPolicy::VendorStage::Interaction)continue;
                    auto components=actor->GetComponentsByClass(type);
                    if(components.Num()!=1)throw std::runtime_error("Network NPC requires exactly one interaction and merchant component");
                    ActorHelper::FunctionCall(components[0],TEXT("/Script/Engine.ActorComponent:SetIsReplicated"))
                        .Arg(TEXT("ShouldReplicate"),true).Invoke();
                }
                ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetReplicateMovement"))
                    .Arg(TEXT("bInReplicateMovement"),true).Invoke();
                ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetReplicates"))
                    .Arg(TEXT("bInReplicates"),true).Invoke();
            }

            PS::RoutineLog("npc",STR("NPC '{}:{}' {}.\n"),
                RC::to_generic_string(definition.ModName),RC::to_generic_string(definition.Id),
                restored?TEXT("restored"):TEXT("created"));
            return actor;
        }
        catch (...)
        {
            const auto failure=std::current_exception();
            try { QueueNpcCleanup(actor); }
            catch(const std::exception& error) {
                ErrorOnce("npc-queue-cleanup:"+definition.ModName+":"+definition.Id,PS::ToWideSafe(error.what()));
            }
            // Cleanup stages are independent. A queue or component failure must
            // not prevent quarantine and native destruction from being tried.
            try { quarantine(); }
            catch(const std::exception& error) {
                ErrorOnce("npc-quarantine-final:"+definition.ModName+":"+definition.Id,PS::ToWideSafe(error.what()));
            }
            try { retire(); }
            catch(const std::exception& error) {
                ErrorOnce("npc-retire:"+definition.ModName+":"+definition.Id,
                    RC::to_generic_string(std::string("Failed to retire NPC proxy after cleanup: ")+error.what()));
            }
            std::rethrow_exception(failure);
        }
    }

    UObject* DragonWildsNpcLoader::FindMeshComponent(UObject* actor) const
    {
        if (!actor)
        {
            return nullptr;
        }

        try
        {
            if (auto* mesh = ActorHelper::GetObjectRef(actor, TEXT("Mesh")))
            {
                return mesh;
            }
        }
        catch (const std::exception&)
        {
            // The generic Character property is not present on every custom
            // proxy class. Fall back to the component list below.
        }

        auto* meshClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.SkeletalMeshComponent"));
        if (!meshClass)
        {
            return nullptr;
        }

        auto* liveActor = actor->IsA(AActor::StaticClass())
            ? static_cast<AActor*>(actor) : nullptr;
        if (!liveActor)
        {
            return nullptr;
        }

        for (auto* component : liveActor->GetComponentsByClass(meshClass))
        {
            if (component && component->IsA(meshClass))
            {
                return component;
            }
        }
        return nullptr;
    }

    AActor* DragonWildsNpcLoader::FindVisualSourceActor(
        const VendorDefinition& definition) const
    {
        if (definition.VisualSourcePath.empty())
        {
            return nullptr;
        }

        const auto slash = definition.VisualSourcePath.find_last_of('/');
        const auto dot = definition.VisualSourcePath.find_last_of('.');
        const auto assetName = definition.VisualSourcePath.substr(
            slash == std::string::npos ? 0 : slash + 1,
            (dot != std::string::npos && dot > slash)
                ? dot - (slash == std::string::npos ? 0 : slash + 1)
                : std::string::npos);
        const auto expectedClassName = assetName.ends_with("_C")
            ? assetName : assetName + "_C";

        TArray<UObject*> instances;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            AActor::StaticClass(), instances, true);
        for (auto* object : instances)
        {
            if (!object || !object->IsA(AActor::StaticClass())
                || object->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }

            auto* actor = static_cast<AActor*>(object);
            if (!actor->GetWorld())
            {
                continue;
            }

            auto* liveClass = actor->GetClassPrivate();
            if (!liveClass)
            {
                continue;
            }
            const auto liveName = RC::to_string(liveClass->GetName());
            const auto livePath = RC::to_string(liveClass->GetPathName());
            if (liveName == expectedClassName
                || livePath.ends_with("." + expectedClassName)
                || livePath == definition.VisualSourcePath)
            {
                return actor;
            }
        }
        return nullptr;
    }

    UClass* DragonWildsNpcLoader::ResolveVisualSourceClass(
        const VendorDefinition& definition) const
    {
        const auto& sourcePath = definition.VisualSourcePath;
        if (sourcePath.empty())
        {
            return nullptr;
        }

        const auto resolveClassObject = [](UObject* object) -> UClass* {
            if (!object)
            {
                return nullptr;
            }
            if (object->IsA<UClass>())
            {
                return static_cast<UClass*>(object);
            }

            auto* generatedProperty = PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("GeneratedClass"));
            auto* generatedObjectProperty = generatedProperty
                ? CastField<FObjectPropertyBase>(generatedProperty) : nullptr;
            if (!generatedObjectProperty)
            {
                return nullptr;
            }

            auto* generatedObject = generatedObjectProperty->GetObjectPropertyValue(
                generatedObjectProperty->ContainerPtrToValuePtr<void>(object));
            return generatedObject && generatedObject->IsA<UClass>()
                ? static_cast<UClass*>(generatedObject) : nullptr;
        };

        const auto sourcePathWide = RC::to_generic_string(sourcePath);
        if (auto* resolved = resolveClassObject(
            ActorHelper::ResolveObject(sourcePathWide)))
        {
            return resolved;
        }

        const auto resolveGeneratedClass = [&](const std::string& classPath) -> UClass* {
            const auto classPathWide = RC::to_generic_string(classPath);
            if (auto* resolved = UECustom::UKismetSystemLibrary::LoadClassAsset_Blocking(
                UECustom::TSoftClassPtr<UObject>(
                    UECustom::FSoftObjectPath(classPathWide))))
            {
                return resolved;
            }
            return resolveClassObject(ActorHelper::ResolveObject(classPathWide));
        };

        const auto slash = sourcePath.find_last_of('/');
        const auto dot = sourcePath.find_last_of('.');
        if (dot != std::string::npos && dot > slash)
        {
            return resolveGeneratedClass(sourcePath);
        }

        const auto assetName = sourcePath.substr(
            slash == std::string::npos ? 0 : slash + 1);
        if (assetName.empty())
        {
            return nullptr;
        }
        return resolveGeneratedClass(
            std::format("{}.{}_C", sourcePath, assetName));
    }

    void DragonWildsNpcLoader::ApplyVendorDisplayName(
        AActor* actor,
        const VendorDefinition& definition)
    {
        const auto displayName = definition.HideName ? std::string{} : definition.DisplayName.empty()
            ? definition.MerchantName : definition.DisplayName;
        if (!actor || (!definition.HideName && displayName.empty()))
        {
            return;
        }
        TrackNpcName(actor,definition);

        // Different gameplay actors expose their name through different
        // reflected fields. DisplayName is preferred, while AIName and
        // AiDisplayName make the proxy compatible with the game's nameplate
        // and health-bar widgets when Character is used as the base class.
        for (const auto* candidate : { "DisplayName", "AIName", "AiDisplayName" })
        {
            auto* property = PropertyHelper::GetPropertyByName(
                actor->GetClassPrivate(), RC::to_generic_string(candidate));
            if (!property)
            {
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(
                    actor, property, displayName);
                PS::Log<LogLevel::Verbose>(
                    STR("Applied visual vendor display name '{}' to {} via {}.\n"),
                    RC::to_generic_string(displayName), actor->GetName(),
                    RC::to_generic_string(candidate));
                return;
            }
            catch (const std::exception& error)
            {
                WarnOnce("display-name:" + definition.ModName + ":" + definition.Id,
                    RC::to_generic_string(std::format(
                        "Vendor '{}' found {} but could not write its display name: {}",
                        definition.Id, candidate, error.what())));
            }
        }

        PS::Log<LogLevel::Verbose>(STR("Vendor '{}': neutral proxy has no actor-name field; merchant row/component naming is handled separately.\n"),
            RC::to_generic_string(definition.Id));
    }

    void DragonWildsNpcLoader::ApplyVendorMaterials(
        UObject* mesh,
        const VendorDefinition& definition) const
    {
        if (!mesh || !definition.Materials.is_array())
        {
            return;
        }

        int32_t slot = 0;
        for (const auto& value : definition.Materials)
        {
            const auto path = value.get<std::string>();
            auto* material = ActorHelper::ResolveObject(
                RC::to_generic_string(path));
            if (!material)
            {
                throw std::runtime_error(std::format(
                    "vendor material '{}' could not be resolved", path));
            }

            ActorHelper::FunctionCall setMaterial(mesh,
                TEXT("/Script/Engine.PrimitiveComponent:SetMaterial"));
            setMaterial.Arg(TEXT("ElementIndex"), slot)
                .Arg(TEXT("Material"), material)
                .Invoke();
            ++slot;
        }
    }

    bool DragonWildsNpcLoader::ApplyVendorVisuals(
        AActor* actor,
        VendorDefinition& definition)
    {
        if(definition.Human)return true;
        if(definition.Resource) { ApplyResourceVisuals(actor,definition);ApplyNpcVisualEffect(actor,definition);return true; }
        auto* mesh = FindMeshComponent(actor);
        if (!mesh)
        {
            throw std::runtime_error(
                "neutral BaseActor did not expose a SkeletalMeshComponent");
        }

        UObject* visualMesh = nullptr;
        UObject* sourceOwner = nullptr;
        UObject* sourceMesh = nullptr;
        if (!definition.VisualMeshPath.empty())
        {
            visualMesh = ActorHelper::ResolveObject(
                RC::to_generic_string(definition.VisualMeshPath));
            if (!visualMesh)
            {
                throw std::runtime_error(std::format(
                    "visual Mesh '{}' could not be resolved",
                    definition.VisualMeshPath));
            }
        }
        else
        {
            // Prefer the Blueprint class default object. This makes the
            // proxy independent of whether a cow (or any other source AI)
            // has spawned in the current world.
            if (auto* sourceClass = ResolveVisualSourceClass(definition))
            {
                auto& defaultObject = sourceClass->GetClassDefaultObject();
                sourceOwner = defaultObject.Get();
            }
            // If a game build exposes only the live generated class, retain a
            // Fall back to a loaded source actor. This is no longer
            // the normal path and is never the spawned vendor actor.
            if (!sourceOwner)
            {
                sourceOwner = FindVisualSourceActor(definition);
            }
            if (!sourceOwner)
            {
                throw std::runtime_error(std::format(
                    "VisualSource '{}' could not resolve a Blueprint class or live source actor",
                    definition.VisualSourcePath));
            }
            sourceMesh = FindMeshComponent(sourceOwner);
            if (!sourceMesh)
            {
                throw std::runtime_error(std::format(
                    "VisualSource '{}' has no SkeletalMeshComponent",
                    definition.VisualSourcePath));
            }
            try
            {
                visualMesh = ActorHelper::GetObjectRef(
                    sourceMesh, TEXT("SkeletalMesh"));
            }
            catch (const std::exception& error)
            {
                throw std::runtime_error(std::format(
                    "could not read SkeletalMesh from VisualSource '{}': {}",
                    definition.VisualSourcePath, error.what()));
            }
            if (!visualMesh)
            {
                throw std::runtime_error(std::format(
                    "VisualSource '{}' has no loaded SkeletalMesh",
                    definition.VisualSourcePath));
            }
        }

        auto* meshType = ActorHelper::ResolveClass(TEXT("/Script/Engine.SkeletalMesh"));
        if (!meshType || !visualMesh->IsA(meshType))
            throw std::runtime_error("Vendor visual asset is not a SkeletalMesh");
        const auto setter=ResolveNpcMeshSetter(mesh,visualMesh);
        const auto chosenSetter=setter.Function->GetPathName();
        RecordPhase(definition, "Mesh.Before", actor, {{"Setter", RC::to_string(chosenSetter)}, {"Mesh", RC::to_string(visualMesh->GetPathName())}});
        setter.Apply(mesh,visualMesh);
        RecordPhase(definition, "Mesh.After", actor);
        auto* skeleton = ActorHelper::GetObjectRef(visualMesh, TEXT("Skeleton"));
        if (!skeleton) throw std::runtime_error("Vendor mesh has no usable Skeleton");

        if (!definition.IdleAnimationPath.empty())
        {
            // PlayAnimation performs the native animation-mode transition.
            auto* idleAnimation = ActorHelper::ResolveObject(
                RC::to_generic_string(definition.IdleAnimationPath));
            if (!idleAnimation)
            {
                throw std::runtime_error(std::format(
                    "IdleAnimation '{}' could not be resolved",
                    definition.IdleAnimationPath));
            }

            auto* animationType = ActorHelper::ResolveClass(TEXT("/Script/Engine.AnimationAsset"));
            if (!animationType || !idleAnimation->IsA(animationType)
                || ActorHelper::GetObjectRef(idleAnimation, TEXT("Skeleton")) != skeleton)
                throw std::runtime_error("IdleAnimation and mesh must reference the same Skeleton");
            RecordPhase(definition, "Animation.Before", actor, {{"Animation", RC::to_string(idleAnimation->GetPathName())}, {"Skeleton", RC::to_string(skeleton->GetPathName())}});
            ActorHelper::FunctionCall playAnimation(mesh,
                TEXT("/Script/Engine.SkeletalMeshComponent:PlayAnimation"));
            playAnimation.Arg(TEXT("NewAnimToPlay"), idleAnimation)
                .Arg(TEXT("bLooping"), true)
                .Invoke();
            RecordPhase(definition, "Animation.After", actor);
        }
        else if(sourceMesh)
        {
            // A source AI Blueprint already carries the animation Blueprint
            // appropriate for its mesh and skeleton. Inherit that class when
            // the author did not deliberately select a single animation asset.
            // This is presentation-only: failure must not abort an otherwise
            // valid NPC, which would leave its dialogue/vendor inaccessible.
            try
            {
                auto* sourceAnimObject=ActorHelper::GetObjectRef(sourceMesh,TEXT("AnimClass"));
                auto* sourceAnimClass=sourceAnimObject && sourceAnimObject->IsA<UClass>()
                    ? static_cast<UClass*>(sourceAnimObject) : nullptr;
                auto* animBase=ActorHelper::ResolveClass(TEXT("/Script/Engine.AnimInstance"));
                if(sourceAnimClass && animBase && sourceAnimClass->IsChildOf(animBase))
                {
                    ActorHelper::FunctionCall(mesh,
                        TEXT("/Script/Engine.SkeletalMeshComponent:SetAnimInstanceClass"))
                        .Arg(TEXT("NewClass"),sourceAnimClass).Invoke();
                    RecordPhase(definition,"Animation.SourceClass",actor,
                        {{"AnimationClass",RC::to_string(sourceAnimClass->GetPathName())}});
                }
            }
            catch(const std::exception& error)
            {
                WarnOnce("npc-source-animation:"+definition.ModName+":"+definition.Id,
                    RC::to_generic_string(std::format(
                        "NPC '{}' could not inherit its source animation class: {}",
                        definition.Id,error.what())));
            }
        }
        ApplyVendorMaterials(mesh, definition);
            if(!definition.EnableCollision || definition.MeshCollision=="None") {
                ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionEnabled"))
                    .Arg(TEXT("NewType"),uint8_t(0)).Invoke();
            } else if(definition.MeshCollision=="Pawn") {
                auto* physics=ActorHelper::GetObjectRef(visualMesh,TEXT("PhysicsAsset"));
                auto* physicsType=ActorHelper::ResolveClass(TEXT("/Script/Engine.PhysicsAsset"));
                if(!physics || !physicsType || !physics->IsA(physicsType))
                    throw std::runtime_error("AI mesh collision requires the selected mesh's PhysicsAsset; use Native or disable collision explicitly for meshes without one");
                HumanCall(mesh,TEXT("/Script/Engine.SkinnedMeshComponent:SetPhysicsAsset"),
                    {{"NewPhysicsAsset",RC::to_string(physics->GetPathName())},{"bForceReInit",true}});
                ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionResponseToChannel"))
                    .Arg(TEXT("Channel"),uint8_t(2)).Arg(TEXT("NewResponse"),uint8_t(2)).Invoke();
                ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.PrimitiveComponent:SetCollisionEnabled"))
                    .Arg(TEXT("NewType"),uint8_t(1)).Invoke();
            }
        auto* capsules=ActorHelper::ResolveClass(TEXT("/Script/Engine.CapsuleComponent"));
        if(!capsules)throw std::runtime_error("NPC capsule class unavailable");
        for(auto* capsule:actor->GetComponentsByClass(capsules))
            NpcSetPawnResponse(capsule,definition.EnableCollision && definition.MeshCollision!="Pawn");
        ApplyNpcVisualEffect(actor,definition);
        return true;
    }

    void DragonWildsNpcLoader::ApplyNpcVisualEffect(UObject* visual,const VendorDefinition& definition)
    {
        if(definition.VisualEffect.empty())return;
        if(definition.VisualEffect.value("Type",std::string{})=="Niagara") {
            if(!NiagaraAttachment::CanRenderLocally())return;
            if(!visual || !visual->IsA<AActor>())throw std::runtime_error("NPC Niagara requires an actor owner");
            auto* actor=static_cast<AActor*>(visual);
            auto* type=ActorHelper::ResolveClass(TEXT("/Script/Niagara.NiagaraComponent"));
            if(!type)throw std::runtime_error("NPC Niagara component unavailable");
            for(auto* component:actor->GetComponentsByClass(type)) {
                if(!component || component->GetOuterPrivate()!=actor)continue;
                ActorHelper::FunctionCall tag(component,TEXT("/Script/Engine.ActorComponent:ComponentHasTag"));
                tag.Arg(TEXT("Tag"),FName(TEXT("RuneSchemaNpcNiagara"),FNAME_Add)).Invoke();
                if(tag.Result<bool>())return;
            }
            auto effect=definition.VisualEffect;
            effect["AutoActivate"]=false;
            auto* component=NiagaraAttachment::Attach(actor,ActorHelper::GetObjectRef(actor,TEXT("RootComponent")),effect);
            if(!component)throw std::runtime_error("NPC Niagara visual did not spawn");
            try {
                component->SetFlags(RF_Transient);
                DialogueField(component,TEXT("ComponentTags"),nlohmann::json::array({"RuneSchemaNpcNiagara"}));
                ActorHelper::FunctionCall(component,TEXT("/Script/Engine.SceneComponent:SetVisibility"))
                    .Arg(TEXT("bNewVisibility"),true).Arg(TEXT("bPropagateToChildren"),false).Invoke();
                ActorHelper::FunctionCall(component,TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"))
                    .Arg(TEXT("NewHidden"),false).Arg(TEXT("bPropagateToChildren"),false).Invoke();
                ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:Activate"))
                    .Arg(TEXT("bReset"),true).Invoke();
            } catch(...) {try {NiagaraAttachment::Destroy(component);}catch(...){}throw;}
            return;
        }
        std::vector<UObject*> roots;
        struct Release {
            std::vector<UObject*>& Roots;
            ~Release(){for(auto* object:Roots)object->ClearRootSet();}
        } release{roots};
        try {
            if(!GhostMaterials::CanRender(visual))return;
            const auto materials=GhostMaterials::Create(visual,definition.VisualEffect,roots);
            if(!GhostMaterials::Apply(visual,materials,RC::to_generic_string("NPC "+definition.Id)))
                throw std::runtime_error("No compatible visual mesh accepted Ghost Glow");
        } catch(const std::exception& error) {
            WarnOnce("npc-visual:"+definition.ModName+":"+definition.Id,
                RC::to_generic_string(std::format("NPC '{}' Ghost Glow could not be fully applied: {}",definition.Id,error.what())));
        }
    }

    void DragonWildsNpcLoader::RecordPhase(const VendorDefinition& definition, const char* phase,
        AActor* actor, const nlohmann::json& details)
    {
        auto& report = m_vendorReport[definition.ModName + ":" + definition.Id];
        report["Build"] = PS::BuildInfo::Name;
        report["Phase"] = phase;
        report["Stage"] = static_cast<int>(definition.Stage);
        report["BaseActor"] = definition.BaseActorClassPath;
        report["DisplayName"] = definition.DisplayName;
        report["MerchantName"] = definition.MerchantName;
        report["RowName"] = definition.RowName;
        if (actor) { report["Actor"] = RC::to_string(actor->GetPathName()); if (auto* world = actor->GetWorld()) report["World"] = RC::to_string(world->GetPathName()); }
        for (const auto& [key, value] : details.items()) report[key] = value;
        WriteVendorStatus();
    }

    UWorld* DragonWildsNpcLoader::FindLoadedWorld() const
    {
        auto* pawn = FindLocalGameplayTestPawn();
        if(pawn)return pawn->GetWorld();
        const auto network=PS::Network::Detect();
        if(network.Mode==PS::Network::Role::Unknown || network.Mode==PS::Network::Role::Standalone)return nullptr;
        return static_cast<UWorld*>(network.World);
    }

    void DragonWildsNpcLoader::SetVendorVisible(AActor* actor, const VendorDefinition& definition)
    {
        ActorHelper::FunctionCall hidden(actor, TEXT("/Script/Engine.Actor:SetActorHiddenInGame"));
        hidden.Arg(TEXT("bNewHidden"), false).Invoke();
        auto* meshClass = ActorHelper::ResolveClass(TEXT("/Script/Engine.SkeletalMeshComponent"));
        if (!meshClass) throw std::runtime_error("SkeletalMeshComponent class unavailable");
        for (auto* mesh : actor->GetComponentsByClass(meshClass)) {
            ActorHelper::FunctionCall visible(mesh, TEXT("/Script/Engine.SceneComponent:SetVisibility"));
            visible.Arg(TEXT("bNewVisibility"), !definition.Resource).Arg(TEXT("bPropagateToChildren"), true).Invoke();
            ActorHelper::FunctionCall show(mesh, TEXT("/Script/Engine.SceneComponent:SetHiddenInGame"));
            show.Arg(TEXT("NewHidden"), definition.Resource).Arg(TEXT("bPropagateToChildren"), true).Invoke();
        }
    }

    void DragonWildsNpcLoader::QueueLoreRequest(UObject* controller,const std::string& entry)
    {
        if(!controller || entry.empty())return;
        const auto existing=std::find_if(m_pendingLoreRequests.begin(),m_pendingLoreRequests.end(),
            [&](const auto& value){return value.WorldGeneration==m_worldGeneration
                && value.Controller.Get()==controller && value.Entry==entry;});
        if(existing!=m_pendingLoreRequests.end())return;
        if(m_pendingLoreRequests.size()>=16)
        {
            ErrorOnce("lore-request-capacity",
                TEXT("Lore popup retry capacity reached; refusing another pending request."));
            return;
        }
        PendingLoreRequest request{};
        request.Controller=PS::WeakObject(controller);
        request.Entry=entry;
        request.WorldGeneration=m_worldGeneration;
        m_pendingLoreRequests.push_back(std::move(request));
    }

    void DragonWildsNpcLoader::PumpLoreRequests(float deltaSeconds)
    {
        if(m_pendingLoreRequests.empty() || !std::isfinite(deltaSeconds) || deltaSeconds<=0)return;
        for(auto iterator=m_pendingLoreRequests.begin();iterator!=m_pendingLoreRequests.end();)
        {
            auto& request=*iterator;
            request.RemainingSeconds-=deltaSeconds;
            request.RetryElapsed+=deltaSeconds;
            auto* controller=request.Controller.Get();
            if(request.WorldGeneration!=m_worldGeneration || !controller || !controller->GetWorld())
            {
                iterator=m_pendingLoreRequests.erase(iterator);
                continue;
            }
            if(request.RemainingSeconds<=0)
            {
                ErrorOnce("lore-popup-timeout:"+request.Entry,
                    TEXT("The local lore UI did not become ready within five seconds."));
                iterator=m_pendingLoreRequests.erase(iterator);
                continue;
            }
            if(request.RetryElapsed<0.2f){++iterator;continue;}
            request.RetryElapsed=0;
            try
            {
                if(OpenLore && OpenLore(controller,request.Entry))
                {
                    iterator=m_pendingLoreRequests.erase(iterator);
                    continue;
                }
            }
            catch(const std::exception& error)
            {
                ErrorOnce("lore-popup:"+request.Entry,PS::ToWideSafe(error.what()));
                iterator=m_pendingLoreRequests.erase(iterator);
                continue;
            }
            ++iterator;
        }
    }

    void DragonWildsNpcLoader::ReleaseVendorTracking()
    {
        m_pendingClientReplicas.clear();
        m_pendingLoreRequests.clear();
        m_clientReplicaElapsed=0;
        // The save system owns successful persistent actors. Teardown releases
        // our transient references; the next session repairs the same GUID.
        m_merchantBindings.clear();
        m_spawnedVendors.clear();
        m_retiredVendors.clear();
        m_npcNames.clear();
    }

    void DragonWildsNpcLoader::OnVendorCellShown(UObject* object)
    {
        const auto thread = m_gameThreadId.load(std::memory_order_relaxed);
        if (thread && thread != GetCurrentThreadId()) return;
        auto* cell = static_cast<UECustom::UWorldPartitionRuntimeLevelStreamingCell*>(object);
        if (!cell || cell->GetIsHLOD()) return;
        auto* world = cell->GetWorld();
        auto* bounds = cell->GetContentBounds();
        if (!world || !bounds || !bounds->bIsValid) return;
        for (auto& definition : m_definitions) {
            if (definition.HelpyTemporary || !definition.Enabled || !definition.SpawnGate.Pending()) continue;
            const auto x = definition.TargetLocation[0], y = definition.TargetLocation[1];
            if (x > bounds->Min.X() && x < bounds->Max.X()
                && y > bounds->Min.Y() && y < bounds->Max.Y()
                && (definition.CellReadyWorld != world || definition.GroundToSurface)) {
                definition.CellReadyWorld = world;
                if(definition.GroundToSurface) {
                    const auto key=definition.ModName+":"+definition.Id;
                    // World Partition can restore a saved proxy without a
                    // full map teardown. Re-run its ground trace and transform
                    // verification whenever the containing cell is shown.
                    std::erase_if(m_spawnedVendors,[&](const auto& entry){return entry.Key==key;});
                    definition.SpawnGate.ResetForMap();
                }
                m_scanBudget.ResetForMap();
            }
        }
    }

    bool DragonWildsNpcLoader::NpcTimeAllows(UObject* context,const VendorDefinition& definition) const
    {
        return TimeOfDay::Allows(context,definition.Time);
    }

    void DragonWildsNpcLoader::ReconcileNpcScales(double deltaSeconds)
    {
        m_npcScaleElapsed += deltaSeconds;
        if (m_npcScaleElapsed < 1.0) return;
        m_npcScaleElapsed = 0.0;

        for (const auto& binding : m_spawnedVendors)
        {
            auto* actor = static_cast<AActor*>(binding.Actor.Get());
            if (!actor || !IsNpcObjectUsable(actor)) continue;
            const auto definition = std::find_if(m_definitions.begin(), m_definitions.end(),
                [&](const auto& value) {
                    return value.ModName + ":" + value.Id == binding.Key;
                });
            if (definition == m_definitions.end()) continue;
            const FVector desired(definition->SpawnScale[0], definition->SpawnScale[1],
                definition->SpawnScale[2]);
            const auto current = actor->GetActorScale3D();
            if (std::abs(current.X() - desired.X()) <= 0.001
                && std::abs(current.Y() - desired.Y()) <= 0.001
                && std::abs(current.Z() - desired.Z()) <= 0.001) continue;
            actor->SetActorScale3D(desired);
            PS::Log<LogLevel::Verbose>(
                STR("Restored authored scale for managed NPC '{}'.\n"),
                PS::ToWideSafe(binding.Key.c_str()));
        }
    }

    void DragonWildsNpcLoader::ReconcileNpcTimeOfDay()
    {
        m_npcTimeDirty=false;
        auto* world=FindLoadedWorld();
        if(!world)return;
        TimeOfDay::Requirement current;
        try { current=TimeOfDay::Current(world); }
        catch(const std::exception& error) {
            m_npcTimeDirty=true;
            WarnOnce("npc-time-transition",PS::ToWideSafe(error.what()));
            return;
        }

        if(current!=m_lastObservedTime) {
            m_lastObservedTime=current;
            auto* station=m_activeVendorStation.Get();
            auto* controller=m_activeVendorController.Get();
            auto found=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& value){
                return value.ModName+":"+value.Id==m_activeVendorDefinition;
            });
            if(station && controller && found!=m_definitions.end() && found->InlineMerchant
                && found->ItemsProperty.empty()) try {
                const auto visible=VisibleVendorItems(*found,controller);
                CreateInlineMerchantRow(*found,&visible);
                ApplyVendorRow(station,*found);
                RefreshClientShop(station);
                PS::RoutineLog("vendors",STR("Merchant '{}:{}' stock refreshed for {}.\n"),
                    RC::to_generic_string(found->ModName),RC::to_generic_string(found->Id),
                    current==TimeOfDay::Requirement::Day?TEXT("day"):TEXT("night"));
            } catch(const std::exception& error) {
                WarnOnce("vendor-time-refresh:"+m_activeVendorDefinition,PS::ToWideSafe(error.what()));
            }
        }

        const auto network=PS::Network::Detect(world);
        if(network.Mode==PS::Network::Role::Unknown || network.Mode==PS::Network::Role::Client)return;

        for(auto& definition:m_definitions) {
            if(definition.HelpyTemporary || !definition.Enabled || definition.Time==TimeOfDay::Requirement::Any)continue;
            const auto key=definition.ModName+":"+definition.Id;
            const bool allowed=definition.Time==current;
            auto entry=std::find_if(m_spawnedVendors.begin(),m_spawnedVendors.end(),
                [&](const auto& value){return value.Key==key;});
            if(allowed) {
                if(entry==m_spawnedVendors.end()
                    && std::none_of(m_pendingNpcCleanup.begin(),m_pendingNpcCleanup.end(),
                        [&](const auto& value){return value.RespawnDefinition==key;})) {
                    definition.SpawnGate.ResetForMap();
                    m_scanBudget.ResetForMap();
                }
                continue;
            }
            if(entry==m_spawnedVendors.end())continue;
            // Constructed UE4SS weak handles may carry serial zero. Resolve the
            // owned persistent GUID again instead of trusting that weak handle
            // for a lifecycle transition.
            auto* actor=FindPersistentVendor(world,definition);
            if(!actor || !IsNpcObjectUsable(actor)) {
                m_spawnedVendors.erase(entry);
                continue;
            }
            const bool conversationActive=std::any_of(m_dialogueSessions.begin(),m_dialogueSessions.end(),
                [&](const auto& value){
                    if(!value.second || value.second->NpcKey!=key)return false;
                    auto* player=ResolveDialoguePlayer(*value.second);
                    if(!player)return false;
                    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DomConversationParticipant"));
                    if(!type)return false;
                    const auto participants=player->GetComponentsByClass(type);
                    return participants.Num()==1 && participants[0] && DialogueIsActive(participants[0]);
                });
            if(conversationActive) {
                // Let the open native conversation finish. Its completion path
                // erases the session; the next tick performs retirement.
                m_npcTimeDirty=true;
                continue;
            }
            const auto attempt=[&](const auto& operation){try{operation();}catch(...) {}};
            attempt([&]{ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorEnableCollision"))
                .Arg(TEXT("bNewActorEnableCollision"),false).Invoke();});
            attempt([&]{ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorHiddenInGame"))
                .Arg(TEXT("bNewHidden"),true).Invoke();});
            std::erase_if(m_merchantBindings,[&](const auto& value){return value.DefinitionKey==key;});
            std::erase_if(m_npcNames,[&](const auto& value){return value.second.Token==actor || (value.second.Token && value.second.Token->GetOuterPrivate()==actor);});
            m_applied.erase(ActorKey(actor,definition));
            if(PublishWorldState) {
                const auto path=RC::to_string(actor->GetPathName());const auto id=definition.ModName+":"+definition.Id+":"+std::to_string(std::hash<std::string>{}(path));
                PublishWorldState(id,nlohmann::json{{"kind","npc"},{"mod",definition.ModName},{"definition",definition.Id},{"fingerprint",NetworkDefinitionFingerprint(actor,definition)},{"actor",path},{"lifecycle","retiring"},{"timeOfDay",TimeOfDay::Name(definition.Time)}}.dump());
            }
            QueueNpcCleanup(actor,key);
            m_spawnedVendors.erase(entry);
            PS::RoutineLog("npc",STR("NPC '{}:{}' retired for {}.\n"),
                RC::to_generic_string(definition.ModName),RC::to_generic_string(definition.Id),
                current==TimeOfDay::Requirement::Day?TEXT("day"):TEXT("night"));
        }
    }

    namespace {
        FStructProperty* VendorGuidProperty(UObject* object) {
            auto* property = object ? PropertyHelper::GetPropertyByName<FStructProperty>(
                object->GetClassPrivate(), TEXT("SpudGuid")) : nullptr;
            if (!property || property->GetArrayDim() != 1
                || property->GetSize() != sizeof(VendorIdentity::Words)
                || !property->GetStruct()
                || property->GetStruct()->GetPathName() != TEXT("/Script/CoreUObject.Guid"))
                return nullptr;
            return property;
        }
    }

    void DragonWildsNpcLoader::SetPersistentVendorId(AActor* actor,
        const VendorDefinition& definition) const
    {
        auto* property = VendorGuidProperty(actor);
        if (!property) throw std::runtime_error("Vendor requires a reflected 16-byte SpudGuid");
        std::memcpy(property->ContainerPtrToValuePtr<void>(actor),
            definition.PersistentId.data(), sizeof(definition.PersistentId));
    }

    AActor* DragonWildsNpcLoader::FindPersistentVendor(UWorld* world,
        const VendorDefinition& definition) const
    {
        auto* actorClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.Actor"));
        if (!actorClass) throw std::runtime_error("Actor class unavailable for persistent vendor lookup");
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(actorClass, objects, true,
            static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad
                | RF_NeedPostLoad | RF_NeedInitialization | RF_BeginDestroyed | RF_FinishDestroyed));
        AActor* match = nullptr;
        for (auto* object : objects) {
            if (!object || object->GetWorld() != world) continue;
            auto* property = VendorGuidProperty(object);
            if (!property) continue;
            if (std::memcmp(property->ContainerPtrToValuePtr<void>(object),
                definition.PersistentId.data(), sizeof(definition.PersistentId)) != 0) continue;
            if(!IsNpcObjectUsable(object))continue;
            const auto index=object->GetInternalIndex();
            auto* slot=index>=0?FUObjectArray::IndexToObject(index):nullptr;
            if(slot && slot->GetUObject()==object
                && std::any_of(m_retiredVendors.begin(),m_retiredVendors.end(),[&](const auto& old) {
                    return VendorIdentity::RetiredInstanceMatches(old.Address,object,old.Index,index,
                        old.Serial,slot->GetSerialNumber(),old.Path==object->GetPathName());
                }))continue;
            if (!object->IsA(definition.BaseActorClass))
                throw std::runtime_error("Vendor GUID belongs to a different actor class; refusing replacement");
            if (match) throw std::runtime_error("Multiple actors share the vendor GUID; refusing spawn/adoption");
            match = static_cast<AActor*>(object);
        }
        return match;
    }

    bool DragonWildsNpcLoader::ApplyVendor(
        AActor* actor,
        VendorDefinition& definition)
    {
        const auto key = ActorKey(actor, definition);
        if (m_applied.contains(key))
        {
            return true;
        }

        try
        {
            RecordPhase(definition, "Interaction.Before", actor);
            auto* interaction = EnsureComponent(actor, definition.InteractionClass);
            if (!interaction) throw std::runtime_error("interaction construction returned null");
            ApplyComponentProperties(interaction, definition.InteractionProperties, definition);
            if (definition.Stage == VendorPolicy::VendorStage::Interaction) {
                RegisterComponent(actor, interaction, definition);
                if(!definition.DialogueKey.empty() || !definition.LoreEntry.empty())BindMerchantInteraction(actor,interaction,nullptr,definition);
                RecordPhase(definition, "Interaction.Ready", actor);
                m_applied.insert(key);
                return true;
            }
            RecordPhase(definition, "Merchant.Before", actor);
            auto* vendor = EnsureComponent(actor, definition.VendorClass);
            if (!interaction || !vendor)
            {
                throw std::runtime_error("component construction returned null");
            }

            ApplyVendorRow(vendor, definition);
            if (auto* displayName=PropertyHelper::GetPropertyByName(vendor->GetClassPrivate(),TEXT("DisplayName")))
                PropertyHelper::CopyJsonValueToContainer(vendor,displayName,definition.MerchantName);
            ApplyComponentProperties(vendor, definition.VendorProperties, definition);
            RegisterComponent(actor, interaction, definition);
            RegisterComponent(actor, vendor, definition);
            RecordPhase(definition, "Merchant.SetupReady", actor);
            BindMerchantInteraction(actor, interaction, vendor, definition);
            RecordPhase(definition, "Interaction.Bound", actor,
                {{"WidgetTypeRequested", definition.WidgetType}});

            m_applied.insert(key);
            PS::Log<LogLevel::Normal>(
                STR("Attached vendor '{}' to actor {} using {}:{}\n"),
                RC::to_generic_string(definition.Id), actor->GetName(),
                RC::to_generic_string(definition.DataTablePath),
                RC::to_generic_string(definition.RowName));
            return true;
        }
        catch (const std::exception& error)
        {
            std::erase_if(m_merchantBindings, [&](const auto& binding) {
                return binding.DefinitionKey == definition.ModName + ":" + definition.Id
                    && binding.Actor.Get() == actor;
            });
            ErrorOnce("apply:" + definition.ModName + ":" + definition.Id,
                RC::to_generic_string(std::format(
                    "Vendor '{}' could not be attached to actor '{}': {}",
                    definition.Id, RC::to_string(actor->GetName()), error.what())));
            return false;
        }
    }

    void DragonWildsNpcLoader::BindMerchantInteraction(
        AActor* actor, UObject* interaction, UObject* station,
        const VendorDefinition& definition)
    {
        if (!actor || !interaction || (!station && definition.DialogueKey.empty() && definition.LoreEntry.empty()))
        {
            throw std::runtime_error("merchant interaction binding received a null object");
        }
        if (m_interactionCallbackId == Hook::ERROR_ID)
        {
            throw std::runtime_error("merchant interaction callback is unavailable");
        }

        const auto key = definition.ModName + ":" + definition.Id;
        const auto existing = std::find_if(m_merchantBindings.begin(),
            m_merchantBindings.end(), [&](const auto& binding) {
                return binding.DefinitionKey == key;
            });
        if (existing != m_merchantBindings.end())
        {
            throw std::runtime_error("merchant interaction was already bound for this definition");
        }
        m_merchantBindings.push_back(
            {key, PS::WeakObject(actor), PS::WeakObject(interaction), PS::WeakObject(station),
                actor, interaction, station});
        const auto& bound = m_merchantBindings.back();
        auto* expected = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Dominion.PlayerInteractionManager:Multicast_AcknowledgeInteractionRequest"), false);
        RecordPhase(definition, "Interaction.BindingDiagnostics", actor, {
            {"InteractionComponent", RC::to_string(interaction->GetPathName())},
            {"StationComponent", station?RC::to_string(station->GetPathName()):std::string{}},
            {"ActorWeakSerial", bound.Actor.ObjectSerialNumber},
            {"InteractionWeakSerial", bound.Interaction.ObjectSerialNumber},
            {"StationWeakSerial", bound.Station.ObjectSerialNumber},
            {"ActorWeakResolves", bound.Actor.Get() == actor},
            {"InteractionWeakResolves", bound.Interaction.Get() == interaction},
            {"StationWeakResolves", bound.Station.Get() == station},
            {"ComponentResolution", definition.HelpyTemporary?"HelpyOwnedLeaseAndOwnedComponents":"SavedActorGuidAndOwnedComponents"},
            {"DispatchRoute", "PlayerInteractionManager.Multicast_AcknowledgeInteractionRequest"},
            {"ExpectedFunctionFound", expected != nullptr},
            {"ExpectedFunctionFlags", expected ? static_cast<uint64_t>(expected->GetFunctionFlags()) : 0},
            {"ExpectedParameterBytes", expected ? expected->GetParmsSize() : 0}
        });
    }

    void DragonWildsNpcLoader::TraceMerchantEvent(
        UObject* source, UFunction* function, void* parameters)
    {
        const auto& settings=PS::PSConfig::Get()->GetSettings();
        if(!settings.advancedRuntime||!settings.npcDiagnostics.interactionTraceExport)return;
        // Only called on the game thread. Never dereference stored comparison
        // tokens: event source/function are supplied live by ProcessEvent.
        // Bound both retained events and disk writes; no general gameplay trace.
        if (m_interactionTraceBudget.Exhausted()) return;
        for (const auto& binding : m_merchantBindings) {
            const char* role = source == binding.InteractionToken ? "Interaction"
                : source == binding.ActorToken ? "Actor"
                : source == binding.StationToken ? "Station" : nullptr;
            if (!role) continue;
            const auto path = RC::to_string(function->GetPathName());
            const auto traceKey = binding.DefinitionKey + ":" + role + ":" + path;
            if (!m_interactionTraceBudget.Admit(traceKey)) return;
            m_interactionTrace.push_back({
                {"Definition", binding.DefinitionKey}, {"Role", role},
                {"Source", RC::to_string(source->GetPathName())},
                {"Function", path},
                {"FunctionFlags", static_cast<uint64_t>(function->GetFunctionFlags())},
                {"ParameterBytes", function->GetParmsSize()},
                {"ParametersPresent", parameters != nullptr},
                {"InteractionWeakMatchesSource", binding.Interaction.Get() == source},
                {"ActorWeakResolves", binding.Actor.Get() != nullptr},
                {"StationWeakResolves", binding.Station.Get() != nullptr}
            });
            WriteVendorInteractionTrace();
            return;
        }
    }

    UObject* DragonWildsNpcLoader::FindCraftingApi(UWorld* world, nlohmann::json& diagnostics) const
    {
        auto& lookup = diagnostics["CraftingApiLookup"];
        lookup = nlohmann::json::object();
        const auto fail = [&](const char* reason) -> UObject* {
            lookup["Result"] = reason;
            throw std::runtime_error(std::string("Crafting UI lookup: ") + reason);
        };
        if (!world) return fail("MissingPlayerWorld");
        lookup["PlayerWorld"] = RC::to_string(world->GetPathName());
        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.HUDUISubsystem"), false);
        auto* apiClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.CraftingUIAPI"), false);
        if (!subsystemClass) return fail("MissingHUDUISubsystemClass");
        if (!apiClass) return fail("MissingCraftingUIAPIClass");

        TArray<UObject*> subsystems;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, subsystems, true,
            static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject
                | RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization
                | RF_BeginDestroyed | RF_FinishDestroyed));
        UObject* selected = nullptr;
        size_t matches = 0;
        lookup["Subsystems"] = nlohmann::json::array();
        for (auto* subsystem : subsystems)
        {
            if (!subsystem) continue;
            auto* subsystemWorld = subsystem->GetWorld();
            auto* outer = subsystem->GetOuterPrivate();
            const bool belongsToWorld = subsystemWorld == world
                || (!subsystemWorld && outer == world);
            lookup["Subsystems"].push_back({
                {"Object", RC::to_string(subsystem->GetPathName())},
                {"World", subsystemWorld ? RC::to_string(subsystemWorld->GetPathName()) : ""},
                {"Outer", outer ? RC::to_string(outer->GetPathName()) : ""},
                {"MatchesPlayerWorld", belongsToWorld}
            });
            if (!belongsToWorld) continue;
            selected = subsystem;
            ++matches;
        }
        lookup["MatchingSubsystemCount"] = matches;
        if (!matches) return fail(subsystems.Num() ? "SubsystemWorldMismatch" : "NoLiveHUDUISubsystem");
        if (matches != 1) return fail("MultipleMatchingHUDUISubsystems");

        // Stock StartingBench exports reference GetCraftingAPI, not a direct field read.
        // Validate the entire no-input/object-return signature before invoking it.
        auto* getter = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Dominion.HUDUISubsystem:GetCraftingAPI"), false);
        if (!getter) return fail("MissingGetCraftingAPI");
        auto* returned = CastField<FObjectPropertyBase>(getter->GetReturnProperty());
        int parameters = 0;
        for (auto* field : TFieldRange<FProperty>(getter, EFieldIterationFlags::Default))
            if (field->HasAnyPropertyFlags(CPF_Parm)) ++parameters;
        auto* returnClass = returned ? returned->GetPropertyClass().Get() : nullptr;
        if (!returned || parameters != 1 || !returnClass
            || !returned->HasAnyPropertyFlags(CPF_Parm)
            || !returned->HasAnyPropertyFlags(CPF_ReturnParm)
            || returned->GetArrayDim() != 1
            || returned->GetElementSize() != sizeof(UObject*)
            || returned->GetOffset_Internal() != 0
            || getter->GetParmsSize() != sizeof(UObject*)
            || returnClass != apiClass)
            return fail("UnsupportedGetCraftingAPISignature");
        const auto generation = m_worldGeneration;
        ActorHelper::FunctionCall call(selected,
            TEXT("/Script/Dominion.HUDUISubsystem:GetCraftingAPI"));
        call.Invoke();
        if (generation != m_worldGeneration) return fail("WorldChangedDuringGetter");
        auto* api = call.Result<UObject*>();
        if (!api) return fail("GetCraftingAPIReturnedNull");
        if (api->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject
            | RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization
            | RF_BeginDestroyed | RF_FinishDestroyed))) return fail("InvalidAPIFlags");
        if (!api->IsA(apiClass)) return fail("UnexpectedAPIClass");
        auto* apiWorld = api->GetWorld();
        auto* apiOuter = api->GetOuterPrivate();
        lookup["API"] = RC::to_string(api->GetPathName());
        lookup["APIOuter"] = apiOuter ? RC::to_string(apiOuter->GetPathName()) : "";
        lookup["APIWorld"] = apiWorld ? RC::to_string(apiWorld->GetPathName()) : "";
        // A UI UObject may not implement GetWorld. Its provenance here is the
        // unique live subsystem's validated getter; reject an explicit other world.
        if (apiWorld && apiWorld != world) return fail("APIWorldMismatch");
        lookup["Result"] = "ResolvedViaGetCraftingAPI";
        return api;
    }

    uint8_t DragonWildsNpcLoader::ResolveWidgetType(UFunction* function,
        const VendorDefinition& definition, nlohmann::json& details) const
    {
        auto* property = function
            ? function->FindProperty(FName(TEXT("Type"), FNAME_Find)) : nullptr;
        UEnum* enumObject = nullptr;
        FNumericProperty* numeric = nullptr;
        if (auto* enumProperty = CastField<FEnumProperty>(property))
        {
            enumObject = enumProperty->GetEnum();
            numeric = enumProperty->GetUnderlyingProperty();
        }
        else if (auto* byteProperty = CastField<FNumericProperty>(property))
        {
            enumObject = byteProperty->GetIntPropertyEnum();
            numeric = byteProperty;
        }
        if (!property || !enumObject || !numeric || property->GetArrayDim() != 1
            || property->GetElementSize() != 1 || numeric->GetElementSize() != 1
            || property->GetOffset_Internal() != 8)
        {
            throw std::runtime_error("ShowCraftingUI Type is not the captured one-byte enum at offset 8");
        }

        const auto lower = [](std::string text) {
            for (auto& character : text)
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            return text;
        };
        const auto wanted = lower(definition.WidgetType);
        nlohmann::json names = nlohmann::json::array();
        bool found = false;
        int64_t resolved = 0;
        for (const auto& pair : enumObject->GetEnumNames())
        {
            auto qualified = RC::to_string(pair.Key.ToString());
            names.push_back(qualified);
            auto shortName = qualified;
            if (const auto separator = shortName.rfind("::"); separator != std::string::npos)
                shortName = shortName.substr(separator + 2);
            if (lower(shortName) == wanted || lower(qualified) == wanted)
            {
                if (found && resolved != pair.Value)
                    throw std::runtime_error("WidgetType matches more than one reflected enum value");
                found = true;
                resolved = pair.Value;
            }
        }
        details["WidgetTypeRequested"] = definition.WidgetType;
        details["WidgetTypeEnum"] = RC::to_string(enumObject->GetPathName());
        details["WidgetTypeMembers"] = std::move(names);
        if (!found)
            throw std::runtime_error(std::format(
                "WidgetType '{}' was not found in the live EWidgetType enum",
                definition.WidgetType));
        if (resolved < 0 || resolved > 255)
            throw std::runtime_error("resolved WidgetType does not fit its reflected byte storage");
        details["WidgetTypeValue"] = resolved;
        return static_cast<uint8_t>(resolved);
    }

    void DragonWildsNpcLoader::OnMerchantInteraction(
        UObject* source, UFunction* function, void* parameters)
    {
        // Global ProcessEvent callbacks also arrive from worker threads. Reject
        // those before touching game-thread-owned containers or guard state.
        const auto gameThreadId = m_gameThreadId.load(std::memory_order_relaxed);
        if (!gameThreadId || GetCurrentThreadId() != gameThreadId) return;
        if (m_handlingInteraction || m_merchantBindings.empty() || !source || !function) return;
        try { TraceMerchantEvent(source, function, parameters); }
        catch (...) { /* Optional tracing must not suppress the existing handler. */ }
        if (!parameters
            || function->GetPathName()
                != TEXT("/Script/Dominion.PlayerInteractionManager:Multicast_AcknowledgeInteractionRequest"))
        {
            return;
        }

        auto* player=source && source->GetOuterPrivate() && source->GetOuterPrivate()->IsA(AActor::StaticClass())
            ? static_cast<AActor*>(source->GetOuterPrivate()) : nullptr;
        auto* playerClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if(!player || !playerClass || !player->IsA(playerClass))return;
        ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        const bool isAuthority=authority.Result<bool>();
        auto* managerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.PlayerInteractionManager"), false);
        if (!player || !managerClass || !source->IsA(managerClass)
            || source->GetOuterPrivate() != player || source->GetWorld() != player->GetWorld()
            || source->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject
                | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed))) return;
        const auto managerPath = RC::to_string(source->GetPathName());
        nlohmann::json acknowledgement;
        try {
            auto* componentField = CastField<FObjectPropertyBase>(function->FindProperty(
                FName(TEXT("InteractionComponent"), FNAME_Find)));
            auto* releaseField = CastField<FBoolProperty>(function->FindProperty(
                FName(TEXT("bIsRelease"), FNAME_Find)));
            auto* secondaryField = CastField<FBoolProperty>(function->FindProperty(
                FName(TEXT("bIsSecondaryInteraction"), FNAME_Find)));
            const std::array<FProperty*, 3> fields{componentField, releaseField, secondaryField};
            std::array<VendorAcknowledgement::FieldRange, 3> ranges{};
            int count = 0;
            for (auto* field : TFieldRange<FProperty>(function, EFieldIterationFlags::Default)) {
                if (!field->HasAnyPropertyFlags(CPF_Parm)) continue;
                ++count;
                if (field->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)
                    || std::find(fields.begin(), fields.end(), field) == fields.end())
                    throw std::runtime_error("acknowledgement has unexpected parameters");
            }
            for (size_t i = 0; i < fields.size(); ++i) {
                auto* field = fields[i];
                if (!field || !field->HasAnyPropertyFlags(CPF_Parm)
                    || field->GetArrayDim() != 1 || field->GetOffset_Internal() < 0
                    || field->GetElementSize() <= 0)
                    throw std::runtime_error("acknowledgement parameter metadata unavailable");
                ranges[i] = {static_cast<size_t>(field->GetOffset_Internal()),
                    static_cast<size_t>(field->GetElementSize())};
            }
            if (count != 3 || function->GetReturnProperty()
                || componentField->GetElementSize() != sizeof(UObject*)
                || !releaseField->IsNativeBool() || releaseField->GetByteOffset() != 0
                || releaseField->GetElementSize() != sizeof(bool)
                || !secondaryField->IsNativeBool() || secondaryField->GetByteOffset() != 0
                || secondaryField->GetElementSize() != sizeof(bool)
                || !VendorAcknowledgement::ValidLayout(function->GetParmsSize(), ranges))
                throw std::runtime_error("acknowledgement layout is not the supported object/two-bool input");
            const bool release = releaseField->GetPropertyValue(releaseField->ContainerPtrToValuePtr<void>(parameters));
            const bool secondary = secondaryField->GetPropertyValue(secondaryField->ContainerPtrToValuePtr<void>(parameters));
            if (!VendorAcknowledgement::PrimaryPress(release, secondary)) return;
            auto* componentClass = componentField->GetPropertyClass().Get();
            auto* target = componentField->GetObjectPropertyValue(componentField->ContainerPtrToValuePtr<void>(parameters));
            if (!target || !componentClass || !target->IsA(componentClass)) return;
            acknowledgement = {{"InteractionManager", managerPath},
                {"AcknowledgedComponent", RC::to_string(target->GetPathName())},
                {"bIsRelease", release}, {"bIsSecondaryInteraction", secondary},
                {"AcknowledgementParameterBytes", function->GetParmsSize()},
                {"AcknowledgementOffsets", {ranges[0].Offset, ranges[1].Offset, ranges[2].Offset}}};
            source = target; // Live event argument, never a cached weak/raw reference.
        } catch (const std::exception& error) {
            if (!m_vendorReport.contains("AcknowledgementError")) {
                m_vendorReport["AcknowledgementError"] = error.what();
                WriteVendorStatus();
            }
            return;
        }

        auto bindingEntry = std::find_if(m_merchantBindings.begin(),
            m_merchantBindings.end(), [&](const auto& candidate) {
                // Cheap comparison only. GUID and live component ownership are
                // independently validated below before this can open anything.
                return candidate.ActorToken == source->GetOuterPrivate();
            });
        if (bindingEntry == m_merchantBindings.end()) return;
        // A reflected call can reenter map teardown and clear the bindings.
        const auto bindingCopy = *bindingEntry;
        const auto* binding = &bindingCopy;

        auto definitionEntry = std::find_if(m_definitions.begin(), m_definitions.end(),
            [&](const auto& candidate) {
                return candidate.ModName + ":" + candidate.Id == binding->DefinitionKey;
            });
        if (definitionEntry == m_definitions.end() || !definitionEntry->Enabled) return;
        const auto definitionCopy = *definitionEntry;
        const auto* definition = &definitionCopy;
        if(!isAuthority && definition->LoreEntry.empty())return;
        if(isAuthority && definition->Time!=TimeOfDay::Requirement::Any && !NpcTimeAllows(player,*definition))return;
        const auto generation = m_worldGeneration;

        struct InteractionGuard {
            bool& Active;
            explicit InteractionGuard(bool& active) : Active(active) { Active = true; }
            ~InteractionGuard() { Active = false; }
        } guard{m_handlingInteraction};
        nlohmann::json diagnostics = std::move(acknowledgement);
        diagnostics["DispatchRoute"] = "PlayerInteractionManager.Multicast_AcknowledgeInteractionRequest";

        try
        {
            if (source->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject
                | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
                throw std::runtime_error("interaction source is not a live instance");
            if (!source->GetWorld()) throw std::runtime_error("interaction source has no world");
            auto* actor = definition->HelpyTemporary?FindHelpyNpc(source->GetWorld(),*definition):FindPersistentVendor(source->GetWorld(), *definition);
            if (!actor || source->GetOuterPrivate() != actor)
                throw std::runtime_error("interaction source is not owned by the unique saved or Helpy-leased vendor");
            auto findOwned = [&](UClass* type) -> UObject* {
                if (!type) throw std::runtime_error("component class unavailable");
                UObject* found = nullptr;
                auto components = actor->GetComponentsByClass(type);
                if (generation != m_worldGeneration)
                    throw std::runtime_error("world changed while resolving vendor components");
                for (auto* component : components) {
                    if (!component || component->GetOuterPrivate() != actor
                        || !component->IsA(type)
                        || component->HasAnyFlags(static_cast<EObjectFlags>(
                            RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
                        continue;
                    if (found) throw std::runtime_error("ambiguous owned vendor components");
                    found = component;
                }
                return found;
            };
            if (findOwned(definition->InteractionClass) != source)
                throw std::runtime_error("event source is not the vendor's unique interaction component");
            if(!definition->LoreEntry.empty()) {
                auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
                if(!controller)return;
                ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
                if(!local.Result<bool>())return;
                if(!OpenLore)throw std::runtime_error("Lore presentation loader is unavailable");
                if(!OpenLore(controller,definition->LoreEntry))QueueLoreRequest(controller,definition->LoreEntry);
                return;
            }
            if(!definition->DialogueKey.empty()) {
                OpenDialogue(actor,player,*definition);
                return;
            }
            OpenNpcShop(actor,player,*definition);
        }
        catch (const std::exception& error)
        {
            if (generation != m_worldGeneration) return;
            diagnostics["Error"] = error.what();
            try {
                RecordPhase(*definition, "Shop.OpenFailed",
                    nullptr, diagnostics);
                ErrorOnce("shop:" + binding->DefinitionKey,
                    RC::to_generic_string(std::format(
                        "NPC '{}' could not open its interaction: {}",
                        definition->Id, error.what())));
            } catch (...) {}
        }
        catch (...)
        {
            if (generation != m_worldGeneration) return;
            try {
                RecordPhase(*definition, "Shop.OpenFailed",
                    nullptr,
                    {{"Error", "unknown interaction callback failure"}});
            } catch (...) {}
        }
    }

    void DragonWildsNpcLoader::OpenNpcShop(AActor* actor,AActor* player,const VendorDefinition& value) {
        if(!actor || !player || actor->GetWorld()!=player->GetWorld()
            || !value.VendorClass)throw std::runtime_error("Shop requires an authoritative player and owned NPC");
        ActorHelper::FunctionCall playerAuthority(player,TEXT("/Script/Engine.Actor:HasAuthority"));playerAuthority.Invoke();
        if(!playerAuthority.Result<bool>())throw std::runtime_error("Shop dispatch requires player authority");
        if(value.HelpyTemporary&&FindHelpyNpc(actor->GetWorld(),value)!=actor)
            throw std::runtime_error("Temporary NPC has expired or no longer owns this interaction.");
        const auto generation=m_worldGeneration;
        const auto* definition=&value;
        nlohmann::json diagnostics=nlohmann::json::object();
            if(!definition->RequiredFlag.empty()) {
                auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
                const auto character=DialogueCharacter(controller);
                if(!SavedDialogue(m_quests,controller,character,definition->RequiredFlag).HasFlag(definition->RequiredFlag)) {
                    auto locked=*definition;
                    locked.DialogueKey=definition->LockedDialogueKey;
                    OpenDialogue(actor,player,locked);
                    return;
                }
            }
            UObject* station=nullptr;
            for(auto* component:actor->GetComponentsByClass(definition->VendorClass)) {
                if(!component || component->GetOuterPrivate()!=actor || component->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))continue;
                if(station)throw std::runtime_error("Ambiguous NPC merchant component");
                station=component;
            }
            if (!player || !actor || !station || !actor->GetWorld()
                || player->GetWorld() != actor->GetWorld()
                || station->GetWorld() != actor->GetWorld())
                throw std::runtime_error("interaction objects are no longer live in the same world");
            if (generation != m_worldGeneration) return;
            RecordPhase(*definition, "Interaction.Received", actor, diagnostics);

            auto* controllerProperty = CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(player->GetClassPrivate(), TEXT("Controller")));
            auto* controller = controllerProperty ? controllerProperty->GetObjectPropertyValue(
                controllerProperty->ContainerPtrToValuePtr<void>(player)) : nullptr;
            auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"), false);
            if (!controller || !controllerClass || !controller->IsA(controllerClass)
                || controller->GetWorld() != actor->GetWorld()
                || controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject
                    | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
                throw std::runtime_error("local player's live Dominion controller is unavailable");
            if (generation != m_worldGeneration) return;
            auto* show = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController:Client_ToggleCraftingMenu"), false);
            if (!show || show->GetParmsSize() != 8 || show->GetReturnProperty())
                throw std::runtime_error("Client_ToggleCraftingMenu no longer has the captured 8-byte layout");

            auto* stationProperty = CastField<FObjectPropertyBase>(
                show->FindProperty(FName(TEXT("CraftingStationComponent"), FNAME_Find)));
            auto* expectedStationClass = stationProperty
                ? stationProperty->GetPropertyClass().Get() : nullptr;
            int showInputCount = 0;
            for (auto* field : TFieldRange<FProperty>(show, EFieldIterationFlags::Default))
            {
                if (!field->HasAnyPropertyFlags(CPF_Parm)
                    || field->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
                ++showInputCount;
                if (field->HasAnyPropertyFlags(CPF_OutParm))
                    throw std::runtime_error("Client_ToggleCraftingMenu unexpectedly has an output parameter");
            }
            if (showInputCount != 1 || !stationProperty || !expectedStationClass
                || stationProperty->GetOffset_Internal() != 0
                || stationProperty->GetElementSize() != sizeof(UObject*)
                || !station->IsA(expectedStationClass))
                throw std::runtime_error("Client_ToggleCraftingMenu station no longer matches the captured layout");

            diagnostics.update({
                {"PlayerController", RC::to_string(controller->GetPathName())},
                {"ShopRoute", "DominionPlayerController.Client_ToggleCraftingMenu"},
                {"StationComponent", RC::to_string(station->GetPathName())},
                {"Player", RC::to_string(player->GetPathName())}
            });
            m_activeVendorStation=PS::WeakObject(station);
            m_activeVendorController=PS::WeakObject(controller);
            m_activeVendorDefinition=definition->ModName+":"+definition->Id;
            if(definition->InlineMerchant && definition->ItemsProperty.empty()) {
                if(!m_recipes)throw std::runtime_error("Store recipe service unavailable");
                const auto visible=VisibleVendorItems(*definition,controller);
                CreateInlineMerchantRow(*definition,&visible);
                const auto owner=definition->StoreOwner.empty()
                    ? VendorOffers::Owner(definition->ModName,definition->Id) : definition->StoreOwner;
                diagnostics["OfferAvailability"]=m_recipes->PrepareStoreForPlayer(owner,visible,controller);
            }
            if(definition->HelpyTemporary&&FindHelpyNpc(actor->GetWorld(),*definition)!=actor)
                throw std::runtime_error("Temporary NPC expired while preparing its storefront.");
            RecordPhase(*definition, "Shop.Opening", actor, diagnostics);
            ActorHelper::FunctionCall call(controller,
                TEXT("/Script/Dominion.DominionPlayerController:Client_ToggleCraftingMenu"));
            call.Arg(TEXT("CraftingStationComponent"), station).Invoke();
            // Map teardown may run synchronously during a reflected UI call.
            // Do not report success against an actor/binding that was removed.
            if (generation != m_worldGeneration || std::none_of(m_merchantBindings.begin(), m_merchantBindings.end(),
                [&](const auto& current) {
                    return current.DefinitionKey == definition->ModName+":"+definition->Id
                        && current.ActorToken == actor;
                })) return;
            RecordPhase(*definition, "Shop.OpenRequested", actor, diagnostics);
            PS::Log<LogLevel::Normal>(
                STR("Requested merchant UI for vendor '{}' through Client_ToggleCraftingMenu.\n"),
                RC::to_generic_string(definition->Id));
    }

    UObject* DragonWildsNpcLoader::EnsureComponent(
        AActor* actor,
        UClass* componentClass)
    {
        auto components = actor->GetComponentsByClass(componentClass);
        for (auto* component : components)
        {
            if (component && component->IsA(componentClass))
            {
                return component;
            }
        }

        // This is the preferred path. AddComponentByClass is a reflected
        // Blueprint-callable engine helper and performs the normal component
        // ownership/registration bookkeeping. DeferredFinish lets us write
        // the vendor row before the component is finalized.
        try
        {
            const FTransform relativeTransform{};
            bool manualAttachment = true;
            bool deferredFinish = true;
            ActorHelper::FunctionCall add(actor,
                TEXT("/Script/Engine.Actor:AddComponentByClass"));
            // AddComponentByClass uses Class; GetComponentsByClass uses ComponentClass.
            add.Arg(TEXT("Class"), componentClass)
                .Arg(TEXT("bManualAttachment"), manualAttachment)
                .Arg(TEXT("RelativeTransform"), relativeTransform)
                .Arg(TEXT("bDeferredFinish"), deferredFinish)
                .Invoke();
            auto* component = add.Result<UObject*>();
            if (component)
            {
                m_createdComponents.insert(component);
                return component;
            }
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(std::string("Engine component creation failed: ") + error.what());
        }
        throw std::runtime_error("AddComponentByClass returned null; no manual ownership fallback is allowed");
    }

    nlohmann::json DragonWildsNpcLoader::VisibleVendorItems(
        const VendorDefinition& definition,UObject* controller)
    {
        std::optional<int> power;
        const bool needsPower=std::ranges::any_of(definition.CategoryRules,[](const auto& rule){
            return rule.MinimumPowerLevel.has_value() || rule.MaximumPowerLevel.has_value();
        }) || std::ranges::any_of(definition.Items,[](const auto& item){return item.contains("MinPowerLevel") || item.contains("MaxPowerLevel");});
        if(needsPower) {
            std::vector<UObject*> candidates;
            try {
                ActorHelper::FunctionCall pawn(controller,TEXT("/Script/Engine.Controller:K2_GetPawn"));
                pawn.Invoke();if(auto* value=pawn.Result<UObject*>())candidates.push_back(value);
            }catch(...) {}
            if(controller)candidates.push_back(controller);
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
                TEXT("/Script/Dominion.PowerLevelInterface:GetPowerLevel"),false);
            auto* result=function?CastField<FNumericProperty>(function->GetReturnProperty()):nullptr;
            if(function && result && result->GetArrayDim()==1)for(auto* candidate:candidates)try {
                ActorHelper::FunctionCall call(candidate,function);call.Invoke();
                const auto value=call.NumericResult();
                if(std::isfinite(value) && value>=0 && value<=100) {power=static_cast<int>(std::floor(value));break;}
            }catch(...) {}
            // Some player builds expose the replicated value as a numeric
            // property without implementing the interface on the pawn.
            if(!power)for(auto* candidate:candidates)for(const auto* field:{TEXT("PowerLevel"),TEXT("CurrentPowerLevel")})try {
                auto* property=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(candidate->GetClassPrivate(),field));
                if(!property||property->GetArrayDim()!=1)continue;
                const auto value=property->IsFloatingPoint()?property->GetFloatingPointPropertyValue(property->ContainerPtrToValuePtr<void>(candidate)):
                    static_cast<double>(property->GetSignedIntPropertyValue(property->ContainerPtrToValuePtr<void>(candidate)));
                if(std::isfinite(value)&&value>=0&&value<=100){power=static_cast<int>(std::floor(value));break;}
            }catch(...) {}
        }
        auto time=TimeOfDay::Requirement::Any;
        if(std::ranges::any_of(definition.CategoryRules,[](const auto& rule){return rule.Time!=TimeOfDay::Requirement::Any;})
            || std::ranges::any_of(definition.Items,[](const auto& item){return item.contains("TimeOfDay");}))
            time=TimeOfDay::Current(controller);
        const auto completed=[&](const std::string& quest){DialogueCompletionBinding gate;gate.GateQuest=quest;gate.GateStates={"Completed"};return DialogueGateAllows(gate,controller);};
        return VendorCategoryGate::Filter(definition.Items,definition.CategoryRules,power,time,completed);
    }

    void DragonWildsNpcLoader::CreateInlineMerchantRow(
        const VendorDefinition& definition,const nlohmann::json* visibleItems)
    {
        auto* tableObject = ActorHelper::ResolveObject(
            RC::to_generic_string(definition.DataTablePath));
        auto* dataTableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.DataTable"));
        if (!tableObject || !dataTableClass || !tableObject->IsA(dataTableClass))
        {
            throw std::runtime_error(std::format(
                "inline merchant DataTable '{}' was unavailable",
                definition.DataTablePath));
        }

        auto* dataTable = static_cast<UDataTable*>(tableObject);
        auto* rowStruct = dataTable->GetRowStruct().Get();
        if (!rowStruct)
        {
            throw std::runtime_error(std::format(
                "inline merchant DataTable '{}' has no row struct",
                definition.DataTablePath));
        }

        const FName rowName(RC::to_generic_string(definition.RowName), FNAME_Add);
        const auto owner=definition.StoreOwner.empty()
            ? VendorOffers::Owner(definition.ModName,definition.Id) : definition.StoreOwner;
        uint8_t* existingOwned=nullptr;
        for (auto created = m_createdRows.begin(); created != m_createdRows.end(); ++created)
        {
            if (FName(RC::to_generic_string(created->RowName),FNAME_Find) == rowName
                && created->Table == dataTable && HasLiveTableLease(dataTable))
            {
                if(created->Owner!=owner)throw std::runtime_error("Merchant RowName already belongs to another vendor; use a unique RowName");
                const auto* existing = dataTable->FindRowUnchecked(rowName);
                if (existing && existing == created->RowAddress) {
                    existingOwned=const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(existing));
                    break;
                }
                if (existing) throw std::runtime_error("Owned merchant row was replaced externally; refusing to overwrite it");
                m_createdRows.erase(created);
                break; // Our row was removed; recreate it from this definition.
            }
        }
        if (!existingOwned && dataTable->FindRowUnchecked(rowName))
        {
            throw std::runtime_error("Inline merchant RowName already exists; choose a unique RowName or omit Items to explicitly bind that existing row");
        }

        FManagedStruct rowData(rowStruct);
        // Vendor presentation belongs to the custom station row, not EWidgetType.
        // Set only our newly allocated row; never modify a stock merchant row.
        auto* vendorFlag = CastField<FBoolProperty>(
            PropertyHelper::GetPropertyByName(rowStruct, TEXT("bIsVendor")));
        if (!vendorFlag || vendorFlag->GetArrayDim() != 1)
            throw std::runtime_error("inline merchant row has no supported bIsVendor boolean");
        PropertyHelper::CopyJsonValueToContainer(rowData.GetData(), vendorFlag, true);
        auto* repairFlag = CastField<FBoolProperty>(
            PropertyHelper::GetPropertyByName(rowStruct, TEXT("bHaveRepairOption")));
        if (!repairFlag || repairFlag->GetArrayDim() != 1)
            throw std::runtime_error("inline merchant row has no supported bHaveRepairOption boolean");
        PropertyHelper::CopyJsonValueToContainer(rowData.GetData(), repairFlag, definition.Repairable);
        auto* masterworkFlag = CastField<FBoolProperty>(
            PropertyHelper::GetPropertyByName(rowStruct, TEXT("bHaveMasterworkOption")));
        if (!masterworkFlag || masterworkFlag->GetArrayDim() != 1)
            throw std::runtime_error("inline merchant row has no supported bHaveMasterworkOption boolean");
        PropertyHelper::CopyJsonValueToContainer(rowData.GetData(), masterworkFlag, definition.Masterworkable);
        if(!definition.VendorHeaderImage.empty()) {
            auto* header=CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(rowStruct,TEXT("VendorHeaderImage")));
            if(!header || header->GetArrayDim()!=1)
                throw std::runtime_error("Merchant row has no supported VendorHeaderImage soft reference");
            auto* texture=ActorHelper::ResolveObject(RC::to_generic_string(definition.VendorHeaderImage));
            auto* expected=header->GetPropertyClass().Get();
            if(!texture || !expected || !texture->IsA(expected))
                throw std::runtime_error("VendorHeaderImage could not resolve to the required asset type: "+definition.VendorHeaderImage);
            PropertyHelper::CopyJsonValueToContainer(rowData.GetData(),header,RC::to_string(texture->GetPathName()));
        }
        if (!vendorFlag->GetPropertyValue(vendorFlag->ContainerPtrToValuePtr<void>(rowData.GetData())))
            throw std::runtime_error("inline merchant bIsVendor did not read back as true");
        if (repairFlag->GetPropertyValue(repairFlag->ContainerPtrToValuePtr<void>(rowData.GetData())) != definition.Repairable)
            throw std::runtime_error("inline merchant bHaveRepairOption did not read back as requested");
        if (masterworkFlag->GetPropertyValue(masterworkFlag->ContainerPtrToValuePtr<void>(rowData.GetData())) != definition.Masterworkable)
            throw std::runtime_error("inline merchant bHaveMasterworkOption did not read back as requested");
        bool wroteName = false;
        if (!definition.MerchantName.empty())
        {
            for (const auto* candidate : {
                "DisplayNameOverride", "Name", "VendorName", "MerchantName", "DisplayName", "ShopName"
            })
            {
                auto* property = PropertyHelper::GetPropertyByName(
                    rowStruct, RC::to_generic_string(candidate));
                if (!property)
                {
                    continue;
                }
                try
                {
                    PropertyHelper::CopyJsonValueToContainer(
                        rowData.GetData(), property, definition.MerchantName);
                    wroteName = true;
                    break;
                }
                catch (const std::exception&)
                {
                    // A differently typed field with the same semantic name
                    // is not a valid display-name target; try the next alias.
                }
            }
        }

        FProperty* itemsProperty = nullptr;
        auto sourceItems=visibleItems?*visibleItems:definition.Items;
        sourceItems=VendorOffers::OrderedWithinCategories(sourceItems);
        auto rowItems=sourceItems;
        if (!definition.ItemsProperty.empty())
        {
            itemsProperty = PropertyHelper::GetPropertyByName(
                rowStruct, RC::to_generic_string(definition.ItemsProperty));
            if (!itemsProperty)
            {
                throw std::runtime_error(std::format(
                    "inline merchant ItemsProperty '{}' was not found on row struct {}",
                    definition.ItemsProperty, RC::to_string(rowStruct->GetName())));
            }
        }
        else
        {
            // Dragonwilds' CraftingStationDataTableRow holds categories of
            // RecipeData soft references, not direct item/price records.
            itemsProperty=PropertyHelper::GetPropertyByName(rowStruct,TEXT("LabeledRecipes"));
            if(itemsProperty) {
                if(!m_recipes)throw std::runtime_error("Vendor recipe service unavailable");
                auto* groups=CastField<FArrayProperty>(itemsProperty);
                auto* group=groups?CastField<FStructProperty>(groups->GetInner()):nullptr;
                auto* type=group?group->GetStruct().Get():nullptr;
                auto* label=type?PropertyHelper::GetPropertyByName(type,TEXT("Label")):nullptr;
                auto* collection=type?CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(type,TEXT("Collection"))):nullptr;
                if(!label || !collection || !CastField<FSoftObjectProperty>(collection->GetInner()))
                    throw std::runtime_error("LabeledRecipes does not expose the expected Label/Collection soft-reference layout");
                rowItems=nlohmann::json::array();
                size_t index=0;
                for(const auto& item:sourceItems) {
                    const auto slot=item.value("_RecipeSlot",std::to_string(index++));
                    try {
                    auto properties=VendorOffers::Properties(item);
                    auto* itemClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
                    for(const auto* field:{"ItemsCreated","ItemsConsumed"}) {
                        auto& path=properties[field][0]["ItemData"];
                        auto* asset=ActorHelper::ResolveObject(RC::to_generic_string(path.get<std::string>()));
                        if(!asset || !itemClass || !asset->IsA(itemClass))
                            throw std::runtime_error("Vendor offer Item/Currency did not resolve to ItemData: "+path.get<std::string>());
                        path=RC::to_string(asset->GetPathName());
                    }
                    auto* recipe=m_recipes->EnsureVendorRecipe(owner+":"+slot,
                        VendorOffers::Identity(owner,slot),properties);
                    const auto category=VendorOffers::Category(item);
                    auto found=std::find_if(rowItems.begin(),rowItems.end(),[&](const auto& group){return group["Label"]==category;});
                    if(found==rowItems.end()) {
                        rowItems.push_back({{"Label",category},{"Collection",nlohmann::json::array()}});
                        found=std::prev(rowItems.end());
                    }
                    (*found)["Collection"].push_back({{"AssetPathName",RC::to_string(recipe->GetPathName())},{"SubPathString",""}});
                    }catch(const std::exception& error) {
                        WarnOnce("vendor-offer:"+owner+":"+slot,RC::to_generic_string("[DEGRADED][LOADER:vendors][MOD:"+definition.ModName+"][VENDOR:"+definition.Id+"][OFFER:"+slot+"] Item/currency did not resolve; offer disabled, remaining offers continue: "+error.what()));
                    }
                }
            }
            else
            for (const auto* candidate : {
                "Items", "VendorItems", "MerchantItems", "ItemEntries",
                "Entries", "Products", "Offers"
            })
            {
                auto* property = PropertyHelper::GetPropertyByName(
                    rowStruct, RC::to_generic_string(candidate));
                if (property && CastField<FArrayProperty>(property))
                {
                    itemsProperty = property;
                    break;
                }
            }
        }

        auto* itemsArray = itemsProperty
            ? CastField<FArrayProperty>(itemsProperty) : nullptr;
        if (!itemsArray)
        {
            throw std::runtime_error(std::format(
                "inline merchant row struct {} has no reflected item array; set ItemsProperty after inspecting it",
                RC::to_string(rowStruct->GetName())));
        }

        const bool nativeCategories = definition.ItemsProperty.empty()
            && itemsProperty->GetName() == TEXT("LabeledRecipes");
        try
        {
            if (nativeCategories)
                VendorCategoryText::WriteGroups(rowData.GetData(), itemsArray, rowItems);
            else {
                for(auto& item:rowItems)if(item.is_object())item.erase("Order");
                PropertyHelper::CopyJsonValueToContainer(rowData.GetData(), itemsArray, rowItems);
            }
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(std::format(
                "inline merchant Items could not be written to row property '{}': {}",
                RC::to_string(itemsProperty->GetName()), error.what()));
        }

        if(existingOwned) {
            // Refresh the complete owned row so presentation and capability flags
            // stay in sync with its offers after a reload.
            rowStruct->CopyScriptStruct(existingOwned,rowData.GetData());
            if (nativeCategories) {
                VendorCategoryText::VerifyGroups(existingOwned, itemsArray, rowItems);
                // Count the verified groups, not individual offers. Rewriting an
                // owned row updates this many categories, including unchanged labels.
                PS::Log<LogLevel::Verbose>(
                    STR("Updated runtime merchant row '{}' in {} with {} item definition(s); {} categories updated.\n"),
                    rowName.ToString(), dataTable->GetNamePrivate().ToString(),
                    sourceItems.size(), rowItems.size());
            }
            return;
        }
        if (!HasLiveTableLease(dataTable))
        {
            const bool addedRoot = !dataTable->IsRootSet();
            m_tableLeases.push_back({dataTable, dataTable->GetInternalIndex(), addedRoot});
            if (addedRoot) dataTable->SetRootSet();
            PS::Log<LogLevel::Verbose>(STR("Vendor table lifetime pinned: {} (weak serial {}; root owned {}).\n"),
                dataTable->GetPathName(), PS::WeakObject(dataTable).ObjectSerialNumber, addedRoot);
        }
        dataTable->AddRow(rowName,
            *reinterpret_cast<RC::Unreal::FTableRowBase*>(rowData.GetData()));
        m_createdRows.push_back({
            dataTable, definition.RowName, owner,
            dataTable->FindRowUnchecked(rowName)
        });

        if (nativeCategories)
        {
            VendorCategoryText::VerifyGroups(dataTable->FindRowUnchecked(rowName),
                itemsArray, rowItems);
            // Fold category totals into the existing merchant-creation summary.
            // Verification remains active regardless of the configured log level.
            if (wroteName)
            {
                PS::Log<LogLevel::Verbose>(
                    STR("Created runtime merchant row '{}' in {} with {} item definition(s) and name '{}'; {} categories created.\n"),
                    rowName.ToString(), dataTable->GetNamePrivate().ToString(),
                    sourceItems.size(), RC::to_generic_string(definition.MerchantName),
                    rowItems.size());
            }
            else
            {
                PS::Log<LogLevel::Verbose>(
                    STR("Created runtime merchant row '{}' in {} with {} item definition(s); {} categories created.\n"),
                    rowName.ToString(), dataTable->GetNamePrivate().ToString(),
                    sourceItems.size(), rowItems.size());
            }
        }
        else if (wroteName)
        {
            PS::Log<LogLevel::Normal>(
                STR("Created runtime merchant row '{}' in {} with {} item definition(s) and name '{}'.\n"),
                rowName.ToString(), dataTable->GetNamePrivate().ToString(),
                sourceItems.size(), RC::to_generic_string(definition.MerchantName));
        }
        else
        {
            PS::Log<LogLevel::Normal>(
                STR("Created runtime merchant row '{}' in {} with {} item definition(s).\n"),
                rowName.ToString(), dataTable->GetNamePrivate().ToString(),
                sourceItems.size());
        }
    }

    bool DragonWildsNpcLoader::HasLiveTableLease(UDataTable* table) const
    {
        for (const auto& lease : m_tableLeases)
        {
            if (lease.Table != table || lease.Index < 0) continue;
            auto* slot = FUObjectArray::IndexToObject(lease.Index);
            // Do not dereference a saved pointer before validating its slot.
            // Rooted assets cannot be collected; serial zero is legal here.
            if (slot && PinnedObjectSlotMatches(table, slot->GetUObject(),
                slot->IsRootSet(), slot->IsValid(false))) return true;
        }
        return false;
    }

    void DragonWildsNpcLoader::RemoveCreatedMerchantRows()
    {
        for (const auto& created : m_createdRows)
        {
            if (HasLiveTableLease(created.Table))
            {
                try
                {
                    auto* dataTable = created.Table;
                    const FName rowName(RC::to_generic_string(created.RowName), FNAME_Find);
                    // Never remove a row another writer has replaced.
                    if (dataTable->FindRowUnchecked(rowName) == created.RowAddress)
                        dataTable->RemoveRow(rowName);
                }
                catch (...)
                {
                    // Shutdown may fail. The validated rooted lease
                    // avoids the pinned UE4SS zero-serial weak-pointer issue.
                }
            }
        }
        m_createdRows.clear();
        for (const auto& lease : m_tableLeases)
        {
            if (lease.AddedRoot && HasLiveTableLease(lease.Table))
            {
                try { lease.Table->ClearRootSet(); } catch (...) {}
            }
        }
        m_tableLeases.clear();
    }

    void DragonWildsNpcLoader::ApplyComponentProperties(
        UObject* component,
        const nlohmann::json& properties,
        const VendorDefinition& definition)
    {
        if (!properties.is_object())
        {
            return;
        }
        for (const auto& [name, value] : properties.items())
        {
            if (name.starts_with("$"))
            {
                continue;
            }
            auto* property = PropertyHelper::GetPropertyByName(
                component->GetClassPrivate(), RC::to_generic_string(name));
            if (!property)
            {
                throw std::runtime_error(std::format(
                    "Vendor '{}' property '{}' was not found on {}",
                    definition.Id, name,
                    RC::to_string(component->GetClassPrivate()->GetName())));
            }
            PropertyHelper::CopyJsonValueToContainer(component, property, value);
        }
    }

    void DragonWildsNpcLoader::ApplyVendorRow(
        UObject* component,
        const VendorDefinition& definition)
    {
        auto* tableObject = ActorHelper::ResolveObject(
            RC::to_generic_string(definition.DataTablePath));
        auto* dataTableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.DataTable"));
        if (!tableObject || !dataTableClass || !tableObject->IsA(dataTableClass))
        {
            throw std::runtime_error(std::format(
                "vendor DataTable '{}' was unavailable",
                definition.DataTablePath));
        }

        auto* dataTable = static_cast<UDataTable*>(tableObject);
        const FName rowName(RC::to_generic_string(definition.RowName), FNAME_Add);
        if (!dataTable->FindRowUnchecked(rowName))
        {
            throw std::runtime_error(std::format(
                "vendor row '{}' was not found in '{}'",
                definition.RowName, definition.DataTablePath));
        }

        auto* handleProperty = CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(component->GetClassPrivate(),
                RC::to_generic_string(definition.RowHandleProperty)));
        auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
        auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
        auto* rowProperty = handleStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
        if (!handleProperty || !handleStruct || !tableProperty || !rowProperty)
        {
            throw std::runtime_error(std::format(
                "row handle '{}' was unavailable on {}",
                definition.RowHandleProperty,
                RC::to_string(component->GetClassPrivate()->GetName())));
        }

        auto* handle = handleProperty->ContainerPtrToValuePtr<void>(component);
        std::memcpy(tableProperty->ContainerPtrToValuePtr<void>(handle),
            &tableObject, sizeof(tableObject));
        rowProperty->SetPropertyValue(
            rowProperty->ContainerPtrToValuePtr<void>(handle), rowName);
    }

    void DragonWildsNpcLoader::RegisterComponent(
        AActor* actor,
        UObject* component,
        const VendorDefinition& definition)
    {
        if (!m_createdComponents.contains(component))
        {
            return;
        }

        try
        {
                const FTransform relativeTransform{};
                bool manualAttachment = true;
                ActorHelper::FunctionCall finish(actor,
                    TEXT("/Script/Engine.Actor:FinishAddComponent"));
                finish.Arg(TEXT("Component"), component)
                    .Arg(TEXT("bManualAttachment"), manualAttachment)
                    .Arg(TEXT("RelativeTransform"), relativeTransform)
                    .Invoke();
            m_createdComponents.erase(component);
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(std::format(
                "Vendor '{}' could not register component '{}': {}",
                definition.Id, RC::to_string(component->GetName()), error.what()));
        }
    }

    bool DragonWildsNpcLoader::WarnOnce(
        const std::string& key,
        const RC::StringType& message)
    {
        if (!m_warnings.insert(key).second)
        {
            return false;
        }
        PS::Log<LogLevel::Warning>(STR("{}\n"), message);
        return true;
    }

    bool DragonWildsNpcLoader::ErrorOnce(
        const std::string& key,
        const RC::StringType& message)
    {
        if (!m_errors.insert(key).second)
        {
            return false;
        }
        PS::Log<LogLevel::Error>(STR("{}\n"), message);
        return true;
    }

    std::string DragonWildsNpcLoader::ActorKey(
        AActor* actor,
        const VendorDefinition& definition)
    {
        return definition.ModName + ":" + definition.Id + ":"
            + std::to_string(reinterpret_cast<std::uintptr_t>(actor));
    }
}
