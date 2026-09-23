#include "Generator/HelpyReferencePolicy.h"
#include "Generator/F2CatalogPlan.h"
#include "Generator/F2BundledPaths.h"
#include "Generator/ItemCatalogMetadata.h"
#include "Runtime/F2CatalogStore.h"
#include "Runtime/F2ReferenceIndex.h"
#include "Runtime/AuthoredFile.h"
#include "SDK/Helper/CookedAssetLookup.h"
#include "Loader/DragonWildsAssetModLoader.h"
#include "Loader/DragonWildsNpcLoader.h"
#include "Loader/HelpyNpcGuards.h"
#include "Loader/AssetMetadataRegistry.h"
#include "Loader/AssetAuthoringMetadata.h"
#include "Utility/NativeFunctionHook.h"
#include "Runtime/NetworkContext.h"
#include "Loader/PlayerActivityEvents.h"
#include "Loader/NiagaraAttachment.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "SDK/WeakObjectHandle.h"
using namespace DragonWilds::SpawnRuntime;
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <cctype>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>
#include <thread>
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Helpers/Casting.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Transform.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "Unreal/UAssetRegistry.hpp"
#include "Unreal/UAssetRegistryHelpers.hpp"
#include "Unreal/FAssetData.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/EventIdentity.h"
#include "Loader/PlayerGhost.h"
#include "Generator/EventParameters.h"
#include "Generator/ToolRequest.h"
#include "Generator/QuickMenuUI.h"
#include "Generator/QuickMenuCatalogRules.h"
#include "Generator/SpawnAuthoring.h"
#include "Generator/AssetSearch.h"
#include "Generator/ItemIconThumbnail.h"
#include "Loader/SpawnAuthoringFields.h"
#include "Loader/SpawnPlacementDraft.h"
#include "Loader/TimeOfDayRuntime.h"
#include "Core/ConfigFiles.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include <Windows.h>
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/PlayerAttributeNames.h"
#include "Core/JsonPatchDirective.h"
#include "Core/JsonLoadOrderMerge.h"
#include "Runtime/HostServices.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    UObject* GetAIDirector(UObject* worldContext)
    {
        return CallWorldContextGetter(TEXT("/Script/Dominion.AiDirector:Get"),
            TEXT("/Script/Dominion.Default__AiDirector"), worldContext);
    }

    FGuid StableGuid(const std::string& identity)
    {
        uint32 lanes[] = { 2166136261u, 2166136261u ^ 0x9E3779B9u,
            2166136261u ^ 0x85EBCA6Bu, 2166136261u ^ 0xC2B2AE35u };
        for (const auto byte : identity)
        {
            for (auto& lane : lanes)
            {
                lane = (lane ^ static_cast<uint8>(byte)) * 16777619u;
            }
        }

        FGuid id{};
        std::memcpy(&id, lanes, sizeof(id));
        return id;
    }

    constexpr uint32 ActorGuidMagic = 0x48435352;

    FGuid StableActorGuid(const std::string& identity)
    {
        auto id = StableGuid(identity);
        std::memcpy(&id, &ActorGuidMagic, sizeof(ActorGuidMagic));
        return id;
    }

    UClass* ResolveBuildingActorClass(const nlohmann::json& piece)
    {
        if (piece.contains("Class") && piece.at("Class").is_string())
        {
            const auto path = piece.at("Class").get<std::string>();
            if (!path.empty())
                if (auto* type = DragonWilds::ActorHelper::ResolveClass(RC::to_generic_string(path)))
                    return type;
        }
        if (!piece.contains("Building") || !piece.at("Building").is_string()) return nullptr;
        auto* building = DragonWilds::ActorHelper::ResolveObject(
            RC::to_generic_string(piece.at("Building").get<std::string>()));
        auto* soft = building ? CastField<FSoftObjectProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(building->GetClassPrivate(), TEXT("BuildableActor"))) : nullptr;
        if (!soft || soft->GetArrayDim() != 1
            || soft->GetElementSize() != sizeof(UECustom::FSoftObjectPtr)) return nullptr;
        const auto& reference = *soft->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(building);
        auto* loaded = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(
            UECustom::TSoftObjectPtr<UObject>(reference.ObjectID));
        return loaded && loaded->IsA<UClass>() ? static_cast<UClass*>(loaded) : nullptr;
    }

    FTransform AssemblyComponentTransform(UObject* component)
    {
        try
        {
            DragonWilds::ActorHelper::FunctionCall call(component,
                TEXT("/Script/Engine.SceneComponent:K2_GetComponentToWorld"));
            call.Invoke();
            return call.Result<FTransform>();
        }
        catch (...)
        {
            auto read = [component]<typename T>(const TCHAR* name) {
                auto* property = CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(
                    component->GetClassPrivate(), name));
                if (!property || property->GetSize() != sizeof(T))
                    throw std::runtime_error("building mesh component transform is unavailable");
                T value{};
                std::memcpy(&value, property->ContainerPtrToValuePtr<void>(component), sizeof(T));
                return value;
            };
            return FTransform(read.template operator()<FRotator>(TEXT("RelativeRotation")),
                read.template operator()<FVector>(TEXT("RelativeLocation")),
                read.template operator()<FVector>(TEXT("RelativeScale3D")));
        }
    }

    FTransform ComposeAssemblyTransforms(const FTransform& component,
        const FTransform& piece)
    {
        auto* library = DragonWilds::ActorHelper::ResolveObject(
            TEXT("/Script/Engine.Default__KismetMathLibrary"));
        if (!library) throw std::runtime_error("Kismet transform composition is unavailable");
        DragonWilds::ActorHelper::FunctionCall call(library,
            TEXT("/Script/Engine.KismetMathLibrary:ComposeTransforms"));
        call.Arg(TEXT("A"), component).Arg(TEXT("B"), piece).Invoke();
        return call.Result<FTransform>();
    }

    bool HasActorGuidMagic(const FGuid& id)
    {
        uint32 head = 0;
        std::memcpy(&head, &id, sizeof(head));
        return head == ActorGuidMagic;
    }

    FGuid ReadGuidProperty(UObject* object, FProperty* property)
    {
        FGuid id{};
        std::memcpy(&id, reinterpret_cast<uint8*>(object) + property->GetOffset_Internal(), sizeof(id));
        return id;
    }

    void SetGuidProperty(AActor* actor, const TCHAR* propertyName, const FGuid& id)
    {
        auto* property = DragonWilds::PropertyHelper::GetPropertyByName(actor->GetClassPrivate(), propertyName);
        if (!property || property->GetSize() != sizeof(FGuid))
        {
            throw std::runtime_error("Guid property was unavailable on the spawned actor");
        }

        std::memcpy(reinterpret_cast<uint8*>(actor) + property->GetOffset_Internal(), &id, sizeof(id));
    }

    void ApplyEntryProperties(UObject* actor, const nlohmann::json& properties)
    {
        auto* actorClass = actor->GetClassPrivate();
        for (const auto& [propertyName, propertyValue] : properties.items())
        {
            auto propertyNameWide = RC::to_generic_string(propertyName);
            auto* property = DragonWilds::PropertyHelper::GetPropertyByName(actorClass, propertyNameWide);
            if (!property)
            {
                PS::Log<LogLevel::Warning>(STR("Property '{}' does not exist in {}\n"), propertyNameWide, actorClass->GetName());
                continue;
            }

            try
            {
                DragonWilds::PropertyHelper::CopyJsonValueToContainer(actor, property, propertyValue);
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed writing '{}': {}\n"), propertyNameWide, PS::ToWideSafe(e.what()));
            }
        }
    }

    bool InvokeAIDirectorSpawnFunction(UObject* director,
        const TCHAR* functionPath, AActor* spawnPoint, bool allowNoArguments)
    {
        if (!director) return false;
        auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, functionPath);
        if (!function) return false;

        FProperty* objectInput = nullptr;
        int inputCount = 0;
        for (auto* property : TFieldRange<FProperty>(
            function, EFieldIterationFlags::IncludeDeprecated))
        {
            const auto flags = property->GetPropertyFlags();
            if (!(flags & CPF_Parm) || (flags & CPF_ReturnParm)
                || (flags & CPF_OutParm)) continue;
            inputCount++;
            if (CastField<FObjectProperty>(property)) objectInput = property;
        }

        try
        {
            auto call = DragonWilds::ActorHelper::FunctionCall(
                director, functionPath);
            if (inputCount == 1 && objectInput)
            {
                call.Arg(objectInput->GetName().c_str(), spawnPoint).Invoke();
                return true;
            }
            if (inputCount == 0 && allowNoArguments)
            {
                call.Invoke();
                return true;
            }
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(
                STR("AI director call '{}' failed safely: {}\n"),
                functionPath, PS::ToWideSafe(error.what()));
            return false;
        }

        PS::Log<LogLevel::Warning>(
            STR("AI director function '{}' has an unsupported reflected signature ({} input parameter(s)).\n"),
            functionPath, inputCount);
        return false;
    }

    bool SetInstanceText(UObject* object, const TCHAR* propertyName,
        const std::string& value)
    {
        if (!object) return false;
        auto* property = DragonWilds::PropertyHelper::CastProperty<FTextProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), propertyName));
        if (!property) return false;
        try
        {
            DragonWilds::PropertyHelper::SetTextPropertyValueFromJsonValue(
                property->ContainerPtrToValuePtr<void>(object), property, value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool SetTextBlockText(UObject* textBlock, const std::string& value)
    {
        if (!textBlock) return false;
        auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/UMG.TextBlock:SetText"));
        auto* input = function
            ? function->FindProperty(FName(TEXT("InText"), FNAME_Find)) : nullptr;
        if (!function || !input || input->GetOffset_Internal() < 0) return false;
        std::vector<uint8> params(function->GetParmsSize(), 0);
        try
        {
            input->InitializeValue_InContainer(params.data());
            DragonWilds::PropertyHelper::CopyJsonValueToContainer(
                params.data(), input, value);
            textBlock->ProcessEvent(function, params.data());
            input->DestroyValue_InContainer(params.data());
            return true;
        }
        catch (...)
        {
            try { input->DestroyValue_InContainer(params.data()); }
            catch (...) {}
            return false;
        }
    }

    void ApplyActorDisplayName(UObject* actor, const std::string& displayName)
    {
        if (!actor || displayName.empty()) return;

        auto* property = DragonWilds::PropertyHelper::GetPropertyByName(
            actor->GetClassPrivate(), TEXT("DisplayName"));
        if (!property)
        {
            PS::Log<LogLevel::Warning>(
                STR("Custom actor name '{}' could not be applied: DisplayName is unavailable on {}.\n"),
                PS::ToWideSafe(displayName.c_str()), actor->GetClassPrivate()->GetName());
            return;
        }

        try
        {
            DragonWilds::PropertyHelper::CopyJsonValueToContainer(
                actor, property, displayName);
            PS::Log<LogLevel::Verbose>(
                STR("Applied instance display name '{}' to managed actor {}.\n"),
                PS::ToWideSafe(displayName.c_str()), actor->GetClassPrivate()->GetName());
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(
                STR("Custom actor name '{}' failed safely on {}: {}\n"),
                PS::ToWideSafe(displayName.c_str()), actor->GetClassPrivate()->GetName(),
                PS::ToWideSafe(error.what()));
        }
    }

}

namespace DragonWilds {
#include "SpawnItemIcons.inl"
#include "SpawnTools.inl"
#include "SpawnAdditionalDrops.inl"

    DragonWildsSpawnLoader::DragonWildsSpawnLoader() : DragonWildsModLoaderBase("spawns")
    {
        SetDisplayName(TEXT("Spawn Loader"));
    }

    DragonWildsSpawnLoader::~DragonWildsSpawnLoader()
    {
        g_f2ReferenceJob.Shutdown(); // join copied-string worker before unloading this DLL
        ClearBonusRows();
        if (m_onLevelShownFunction && m_onLevelShownCallbackId != 0)
        {
            m_onLevelShownFunction->UnregisterHook(m_onLevelShownCallbackId);
        }
        if (m_aiScaleFunction && m_aiScaleCallbackId != 0)
        {
            m_aiScaleFunction->UnregisterHook(m_aiScaleCallbackId);
        }
        DragonWildsBlueprintModLoader::SetActorInitializedObserver(nullptr);
        if (m_healthBarSetTextFunction && m_healthBarSetTextCallbackId != 0)
        {
            m_healthBarSetTextFunction->UnregisterHook(m_healthBarSetTextCallbackId);
        }
        if (m_playerPostLoginFunction && m_playerPostLoginCallbackId != 0)
        {
            m_playerPostLoginFunction->UnregisterHook(m_playerPostLoginCallbackId);
        }
        if (m_playerClientRestartFunction && m_playerClientRestartCallbackId != 0)
        {
            m_playerClientRestartFunction->UnregisterHook(m_playerClientRestartCallbackId);
        }
        if (m_playerPossessionAckFunction && m_playerPossessionAckCallbackId != 0)
            m_playerPossessionAckFunction->UnregisterHook(m_playerPossessionAckCallbackId);
        if (m_playerPawnStateFunction && m_playerPawnStateCallbackId != 0)
        {
            m_playerPawnStateFunction->UnregisterHook(m_playerPawnStateCallbackId);
        }
        if (m_playerTagsChangedFunction && m_playerTagsChangedCallbackId != 0)
        {
            m_playerTagsChangedFunction->UnregisterHook(m_playerTagsChangedCallbackId);
        }
        if (m_playerDamageReceivedFunction && m_playerDamageReceivedCallbackId != 0)
        {
            m_playerDamageReceivedFunction->UnregisterHook(m_playerDamageReceivedCallbackId);
        }
        if (m_activityObserver != Hook::ERROR_ID) Hook::UnregisterCallback(m_activityObserver);
        if (m_respawnObserver != Hook::ERROR_ID) Hook::UnregisterCallback(m_respawnObserver);
        m_observedActivities.clear();
        for (const auto& hook : m_playerActivityHooks)
        {
            if (hook.Function && hook.CallbackId != 0)
                hook.Function->UnregisterHook(hook.CallbackId);
        }
        if (m_spawnTickCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_spawnTickCallbackId);
        }
        if (m_worldTeardownCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_worldTeardownCallbackId);
        }
        if (m_protectedBuildingDestroyGuard != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_protectedBuildingDestroyGuard);
        }
        PlayerGhost::Clear();
        for (const auto& ref : m_rootedVisualEffectMaterials)
            if (auto* material=ref.Get()) if (material->IsRootSet()) material->ClearRootSet();
    }

    void DragonWildsSpawnLoader::OnLoad(const fs::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::GameInstanceInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            m_spawnDocuments.push_back({modName, data});
        });
    }

    void DragonWildsSpawnLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase)
    {
        if (phase != EEngineLifecyclePhase::GameInstanceInit || m_spawnDocuments.empty()) return;

        m_reportedNewSpawns = 0;
        m_reportedAlteredSpawns = 0;
        m_reportedSpawnErrors = 0;

        struct Definition { RC::StringType Owner; std::string Reference; nlohmann::json Body; };
        struct Patch { RC::StringType Owner; JsonPatchDirective::Directive Directive; };
        std::vector<Definition> definitions;
        std::vector<Patch> patches;
        std::unordered_map<std::string, std::size_t> byReference;

        for (const auto& owned : m_spawnDocuments)
        {
            if (!owned.Document.is_array())
            {
                PS::Log<LogLevel::Error>(STR("Spawn file for {} must be an array of spawn entries.\n"), owned.ModName);
                continue;
            }
            std::size_t ordinal = 0;
            for (const auto& incoming : owned.Document)
            {
                ++ordinal;
                try
                {
                    static constexpr std::array<std::string_view, 3> protectedIdentity{"Id", "$Id", "Type"};
                    if (const auto patch = JsonPatchDirective::Parse(incoming, protectedIdentity, "spawn"))
                    {
                        patches.push_back({owned.ModName, *patch});
                        continue;
                    }
                    if (!incoming.is_object()) throw std::runtime_error("spawn entry must be an object");
                    std::string identity;
                    if (incoming.contains("$Id") && incoming.at("$Id").is_string()) identity = incoming.at("$Id").get<std::string>();
                    else if (incoming.contains("Id") && incoming.at("Id").is_string()) identity = incoming.at("Id").get<std::string>();
                    const auto reference = identity.empty()
                        ? RC::to_string(owned.ModName) + ":#" + std::to_string(ordinal)
                        : RC::to_string(owned.ModName) + ":" + identity;
                    if (identity.empty())
                    {
                        definitions.push_back({owned.ModName, reference, incoming});
                        continue;
                    }
                    if (const auto found = byReference.find(reference); found != byReference.end())
                        JsonLoadOrderMerge::Apply(definitions.at(found->second).Body, incoming, true);
                    else
                    {
                        byReference.emplace(reference, definitions.size());
                        definitions.push_back({owned.ModName, reference, incoming});
                    }
                }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Error>(STR("Spawn definition from {} was rejected: {}.\n"),
                        owned.ModName, PS::ToWideSafe(error.what()));
                }
            }
        }

        PatchConflicts patchConflicts;
        size_t patched = 0, patchErrors = 0;
        for (auto& patch : patches)
        {
            auto reference = patch.Directive.Reference;
            if (reference.find(':') == std::string::npos)
                reference = RC::to_string(patch.Owner) + ":" + reference;
            const auto found = byReference.find(reference);
            if (found == byReference.end())
            {
                PS::Log<LogLevel::Error>(STR("{}: spawn $Patch target '{}' was not loaded; no spawn was created.\n"),
                    patch.Owner, RC::to_generic_string(reference));
                ++patchErrors;
                continue;
            }
            const auto stats = JsonPatchDirective::Apply(
                definitions.at(found->second).Body, patch.Directive, true);
            WarnPatchConflicts(patchConflicts, "spawns:" + reference, patch.Directive.Changes, RC::to_string(patch.Owner));
            ++patched;
            ++m_reportedAlteredSpawns;
            PS::Log<LogLevel::Verbose>( STR("{} patched spawn '{}' ({} fields overwritten).\n"),
                patch.Owner, RC::to_generic_string(reference), stats.FieldsOverwritten);
        }

        m_reportedSpawnErrors += patchErrors;

        for (auto& definition : definitions)
        {
            definition.Body.erase("$Id");
            LoadSpawns(nlohmann::json::array({definition.Body}), definition.Owner);
        }
        m_spawnDocuments.clear();
        if (m_reportedNewSpawns || m_reportedAlteredSpawns || m_reportedSpawnErrors)
        {
            const auto ai = std::count_if(m_spawns.begin(), m_spawns.end(),
                [](const SpawnInfo& spawn) { return spawn.Type == ESpawnEntryType::AISpawnPoint; });
            const auto bosses = std::count_if(m_spawns.begin(), m_spawns.end(),
                [](const SpawnInfo& spawn) {
                    return spawn.Type == ESpawnEntryType::AISpawnPoint
                        && !spawn.BossName.empty();
                });
            const auto isResourceNode=[](const SpawnInfo& spawn) {
                    return spawn.Type == ESpawnEntryType::Actor
                        && (spawn.ClassPath.find(TEXT("/Trees/")) != RC::StringType::npos
                            || spawn.ClassPath.find(TEXT("/Mining/")) != RC::StringType::npos
                            || spawn.ClassPath.find(TEXT("ResourceNode")) != RC::StringType::npos
                            || spawn.ClassPath.find(TEXT("OreNode")) != RC::StringType::npos);
                };
            const auto resourceNodes = std::count_if(m_spawns.begin(), m_spawns.end(),isResourceNode);
            const auto otherActors = std::count_if(m_spawns.begin(), m_spawns.end(),
                [&](const SpawnInfo& spawn) {
                    return spawn.Type == ESpawnEntryType::Actor && !isResourceNode(spawn);
                });
            const auto removals = std::count_if(m_spawns.begin(), m_spawns.end(),
                [](const SpawnInfo& spawn) { return spawn.Type == ESpawnEntryType::RemoveActor; });
            PS::RoutineLog("spawns",
                STR("Spawns: {} new, {} altered, {} errors; {} AI ({} bosses), {} resource nodes, {} other actors, {} removals.\n"),
                m_reportedNewSpawns, m_reportedAlteredSpawns, m_reportedSpawnErrors,
                ai, bosses, resourceNodes, otherActors, removals);
        }
        TryProcessSpawns(m_readyWorld, nullptr, STR("initial load"));
    }

    void DragonWildsSpawnLoader::OnAutoReload(const RC::StringType& modName, const fs::path& modFilePath)
    {
        {
            std::scoped_lock lock{m_toolSpawnFileMutex};
            if(m_toolSpawnFiles.contains(modFilePath.lexically_normal())) {
                PS::Log<LogLevel::Warning>(STR("Authored spawn file already installed live; edits require restart: {}\n"),modFilePath.wstring());return;
            }
        }
        const auto prefix=RC::to_string(modName)+":";
        if(std::any_of(m_eventTemplates.begin(),m_eventTemplates.end(),[&](const auto& entry){return entry.first.starts_with(prefix);})) {
            PS::Log<LogLevel::Warning>(TEXT("Event spawn template changes in {} require a restart.\n"),modName);return;
        }
        const bool hasLiveSpawn = std::any_of(m_spawns.begin(), m_spawns.end(), [&](const SpawnInfo& spawn) {
            return spawn.ModName == modName && spawn.bExistsInWorld;
        });
        if (hasLiveSpawn)
        {
            PS::Log<LogLevel::Warning>(
                STR("Ignored live spawn changes for {}. Restart the game to load the new settings. "
                    "A respawn countdown already underway keeps its original deadline; "
                    "the new RespawnDuration applies after the AI next spawns and dies.\n"), modName);
            return;
        }

        std::erase_if(m_spawns, [&](const SpawnInfo& spawn) { return spawn.ModName == modName; });

        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            LoadSpawns(data, modName);
        });

        if (IsWorldStillLoaded(m_readyWorld))
        {
            TryProcessSpawns(m_readyWorld, nullptr, STR("auto-reload"));
        }
        else
        {
            m_readyWorld = nullptr;
        }
    }

    bool DragonWildsSpawnLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit;
    }

    bool DragonWildsSpawnLoader::OnInitialize()
    {
        SetupAIScaleHook();
        SetupAIBindingHooks();
        SetupPlayerJoinHooks();
        return SetupProtectedBuildingDestroyGuard()
            && SetupWorldReadyHook() && SetupSpawnTick();
    }

    bool DragonWildsSpawnLoader::IsProtectedBuildingActor(AActor* actor) const
    {
        if (!actor || actor->HasAnyFlags(static_cast<EObjectFlags>(
            RF_BeginDestroyed | RF_FinishDestroyed))) return false;
        return std::any_of(m_spawns.begin(), m_spawns.end(),
            [actor](const SpawnInfo& spawn)
            {
                if (!spawn.bBuildingProp || spawn.bAllowDeconstruction) return false;
                if (spawn.LiveActor.Get() == actor) return true;
                auto* guidProperty = PropertyHelper::GetPropertyByName(
                    actor->GetClassPrivate(), TEXT("SpudGuid"));
                if (!guidProperty || guidProperty->GetSize() != sizeof(FGuid))
                    return false;
                const auto actorId = ReadGuidProperty(actor, guidProperty);
                return std::memcmp(&actorId, &spawn.StableId, sizeof(FGuid)) == 0;
            });
    }

    bool DragonWildsSpawnLoader::SetupProtectedBuildingDestroyGuard()
    {
        if (m_protectedBuildingDestroyGuard != Hook::ERROR_ID) return true;
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("ProtectedBuildingDestroyGuard");
        const auto ownerThread = std::this_thread::get_id();
        m_protectedBuildingDestroyGuard = Hook::RegisterProcessEventPreCallback(
            [this, ownerThread](Hook::TCallbackIterationData<void>& iteration,
                UObject* source, UFunction* function, void* parameters)
            {
                if (std::this_thread::get_id() != ownerThread
                    || m_allowManagedBuildingDestroy || !source || !function
                    || !source->IsA<AActor>()) return;
                auto* actor = static_cast<AActor*>(source);
                if (!IsProtectedBuildingActor(actor)) return;
                const auto functionName = function->GetFName();
                if (functionName == FName(TEXT("CanBeDestroyedByPlayer"), FNAME_Add)) {
                    // Use the game's own demolition authorization contract so
                    // build-mode clients receive the ordinary protected result
                    // before any destroy request is attempted.
                    auto* result = CastField<FEnumProperty>(function->GetReturnProperty());
                    if (!parameters || !result) return;
                    try {
                        PropertyHelper::CopyJsonValueToContainer(parameters, result,
                            "EBuildingDestroyRequestResult::CantDestroyProtected");
                        iteration.PreventOriginalFunctionCall();
                    }
                    catch (const std::exception& error) {
                        PS::Log<LogLevel::Error>(
                            STR("Protected BuildingProp native demolition result could not be written: {}\n"),
                            PS::ToWideSafe(error.what()));
                    }
                    return;
                }
                if (functionName != FName(TEXT("K2_DestroyActor"), FNAME_Add)) return;
                iteration.PreventOriginalFunctionCall();
                PS::Log<LogLevel::Warning>(
                    STR("Protected BuildingProp '{}' ignored a player deconstruction request.\n"),
                    actor->GetName());
            }, options);
        if (m_protectedBuildingDestroyGuard == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(
                TEXT("Protected BuildingProp destruction guard could not be registered.\n"));
            return false;
        }
        return true;
    }

    void DragonWildsSpawnLoader::LoadSpawns(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (data.is_object() && data.value("$dumpAIClasses", false))
        {
            DumpAIClasses();
            return;
        }

        if (!data.is_array())
        {
            PS::Log<LogLevel::Error>(STR("Spawn file for {} must be an array of spawn entries.\n"), modName);
            return;
        }

        for (const auto& value : data)
        {
            try
            {
                for(const auto& point:SpawnFields::Expand(value))RegisterSpawn(point, modName);
            }
            catch (const std::exception& e)
            {
                ++m_reportedSpawnErrors;
                PS::Log<LogLevel::Error>(STR("Failed to register spawn in {}: {}\n"), modName, PS::ToWideSafe(e.what()));
            }
        }
    }

    void DragonWildsSpawnLoader::RegisterSpawn(const nlohmann::json& value, const RC::StringType& modName)
    {
        if(value.contains("EventOnly")) {
            auto entry=Events::ParseSpawn(RC::to_string(modName),value);
            if(m_eventTemplates.size()>=256 || !m_eventTemplates.emplace(entry.Key,std::move(entry)).second)
                throw std::runtime_error("Duplicate event spawn ID or template limit exceeded");
            ++m_reportedNewSpawns;return;
        }
        PS::JsonHelpers::ValidateFieldExists(value, "Type");
        PS::JsonHelpers::ValidateFieldExists(value, "Location");

        SpawnInfo spawn{};
        spawn.AuthoredDefinition=value;
        if(value.contains("AdditionalDrops")) {
            SpawnFields::Drops(value.at("AdditionalDrops"));
            const auto additionalDropType=value.at("Type").get<std::string>();
            if(additionalDropType!="AISpawnPoint" && additionalDropType!="Actor")
                throw std::runtime_error("AdditionalDrops requires AISpawnPoint or Actor resource entries");
            spawn.AdditionalDrops=value.at("AdditionalDrops");
        }
        spawn.EntryId=RC::to_generic_string(value.value("Id",value.value("$Id",std::string("entry_")+std::to_string(m_spawns.size()+1))));
        spawn.ModName = modName;
        if(value.contains("QuestCompleted")) {
            if(!value.at("QuestCompleted").is_string())throw std::runtime_error("QuestCompleted must be a quest ID");
            auto quest=value.at("QuestCompleted").get<std::string>();
            if(quest.empty() || quest.size()>512)throw std::runtime_error("QuestCompleted must contain a bounded quest ID");
            spawn.QuestCompleted=quest.find(':')==std::string::npos?RC::to_string(modName)+":"+quest:quest;
            if(value.contains("PersistAfterCondition")) {
                if(!value.at("PersistAfterCondition").is_boolean())throw std::runtime_error("PersistAfterCondition must be boolean");
                spawn.bPersistAfterCondition=value.at("PersistAfterCondition").get<bool>();
            }
            if(value.contains("DuplicateRadius")) {
                if(!value.at("DuplicateRadius").is_number())throw std::runtime_error("DuplicateRadius must be numeric");
                spawn.DuplicateRadius=value.at("DuplicateRadius").get<double>();
                if(!std::isfinite(spawn.DuplicateRadius) || spawn.DuplicateRadius<0 || spawn.DuplicateRadius>10000)
                    throw std::runtime_error("DuplicateRadius must be finite and between 0 and 10000 centimetres");
            }
        } else if(value.contains("PersistAfterCondition") || value.contains("DuplicateRadius"))
            throw std::runtime_error("PersistAfterCondition and DuplicateRadius require QuestCompleted");
        auto locationInput = value;
        const auto& authoredLocation = value.at("Location");
        const auto parsedLocation=SpawnFields::ParseLocation(authoredLocation);
        locationInput["Location"]=parsedLocation.Vector;
        if (parsedLocation.Ground) {
            spawn.bGroundToSurface = true;
            spawn.bGroundingResolved = false;
            spawn.GroundZOffset = parsedLocation.GroundOffset;
        }
        PS::JsonHelpers::ParseVector(locationInput, "Location", spawn.Location);
        spawn.AuthoredLocation = spawn.Location;
        // Keep the authored identity stable while giving every /spawns actor
        // a small landscape clearance at its final placement.
        spawn.Location=FVector(spawn.Location.X(),spawn.Location.Y(),spawn.Location.Z()+10.0);
        // Persistent placements default to a blocking-surface trace so an
        // authored approximation cannot leave AI, resources, or props
        // floating. Authors can explicitly opt out for intentional airborne
        // actors; the legacy '$' Z shorthand remains supported.
        if(!spawn.bGroundToSurface)spawn.bGroundToSurface=true;
        spawn.bGroundingResolved=false;
        if(value.contains("GroundToSurface")) {
            if(!value.at("GroundToSurface").is_boolean())throw std::runtime_error("GroundToSurface must be boolean");
            spawn.bGroundToSurface=value.at("GroundToSurface").get<bool>();
            spawn.bGroundingResolved=!spawn.bGroundToSurface;
        }
        if(value.contains("GroundOffset")) {
            if(parsedLocation.Ground)throw std::runtime_error("Use Location.Z '$+offset' or GroundOffset, not both");
            if(!value.at("GroundOffset").is_number())throw std::runtime_error("GroundOffset must be numeric");
            spawn.GroundZOffset=value.at("GroundOffset").get<double>();
            if(!std::isfinite(spawn.GroundZOffset) || std::abs(spawn.GroundZOffset)>100000)
                throw std::runtime_error("GroundOffset must be finite and between -100000 and 100000");
        }

        if (PS::JsonHelpers::FieldExists(value, "Rotation"))
        {
            PS::JsonHelpers::ParseRotator(value, "Rotation", spawn.Rotation);
        }

        if (PS::JsonHelpers::FieldExists(value, "Scale"))
        {
            const auto& scale = value.at("Scale");
            if (scale.is_number())
            {
                const auto uniformScale = scale.get<double>();
                spawn.Scale = FVector(uniformScale, uniformScale, uniformScale);
            }
            else
            {
                PS::JsonHelpers::ParseVector(value, "Scale", spawn.Scale);
            }
            if (spawn.Scale.X() <= 0.0 || spawn.Scale.Y() <= 0.0 || spawn.Scale.Z() <= 0.0)
            {
                throw std::runtime_error("Scale components must be greater than zero");
            }
        }

        if (PS::JsonHelpers::FieldExists(value, "DropIncreasePercent"))
        {
            const auto& percent = value.at("DropIncreasePercent");
            if (!percent.is_number())
            {
                throw std::runtime_error("DropIncreasePercent must be a number");
            }
            const auto percentValue = percent.get<double>();
            if (!std::isfinite(percentValue) || percentValue < 0.0)
            {
                throw std::runtime_error("DropIncreasePercent must be finite and at least zero");
            }
            spawn.DropMultiplier = 1.0 + percentValue / 100.0;
        }
        if (PS::JsonHelpers::FieldExists(value, "DisplayName"))
        {
            PS::JsonHelpers::ParseString(value, "DisplayName", spawn.DisplayName);
            if (spawn.DisplayName.empty() || spawn.DisplayName.size() > 128)
            {
                throw std::runtime_error("DisplayName must contain between 1 and 128 characters");
            }
        }
        if (PS::JsonHelpers::FieldExists(value, "BossName"))
        {
            PS::JsonHelpers::ParseString(value, "BossName", spawn.BossName);
            if (spawn.BossName.empty() || spawn.BossName.size() > 128)
                throw std::runtime_error("BossName must contain between 1 and 128 characters");
        }
        if (PS::JsonHelpers::FieldExists(value, "LootRow"))
        {
            PS::JsonHelpers::ParseString(value, "LootRow", spawn.LootRow);
            if (spawn.LootRow.empty() || spawn.LootRow.size() > 256)
                throw std::runtime_error("LootRow must contain between 1 and 256 characters");
        }
        if (PS::JsonHelpers::FieldExists(value, "VisualEffect")) {
            spawn.VisualEffect = ValidateVisualEffect(value.at("VisualEffect"));
            if(spawn.VisualEffect.value("Type",std::string("Ghost"))=="Niagara"
                && spawn.VisualEffect.contains("Target")
                && spawn.VisualEffect.at("Target")!="ActorRoot"
                && spawn.VisualEffect.at("Target")!="ActorMesh")
                throw std::runtime_error("Spawn Niagara VisualEffect.Target must be ActorRoot, ActorMesh or omitted");
        }
        std::string type;
        PS::JsonHelpers::ParseString(value, "Type", type);
        if(value.contains("SpawnRadiusMeters") && type!="AISpawnPoint")
            throw std::runtime_error("SpawnRadiusMeters is only supported on ordinary AISpawnPoint entries");
        if (type == "AISpawnPoint")
        {
            RegisterAISpawnPoint(spawn, value);
        }
        else if (type == "Actor")
        {
            RegisterActor(spawn, value);
        }
        else if (type == "BuildingProp")
        {
            RegisterBuildingProp(spawn, value);
        }
        else if (type == "StaticAssembly")
        {
            RegisterStaticAssembly(spawn, value);
        }
        else if (type == "RemoveActor")
        {
            spawn.bGroundToSurface=false;spawn.bGroundingResolved=true;
            RegisterRemoveActor(spawn, value);
        }
        else
        {
            throw std::runtime_error("Type must be 'AISpawnPoint', 'Actor', 'BuildingProp', 'StaticAssembly' or 'RemoveActor'");
        }

        m_spawns.push_back(std::move(spawn));

        ++m_reportedNewSpawns;
    }

    void DragonWildsSpawnLoader::RegisterAISpawnPoint(SpawnInfo& spawn, const nlohmann::json& value)
    {
        PS::JsonHelpers::ValidateFieldExists(value, "AIClass");
        spawn.Type = ESpawnEntryType::AISpawnPoint;
        spawn.ClassPath = TEXT("/Script/Dominion.AISpawnPoint");
        spawn.Properties = nlohmann::json::object();
        spawn.CharacterProperties = nlohmann::json::object();
        spawn.ComponentProperties = nlohmann::json::object();

        const auto parseCombatMultiplier = [&](const char* field, double& destination) {
            if (!PS::JsonHelpers::FieldExists(value, field)) return;
            const auto& candidate = value.at(field);
            if (!candidate.is_number())
                throw std::runtime_error(std::string(field) + " must be a number");
            destination = candidate.get<double>();
            if (!std::isfinite(destination) || destination < 0.1 || destination > 100.0)
                throw std::runtime_error(std::string(field)
                    + " must be finite and between 0.1 and 100");
        };
        parseCombatMultiplier("HealthMultiplier", spawn.HealthMultiplier);
        parseCombatMultiplier("DamageMultiplier", spawn.DamageMultiplier);

        spawn.StableId = StableGuid(std::format("{}|{}|{:.17g}|{:.17g}|{:.17g}",
            RC::to_string(spawn.ModName), RC::to_string(spawn.ClassPath),
            spawn.AuthoredLocation.X(), spawn.AuthoredLocation.Y(), spawn.AuthoredLocation.Z()));

        std::string aiClass;
        PS::JsonHelpers::ParseString(value, "AIClass", aiClass);
        spawn.AIClassPath = RC::to_generic_string(aiClass);
        spawn.Properties["AIClass"] = aiClass;
        // Preserve legacy GUIDs, but give each newly-authored v5 placement its
        // own stable identity even when two intentional placements coincide.
        if(RC::to_string(spawn.EntryId).starts_with("RS_v5_"))
            spawn.StableId=StableGuid(RC::to_string(spawn.ModName)+"|v5-ai|"+RC::to_string(spawn.EntryId));

        auto* aiClassObject = ResolveClass(RC::to_generic_string(aiClass));
        auto* aiBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        if (!aiClassObject || !aiBaseClass || !aiClassObject->IsChildOf(aiBaseClass))
        {
            throw std::runtime_error("AIClass must resolve to a DominionAICharacter class");
        }

        // Blueprint spawn points perform the required AI director registration.
        const std::unordered_map<std::string, const wchar_t*> nativeSpawnPoints{
            {"/FutureMajorVersion/Gameplay/AI/ZombieFaction/Zogre/BP_AI_Zogre_Character.BP_AI_Zogre_Character_C",
                TEXT("/FutureMajorVersion/Gameplay/World/Spawners/AI/SpawnPoints/BP_SpawnPoint_Zogre.BP_SpawnPoint_Zogre_C")},
            {"/FutureMajorVersion/Gameplay/AI/SkeletonFaction/MeleeSkeleton/OneHandSwordVariant/BP_AI_SkeletalWarrior_Character.BP_AI_SkeletalWarrior_Character_C",
                TEXT("/FutureMajorVersion/Gameplay/World/Spawners/AI/SpawnPoints/BP_SpawnPoint_Skeleton_Melee_1H.BP_SpawnPoint_Skeleton_Melee_1H_C")},
            {"/FutureMajorVersion/Gameplay/AI/SkeletonFaction/MeleeSkeleton/TwoHandSwordVariant/BP_AI_SkeletalMarauder_Character.BP_AI_SkeletalMarauder_Character_C",
                TEXT("/FutureMajorVersion/Gameplay/World/Spawners/AI/SpawnPoints/BP_SpawnPoint_Skeleton_Melee_2H.BP_SpawnPoint_Skeleton_Melee_2H_C")},
            {"/FutureMajorVersion/Gameplay/AI/SkeletonFaction/RangedSkeleton/BP_AI_SkeletalArcher_Character.BP_AI_SkeletalArcher_Character_C",
                TEXT("/FutureMajorVersion/Gameplay/World/Spawners/AI/SpawnPoints/BP_SpawnPoint_Skeleton_Ranged.BP_SpawnPoint_Skeleton_Ranged_C")},
        };
        if (const auto native = nativeSpawnPoints.find(aiClass);
            native != nativeSpawnPoints.end())
        {
            if (auto* nativeClass = ResolveClass(native->second);
                nativeClass && ActorHelper::IsActorClass(nativeClass)
                    && !ActorHelper::IsAbstract(nativeClass))
            {
                spawn.ClassPath = nativeClass->GetPathName();
                PS::Log<LogLevel::Verbose>(
                    STR("Using native cooked spawn-point Blueprint '{}' for {}.\n"),
                    spawn.ClassPath, spawn.ModName);
            }
        }

        auto copy = [&](const char* source, const char* property) {
            if (PS::JsonHelpers::FieldExists(value, source))
            {
                spawn.Properties[property] = value.at(source);
            }
        };

        if(value.contains("PowerLevel")) {
            if(!value.at("PowerLevel").is_number_integer())throw std::runtime_error("PowerLevel must be an integer");
            PS::Authoring::ValidatePower(value.at("PowerLevel").get<int>());
        }
        copy("PowerLevel", "PowerLevel");
        copy("Mandatory", "bMandatorySpawn");
        copy("Respawn", "bShouldRespawn");
        copy("RespawnDuration", "RespawnDuration");
        copy("DespawnBehaviour", "DespawnBehaviour");
        copy("RequiresActivation", "bRequiresActivation");
        copy("IgnoreNavmeshRequirement", "bIgnoreNavmeshRequirement");
        copy("RoamGoalQueryType", "RoamGoalQueryTypeOverride");
        if(PS::JsonHelpers::FieldExists(value,"TimeOfDay")) {
            if(!value.at("TimeOfDay").is_string())throw std::runtime_error("TimeOfDay must be Any, Day, or Night");
            const auto requirement=TimeOfDay::Parse(value.at("TimeOfDay").get<std::string>());
            spawn.Time=requirement;
            if(requirement!=TimeOfDay::Requirement::Any) {
                spawn.Properties["bRequiresTimeOfDay"]=true;
                spawn.Properties["TriggeredTimeOfDay"]=TimeOfDay::Name(requirement);
            }
        }

        bool requestsRoaming = false;
        if (PS::JsonHelpers::FieldExists(value, "AmbientBehaviour"))
        {
            if (!value.at("AmbientBehaviour").is_string())
                throw std::runtime_error("AmbientBehaviour must be a string");
            const auto behavior = value.at("AmbientBehaviour").get<std::string>();
            requestsRoaming = behavior == "Roam"
                || behavior == "EAmbientAIBehaviour::Roam";
            spawn.Properties["AmbientBehaviour"] = behavior;
        }

        const auto& roaming = PS::PSConfig::Get()->GetSettings().spawnBehavior;
        if (requestsRoaming && !roaming.enableNativeRoaming)
        {
            spawn.Properties["AmbientBehaviour"] = "Idle";
            PS::Log<LogLevel::Warning>(STR("{} requested roaming, but native roaming is disabled in RuneSchema settings.\n"),
                spawn.ModName);
        }
        else if (requestsRoaming || PS::JsonHelpers::FieldExists(value, "RoamRadius"))
        {
            spawn.Properties["bOverrideRoamBehaviour"] = true;
            const double radius = PS::JsonHelpers::FieldExists(value, "RoamRadius")
                ? value.at("RoamRadius").get<double>() : roaming.defaultRoamRadius;
            const double maxZ = PS::JsonHelpers::FieldExists(value, "RoamMaxZTolerance")
                ? value.at("RoamMaxZTolerance").get<double>() : roaming.defaultRoamMaxZTolerance;
            if (!std::isfinite(radius) || radius <= 0.0 || radius > 100000.0)
                throw std::runtime_error("RoamRadius must be between 0 and 100000");
            if (!std::isfinite(maxZ) || maxZ < 0.0 || maxZ > 100000.0)
                throw std::runtime_error("RoamMaxZTolerance must be between 0 and 100000");
            spawn.Properties["RoamDistance"] = radius;
            spawn.Properties["RoamMaxZTolerance"] = maxZ;
        }

        if (PS::JsonHelpers::FieldExists(value, "SentryRadius"))
        {
            spawn.Properties["SentryModeRange"] = value.at("SentryRadius");
        }

        const bool hasHearingRange = PS::JsonHelpers::FieldExists(value, "HearingRange");
        const bool hasHearingZRange = PS::JsonHelpers::FieldExists(value, "HearingZRange");
        if (hasHearingRange || hasHearingZRange)
        {
            spawn.Properties["bOverridePerceptionBehaviour"] = true;
            if (hasHearingRange) copy("HearingRange", "HearingRangeOverride");
            if (hasHearingZRange) copy("HearingZRange", "HearingZRangeOverride");
        }

        if (PS::JsonHelpers::FieldExists(value, "IdleAnimTag"))
        {
            std::string idleAnimTag;
            PS::JsonHelpers::ParseString(value, "IdleAnimTag", idleAnimTag);
            spawn.Properties["IdleAnimOverrideTag"] = { { "TagName", idleAnimTag } };
        }

        if (PS::JsonHelpers::FieldExists(value, "Tags"))
        {
            const auto& tags = value.at("Tags");
            if (!tags.is_array())
            {
                throw std::runtime_error("Tags must be an array of gameplay tag strings");
            }
            spawn.Properties["TagsAppliedToSpawnedAI"] = tags;
        }

        if (PS::JsonHelpers::FieldExists(value, "Variants"))
        {
            RegisterAISpawnVariants(spawn, value.at("Variants"), aiBaseClass);
        }

        spawn.Properties.update(Spawns::RadiusProperties(value));

        if (PS::JsonHelpers::FieldExists(value, "Properties"))
        {
            const auto& properties = value.at("Properties");
            if (!properties.is_object())
            {
                throw std::runtime_error("Properties must be an object");
            }
            for (const auto& [name, propertyValue] : properties.items())
            {
                spawn.Properties[name] = propertyValue;
            }
        }

        if (PS::JsonHelpers::FieldExists(value, "CharacterProperties"))
        {
            if (!value.at("CharacterProperties").is_object())
                throw std::runtime_error("CharacterProperties must be an object");
            spawn.CharacterProperties = value.at("CharacterProperties");
        }
        if (PS::JsonHelpers::FieldExists(value, "ComponentProperties"))
        {
            if (!value.at("ComponentProperties").is_object())
                throw std::runtime_error("ComponentProperties must be an object");
            for (const auto& [component, properties] : value.at("ComponentProperties").items())
            {
                if (!properties.is_object())
                    throw std::runtime_error("each ComponentProperties entry must be an object");
                spawn.ComponentProperties[component] = properties;
            }
        }
    }

    void DragonWildsSpawnLoader::RegisterAISpawnVariants(SpawnInfo& spawn, const nlohmann::json& variants, RC::Unreal::UClass* aiBaseClass)
    {
        if (!variants.is_array() || variants.empty())
        {
            throw std::runtime_error("Variants must be a non-empty array of objects");
        }

        auto parsed = nlohmann::json::array();
        for (const auto& variant : variants)
        {
            if (!variant.is_object())
            {
                throw std::runtime_error("Each variant must be an object with an AIClass");
            }

            PS::JsonHelpers::ValidateFieldExists(variant, "AIClass");
            std::string aiClass;
            PS::JsonHelpers::ParseString(variant, "AIClass", aiClass);

            auto* variantClass = ResolveClass(RC::to_generic_string(aiClass));
            if (!variantClass || !variantClass->IsChildOf(aiBaseClass))
            {
                throw std::runtime_error(std::format("Variant class '{}' must resolve to a DominionAICharacter class", aiClass));
            }

            nlohmann::json entry = { { "AIClass", aiClass } };

            const bool hasProgressTag = PS::JsonHelpers::FieldExists(variant, "WorldProgressTag");
            const bool hasProgressValue = PS::JsonHelpers::FieldExists(variant, "WorldProgressValue");
            if (hasProgressValue && !hasProgressTag)
            {
                throw std::runtime_error("WorldProgressValue requires a WorldProgressTag");
            }
            if (hasProgressTag)
            {
                std::string progressTag;
                PS::JsonHelpers::ParseString(variant, "WorldProgressTag", progressTag);
                entry["bRequiresWorldProgressValue"] = true;
                entry["WorldProgressValueTag"] = { { "TagName", progressTag } };
                if (hasProgressValue)
                {
                    entry["WorldProgressValue"] = variant.at("WorldProgressValue");
                }
            }

            parsed.push_back(std::move(entry));
        }

        spawn.Properties["bChooseAIFromVariantList"] = true;
        spawn.Properties["AISpawnVariants"] = std::move(parsed);
    }

    void DragonWildsSpawnLoader::RegisterActor(SpawnInfo& spawn, const nlohmann::json& value)
    {
        PS::JsonHelpers::ValidateFieldExists(value, "Id");
        PS::JsonHelpers::ValidateFieldExists(value, "Class");
        spawn.Type = ESpawnEntryType::Actor;
        spawn.Properties = nlohmann::json::object();
        spawn.ComponentProperties = nlohmann::json::object();

        std::string entryId;
        PS::JsonHelpers::ParseString(value, "Id", entryId);
        if (entryId.empty())
        {
            throw std::runtime_error("Id must not be empty");
        }
        spawn.EntryId = RC::to_generic_string(entryId);

        std::string classPath;
        PS::JsonHelpers::ParseString(value, "Class", classPath);
        spawn.ClassPath = RC::to_generic_string(classPath);

        auto* actorClass = ResolveClass(spawn.ClassPath);
        if (!actorClass)
        {
            throw std::runtime_error(std::format("Class '{}' was not found", classPath));
        }

        auto* guidProperty = PropertyHelper::GetPropertyByName(actorClass, TEXT("SpudGuid"));
        if (!guidProperty || guidProperty->GetSize() != sizeof(FGuid))
        {
            throw std::runtime_error(std::format(
                "Class '{}' has no SpudGuid property; only save-persistent world actors are supported", classPath));
        }

        const auto identity = std::format("{}|actor|{}", RC::to_string(spawn.ModName), entryId);
        spawn.StableId = StableActorGuid(identity);
        spawn.LegacyId = StableGuid(identity);
        spawn.PersistentPlacementKey = identity;

        if (PS::JsonHelpers::FieldExists(value, "UseNativeRespawn"))
        {
            if (!value.at("UseNativeRespawn").is_boolean())
                throw std::runtime_error("UseNativeRespawn must be a boolean");
            spawn.bUseNativeRespawn = value.at("UseNativeRespawn").get<bool>();
        }
        if(value.contains("TimeOfDay")) {
            if(!value.at("TimeOfDay").is_string())throw std::runtime_error("TimeOfDay must be Any, Day, or Night");
            spawn.Time=TimeOfDay::Parse(value.at("TimeOfDay").get<std::string>());
            if(spawn.Time!=TimeOfDay::Requirement::Any && spawn.bUseNativeRespawn)
                throw std::runtime_error("Timed actors cannot use native respawn persistence");
        }

        if (PS::JsonHelpers::FieldExists(value, "Properties"))
        {
            const auto& properties = value.at("Properties");
            if (!properties.is_object())
            {
                throw std::runtime_error("Properties must be an object");
            }
            for (const auto& [name, propertyValue] : properties.items())
            {
                spawn.Properties[name] = propertyValue;
            }
        }

        if (PS::JsonHelpers::FieldExists(value, "ComponentProperties"))
        {
            if (!value.at("ComponentProperties").is_object())
                throw std::runtime_error("ComponentProperties must be an object");
            for (const auto& [component, properties]
                : value.at("ComponentProperties").items())
            {
                if (!properties.is_object())
                    throw std::runtime_error(
                        "each ComponentProperties entry must be an object");
                spawn.ComponentProperties[component] = properties;
            }
        }
    }

    void DragonWildsSpawnLoader::RegisterBuildingProp(SpawnInfo& spawn,const nlohmann::json& value)
    {
        PS::JsonHelpers::ValidateFieldExists(value,"Id");
        PS::JsonHelpers::ValidateFieldExists(value,"Building");
        if(!value.at("Building").is_string())throw std::runtime_error("Building must be a BuildingPieceData asset path");
        const auto buildingPath=value.at("Building").get<std::string>();
        auto* building=ActorHelper::ResolveObject(RC::to_generic_string(buildingPath));
        auto* buildingType=ResolveClass(TEXT("/Script/Dominion.BuildingPieceData"));
        if(!building || !buildingType || !building->IsA(buildingType))
            throw std::runtime_error("Building did not resolve to BuildingPieceData");

        auto* buildable=CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(
            building->GetClassPrivate(),TEXT("BuildableActor")));
        if(!buildable || buildable->GetArrayDim()!=1
            || buildable->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))
            throw std::runtime_error("BuildingPieceData BuildableActor layout changed");
        const auto& reference=*buildable->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(building);
        auto* classObject=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(
            UECustom::TSoftObjectPtr<UObject>(reference.ObjectID));
        UClass* actorClass=classObject && classObject->IsA<UClass>()?static_cast<UClass*>(classObject):nullptr;
        auto classPath=reference.ObjectID.GetLongPackageFName().ToString()+TEXT(".")+reference.ObjectID.GetAssetFName().ToString();
        if(!actorClass)actorClass=ResolveClass(classPath);
        if(!actorClass && !classPath.ends_with(TEXT("_C")))actorClass=ResolveClass(classPath+TEXT("_C"));
        if(!actorClass || !ActorHelper::IsActorClass(actorClass) || ActorHelper::IsAbstract(actorClass))
            throw std::runtime_error("BuildingPieceData BuildableActor did not resolve to a concrete Actor class");

        auto actorValue=value;
        actorValue["Class"]=RC::to_string(actorClass->GetPathName());
        RegisterActor(spawn,actorValue);
        spawn.bBuildingProp=true;
        spawn.BuildingDataPath=buildingPath;

        for(const auto* name:{TEXT("BuildingPieceData"),TEXT("BuildingData"),TEXT("BuildingPiece")}) {
            auto* property=PropertyHelper::GetPropertyByName(actorClass,name);
            // Tool/native binding code only knows how to round-trip a hard UObject
            // reference or a soft-object reference. Do not accept weak/lazy/class
            // object properties merely because they derive from FObjectPropertyBase.
            if(!CastField<FObjectProperty>(property) && !CastField<FSoftObjectProperty>(property))continue;
            auto* objectProperty=CastField<FObjectPropertyBase>(property);
            if(!objectProperty || objectProperty->GetArrayDim()!=1)continue;
            auto* expected=objectProperty->GetPropertyClass().Get();
            if(expected && building->IsA(expected)) {spawn.BuildingObjectProperty=RC::to_string(property->GetName());break;}
        }
        auto* dataIndex=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
            building->GetClassPrivate(),TEXT("BuildingPieceDataIndex")));
        auto* actorIndex=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
            actorClass,TEXT("BuildingPieceDataIndex")));
        if(dataIndex && actorIndex && dataIndex->IsInteger() && actorIndex->IsInteger()
            && dataIndex->GetArrayDim()==1 && actorIndex->GetArrayDim()==1) {
            spawn.BuildingDataIndex=dataIndex->GetSignedIntPropertyValue(
                dataIndex->ContainerPtrToValuePtr<void>(building));
            if(spawn.BuildingDataIndex<0)throw std::runtime_error("BuildingPieceDataIndex is not initialized");
            spawn.bHasBuildingDataIndex=true;
        }
        if(spawn.BuildingObjectProperty.empty() && !spawn.bHasBuildingDataIndex)
            throw std::runtime_error("Buildable actor exposes neither BuildingPieceData nor BuildingPieceDataIndex");
        if(value.contains("Properties") && (value.at("Properties").contains("BuildingPieceData")
            || value.at("Properties").contains("BuildingData")
            || value.at("Properties").contains("BuildingPiece")
            || value.at("Properties").contains("BuildingPieceDataIndex")))
            throw std::runtime_error("BuildingProp native data binding cannot be overridden through Properties");

        if(value.contains("AllowDeconstruction")) {
            if(!value.at("AllowDeconstruction").is_boolean())throw std::runtime_error("AllowDeconstruction must be a boolean");
            spawn.bAllowDeconstruction=value.at("AllowDeconstruction").get<bool>();
        }
    }

    void DragonWildsSpawnLoader::RegisterStaticAssembly(SpawnInfo& spawn,
        const nlohmann::json& value)
    {
        PS::JsonHelpers::ValidateFieldExists(value, "Id");
        PS::JsonHelpers::ValidateFieldExists(value, "Pieces");
        if (!value.at("Id").is_string() || value.at("Id").get_ref<const std::string&>().empty())
            throw std::runtime_error("StaticAssembly Id must be a non-empty string");
        if (!value.at("Pieces").is_array() || value.at("Pieces").empty()
            || value.at("Pieces").size() > 4096)
            throw std::runtime_error("StaticAssembly Pieces must contain 1-4096 entries");
        for (const auto& piece : value.at("Pieces"))
        {
            if (!piece.is_object() || !piece.contains("PieceId")
                || !piece.at("PieceId").is_number_integer()
                || !piece.contains("Building") || !piece.at("Building").is_string()
                || !piece.contains("Location") || !piece.at("Location").is_object()
                || !piece.contains("Rotation") || !piece.at("Rotation").is_object()
                || !piece.contains("Scale") || !piece.at("Scale").is_object())
                throw std::runtime_error("StaticAssembly piece contract is incomplete");
        }
        spawn.Type = ESpawnEntryType::StaticAssembly;
        spawn.AssemblyPieces = value.at("Pieces");
        spawn.CollisionProfile = value.value("CollisionProfile", std::string("BlockAll"));
        if (spawn.CollisionProfile.empty() || spawn.CollisionProfile.size() > 64)
            throw std::runtime_error("StaticAssembly CollisionProfile must contain 1-64 characters");
        spawn.StableId = StableActorGuid(std::format("{}|static-assembly|{}",
            RC::to_string(spawn.ModName), RC::to_string(spawn.EntryId)));
        spawn.PersistentPlacementKey = std::format("{}|static-assembly|{}",
            RC::to_string(spawn.ModName), RC::to_string(spawn.EntryId));
        spawn.bGroundToSurface = false;
        spawn.bGroundingResolved = true;
    }

    void DragonWildsSpawnLoader::ApplyBuildingData(AActor* actor,const SpawnInfo& spawn)
    {
        if(!spawn.bBuildingProp)return;
        auto* building=ActorHelper::ResolveObject(RC::to_generic_string(spawn.BuildingDataPath));
        if(!actor || !building)throw std::runtime_error("BuildingProp data binding is unavailable");
        bool applied=false;
        if(!spawn.BuildingObjectProperty.empty()) {
            auto* property=PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),
                RC::to_generic_string(spawn.BuildingObjectProperty));
            if(!property)throw std::runtime_error("Buildable actor data property disappeared");
            PropertyHelper::CopyJsonValueToContainer(actor,property,spawn.BuildingDataPath);
            if(auto* objectProperty=CastField<FObjectProperty>(property)) {
                UObject* current=nullptr;
                std::memcpy(&current,objectProperty->ContainerPtrToValuePtr<void>(actor),sizeof(current));
                if(current!=building)throw std::runtime_error("BuildingProp object binding did not round-trip");
                applied=true;
            } else if(auto* softProperty=CastField<FSoftObjectProperty>(property)) {
                const auto& current=*softProperty->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(actor);
                const UECustom::FSoftObjectPath expected(building->GetPathName());
                if(current.ObjectID.AssetPath.GetPackageName()!=expected.AssetPath.GetPackageName()
                    || current.ObjectID.AssetPath.GetAssetName()!=expected.AssetPath.GetAssetName())
                    throw std::runtime_error("BuildingProp soft-object binding did not round-trip");
                applied=true;
            }
        }
        if(spawn.bHasBuildingDataIndex) {
            auto* property=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                actor->GetClassPrivate(),TEXT("BuildingPieceDataIndex")));
            if(!property || !property->IsInteger())throw std::runtime_error("Buildable actor data index disappeared");
            property->SetIntPropertyValue(property->ContainerPtrToValuePtr<void>(actor),spawn.BuildingDataIndex);
            if(property->GetSignedIntPropertyValue(property->ContainerPtrToValuePtr<void>(actor))!=spawn.BuildingDataIndex)
                throw std::runtime_error("BuildingProp index binding did not round-trip");
            applied=true;
        }
        if(!applied)throw std::runtime_error("BuildingProp native data was not applied");
    }

    void DragonWildsSpawnLoader::RegisterRemoveActor(SpawnInfo& spawn, const nlohmann::json& value)
    {
        PS::JsonHelpers::ValidateFieldExists(value, "Class");
        spawn.Type = ESpawnEntryType::RemoveActor;

        std::string classPath;
        PS::JsonHelpers::ParseString(value, "Class", classPath);
        spawn.ClassPath = RC::to_generic_string(classPath);

        if (!ResolveClass(spawn.ClassPath))
        {
            throw std::runtime_error(std::format("Class '{}' was not found", classPath));
        }

        if (PS::JsonHelpers::FieldExists(value, "Radius"))
        {
            spawn.RemoveRadius = value.at("Radius").get<float>();
            if (spawn.RemoveRadius <= 0.0f)
            {
                throw std::runtime_error("Radius must be greater than zero");
            }
        }
    }

    UClass* DragonWildsSpawnLoader::ResolveClass(const RC::StringType& classPath)
    {
        return ActorHelper::ResolveClass(classPath);
    }

    void DragonWildsSpawnLoader::DumpAIClasses()
    {
        auto* baseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        if (!baseClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to find DominionAICharacter.\n"));
            return;
        }

        TArray<UObject*> classes;
        UECustom::UObjectGlobals::GetObjectsOfClass(UClass::StaticClass(), classes, true, static_cast<EObjectFlags>(0));

        PS::Log<LogLevel::Normal>(STR("--- AI classes currently loaded ---\n"));

        int found = 0;
        for (auto* object : classes)
        {
            auto* candidate = static_cast<UClass*>(object);
            if (!candidate || candidate == baseClass || !candidate->IsChildOf(baseClass))
            {
                continue;
            }

            PS::Log<LogLevel::Normal>(STR("  {}\n"), candidate->GetPathName());
            found++;
        }

        PS::Log<LogLevel::Normal>(STR("--- {} AI class(es), loaded ones only ---\n"), found);
    }

    bool DragonWildsSpawnLoader::SetupWorldReadyHook()
    {
        m_onLevelShownFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.WorldPartitionRuntimeLevelStreamingCell:OnLevelShown"));
        if (!m_onLevelShownFunction)
        {
            PS::Log<LogLevel::Error>(STR("Unable to hook OnLevelShown; spawns will not be created.\n"));
            return false;
        }

        m_onLevelShownCallbackId = PS::RegisterNativePostHook(m_onLevelShownFunction,
            [this](UnrealScriptFunctionCallableContext& context, void*) {
                OnCellShown(context.Context);
            });
        return m_onLevelShownCallbackId != 0;
    }

    void DragonWildsSpawnLoader::SetupAIScaleHook()
    {
        m_aiScaleFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter:SetAILocomotion"));
        if (!m_aiScaleFunction)
        {
            PS::Log<LogLevel::Warning>(
                STR("Unable to hook DominionAICharacter.SetAILocomotion; AI Scale will apply during world processing.\n"));
            return;
        }

        m_aiScaleCallbackId = PS::RegisterNativePostHook(m_aiScaleFunction,
            [this](UnrealScriptFunctionCallableContext& context, void*) {
                ApplyAIScale(context.Context);
            });
        if (m_aiScaleCallbackId == 0)
        {
            PS::Log<LogLevel::Warning>(
                STR("Unable to register the AI Scale hook; AI Scale will apply during world processing.\n"));
        }
    }

    void DragonWildsSpawnLoader::SetupAIBindingHooks()
    {
        DragonWildsBlueprintModLoader::SetActorInitializedObserver(
            [this](AActor* actor) {
                try { ApplyAIScale(actor); }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Warning>(
                        STR("Native AI instance binding failed safely: {}\n"),
                        PS::ToWideSafe(error.what()));
                }
                catch (...)
                {
                    PS::Log<LogLevel::Warning>(
                        STR("Native AI instance binding failed safely with an unknown error.\n"));
                }
            });

        m_healthBarSetTextFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/UMG.TextBlock:SetText"));
        if (m_healthBarSetTextFunction)
        {
            m_healthBarSetTextCallbackId = PS::RegisterNativePostHook(m_healthBarSetTextFunction,
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    OnHealthBarTextSet(context.Context);
                });
        }

        PS::Log<LogLevel::Verbose>(
            STR("Per-instance AI binding uses native PostInitializeComponents "
                "(health-bar text hook={}); no AI world scan is enabled.\n"),
            m_healthBarSetTextCallbackId != 0);
    }

    void DragonWildsSpawnLoader::SetupPlayerJoinHooks()
    {
        m_playerPossessionAckFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.PlayerController:ServerAcknowledgePossession"));
        if (m_playerPossessionAckFunction && (m_playerPossessionAckFunction->GetFunctionFlags() & FUNC_Native))
            m_playerPossessionAckCallbackId = PS::RegisterNativePostHook(m_playerPossessionAckFunction,
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    auto* controller=context.Context;
                    if(!controller || controller->GetWorld()!=m_readyWorld || !IsWorldStillLoaded(m_readyWorld))return;
                    try {
                        ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));
                        authority.Invoke();if(!authority.Result<bool>())return;
                        ActorHelper::FunctionCall getPawn(controller,TEXT("/Script/Engine.Controller:K2_GetPawn"));
                        getPawn.Invoke();auto* pawn=getPawn.Result<UObject*>();
                        if(!pawn || pawn->GetWorld()!=m_readyWorld)return;
                        ApplyPlayerRules();
                    } catch(const std::exception& error) {
                        PS::Log<LogLevel::Verbose>(STR("Player possession readiness deferred: {}\n"),PS::ToWideSafe(error.what()));
                    }
                });
        PS::Log<LogLevel::Verbose>(STR("/players server possession acknowledgement hook: {}.\n"),m_playerPossessionAckCallbackId!=0);
        m_playerPostLoginFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.GameModeBase:K2_PostLogin"));
        // K2_PostLogin is commonly a Blueprint event.  Never send a
        // non-native function through the native function-pointer hook API.
        if (m_playerPostLoginFunction
            && (m_playerPostLoginFunction->GetFunctionFlags() & FUNC_Native))
        {
            m_playerPostLoginCallbackId = PS::RegisterNativePostHook(m_playerPostLoginFunction,
                [this](UnrealScriptFunctionCallableContext&, void*) {
                    ApplyPlayerRules();
                });
        }

        m_playerClientRestartFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.PlayerController:ClientRestart"));
        if (m_playerClientRestartFunction)
        {
            m_playerClientRestartCallbackId = PS::RegisterNativePostHook(m_playerClientRestartFunction,
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    ApplyPlayerRules();
                    try
                    {
                        auto getPawn = ActorHelper::FunctionCall(context.Context,
                            STR("/Script/Engine.Controller:K2_GetPawn"));
                        getPawn.Invoke();
                        auto* pawn = getPawn.Result<UObject*>();
                        ConfirmPendingRespawn(pawn);
                        ApplyClientPlayerVisualRules(pawn);
                    }
                    catch (...) {}
                });
        }

        m_playerPawnStateFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.Pawn:OnRep_PlayerState"));
        if (m_playerPawnStateFunction)
        {
            m_playerPawnStateCallbackId = PS::RegisterNativePostHook(m_playerPawnStateFunction,
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    ConfirmPendingRespawn(context.Context);
                    ApplyClientPlayerVisualRules(context.Context);
                });
        }

        m_playerTagsChangedFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr,
            TEXT("/Script/Dominion.DominionPlayerCharacter:HandleGameplayEffectTagsChanged"));
        if (m_playerTagsChangedFunction)
        {
            m_playerTagsChangedCallbackId = PS::RegisterNativePostHook(m_playerTagsChangedFunction,
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    ApplyClientPlayerVisualRules(context.Context, true);
                });
        }

        m_playerDamageReceivedFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr,
            TEXT("/Game/Gameplay/Character/Components/BP_Components_PlayerDamage."
                "BP_Components_PlayerDamage_C:BP_OnAnyDamageReceived"));
        // BP_OnAnyDamageReceived is observed by the guarded ProcessEvent
        // activity observer below; it is not a native function-pointer hook.
        if (m_playerDamageReceivedFunction
            && (m_playerDamageReceivedFunction->GetFunctionFlags() & FUNC_Native))
        {
            m_playerDamageReceivedCallbackId = PS::RegisterNativePostHook(m_playerDamageReceivedFunction,
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    UObject* pawn = nullptr;
                    try
                    {
                        pawn = ActorHelper::GetObjectRef(
                            context.Context, TEXT("PlayerCharacter"));
                    }
                    catch (...) {}
                    if (!pawn && context.Context)
                        pawn = context.Context->GetOuterPrivate();
                    ApplyClientPlayerVisualRules(pawn, true);
                });
        }

        SetupPlayerActivityHooks();

        if (m_playerPostLoginCallbackId == 0 && m_playerClientRestartCallbackId == 0
            && m_playerPawnStateCallbackId == 0 && m_playerPossessionAckCallbackId == 0)
        {
            PS::Log<LogLevel::Error>(
                STR("Unable to register native player join hooks; /players will only apply during initial world setup.\n"));
            return;
        }

        PS::Log<LogLevel::Verbose>(
            STR("/players is event-driven (PostLogin={}, ClientRestart={}, "
                "OnRep_PlayerState={}, TagsChanged={}, PlayerDamage={}, "
                "ActivityHooks={}); conditional nameplates refresh at 10 Hz.\n"),
            m_playerPostLoginCallbackId != 0, m_playerClientRestartCallbackId != 0,
            m_playerPawnStateCallbackId != 0, m_playerTagsChangedCallbackId != 0,
            m_playerDamageReceivedCallbackId != 0, m_playerActivityHooks.size());
    }

    void DragonWildsSpawnLoader::ConfirmPendingRespawn(UObject* pawn)
    {
        if (!pawn || !m_readyWorld || pawn->GetWorld() != m_readyWorld) return;

        UObject* playerState = nullptr;
        try
        {
            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(pawn, TEXT("PlayerState"));
            playerState = statePointer ? statePointer->Get() : nullptr;
        }
        catch (...) {}

        int32_t playerId = -1;
        if (playerState) try
        {
            auto getId = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerId"));
            getId.Invoke();
            playerId = getId.Result<int32_t>();
        }
        catch (...) {}

        auto pending = std::find_if(m_pendingRespawns.begin(), m_pendingRespawns.end(),
            [pawn, playerState, playerId](const auto& value)
            {
                if (value.Pawn.Get() == pawn) return true;
                if (playerState && value.PlayerState.Get() == playerState) return true;
                return playerId >= 0 && value.PlayerId >= 0 && value.PlayerId == playerId;
            });
        if (pending == m_pendingRespawns.end()) return;

        pending->Pawn = PS::WeakObject(pawn);
        if (playerState) pending->PlayerState = PS::WeakObject(playerState);
        if (playerId >= 0) pending->PlayerId = playerId;
        pending->Confirmed = true;
    }

    void DragonWildsSpawnLoader::SetupPlayerActivityHooks()
    {
        m_playerProcessEventActivityPaths.clear();
        m_playerProcessEventActivityNames.clear();
        m_playerNativeActivityPaths.clear();
        const auto addProcessEventPath = [this](const std::string& path) {
            // Keep every configured exact path in the filtered ProcessEvent
            // observer. Native post-hooks remain the fast path, but this
            // fallback also catches Blueprint-exposed or inherited functions
            // that the game presents through ProcessEvent at runtime.
            if (ActivityFunctionPath(path)) {
                m_playerProcessEventActivityPaths.insert(path);
                m_playerProcessEventActivityNames.insert(
                    RC::to_generic_string(ActivityFunctionName(path)));
            }
        };
        for (const auto* path : {SkillXPEvent, ToolAttackEvent, PlayerDamageEvent,
            PlayerRespawnEvent, RespawnFinishedEvent})
            addProcessEventPath(path);
        // Observe the complete emote lifecycle. Remote players may enter via
        // replicated Play/Stop routes without repeating the local radial-menu
        // selection notifications.
        addProcessEventPath(PlayerEmotePlayEvent);
        addProcessEventPath(PlayerEmoteStopEvent);
        addProcessEventPath(PlayerEmoteNotifyEvent);
        addProcessEventPath(PlayerEmoteSelectionChangedEvent);
        for (const auto& rule : m_playerRules)
            for (const auto& event : rule.Nameplate.Events)
                addProcessEventPath(event["Function"].get<std::string>());

        if (m_respawnObserver == Hook::ERROR_ID && std::any_of(m_playerRules.begin(),m_playerRules.end(),
            [](const auto& rule){return !rule.VisualEffect.empty() && rule.VisualEffectTrigger=="Respawn";})) {
            Hook::FCallbackOptions options{};
            options.OwnerModName=TEXT("RuneSchema"); options.HookName=TEXT("ConfirmedPlayerRespawn");
            const auto ownerThread=std::this_thread::get_id();
            m_respawnObserver=Hook::RegisterProcessEventPreCallback(
                [this,ownerThread](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void*) {
                    if(std::this_thread::get_id()!=ownerThread || !m_readyWorld || !source || !function) return;
                    try {
                        if(function->GetFName()!=FName(TEXT("Multicast_Respawn"),FNAME_Add)
                            || to_string(function->GetPathName())!="/Script/Dominion.PlayerRespawnComponent:Multicast_Respawn") return;
                        auto* pawn=ResolvePlayerPawnFromActivity(source);
                        if(!pawn || pawn->GetWorld()!=m_readyWorld || !PlayerGhost::IsDead(pawn)) return;
                        if(std::any_of(m_pendingRespawns.begin(),m_pendingRespawns.end(),[pawn](const auto& p){return p.Pawn.Get()==pawn;})) return;
                        UObject* playerState = nullptr;
                        try {
                            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                                TObjectPtr<UObject>>(pawn, TEXT("PlayerState"));
                            playerState = statePointer ? statePointer->Get() : nullptr;
                        }
                        catch (...) {}
                        int32_t playerId = -1;
                        if (playerState) try {
                            auto getId = ActorHelper::FunctionCall(
                                playerState, STR("/Script/Engine.PlayerState:GetPlayerId"));
                            getId.Invoke();
                            playerId = getId.Result<int32_t>();
                        }
                        catch (...) {}
                        if(m_pendingRespawns.size()<16)
                            m_pendingRespawns.push_back({PS::WeakObject(pawn),
                                PS::WeakObject(playerState),playerId,30.0,false});
                    } catch(...) {}
                },options);
            if(m_respawnObserver==Hook::ERROR_ID)
                PS::Log<LogLevel::Warning>(TEXT("Timed respawn observation unavailable.\n"));
        }
        if (m_activityObserver == Hook::ERROR_ID) {
            Hook::FCallbackOptions options{};
            options.OwnerModName = TEXT("RuneSchema");
            options.HookName = TEXT("PlayerActivityObservation");
            const auto ownerThread = std::this_thread::get_id();
            m_activityObserver = Hook::RegisterProcessEventPostCallback(
                [this, ownerThread](Hook::TCallbackIterationData<void>&, UObject* source, UFunction* function, void* parameters) {
                    if (std::this_thread::get_id() != ownerThread || !m_readyWorld || !source || !function) return;
                    try {
                        const auto name = function->GetName();
                        // Native time-sensitive campfires and roadside torches
                        // bind an InGameTimeSensorComponent to these Blueprint
                        // events.  Force the next lifecycle reconciliation now;
                        // the authoritative replicated clock is still read by
                        // ReconcileTimedBuildingProps before changing actors.
                        if(name==TEXT("OnEnterTimeFrameDynamic_Event")
                            || name==TEXT("OnExitTimeFrameDynamic_Event")) {
                            m_buildingTimeElapsed=1.0;
                            return;
                        }
                        if (!m_playerProcessEventActivityNames.contains(name)) return;
                        const auto path = to_string(function->GetPathName());
                        if (!m_playerProcessEventActivityPaths.contains(path)
                            && path != SkillXPEvent && path != ToolAttackEvent
                            && path != PlayerRespawnEvent && path != RespawnFinishedEvent
                            && path != PlayerDamageEvent) return;
                        // A successfully installed direct native hook already
                        // delivers this event. Do not process it a second time
                        // through the global observer; if the function is not
                        // native, it will not be in this set and the fallback
                        // below remains active.
                        const bool emotePath = path == PlayerEmotePlayEvent
                            || path == PlayerEmoteStopEvent
                            || path == PlayerEmoteNotifyEvent
                            || path == PlayerEmoteSelectionChangedEvent;
                        if (m_playerNativeActivityPaths.contains(path)) return;
                        auto* pawn = ResolvePlayerPawnFromActivity(source);
                        if (!pawn || pawn->GetWorld() != m_readyWorld) return;
                        if (path == PlayerDamageEvent) {
                            // BP_OnAnyDamageReceived is a Blueprint event, so it
                            // cannot rely on RegisterNativePostHook. Observe it
                            // here after the event has updated FatalDamageInfo.
                            ApplyClientPlayerVisualRules(pawn, true);
                        } else if (path == PlayerRespawnEvent) {
                            ConfirmPendingRespawn(pawn);
                        } else if (path == RespawnFinishedEvent) {
                            ConfirmPendingRespawn(pawn);
                            auto pending = std::find_if(m_pendingRespawns.begin(), m_pendingRespawns.end(),
                                [pawn](const auto& value) { return value.Pawn.Get() == pawn; });
                            if (pending != m_pendingRespawns.end() && pending->Confirmed)
                                pending->FinishObserved = true;
                        } else if (path == SkillXPEvent) {
                            m_observedActivities.push_back({PS::WeakObject(pawn), path, {},
                                PS::InspectionTools::CaptureEventParameters(function, parameters)});
                        } else if (emotePath) {
                            // Keep emotes out of the generic attack classifier.
                            // Stop deliberately has no state; its exact event
                            // path clears any active emote presentation.
                            if (m_observedActivities.size() < 256)
                                m_observedActivities.push_back({PS::WeakObject(pawn), path,
                                    path == PlayerEmoteStopEvent ? std::string{}
                                        : ClassifyPlayerEmoteActivity(source),
                                    PS::InspectionTools::CaptureEventParameters(function, parameters)});
                        } else if (std::any_of(m_playerRules.begin(), m_playerRules.end(),
                            [&path](const auto& rule) {
                                return std::any_of(rule.Nameplate.Events.begin(), rule.Nameplate.Events.end(),
                                    [&path](const auto& event) { return event["Function"] == path; });
                            })) {
                            // Explicit Blueprint events use the same queue and
                            // matcher as native events. Only configured exact
                            // paths reach this branch, so there is no global
                            // Blueprint recorder or action polling.
                            if (m_observedActivities.size() < 256)
                                m_observedActivities.push_back({PS::WeakObject(pawn), path,
                                    (path == PlayerEmotePlayEvent || path == PlayerEmoteNotifyEvent
                                        || path == PlayerEmoteSelectionChangedEvent)
                                        ? ClassifyPlayerEmoteActivity(source) : std::string{},
                                    PS::InspectionTools::CaptureEventParameters(function, parameters)});
                        } else m_observedActivities.push_back({PS::WeakObject(pawn), path,
                            ClassifyPlayerAttackActivity(source), nlohmann::json::object()});
                    } catch (...) {}
                }, options);
            if (m_activityObserver == Hook::ERROR_ID)
                PS::Log<LogLevel::Warning>(TEXT("Player activity observer unavailable; XP and tool-swing icons are disabled.\n"));
        }
        const auto registerActivityHook = [this](const TCHAR* functionPath,
            const std::function<std::string(UObject*)>& classify) {
            if (to_string(functionPath) == ToolAttackEvent) return m_activityObserver != Hook::ERROR_ID;
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr, functionPath);
            if (!function) return false;
            // A reflected Blueprint event is already covered by the filtered
            // ProcessEvent observer. It is not a native-hook failure and should
            // not produce a misleading warning.
            if (!(function->GetFunctionFlags() & FUNC_Native)) return true;
            if (std::any_of(m_playerActivityHooks.begin(),m_playerActivityHooks.end(),
                [function](const auto& hook){return hook.Function==function;})) return true;
            if (m_playerActivityHooks.size()>=66) return false;
            const auto eventPath=to_string(function->GetPathName());
            const auto ownerThread=std::this_thread::get_id();
            const auto callbackId = PS::RegisterNativePostHook(function,
                [this, classify, function, eventPath, ownerThread](UnrealScriptFunctionCallableContext& context, void*) {
                    if (std::this_thread::get_id()!=ownerThread) return;
                    static thread_local bool processing=false;
                    if (processing) return;
                    struct Guard { bool& flag; Guard(bool& value):flag(value){flag=true;} ~Guard(){flag=false;} } guard(processing);
                    try
                    {
                        auto* pawn=ResolvePlayerPawnFromActivity(context.Context);
                        if (!pawn || pawn->GetWorld()!=m_readyWorld) return;
                        const auto state = classify ? classify(context.Context) : std::string{};
                        const bool configured=std::any_of(m_playerRules.begin(),m_playerRules.end(),
                            [&eventPath](const auto& rule) {
                                return std::any_of(rule.Nameplate.Events.begin(),rule.Nameplate.Events.end(),
                                    [&eventPath](const auto& event){return event["Function"]==eventPath;});
                            });
                        const bool emoteEvent = eventPath == PlayerEmotePlayEvent
                            || eventPath == PlayerEmoteStopEvent
                            || eventPath == PlayerEmoteNotifyEvent
                            || eventPath == PlayerEmoteSelectionChangedEvent;
                        if (configured || emoteEvent) {
                            const auto parameters=PS::InspectionTools::CaptureEventParameters(function,context.TheStack.Locals());
                            ApplyClientPlayerVisualRules(pawn,true,eventPath,&parameters,state);
                        } else if (!state.empty()) ApplyClientPlayerVisualRules(pawn,true,{},nullptr,state);
                    }
                    catch (...) {}
                });
            if (callbackId == 0) return false;
            m_playerActivityHooks.push_back({function, callbackId});
            m_playerNativeActivityPaths.insert(eventPath);
            return true;
        };
        registerActivityHook(
            TEXT("/Script/Dominion.PlayerMagicComponent:Multicast_SendPayloadForSpellCasting"),
            [this](UObject* source) { return ClassifyPlayerSpellActivity(source); });
        // Native events use direct post-hooks. Explicit Blueprint events stay
        // on the filtered ProcessEvent observer above.
        std::unordered_set<std::string> customEvents;
        for (const auto& rule:m_playerRules)
            for (const auto& event:rule.Nameplate.Events) {
                const auto path=event["Function"].get<std::string>();
                if (!path.starts_with("/Script/")) continue;
                if (!customEvents.insert(path).second) continue;
                if (!m_playerActivityAttemptedFunctions.insert(path).second) continue;
                if (customEvents.size()>64) {
                    PS::Log<LogLevel::Warning>(TEXT("Activity event hook limit (64) reached.\n"));
                    break;
                }
                if (!registerActivityHook(PS::ToWideSafe(path.c_str()).c_str(),{})) {
                    // HealthComponent:OnHealEvent is a known Blueprint-only
                    // base alias in this game build. Its concrete BP event is
                    // still observed through ProcessEvent, so do not report a
                    // misleading native-hook warning for this one path.
                    if (path != PlayerHealBlueprintNativeAlias)
                    PS::Log<LogLevel::Warning>(TEXT("Native activity event unavailable: {}\n"),
                        PS::ToWideSafe(path.c_str()));
                }
            }
        // Install direct hooks for every native emote route. ProcessEvent stays
        // as the Blueprint/RPC fallback and supplies post-call component state.
        registerActivityHook(PS::ToWideSafe(PlayerEmotePlayEvent).c_str(),
            [this](UObject* source) { return ClassifyPlayerEmoteActivity(source); });
        registerActivityHook(PS::ToWideSafe(PlayerEmoteStopEvent).c_str(), {});
        registerActivityHook(PS::ToWideSafe(PlayerEmoteNotifyEvent).c_str(),
            [this](UObject* source) { return ClassifyPlayerEmoteActivity(source); });
        registerActivityHook(PS::ToWideSafe(PlayerEmoteSelectionChangedEvent).c_str(),
            [this](UObject* source) { return ClassifyPlayerEmoteActivity(source); });
    }

    UObject* ResolveAIHealthComponent(UObject* character)
    {
        if (!character) return nullptr;
        UObject* fallback = nullptr;
        for (const auto* name : {TEXT("HealthComponent"), TEXT("Health Component"),
            TEXT("BP_Components_Health")})
        {
            if (!DragonWilds::PropertyHelper::GetPropertyByName(
                    character->GetClassPrivate(), name)) continue;
            UObject* health = nullptr;
            try { health = DragonWilds::ActorHelper::GetObjectRef(character, name); }
            catch (...) { continue; }
            if (!health) continue;
            if (!fallback) fallback = health;
            try
            {
                auto getMax = DragonWilds::ActorHelper::FunctionCall(
                    health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                getMax.Invoke();
                if (const double maximum = getMax.NumericResult();
                    std::isfinite(maximum) && maximum > 0.0) return health;
            }
            catch (...) {}
        }
        return fallback;
    }

    DragonWildsSpawnLoader::SpawnInfo* DragonWildsSpawnLoader::ResolveAIBinding(const LiveAIBinding& binding)
    {
        if (binding.EventSpawn) return binding.EventSpawn.get();
        const auto found = std::find_if(m_spawns.begin(), m_spawns.end(),
            [&](const SpawnInfo& entry) {
                return entry.Type == ESpawnEntryType::AISpawnPoint
                    && std::memcmp(&entry.StableId, &binding.SpawnId, sizeof(binding.SpawnId)) == 0;
            });
        return found == m_spawns.end() ? nullptr : &*found;
    }

    DragonWildsSpawnLoader::SpawnInfo*
        DragonWildsSpawnLoader::ResolveAISpawnForCharacter(UObject* character)
    {
        if (!character) return nullptr;
        std::erase_if(m_liveAIBindings,
            [](const LiveAIBinding& binding) { return !binding.Actor.Get(); });

        const auto findById = [&](const FGuid& id) -> SpawnInfo* {
            const auto found = std::find_if(m_spawns.begin(), m_spawns.end(),
                [&](const SpawnInfo& entry) {
                    return entry.Type == ESpawnEntryType::AISpawnPoint
                        && std::memcmp(&entry.StableId, &id, sizeof(id)) == 0;
                });
            return found == m_spawns.end() ? nullptr : &*found;
        };
        for (const auto& binding : m_liveAIBindings)
        {
            if (binding.Actor.Get() == character) return ResolveAIBinding(binding);
        }

        auto* aiBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        auto* spawnInfoProperty = aiBaseClass
            ? PropertyHelper::GetPropertyByName<FStructProperty>(aiBaseClass, TEXT("SpawnInfo")) : nullptr;
        auto* sourceIdProperty = spawnInfoProperty && spawnInfoProperty->GetStruct()
            ? PropertyHelper::GetPropertyByName(
                spawnInfoProperty->GetStruct().Get(), TEXT("SpawnSourceId")) : nullptr;
        if (spawnInfoProperty && sourceIdProperty
            && sourceIdProperty->GetSize() == sizeof(FGuid))
        {
            FGuid sourceId{};
            std::memcpy(&sourceId, reinterpret_cast<uint8*>(character)
                + spawnInfoProperty->GetOffset_Internal()
                + sourceIdProperty->GetOffset_Internal(), sizeof(sourceId));
            if (auto* exact = findById(sourceId))
            {
                m_liveAIBindings.push_back({PS::WeakObject(character), exact->StableId});
                PS::Log<LogLevel::Verbose>(
                    STR("Bound emitted {} to RuneSchema spawn '{}' by native SpawnSourceId.\n"),
                    character->GetClassPrivate()->GetName(),
                    PS::ToWideSafe(exact->DisplayName.c_str()));
                return exact;
            }
        }

        // SpawnInfo may receive its Guid after the AI is emitted.
        auto* actor = static_cast<AActor*>(character);
        const auto location = ActorHelper::GetActorLocation(actor);
        SpawnInfo* nearest = nullptr;
        double nearestDistanceSquared = 1000.0 * 1000.0;
        for (auto& candidate : m_spawns)
        {
            if (candidate.Type != ESpawnEntryType::AISpawnPoint
                || !candidate.AdditionalDrops.empty()
                || (candidate.DisplayName.empty() && candidate.BossName.empty()
                    && candidate.LootRow.empty())) continue;
            if (!candidate.AIClassPath.empty())
            {
                auto* expectedClass = ResolveClass(candidate.AIClassPath);
                if (!expectedClass || !character->IsA(expectedClass)) continue;
            }
            const bool alreadyBound = std::any_of(
                m_liveAIBindings.begin(), m_liveAIBindings.end(),
                [&](const LiveAIBinding& binding) {
                    return binding.Actor.Get()
                        && std::memcmp(&binding.SpawnId, &candidate.StableId,
                            sizeof(candidate.StableId)) == 0;
                });
            if (alreadyBound) continue;
            const auto dx = location.X() - candidate.Location.X();
            const auto dy = location.Y() - candidate.Location.Y();
            const auto dz = location.Z() - candidate.Location.Z();
            const auto distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared < nearestDistanceSquared)
            {
                nearestDistanceSquared = distanceSquared;
                nearest = &candidate;
            }
        }
        if (nearest)
        {
            m_liveAIBindings.push_back({PS::WeakObject(character), nearest->StableId});
            PS::Log<LogLevel::Verbose>(
                STR("Bound emitted {} to nearby RuneSchema spawn '{}' at {:.0f} units.\n"),
                character->GetClassPrivate()->GetName(),
                PS::ToWideSafe(nearest->DisplayName.c_str()),
                std::sqrt(nearestDistanceSquared));
        }
        return nearest;
    }

    void DragonWildsSpawnLoader::ApplyAIScale(UObject* character)
    {
        auto* aiBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        if (!character || !aiBaseClass || !character->IsA(aiBaseClass))
        {
            return;
        }

        if (auto* spawn = ResolveAISpawnForCharacter(character))
        {
            auto* actor = static_cast<AActor*>(character);
            actor->SetActorScale3D(spawn->Scale);
            ApplyAIDisplayName(character, spawn->DisplayName, spawn->BossName);
            ApplyAILootRow(character, spawn->LootRow);
            ApplyAIProperties(character, spawn->CharacterProperties,
                spawn->ComponentProperties);
            ApplyVisualEffect(character, spawn->VisualEffect,
                STR("Spawn from '") + spawn->ModName + STR("'"));
            ApplyCombatMultipliers(character, spawn->HealthMultiplier,
                spawn->DamageMultiplier);
            ApplyDropMultiplier(actor, spawn->DropMultiplier);
            ApplyAdditionalDrops(actor,spawn->AdditionalDrops);
        }
    }

    void DragonWildsSpawnLoader::ApplyAIProperties(UObject* character,
        const nlohmann::json& characterProperties,
        const nlohmann::json& componentProperties)
    {
        if (!character || m_characterPropertiesAppliedActors.contains(character)) return;
        ApplyEntryProperties(character, characterProperties);
        for (const auto& [componentName, properties] : componentProperties.items())
        {
            UObject* component = nullptr;
            try
            {
                const auto wideName = RC::to_generic_string(componentName);
                component = ActorHelper::GetObjectRef(character, wideName.c_str());
            }
            catch (...) {}
            if (!component)
            {
                PS::Log<LogLevel::Warning>(STR("Spawn component '{}' was unavailable on {}.\n"),
                    RC::to_generic_string(componentName),
                    character->GetClassPrivate()->GetName());
                continue;
            }
            ApplyEntryProperties(component, properties);
        }
        m_characterPropertiesAppliedActors.insert(character);
    }

    void DragonWildsSpawnLoader::ApplyCombatMultipliers(
        UObject* character, double healthMultiplier, double damageMultiplier)
    {
        if (!character || m_combatScaledActors.contains(character)
            || (healthMultiplier == 1.0 && damageMultiplier == 1.0)) return;

        bool healthApplied = healthMultiplier == 1.0;
        bool damageApplied = damageMultiplier == 1.0;
        if (!healthApplied)
        {
            try
            {
                if (auto* health = ResolveAIHealthComponent(character))
                {
                    auto getMax = ActorHelper::FunctionCall(
                        health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                    getMax.Invoke();
                    const double oldMaximum = getMax.NumericResult();
                    if (!std::isfinite(oldMaximum) || oldMaximum <= 0.0) return;
                    const double desiredMaximum = oldMaximum * healthMultiplier;
                    auto modifyMax = ActorHelper::FunctionCall(
                        health, STR("/Script/Dominion.HealthComponent:ModifyMaxHealth"));
                    modifyMax.FirstNumericArg(desiredMaximum).Invoke();
                    auto verifyMax = ActorHelper::FunctionCall(
                        health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                    verifyMax.Invoke();
                    healthApplied = std::abs(verifyMax.NumericResult() - desiredMaximum) < 0.01;
                    if (healthApplied)
                    {
                        auto setHealth = ActorHelper::FunctionCall(
                            health, STR("/Script/Dominion.HealthComponent:SetHealth"));
                        setHealth.FirstNumericArg(desiredMaximum).Invoke();
                    }
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(STR("Enemy health scaling was unavailable for {}: {}\n"),
                    character->GetClassPrivate()->GetName(), PS::ToWideSafe(error.what()));
            }
        }
        if (!damageApplied)
        {
            for (auto* property = character->GetClassPrivate()->GetPropertyLink();
                property; property = property->GetPropertyLinkNext())
            {
                auto* objectProperty = CastField<FObjectProperty>(property);
                if (!objectProperty) continue;
                auto propertyName = RC::to_string(property->GetName());
                std::transform(propertyName.begin(), propertyName.end(), propertyName.begin(),
                    [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                if (propertyName.find("attack") == std::string::npos
                    && propertyName.find("damage") == std::string::npos
                    && propertyName.find("attribute") == std::string::npos) continue;
                auto* address = objectProperty->ContainerPtrToValuePtr<void>(character);
                auto* component = address ? *reinterpret_cast<UObject**>(address) : nullptr;
                if (!component) continue;
                for (const auto* candidate : {TEXT("DamageMultiplier"), TEXT("DamageScale"),
                    TEXT("OutgoingDamageMultiplier"), TEXT("AttackDamageMultiplier")})
                {
                    auto* numeric = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                        component->GetClassPrivate(), candidate));
                    if (!numeric || !numeric->IsFloatingPoint()) continue;
                    auto* target = numeric->ContainerPtrToValuePtr<void>(component);
                    numeric->SetFloatingPointPropertyValue(target,
                        numeric->GetFloatingPointPropertyValue(target) * damageMultiplier);
                    damageApplied = true;
                }
            }
        }
        m_combatScaledActors.insert(character);
        if (!healthApplied || !damageApplied)
            PS::Log<LogLevel::Warning>(STR("Enemy combat scaling was only partially supported by {} (health: {}, damage: {}).\n"),
                character->GetClassPrivate()->GetName(), healthApplied, damageApplied);
    }

    bool DragonWildsSpawnLoader::ApplyAILootRow(
        UObject* character, const std::string& lootRow)
    {
        if (!character || lootRow.empty()) return false;
        if(auto found=m_bonusApplied.find(character);found!=m_bonusApplied.end() && found->second.Get()==character)return true;
        UObject* loot = nullptr;
        for (const auto* name : {TEXT("LootDrop"), TEXT("LootDrop_GEN_VARIABLE")})
        {
            loot = ActorHelper::GetObjectRef(character, name);
            if (loot) break;
        }
        if (!loot)
        {
            if (m_lootRowWarningActors.insert(character).second)
                PS::Log<LogLevel::Warning>(
                    STR("AI LootDrop component was unavailable on {}.\n"),
                    character->GetClassPrivate()->GetName());
            return false;
        }

        auto* handleProperty = CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                loot->GetClassPrivate(), TEXT("EnemyTableRowHandle")));
        auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
        auto* tableProperty = handleStruct ? CastField<FObjectProperty>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
        auto* rowProperty = handleStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
        if (!handleProperty || !tableProperty || !rowProperty
            || handleProperty->GetArrayDim()!=1 || tableProperty->GetArrayDim()!=1 || rowProperty->GetArrayDim()!=1
            || tableProperty->GetElementSize()!=sizeof(UObject*) || rowProperty->GetElementSize()!=sizeof(FName)
            || tableProperty->GetOffset_Internal()<0 || rowProperty->GetOffset_Internal()<0
            || tableProperty->GetOffset_Internal()+tableProperty->GetElementSize()>handleProperty->GetElementSize()
            || rowProperty->GetOffset_Internal()+rowProperty->GetElementSize()>handleProperty->GetElementSize())
        {
            if (m_lootRowWarningActors.insert(character).second)
                PS::Log<LogLevel::Warning>(
                    STR("AI EnemyTableRowHandle was unavailable on {}.\n"),
                    character->GetClassPrivate()->GetName());
            return false;
        }

        auto* handle = handleProperty->ContainerPtrToValuePtr<void>(loot);
        UObject* selectedTable=nullptr;
        std::memcpy(&selectedTable,tableProperty->ContainerPtrToValuePtr<void>(handle),sizeof(selectedTable));
        const FName requestedRow(RC::to_generic_string(lootRow), FNAME_Add);
        if (auto* commonObject = ActorHelper::ResolveObject(TEXT(
                "/Game/Gameplay/Items/LootDropTables/DT_EnemyLootDropTable."
                "DT_EnemyLootDropTable")))
        {
            if(!commonObject->IsA<UDataTable>())return false;
            auto* commonTable = static_cast<UDataTable*>(commonObject);
            if (commonTable->FindRowUnchecked(requestedRow))
            {
                selectedTable=commonTable;
            }
        }
        if(!selectedTable || !selectedTable->IsA<UDataTable>() || !static_cast<UDataTable*>(selectedTable)->FindRowUnchecked(requestedRow)) {
            if(m_lootRowWarningActors.insert(character).second)
                PS::Log<LogLevel::Warning>(STR("Enemy loot row '{}' is unavailable; existing loot preserved.\n"),RC::to_generic_string(lootRow));
            return false;
        }
        std::memcpy(tableProperty->ContainerPtrToValuePtr<void>(handle),&selectedTable,sizeof(selectedTable));
        rowProperty->SetPropertyValue(
            rowProperty->ContainerPtrToValuePtr<void>(handle),
            FName(RC::to_generic_string(lootRow), FNAME_Add));
        if (m_lootRowConfiguredActors.insert(character).second)
            PS::Log<LogLevel::Verbose>(STR("Applied enemy loot row '{}' to {}.\n"),
                RC::to_generic_string(lootRow), character->GetClassPrivate()->GetName());
        return true;
    }

    void DragonWildsSpawnLoader::ApplyAIDisplayName(
        UObject* character, const std::string& displayName,
        const std::string& bossName)
    {
        if (!character || (displayName.empty() && bossName.empty())) return;
        const auto& regularValue = displayName.empty() ? bossName : displayName;
        const auto& bossValue = bossName.empty() ? regularValue : bossName;
        const bool actorNameApplied = SetInstanceText(
            character, TEXT("AIName"), regularValue);
        bool actorBossNameApplied = false;
        try
        {
            auto* bossHealthBarClass = ActorHelper::ResolveClass(
                TEXT("/Game/Gameplay/AI/Components/BP_BossAiHealthBarComponent.BP_BossAiHealthBarComponent_C"));
            if (bossHealthBarClass)
            {
                auto getBossHealthBar = ActorHelper::FunctionCall(character,
                    STR("/Script/Engine.Actor:GetComponentByClass"));
                getBossHealthBar.Arg(TEXT("ComponentClass"), bossHealthBarClass).Invoke();
                if (auto* bossHealthBar = getBossHealthBar.Result<UObject*>())
                    actorBossNameApplied = SetInstanceText(
                        bossHealthBar, TEXT("AiDisplayName"), bossValue);
            }
        }
        catch (...) {}
        auto* textBlock = ResolveAINameTextBlock(character, false);
        auto* bossTextBlock = ResolveAINameTextBlock(character, true);
        bool widgetNameApplied = false;
        if (textBlock)
        {
            try
            {
                m_applyingHealthBarName = true;
                widgetNameApplied = SetTextBlockText(textBlock, regularValue);
                m_applyingHealthBarName = false;
            }
            catch (...)
            {
                m_applyingHealthBarName = false;
            }
        }
        if (bossTextBlock)
        {
            try
            {
                m_applyingHealthBarName = true;
                widgetNameApplied = SetTextBlockText(bossTextBlock, bossValue)
                    || widgetNameApplied;
                m_applyingHealthBarName = false;
            }
            catch (...)
            {
                m_applyingHealthBarName = false;
            }
        }

        if (widgetNameApplied)
        {
            if (m_customNamedAIActors.insert(character).second)
                PS::Log<LogLevel::Verbose>(
                    STR("Applied custom AI health-bar name '{}' to {} "
                        "(AIName={}, BossName={}).\n"),
                    PS::ToWideSafe((bossTextBlock ? bossValue : regularValue).c_str()),
                    character->GetClassPrivate()->GetName(), actorNameApplied,
                    actorBossNameApplied);
        }
        else if (m_customNameWarningActors.insert(character).second)
        {
            PS::Log<LogLevel::Verbose>(
                STR("Custom AI name '{}' is bound to {}; the bounded health-bar retry will apply it when its widget appears (AIName={}, BossName={}).\n"),
                PS::ToWideSafe(regularValue.c_str()),
                character->GetClassPrivate()->GetName(), actorNameApplied,
                actorBossNameApplied);
        }
    }

    UObject* DragonWildsSpawnLoader::ResolveAINameTextBlock(
        UObject* character, bool bossName)
    {
        if (!character) return nullptr;
        try
        {
            const auto* componentPath = bossName
                ? TEXT("/Game/Gameplay/AI/Components/BP_BossAiHealthBarComponent.BP_BossAiHealthBarComponent_C")
                : TEXT("/Game/Gameplay/AI/Components/BP_AiHealthBarComponent.BP_AiHealthBarComponent_C");
            auto* healthBarClass = ActorHelper::ResolveClass(componentPath);
            if (!healthBarClass) return nullptr;
            auto getHealthBar = ActorHelper::FunctionCall(character,
                STR("/Script/Engine.Actor:GetComponentByClass"));
            getHealthBar.Arg(TEXT("ComponentClass"), healthBarClass).Invoke();
            auto* healthBar = getHealthBar.Result<UObject*>();
            if (!healthBar) return nullptr;
            UObject* widget = nullptr;
            if (bossName)
            {
                widget = ActorHelper::GetObjectRef(healthBar, TEXT("WidgetInstance"));
            }
            else
            {
                auto getWidget = ActorHelper::FunctionCall(healthBar,
                    STR("/Script/UMG.WidgetComponent:GetUserWidgetObject"));
                getWidget.Invoke();
                widget = getWidget.Result<UObject*>();
            }
            if (!widget) return nullptr;
            auto* widgetTree = ActorHelper::GetObjectRef(widget, TEXT("WidgetTree"));
            if (!widgetTree) return nullptr;
            if (auto* direct = ActorHelper::GetObjectRef(
                    widget, TEXT("EnemyNameTextBlock"))) return direct;
            const auto namePath = std::format(
                STR("{}.EnemyNameTextBlock"), widgetTree->GetPathName());
            return UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, namePath.c_str(), false);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void DragonWildsSpawnLoader::OnHealthBarTextSet(UObject* textBlock)
    {
        if (!textBlock || m_applyingHealthBarName) return;
        std::erase_if(m_liveAIBindings,
            [](const LiveAIBinding& binding) { return !binding.Actor.Get(); });
        for (const auto& binding : m_liveAIBindings)
        {
            auto* actor = binding.Actor.Get();
            if (!actor) continue;
            auto* regularText = ResolveAINameTextBlock(actor, false);
            auto* bossText = ResolveAINameTextBlock(actor, true);
            if (regularText != textBlock && bossText != textBlock) continue;
            const auto* spawn = ResolveAIBinding(binding);
            if (!spawn
                || (spawn->DisplayName.empty() && spawn->BossName.empty())) return;
            const auto& regularValue = spawn->DisplayName.empty()
                ? spawn->BossName : spawn->DisplayName;
            const auto& selectedValue = bossText == textBlock
                && !spawn->BossName.empty() ? spawn->BossName : regularValue;
            try
            {
                m_applyingHealthBarName = true;
                const bool applied = SetTextBlockText(
                    textBlock, selectedValue);
                m_applyingHealthBarName = false;
                if (applied && m_customNamedAIActors.insert(actor).second)
                    PS::Log<LogLevel::Verbose>(
                        STR("Applied custom AI health-bar name '{}' to {} when its widget initialized.\n"),
                        PS::ToWideSafe(selectedValue.c_str()),
                        actor->GetClassPrivate()->GetName());
            }
            catch (...)
            {
                m_applyingHealthBarName = false;
            }
            return;
        }
    }

    void DragonWildsSpawnLoader::RetryPendingAINames(double deltaSeconds)
    {
        if (m_liveAIBindings.empty()) return;
        m_aiNameRetryAccumulator += std::max(0.0, deltaSeconds);
        if (m_aiNameRetryAccumulator < 0.25) return;
        m_aiNameRetryAccumulator = 0.0;

        std::erase_if(m_liveAIBindings,
            [&](const LiveAIBinding& binding) {
                auto* actor = binding.Actor.Get();
                if (actor) return false;
                return true;
            });
        for (const auto& binding : m_liveAIBindings)
        {
            auto* actor = binding.Actor.Get();
            if (!actor || m_customNamedAIActors.contains(actor)) continue;
            auto& attempts = m_aiNameRetryAttempts[actor];
            if (attempts >= 120) continue;
            ++attempts;
            const auto* spawn = ResolveAIBinding(binding);
            if (spawn)
                ApplyAIDisplayName(actor, spawn->DisplayName, spawn->BossName);
        }
    }

    int DragonWildsSpawnLoader::ApplyDropMultiplierToObject(UObject* object, double multiplier)
    {
        if (!object || multiplier <= 1.0)
        {
            return 0;
        }

        auto* arrayProperty = PropertyHelper::GetPropertyByName<FArrayProperty>(
            object->GetClassPrivate(), TEXT("ItemsToDrop"));
        auto* structProperty = arrayProperty
            ? CastField<FStructProperty>(arrayProperty->GetInner()) : nullptr;
        auto* itemStruct = structProperty ? structProperty->GetStruct().Get() : nullptr;
        auto* minProperty = itemStruct ? CastField<FNumericProperty>(
            PropertyHelper::GetPropertyByName(itemStruct, TEXT("MinToDrop"))) : nullptr;
        auto* maxProperty = itemStruct ? CastField<FNumericProperty>(
            PropertyHelper::GetPropertyByName(itemStruct, TEXT("MaxToDrop"))) : nullptr;
        if (!arrayProperty || !structProperty || !minProperty || !maxProperty
            || !minProperty->IsInteger() || !maxProperty->IsInteger())
        {
            return 0;
        }

        auto multiply = [multiplier](FNumericProperty* property, void* container) {
            auto* valueAddress = property->ContainerPtrToValuePtr<void>(container);
            const auto oldValue = property->GetSignedIntPropertyValue(valueAddress);
            if (oldValue <= 0)
            {
                return;
            }

            const auto scaled = std::ceil(static_cast<double>(oldValue) * multiplier);
            const auto capped = std::min(
                scaled, static_cast<double>(std::numeric_limits<int32>::max()));
            property->SetIntPropertyValue(valueAddress, static_cast<int64>(capped));
        };

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(object);
        UECustom::FScriptArrayHelper helper(array, arrayProperty);
        int adjusted = 0;
        helper.ForEachElement([&](void* element) {
            multiply(minProperty, element);
            multiply(maxProperty, element);
            adjusted++;
        });
        return adjusted;
    }

    void DragonWildsSpawnLoader::ApplyDropMultiplier(UObject* actor, double multiplier)
    {
        if (!PS::PSConfig::Get()->IsExperimentalDropScalingEnabled()
            || !actor || multiplier <= 1.0
            || m_dropScaledActors.contains(actor))
        {
            return;
        }

        int adjusted = ApplyDropMultiplierToObject(actor, multiplier);
        for (const auto* componentName : {
            TEXT("ItemDropComponent"),
            TEXT("ItemDropOnSplitComponent"),
            TEXT("ItemDropOnDestructionComponent") })
        {
            auto* componentProperty = PropertyHelper::GetPropertyByName<FObjectProperty>(
                actor->GetClassPrivate(), componentName);
            if (!componentProperty)
            {
                continue;
            }

            auto* address = componentProperty->ContainerPtrToValuePtr<void>(actor);
            auto* component = address ? *reinterpret_cast<UObject**>(address) : nullptr;
            adjusted += ApplyDropMultiplierToObject(component, multiplier);
        }

        if (adjusted > 0)
        {
            m_dropScaledActors.insert(actor);
            PS::Log<LogLevel::Verbose>(
                STR("Applied {:.2f}x drop scaling to {} item-drop row(s) on {}.\n"),
                multiplier, adjusted, actor->GetClassPrivate()->GetName());
        }
    }

    bool DragonWildsSpawnLoader::SetupSpawnTick()
    {
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DragonWildsSpawnCreate");

        m_worldTeardownCallbackId = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                // Preserve authored item rules while resetting per-world state.
                PlayerGhost::ClearWorld();
                m_readyWorld = nullptr;
                m_pendingWorld = nullptr;
                m_pendingCellBounds.clear();
                m_toolActors.clear();
                m_toolPlacements=nlohmann::json::array();
                PS::SpawnToolRequests::Clear();
                PS::ItemIconRequests::Clear();
                PS::ItemIconRequests::Available=false;
                m_visualEffectAppliedActors.clear();
                m_nameplateAppliedActors.clear();
                m_activeNameplateStates.clear();
                m_observedActivities.clear();
                m_pendingRespawns.clear();
                m_nameplateRefreshElapsed = 0.0;
                m_visualTimerElapsed = 0.0;
                m_sharedSpawnVisuals.clear();
                for (const auto& ref : m_rootedVisualEffectMaterials)
                    if (auto* material=ref.Get()) if (material->IsRootSet()) material->ClearRootSet();
                m_rootedVisualEffectMaterials.clear();
            }, options);
        if (m_worldTeardownCallbackId == Hook::ERROR_ID)
            PS::Log<LogLevel::Warning>(
                STR("World-teardown ghost cleanup could not be registered.\n"));

        options.HookName = TEXT("DragonWildsSpawnCreate");

        m_spawnTickCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float deltaSeconds, bool) {
                PlayerGhost::Flush();
                PumpItemIcons();
                PumpSpawnTools();
                ReconcileTimedBuildingProps(deltaSeconds);
                RetryPendingAINames(deltaSeconds);
                PumpClientSpawnVisuals(deltaSeconds);
                RefreshPlayerNameplates(deltaSeconds);
                if (!m_pendingWorld)
                {
                    return;
                }

                auto* world = m_pendingWorld;
                auto bounds = std::move(m_pendingCellBounds);
                m_pendingCellBounds.clear();
                m_pendingWorld = nullptr;

                if (IsWorldStillLoaded(world))
                {
                    TryProcessSpawns(world, &bounds, STR("cell streamed in"));
                }
            },
            options);

        if (m_spawnTickCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(STR("Unable to register the spawn tick; spawns will not be created.\n"));
            return false;
        }

        return true;
    }

    void DragonWildsSpawnLoader::OnCellShown(UObject* cellObject)
    {
        auto* cell = static_cast<UECustom::UWorldPartitionRuntimeLevelStreamingCell*>(cellObject);
        if (!cell || cell->GetIsHLOD())
        {
            return;
        }

        auto* world = cell->GetWorld();
        auto* bounds = cell->GetContentBounds();
        if (!world || !bounds)
        {
            return;
        }

        if (m_pendingWorld != world)
        {
            m_pendingCellBounds.clear();
            m_pendingWorld = world;
        }
        m_pendingCellBounds.push_back(*bounds);
    }

    bool DragonWildsSpawnLoader::HasLiveSpawnedAI(UWorld* world, SpawnInfo& spawn)
    {
        auto* aiBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        if (!aiBaseClass)
        {
            return false;
        }

        auto* spawnInfoProperty = PropertyHelper::GetPropertyByName<FStructProperty>(aiBaseClass, TEXT("SpawnInfo"));
        if (!spawnInfoProperty || !spawnInfoProperty->GetStruct())
        {
            return false;
        }

        auto* sourceIdProperty = PropertyHelper::GetPropertyByName(spawnInfoProperty->GetStruct().Get(), TEXT("SpawnSourceId"));
        if (!sourceIdProperty || sourceIdProperty->GetSize() != sizeof(FGuid))
        {
            return false;
        }

        TArray<UObject*> instances;
        UECustom::UObjectGlobals::GetObjectsOfClass(aiBaseClass, instances, true, static_cast<EObjectFlags>(0));
        for (auto* object : instances)
        {
            if (!object || object->GetWorld() != world
                || object->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))
            {
                continue;
            }

            FGuid sourceId{};
            auto* data = reinterpret_cast<uint8*>(object)
                + spawnInfoProperty->GetOffset_Internal() + sourceIdProperty->GetOffset_Internal();
            std::memcpy(&sourceId, data, sizeof(sourceId));
            if (std::memcmp(&sourceId, &spawn.StableId, sizeof(sourceId)) == 0)
            {
                ApplyAIScale(object);
                return true;
            }
        }

        return false;
    }

    void DragonWildsSpawnLoader::DestroyLiveSpawnedAI(UWorld* world,const SpawnInfo& spawn)
    {
        auto* aiBaseClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr,nullptr,TEXT("/Script/Dominion.DominionAICharacter"));
        auto* spawnInfoProperty=aiBaseClass
            ? PropertyHelper::GetPropertyByName<FStructProperty>(aiBaseClass,TEXT("SpawnInfo")):nullptr;
        auto* sourceIdProperty=spawnInfoProperty && spawnInfoProperty->GetStruct()
            ? PropertyHelper::GetPropertyByName(spawnInfoProperty->GetStruct().Get(),TEXT("SpawnSourceId")):nullptr;
        if(!aiBaseClass || !spawnInfoProperty || !sourceIdProperty || sourceIdProperty->GetSize()!=sizeof(FGuid))
            throw std::runtime_error("Timed AI cleanup source identity is unavailable");
        TArray<UObject*> instances;
        UECustom::UObjectGlobals::GetObjectsOfClass(aiBaseClass,instances,true,static_cast<EObjectFlags>(0));
        for(auto* object:instances) {
            if(!object || object->GetWorld()!=world || !object->IsA<AActor>())continue;
            FGuid sourceId{};
            auto* data=reinterpret_cast<uint8*>(object)+spawnInfoProperty->GetOffset_Internal()+sourceIdProperty->GetOffset_Internal();
            std::memcpy(&sourceId,data,sizeof(sourceId));
            if(std::memcmp(&sourceId,&spawn.StableId,sizeof(sourceId))==0)
                ActorHelper::DestroyActor(static_cast<AActor*>(object));
        }
    }

    void DragonWildsSpawnLoader::RetireTimedActor(AActor* actor)
    {
        if(!actor)return;
        // A native building can be dormant and can own transient FX.  Wake the
        // actor before changing presentation, remove locally-created FX, and
        // make collision/tick/visibility inert before the authoritative destroy.
        // This leaves no collision frame while the replicated destroy closes
        // the remote actor channel.
        if(const auto applied=m_visualEffectAppliedActors.find(actor);
            applied!=m_visualEffectAppliedActors.end()) {
            if(auto* component=applied->second.Component.Get())NiagaraAttachment::Destroy(component);
            m_visualEffectAppliedActors.erase(applied);
        }
        try {ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:FlushNetDormancy")).Invoke();}catch(...){}
        try {ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorEnableCollision")).Arg(TEXT("bNewActorEnableCollision"),false).Invoke();}catch(...){}
        try {ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorTickEnabled")).Arg(TEXT("bEnabled"),false).Invoke();}catch(...){}
        try {ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorHiddenInGame")).Arg(TEXT("bNewHidden"),true).Invoke();}catch(...){}
        try {ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:ForceNetUpdate")).Invoke();}catch(...){}
        const bool prior = m_allowManagedBuildingDestroy;
        m_allowManagedBuildingDestroy = true;
        try
        {
            ActorHelper::DestroyActor(actor);
            m_allowManagedBuildingDestroy = prior;
        }
        catch (...)
        {
            m_allowManagedBuildingDestroy = prior;
            throw;
        }
    }

    AActor* DragonWildsSpawnLoader::FindActorByStableId(UWorld* world, const FGuid& stableId)
    {
        auto* actorClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.Actor"));
        if (!actorClass)
        {
            throw std::runtime_error("Actor class was unavailable");
        }

        TArray<UObject*> instances;
        UECustom::UObjectGlobals::GetObjectsOfClass(actorClass, instances, true, static_cast<EObjectFlags>(0));
        for (auto* object : instances)
        {
            if (!object || object->GetWorld() != world)
            {
                continue;
            }

            auto* property = PropertyHelper::GetPropertyByName(object->GetClassPrivate(), TEXT("SpudGuid"));
            if (!property || property->GetSize() != sizeof(FGuid))
            {
                continue;
            }

            const auto guid = ReadGuidProperty(object, property);
            if (std::memcmp(&guid, &stableId, sizeof(guid)) == 0)
            {
                return static_cast<AActor*>(object);
            }
        }

        return nullptr;
    }

    bool DragonWildsSpawnLoader::IsWorldStillLoaded(UWorld* world)
    {
        if (!world)
        {
            return false;
        }

        auto* worldClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.World"));
        if (!worldClass)
        {
            return false;
        }

        TArray<UObject*> worlds;
        UECustom::UObjectGlobals::GetObjectsOfClass(worldClass, worlds, true, static_cast<EObjectFlags>(0));
        for (auto* candidate : worlds)
        {
            if (candidate == world)
            {
                return true;
            }
        }

        return false;
    }

    void DragonWildsSpawnLoader::TryProcessSpawns(UWorld* world, const std::vector<UECustom::FBox>* bounds, const wchar_t* trigger)
    {
        if (!world || m_processingSpawns)
        {
            return;
        }

        m_processingSpawns = true;
        try
        {
            const bool authoritative=GetGameMode(world)!=nullptr;
            const bool clientPresentation=!authoritative && PS::Network::Detect(world).Mode==PS::Network::Role::Client;
            if (authoritative || clientPresentation)
            {
                if (world != m_readyWorld)
                {
                    m_dropScaledActors.clear();
                    m_lootRowConfiguredActors.clear();
                    m_lootRowWarningActors.clear();
                    m_combatScaledActors.clear();
                    m_characterPropertiesAppliedActors.clear();
                    m_customNamedAIActors.clear();
                    m_customNameWarningActors.clear();
                    m_aiNameRetryAttempts.clear();
                    m_aiNameRetryAccumulator = 0.0;
                    m_liveAIBindings.clear();
                    PlayerGhost::ClearWorld();
                    m_visualEffectAppliedActors.clear();
                    m_nameplateAppliedActors.clear();
                    m_activeNameplateStates.clear();
                    m_observedActivities.clear();
                    m_pendingRespawns.clear();
                    m_nameplateRefreshElapsed = 0.0;
                m_visualTimerElapsed = 0.0;
                m_buildingTimeElapsed = 0.0;
                    m_sharedSpawnVisuals.clear();
                    for (const auto& ref : m_rootedVisualEffectMaterials)
                        if (auto* material=ref.Get()) if (material->IsRootSet()) material->ClearRootSet();
                    m_rootedVisualEffectMaterials.clear();
                    for (auto& spawn : m_spawns)
                    {
                        spawn.bExistsInWorld = false;
                        spawn.bSpawnFailed = false;
                        spawn.bCellActivated = false;
                        spawn.bTimeAllowed = false;
                        spawn.bDeconstructed = false;
                        spawn.LiveActor = {};
                        spawn.Location = FVector(spawn.AuthoredLocation.X(),spawn.AuthoredLocation.Y(),spawn.AuthoredLocation.Z()+10.0);
                        spawn.bGroundingResolved = !spawn.bGroundToSurface;
                    }
                    m_readyWorld = world;
                    if(authoritative)ApplyPlayerRules();
                    else PS::Log<LogLevel::Verbose>(STR("Client player presentation world ready ({}).\n"),trigger);

                    if (authoritative && !m_spawns.empty())
                    {
                        PS::Log<LogLevel::Verbose>(STR("World ready ({}), processing {} entries.\n"), trigger, m_spawns.size());
                    }
                }

                const bool hasPending = std::any_of(m_spawns.begin(), m_spawns.end(), [](const SpawnInfo& spawn) {
                    return !spawn.bExistsInWorld && !spawn.bSpawnFailed;
                });
                if (authoritative && hasPending && GetAIDirector(world))
                {
                    ProcessSpawns(world, bounds);
                }
                else if (clientPresentation && hasPending)
                {
                    ProcessClientStaticAssemblies(world, bounds);
                }

            }
        }
        catch (const std::exception& e)
        {
            PS::Log<LogLevel::Error>(STR("Failed processing spawns: {}\n"), PS::ToWideSafe(e.what()));
        }
        m_processingSpawns = false;
    }

    void DragonWildsSpawnLoader::ProcessSpawns(UWorld* world, const std::vector<UECustom::FBox>* bounds)
    {
        if (!world)
        {
            return;
        }

        for (auto& spawn : m_spawns)
        {
            if (spawn.bExistsInWorld || spawn.bSpawnFailed)
            {
                continue;
            }

            if (bounds)
            {
                const bool inside = std::any_of(bounds->begin(), bounds->end(),
                    [&](const UECustom::FBox& box) {
                        return spawn.Location.X() > box.Min.X() && spawn.Location.X() < box.Max.X()
                            && spawn.Location.Y() > box.Min.Y() && spawn.Location.Y() < box.Max.Y();
                    });
                if (!inside)
                {
                    continue;
                }
                spawn.bCellActivated = true;
            }
            else if (spawn.Type != ESpawnEntryType::AISpawnPoint)
            {
                continue;
            }
            else spawn.bCellActivated = true;

            try
            {
                if(spawn.Type!=ESpawnEntryType::RemoveActor && !spawn.QuestCompleted.empty()) {
                    if(spawn.bPersistAfterCondition && FindActorByStableId(world,spawn.StableId))spawn.bConditionLatched=true;
                    if(!spawn.bConditionLatched) {
                        bool complete=false;
                        try {complete=IsQuestCompleted && IsQuestCompleted(world,spawn.QuestCompleted);} catch(...) {continue;}
                        if(complete && spawn.bPersistAfterCondition)spawn.bConditionLatched=true;
                        if(!complete)continue;
                    }
                    if(spawn.Type==ESpawnEntryType::Actor && HasEquivalentActorNear(world,spawn)) {
                        spawn.bSatisfiedByExisting=true;spawn.bExistsInWorld=true;continue;
                    }
                }
                if(spawn.Type!=ESpawnEntryType::RemoveActor && spawn.Time!=TimeOfDay::Requirement::Any) {
                    bool allowed=false;
                    try {allowed=TimeOfDay::Allows(world,spawn.Time);} catch(...) {continue;}
                    spawn.bTimeAllowed=allowed;
                    if(!allowed)continue;
                }
                ResolveGroundedLocation(world, spawn);
                switch (spawn.Type)
                {
                case ESpawnEntryType::AISpawnPoint:
                    ProcessAISpawnPointEntry(world, spawn);
                    break;
                case ESpawnEntryType::Actor:
                    ProcessActorEntry(world, spawn);
                    break;
                case ESpawnEntryType::StaticAssembly:
                    ProcessStaticAssemblyEntry(world, spawn);
                    break;
                case ESpawnEntryType::RemoveActor:
                    ProcessRemoveActorEntry(world, spawn);
                    break;
                }
            }
            catch (const std::exception& e)
            {
                spawn.bSpawnFailed = true;
                PS::Log<LogLevel::Error>(STR("Failed to process '{}' for {}: {}\n"),
                    spawn.ClassPath, spawn.ModName, PS::ToWideSafe(e.what()));
            }
        }
    }

    void DragonWildsSpawnLoader::ProcessAISpawnPointEntry(UWorld* world, SpawnInfo& spawn)
    {
        if (HasLiveSpawnedAI(world, spawn))
        {
            spawn.bExistsInWorld = true;
            return;
        }

        CreateSpawn(world, spawn);
    }

    bool DragonWildsSpawnLoader::HasEquivalentActorNear(UWorld* world,const SpawnInfo& spawn)
    {
        if(!world || spawn.DuplicateRadius<=0 || spawn.ClassPath.empty())return false;
        auto* actorClass=ResolveClass(spawn.ClassPath);
        if(!actorClass || !ActorHelper::IsActorClass(actorClass))return false;
        TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(actorClass,objects,true);
        if(objects.Num()<0 || objects.Num()>4096)throw std::runtime_error("Duplicate placement actor roster exceeds the safe bound");
        const double limit=spawn.DuplicateRadius*spawn.DuplicateRadius;
        for(auto* object:objects) {
            auto* actor=object && object->IsA<AActor>()?static_cast<AActor*>(object):nullptr;
            if(!actor || actor->GetWorld()!=world || actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
            const auto at=ActorHelper::GetActorLocation(actor);
            const auto dx=at.X()-spawn.Location.X(),dy=at.Y()-spawn.Location.Y(),dz=at.Z()-spawn.Location.Z();
            if(dx*dx+dy*dy+dz*dz<=limit)return true;
        }
        return false;
    }

    void DragonWildsSpawnLoader::ProcessActorEntry(UWorld* world, SpawnInfo& spawn)
    {
        auto* actorClass = ResolveClass(spawn.ClassPath);
        if (!actorClass)
        {
            throw std::runtime_error(std::format("Class '{}' is no longer available", RC::to_string(spawn.ClassPath)));
        }

        auto* existing = FindActorByStableId(world, spawn.StableId);
        bool needsRecreate = false;
        if (!existing)
        {
            existing = FindActorByStableId(world, spawn.LegacyId);
            needsRecreate = existing != nullptr;
        }

        LoadNativeRespawnState();
        if (!existing && spawn.bUseNativeRespawn
            && m_placedNativeRespawnActors.contains(spawn.PersistentPlacementKey))
        {
            // Preserve saved depletion; do not refill on world reload.
            spawn.bExistsInWorld = true;
            PS::Log<LogLevel::Verbose>(
                STR("Native-respawn actor '{}' is currently absent/depleted; left to the game's replenish cycle.\n"),
                spawn.EntryId);
            return;
        }

        if (existing && !existing->IsA(actorClass))
        {
            needsRecreate = true;
        }

        if (existing && !needsRecreate)
        {
            if(spawn.bBuildingProp)ApplyBuildingData(existing,spawn);
            const auto current = ActorHelper::GetActorLocation(existing);
            const auto dx = current.X() - spawn.Location.X();
            const auto dy = current.Y() - spawn.Location.Y();
            const auto dz = current.Z() - spawn.Location.Z();
            needsRecreate = dx * dx + dy * dy + dz * dz > 50.0 * 50.0;
            if (!needsRecreate)
            {
                existing->SetActorScale3D(spawn.Scale);
                ApplyActorDisplayName(existing, spawn.DisplayName);
                ApplyAIProperties(existing, nlohmann::json::object(),
                    spawn.ComponentProperties);
                ApplyVisualEffect(existing, spawn.VisualEffect,
                    STR("Spawn from '") + spawn.ModName + STR("'"));
                ApplyDropMultiplier(existing, spawn.DropMultiplier);
                ApplyAdditionalDrops(existing, spawn.AdditionalDrops);
                spawn.LiveActor=PS::WeakObjectHandle(existing);
                spawn.bExistsInWorld = true;
                return;
            }
        }

        if (existing)
        {
            ActorHelper::DestroyActor(existing);
        }

        CreateActor(world, spawn);
    }

    void DragonWildsSpawnLoader::ProcessClientStaticAssemblies(UWorld* world,
        const std::vector<UECustom::FBox>* bounds)
    {
        if (!world) return;
        for (auto& spawn : m_spawns)
        {
            if (spawn.Type != ESpawnEntryType::StaticAssembly
                || spawn.bExistsInWorld || spawn.bSpawnFailed) continue;
            if (!bounds) continue;
            const bool inside = std::any_of(bounds->begin(), bounds->end(),
                [&](const UECustom::FBox& box) {
                    return spawn.Location.X() > box.Min.X() && spawn.Location.X() < box.Max.X()
                        && spawn.Location.Y() > box.Min.Y() && spawn.Location.Y() < box.Max.Y();
                });
            if (!inside) continue;
            spawn.bCellActivated = true;
            try { ProcessStaticAssemblyEntry(world, spawn); }
            catch (const std::exception& error)
            {
                spawn.bSpawnFailed = true;
                PS::Log<LogLevel::Error>(
                    STR("[DEGRADED][BUILDING-ASSEMBLY] Client assembly '{}' failed safely: {}\n"),
                    spawn.EntryId, PS::ToWideSafe(error.what()));
            }
        }
    }

    void DragonWildsSpawnLoader::ProcessStaticAssemblyEntry(UWorld* world,
        SpawnInfo& spawn)
    {
        if (auto* live = spawn.LiveActor.Get(); live && live->GetWorld() == world
            && !live->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
        {
            spawn.bExistsInWorld = true;
            return;
        }
        CreateStaticAssembly(world, spawn);
    }

    void DragonWildsSpawnLoader::ProcessRemoveActorEntry(UWorld* world, SpawnInfo& spawn)
    {
        auto* actorClass = ResolveClass(spawn.ClassPath);
        if (!actorClass)
        {
            throw std::runtime_error(std::format("Class '{}' is no longer available", RC::to_string(spawn.ClassPath)));
        }

        TArray<UObject*> instances;
        UECustom::UObjectGlobals::GetObjectsOfClass(actorClass, instances, true, static_cast<EObjectFlags>(0));

        const double radiusSquared = static_cast<double>(spawn.RemoveRadius) * spawn.RemoveRadius;
        int removed = 0;
        int keptLevelPlaced = 0;
        for (auto* object : instances)
        {
            if (!object || object->GetWorld() != world)
            {
                continue;
            }

            auto* actor = static_cast<AActor*>(object);
            const auto location = ActorHelper::GetActorLocation(actor);
            const auto dx = location.X() - spawn.Location.X();
            const auto dy = location.Y() - spawn.Location.Y();
            const auto dz = location.Z() - spawn.Location.Z();
            if (dx * dx + dy * dy + dz * dz > radiusSquared)
            {
                continue;
            }

            if (actor->GetName().find(STR("UAID")) != RC::StringType::npos)
            {
                keptLevelPlaced++;
                continue;
            }

            if (auto* guidProperty = PropertyHelper::GetPropertyByName(actor->GetClassPrivate(), TEXT("SpudGuid"));
                guidProperty && guidProperty->GetSize() == sizeof(FGuid))
            {
                const auto guid = ReadGuidProperty(actor, guidProperty);
                const bool managed = std::any_of(m_spawns.begin(), m_spawns.end(), [&](const SpawnInfo& entry) {
                    return entry.Type == ESpawnEntryType::Actor
                        && std::memcmp(&entry.StableId, &guid, sizeof(guid)) == 0;
                });
                if (managed)
                {
                    continue;
                }
            }

            ActorHelper::DestroyActor(actor);
            removed++;
        }

        spawn.bExistsInWorld = true;
        PS::Log<LogLevel::Verbose>(STR("RemoveActor for {}: removed {} runtime-spawned actor(s), left {} level-placed alone.\n"),
            spawn.ModName, removed, keptLevelPlaced);
    }

    void DragonWildsSpawnLoader::ResolveGroundedLocation(
        UWorld* world, SpawnInfo& spawn)
    {
        if (spawn.bGroundingResolved || !spawn.bGroundToSurface)
        {
            spawn.bGroundingResolved = true;
            return;
        }
        if (!world) throw std::runtime_error("grounding world was unavailable");

        const FVector start(
            spawn.AuthoredLocation.X(), spawn.AuthoredLocation.Y(),
            spawn.AuthoredLocation.Z() + spawn.GroundTraceAbove);
        const FVector end(
            spawn.AuthoredLocation.X(), spawn.AuthoredLocation.Y(),
            spawn.AuthoredLocation.Z() - spawn.GroundTraceBelow);
        FVector impact{};
        std::string traceError;
        std::vector<AActor*> ignoredActors;
        if (auto* existing = FindActorByStableId(world, spawn.StableId))
            ignoredActors.push_back(existing);
        if (auto* legacy = FindActorByStableId(world, spawn.LegacyId);
            legacy && std::find(ignoredActors.begin(), ignoredActors.end(), legacy)
                == ignoredActors.end())
            ignoredActors.push_back(legacy);

        if (!UECustom::UKismetSystemLibrary::LineTraceGround(
                world, start, end, ignoredActors, impact, traceError))
        {
            throw std::runtime_error(
                traceError.empty()
                    ? "Location.Z $ found no blocking ground surface"
                    : "Location.Z $ ground trace failed: " + traceError);
        }

        spawn.Location = FVector(
            spawn.AuthoredLocation.X(), spawn.AuthoredLocation.Y(),
            impact.Z() + spawn.GroundZOffset + 10.0);
        spawn.bGroundingResolved = true;
        PS::Log<LogLevel::Verbose>(
            STR("Resolved Location.Z '$' for {} from trace origin Z {} to ground Z {} (offset {}).\n"),
            spawn.ModName, spawn.AuthoredLocation.Z(), spawn.Location.Z(),
            spawn.GroundZOffset);
    }

    void DragonWildsSpawnLoader::CreateSpawn(UWorld* world, SpawnInfo& spawn)
    {
        auto* spawnClass = ResolveClass(spawn.ClassPath);
        if (!spawnClass)
        {
            throw std::runtime_error(std::format("Class '{}' is no longer available", RC::to_string(spawn.ClassPath)));
        }

        auto* actor = ActorHelper::SpawnActor(world, spawnClass, spawn.Location, spawn.Rotation,
            [&](AActor* spawned) {
                if(spawn.Time!=TimeOfDay::Requirement::Any) {
                    spawned->SetFlags(RF_Transient);
                    if(auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(
                        spawned->GetClassPrivate(),TEXT("bSkipSpudStore"))))
                        skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(spawned),true);
                }
                SetGuidProperty(spawned, TEXT("Guid"), spawn.StableId);
                ApplyEntryProperties(spawned, spawn.Properties);
                if (auto* runtimeProperty = PropertyHelper::GetPropertyByName<FBoolProperty>(
                        spawned->GetClassPrivate(), TEXT("bRegisterAsRuntimeSpawned")))
                {
                    runtimeProperty->SetPropertyValue(
                        runtimeProperty->ContainerPtrToValuePtr<void>(spawned), true);
                }
            });
        actor->SetActorScale3D(spawn.Scale);
        spawn.LiveActor=PS::WeakObjectHandle(actor);

        auto* director = GetAIDirector(world);
        const bool added = InvokeAIDirectorSpawnFunction(director,
            TEXT("/Script/Dominion.AiDirector:OnSpawnPointAdded"), actor, false);
        const bool evaluated = InvokeAIDirectorSpawnFunction(director,
            TEXT("/Script/Dominion.AiDirector:TrySpawningAIForSpawnPoints"),
            actor, true);
        PS::Log<LogLevel::Verbose>(
            STR("Requested native AI-director activation for {} (registered={}, evaluated={}).\n"),
            actor->GetClassPrivate()->GetName(), added, evaluated);

        spawn.bExistsInWorld = true;
        PS::Log<LogLevel::Verbose>(STR("Spawned {} at {} {} {}\n"), actor->GetClassPrivate()->GetName(),
            spawn.Location.X(), spawn.Location.Y(), spawn.Location.Z());
    }

    void DragonWildsSpawnLoader::ValidateEventSpawn(const std::string& key)
    {
        const auto found=m_eventTemplates.find(key);
        if(found==m_eventTemplates.end())throw std::runtime_error("Missing EventOnly spawn template: "+key);
        auto* type=ResolveClass(RC::to_generic_string(found->second.Class));
        auto* base=ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
        if(!type || !base || !type->IsChildOf(base))throw std::runtime_error("Event template must resolve to DominionAICharacter");
        auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bSkipSpudStore")));
        if(!skip || skip->GetArrayDim()!=1)throw std::runtime_error("Event AI save-exclusion flag unavailable");
    }

    nlohmann::json DragonWildsSpawnLoader::EventSpawnManifest(const std::string& key) const
    {
        const auto found=m_eventTemplates.find(key);
        if(found==m_eventTemplates.end())throw std::runtime_error("Event spawn definition is missing: "+key);
        return Events::Identity(found->second);
    }

    bool DragonWildsSpawnLoader::PresentEventIdentity(AActor* actor,const std::string& payload)
    {
        if(!actor || !HasInitialized() || payload.empty())return false;
        if(PS::Network::Detect(actor).Mode!=PS::Network::Role::Client)return false;
        const auto identity=Events::DecodeIdentity(payload);
        const auto key=identity.at("spawn").get<std::string>();
        const auto found=m_eventTemplates.find(key);
        const bool tool=identity.at("kind")=="tool-ai" || identity.at("kind")=="tool-resource";
        if(!tool && found==m_eventTemplates.end())throw std::runtime_error("Client event spawn definition is missing: "+key);
        const auto definition=tool?Events::ToolIdentity(identity):found->second;
        if(!tool)Events::ValidateIdentity(identity,definition);
        if(actor->GetClassPrivate()->GetPathName()!=RC::to_generic_string(definition.Class))
            throw std::runtime_error("Replicated event actor class does not match its definition");
        if(identity.at("kind")=="tool-resource") {
            ApplyActorDisplayName(actor,definition.Name);
            actor->SetActorScale3D(FVector(definition.Scale,definition.Scale,definition.Scale));
            if(!definition.VisualEffect.empty()){auto visual=definition.VisualEffect;visual["Type"]="Ghost";if(!ApplyVisualEffect(actor,visual,TEXT("Tool resource")))return false;}
            return true;
        }
        if(std::none_of(m_liveAIBindings.begin(),m_liveAIBindings.end(),[&](const auto& binding){return binding.Actor.Get()==actor;})) {
            auto naming=std::make_shared<SpawnInfo>();naming->DisplayName=definition.Name;naming->BossName=definition.BossName;
            naming->VisualEffect=definition.VisualEffect;
            if(!naming->VisualEffect.empty())naming->VisualEffect["Type"]="Ghost";
            m_customNamedAIActors.erase(actor);m_aiNameRetryAttempts.erase(actor);
            m_liveAIBindings.push_back({PS::WeakObject(actor),{},naming});
            ApplyVisualEffect(actor,naming->VisualEffect,TEXT("Event AI"));
        }
        ApplyAIDisplayName(actor,definition.Name,definition.BossName);
        if(identity.value("version",0)==3 && PresentEventState)PresentEventState(actor,identity.at("event").get<std::string>());
        return true;
    }

    AActor* DragonWildsSpawnLoader::SpawnEventAI(const std::string& key,UWorld* world,const FVector& position,bool tool,double yaw,const std::string& eventKey,int toolPowerLevel)
    {
        if(!world || !GetGameMode(world))throw std::runtime_error("Event AI can only be spawned by world authority");
        ValidateEventSpawn(key);
        const auto& definition=m_eventTemplates.at(key);
        auto* type=ResolveClass(RC::to_generic_string(definition.Class));
        if(!tool && toolPowerLevel!=-1)throw std::runtime_error("Direct power override requires the authoring tool path");
        if(toolPowerLevel==-1)toolPowerLevel=definition.PowerLevel;
        PS::Authoring::ValidatePower(toolPowerLevel);
        FNumericProperty* power=nullptr;
        if(toolPowerLevel!=-1) {
            power=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PowerLevel")));
            if(!power || power->GetArrayDim()!=1)
                throw std::runtime_error("This AI has no reflected numeric PowerLevel for a direct temporary spawn. Choose Native/default or Permanent spawn (native spawn-point power)");
        }
        auto naming=std::make_shared<SpawnInfo>();
        naming->DisplayName=definition.Name;
        naming->BossName=definition.BossName;
        naming->Scale=FVector(definition.Scale,definition.Scale,definition.Scale);
        naming->VisualEffect=definition.VisualEffect;
        if(!naming->VisualEffect.empty())naming->VisualEffect["Type"]="Ghost";
        auto* actor=ActorHelper::SpawnActor(world,type,position,FRotator(0,yaw,0),[&](AActor* value){
            value->SetFlags(RF_Transient);
            // This callback runs before FinishSpawning/BeginPlay; never invent
            // damage/health multipliers as a substitute for native power.
            if(power)PropertyHelper::CopyJsonValueToContainer(value,power,toolPowerLevel);
            auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bSkipSpudStore")));
            skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(value),true);
            if(!skip->GetPropertyValue(skip->ContainerPtrToValuePtr<void>(value)))throw std::runtime_error("Event save exclusion did not round-trip");
            m_customNamedAIActors.erase(value);
            m_aiNameRetryAttempts.erase(value);
            m_liveAIBindings.push_back({PS::WeakObject(value),{},naming});
        });
        try {
            auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(type,TEXT("bSkipSpudStore")));
            if(!skip || !skip->GetPropertyValue(skip->ContainerPtrToValuePtr<void>(actor)))throw std::runtime_error("Event AI lost save exclusion during creation");
            if(power) {
                auto* address=power->ContainerPtrToValuePtr<void>(actor);
                const auto actual=power->IsInteger()?static_cast<double>(power->GetSignedIntPropertyValue(address)):power->GetFloatingPointPropertyValue(address);
                if(actual!=toolPowerLevel)throw std::runtime_error("Native AI initialization replaced the requested PowerLevel; temporary spawn rolled back");
            }
            actor->SetActorScale3D(FVector(definition.Scale,definition.Scale,definition.Scale));
            if(!definition.LootRow.empty() && !ApplyAILootRow(actor,definition.LootRow))
                throw std::runtime_error("Event enemy loot override could not be applied: "+definition.LootRow);
            ApplyAIDisplayName(actor,definition.Name,definition.BossName);
            ApplyVisualEffect(actor,naming->VisualEffect,TEXT("Event AI"));
            const auto mode=PS::Network::Detect(actor).Mode;
            if(mode!=PS::Network::Role::Standalone) {
                if(mode==PS::Network::Role::Unknown || !PublishEventIdentity)throw std::runtime_error("Event identity transport unavailable");
                auto identity=Events::Identity(definition);
                if(tool)identity["kind"]="tool-ai";
                else if(!eventKey.empty()){identity["version"]=3;identity["event"]=eventKey;}
                PublishEventIdentity(actor,identity.dump());
            }
            return actor;
        }catch(...){ActorHelper::DestroyActor(actor);throw;}
    }

    void DragonWildsSpawnLoader::HandleNetworkWorldState(const std::string& payload)
    {
        try {
            const auto snapshot=nlohmann::json::parse(payload);
            if(snapshot.value("kind",std::string{})!="RuneSchemaWorldSnapshot"
                || snapshot.value("version",0)!=1 || !snapshot.contains("instances")
                || !snapshot["instances"].is_array())return;
            for(const auto& record:snapshot["instances"]) {
                if(!record.is_object() || record.value("kind",std::string{})!="spawn"
                    || record.value("lifecycle",std::string("active"))!="active")continue;
                const auto path=RC::to_generic_string(record.value("actor",std::string{}));
                const auto mod=RC::to_generic_string(record.value("mod",std::string{}));
                const auto definition=RC::to_generic_string(record.value("definition",std::string{}));
                if(path.empty() || mod.empty() || definition.empty())continue;
                if(std::any_of(m_pendingClientVisuals.begin(),m_pendingClientVisuals.end(),
                    [&](const auto& pending){return pending.ActorPath==path;}))continue;
                if(m_pendingClientVisuals.size()>=512) {
                    PS::Log<LogLevel::Warning>(TEXT("[DEGRADED][NETWORK:spawn-visual] Client presentation queue is full; later snapshots can retry.\n"));
                    break;
                }
                m_pendingClientVisuals.push_back({path,mod,definition,0});
            }
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR("[DEGRADED][NETWORK:spawn-visual] World-state snapshot ignored: {}.\n"),PS::ToWideSafe(error.what()));
        }
    }

    void DragonWildsSpawnLoader::PumpClientSpawnVisuals(double deltaSeconds)
    {
        if(m_pendingClientVisuals.empty())return;
        m_clientVisualElapsed+=std::max(0.0,deltaSeconds);
        if(m_clientVisualElapsed<0.5)return;
        m_clientVisualElapsed=0.0;
        auto pending=std::move(m_pendingClientVisuals);
        m_pendingClientVisuals.clear();
        for(auto& entry:pending) {
            auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,entry.ActorPath.c_str(),false);
            const auto definition=std::find_if(m_spawns.begin(),m_spawns.end(),[&](const SpawnInfo& spawn){
                return spawn.ModName==entry.ModName && spawn.EntryId==entry.EntryId;
            });
            if(object && object->IsA<AActor>() && definition!=m_spawns.end()) {
                auto* actor=static_cast<AActor*>(object);
                if(!actor->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))) {
                    ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
                    if(!authority.Result<bool>()) {
                        ApplyVisualEffect(actor,definition->VisualEffect,TEXT("Replicated spawn presentation"));
                        continue;
                    }
                }
            }
            if(++entry.Attempts<20)m_pendingClientVisuals.push_back(std::move(entry));
            else PS::Log<LogLevel::Warning>(STR("[DEGRADED][NETWORK:spawn-visual] Client could not resolve replicated actor '{}'; gameplay remains authoritative.\n"),entry.ActorPath);
        }
    }

    void DragonWildsSpawnLoader::CreateStaticAssembly(UWorld* world,
        SpawnInfo& spawn)
    {
        auto* parentClass = ActorHelper::ResolveClass(TEXT("/Script/Engine.StaticMeshActor"));
        auto* staticComponentClass = ActorHelper::ResolveClass(
            TEXT("/Script/Engine.StaticMeshComponent"));
        auto* hismClass = ActorHelper::ResolveClass(
            TEXT("/Script/Engine.HierarchicalInstancedStaticMeshComponent"));
        if (!parentClass || !staticComponentClass || !hismClass)
            throw std::runtime_error("static assembly engine classes are unavailable");

        struct Bucket { UObject* Mesh=nullptr; std::vector<FTransform> Instances; };
        std::unordered_map<UObject*, Bucket> buckets;
        std::size_t rejected = 0;
        for (const auto& piece : spawn.AssemblyPieces)
        {
            try
            {
                auto* sourceClass = ResolveBuildingActorClass(piece);
                if (!sourceClass || !ActorHelper::IsActorClass(sourceClass))
                    throw std::runtime_error("buildable actor class did not resolve");
                auto* defaults = sourceClass->GetClassDefaultObject().Get();
                if (!defaults || !defaults->IsA<AActor>())
                    throw std::runtime_error("buildable actor defaults are unavailable");

                FVector location{}, scale{1.0,1.0,1.0};
                FRotator rotation{};
                PS::JsonHelpers::ParseVector(piece, "Location", location);
                PS::JsonHelpers::ParseRotator(piece, "Rotation", rotation);
                PS::JsonHelpers::ParseVector(piece, "Scale", scale);
                if (scale.X() <= 0 || scale.Y() <= 0 || scale.Z() <= 0)
                    throw std::runtime_error("piece scale must be positive");
                const FTransform pieceTransform(rotation, location, scale);

                std::size_t meshes = 0;
                for (auto* component : static_cast<AActor*>(defaults)->GetComponentsByClass(
                    staticComponentClass))
                {
                    if (!component || !component->IsA(staticComponentClass)) continue;
                    auto* mesh = ActorHelper::GetObjectRef(component, TEXT("StaticMesh"));
                    if (!mesh) continue;
                    if (!ActorHelper::GetObjectRef(mesh, TEXT("BodySetup")))
                        throw std::runtime_error("cooked mesh has no authored BodySetup collision");
                    const auto componentTransform = AssemblyComponentTransform(component);
                    auto& bucket = buckets[mesh];
                    bucket.Mesh = mesh;
                    bucket.Instances.push_back(ComposeAssemblyTransforms(
                        componentTransform, pieceTransform));
                    ++meshes;
                }
                if (!meshes) throw std::runtime_error(
                    "buildable actor has no cooked StaticMeshComponent");
            }
            catch (const std::exception& error)
            {
                ++rejected;
                PS::Log<LogLevel::Warning>(
                    STR("[DEGRADED][BUILDING-ASSEMBLY] '{}', piece {} was skipped: {}\n"),
                    spawn.EntryId, piece.value("PieceId", int64_t{}),
                    PS::ToWideSafe(error.what()));
            }
        }
        if (buckets.empty())
            throw std::runtime_error("no assembly piece supplied a collision-ready cooked mesh");

        auto* parent = ActorHelper::SpawnActor(world, parentClass, spawn.Location,
            spawn.Rotation, [](AActor* actor) {
                actor->SetFlags(RF_Transient);
                ActorHelper::FunctionCall replicated(actor,
                    TEXT("/Script/Engine.Actor:SetReplicates"));
                replicated.Arg(TEXT("bInReplicates"), false).Invoke();
            }, ESpawnActorScaleMethod::OverrideRootScale);
        parent->SetActorScale3D(spawn.Scale);
        try
        {
            auto* root = ActorHelper::GetObjectRef(parent, TEXT("RootComponent"));
            if (!root) throw std::runtime_error("assembly parent root is unavailable");
            std::size_t instances = 0;
            for (auto& [mesh, bucket] : buckets)
            {
                ActorHelper::FunctionCall add(parent,
                    TEXT("/Script/Engine.Actor:AddComponentByClass"));
                add.Arg(TEXT("Class"), hismClass).Arg(TEXT("bManualAttachment"), true)
                    .Arg(TEXT("RelativeTransform"), FTransform{})
                    .Arg(TEXT("bDeferredFinish"), true).Invoke();
                auto* component = add.Result<UObject*>();
                if (!component || !component->IsA(hismClass)
                    || component->GetOuterPrivate() != parent)
                    throw std::runtime_error("owned HISM component creation failed");
                component->SetFlags(RF_Transient);

                ActorHelper::FunctionCall finish(parent,
                    TEXT("/Script/Engine.Actor:FinishAddComponent"));
                finish.Arg(TEXT("Component"), component)
                    .Arg(TEXT("bManualAttachment"), true)
                    .Arg(TEXT("RelativeTransform"), FTransform{}).Invoke();

                const uint8_t keepRelative = 0;
                ActorHelper::FunctionCall attach(component,
                    TEXT("/Script/Engine.SceneComponent:K2_AttachToComponent"));
                attach.Arg(TEXT("Parent"), root).Arg(TEXT("SocketName"), FName())
                    .Arg(TEXT("LocationRule"), keepRelative)
                    .Arg(TEXT("RotationRule"), keepRelative)
                    .Arg(TEXT("ScaleRule"), keepRelative)
                    .Arg(TEXT("bWeldSimulatedBodies"), false).Invoke();
                if (!attach.Result<bool>())
                    throw std::runtime_error("HISM component attachment failed");

                ActorHelper::FunctionCall setMesh(component,
                    TEXT("/Script/Engine.StaticMeshComponent:SetStaticMesh"));
                setMesh.Arg(TEXT("NewMesh"), mesh).Invoke();
                if (ActorHelper::GetObjectRef(component, TEXT("StaticMesh")) != mesh)
                    throw std::runtime_error("HISM static mesh assignment failed");

                ActorHelper::FunctionCall collision(component,
                    TEXT("/Script/Engine.PrimitiveComponent:SetCollisionProfileName"));
                collision.Arg(TEXT("InCollisionProfileName"),
                    FName(RC::to_generic_string(spawn.CollisionProfile), FNAME_Add))
                    .Arg(TEXT("bUpdateOverlaps"), true).Invoke();
                ActorHelper::FunctionCall enabled(component,
                    TEXT("/Script/Engine.PrimitiveComponent:SetCollisionEnabled"));
                enabled.Arg(TEXT("NewType"), static_cast<uint8_t>(3)).Invoke();
                ActorHelper::FunctionCall ticking(component,
                    TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled"));
                ticking.Arg(TEXT("bEnabled"), false).Invoke();
                ActorHelper::FunctionCall componentReplication(component,
                    TEXT("/Script/Engine.ActorComponent:SetIsReplicated"));
                componentReplication.Arg(TEXT("ShouldReplicate"), false).Invoke();

                for (const auto& transform : bucket.Instances)
                {
                    ActorHelper::FunctionCall instance(component,
                        TEXT("/Script/Engine.InstancedStaticMeshComponent:AddInstance"));
                    instance.Arg(TEXT("InstanceTransform"), transform)
                        .Arg(TEXT("bWorldSpace"), false).Invoke();
                    if (instance.Result<int32>() < 0)
                        throw std::runtime_error("HISM rejected an instance transform");
                    ++instances;
                }
            }
            spawn.LiveActor = PS::WeakObjectHandle(parent);
            spawn.bExistsInWorld = true;
            PS::Log<LogLevel::Normal>(
                STR("[BUILDING-ASSEMBLY] '{}' created one parent with {} HISM bucket(s), {} instance(s), and {} skipped piece(s).\n"),
                spawn.EntryId, buckets.size(), instances, rejected);
        }
        catch (...)
        {
            ActorHelper::DestroyActor(parent);
            throw;
        }
    }

    void DragonWildsSpawnLoader::CreateActor(UWorld* world, SpawnInfo& spawn)
    {
        auto* actorClass = ResolveClass(spawn.ClassPath);
        if (!actorClass)
        {
            throw std::runtime_error(std::format("Class '{}' is no longer available", RC::to_string(spawn.ClassPath)));
        }

        auto* actor = ActorHelper::SpawnActor(world, actorClass, spawn.Location, spawn.Rotation,
            [&](AActor* spawned) {
                if(spawn.Time!=TimeOfDay::Requirement::Any) {
                    spawned->SetFlags(RF_Transient);
                    if(auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(
                        spawned->GetClassPrivate(),TEXT("bSkipSpudStore"))))
                        skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(spawned),true);
                }
                SetGuidProperty(spawned, TEXT("SpudGuid"), spawn.StableId);
                ApplyBuildingData(spawned,spawn);
                ApplyEntryProperties(spawned, spawn.Properties);
            }, ESpawnActorScaleMethod::OverrideRootScale);
        actor->SetActorScale3D(spawn.Scale);
        ApplyActorDisplayName(actor, spawn.DisplayName);
        ApplyAIProperties(actor, nlohmann::json::object(),
            spawn.ComponentProperties);
        ApplyVisualEffect(actor, spawn.VisualEffect,
            STR("Spawn from '") + spawn.ModName + STR("'"));
        ApplyDropMultiplier(actor, spawn.DropMultiplier);
        ApplyAdditionalDrops(actor, spawn.AdditionalDrops);
        spawn.LiveActor=PS::WeakObjectHandle(actor);
        if(PublishWorldState && !spawn.VisualEffect.empty()) {
            const auto instance=RC::to_string(spawn.ModName)+":"+RC::to_string(spawn.EntryId);
            PublishWorldState(instance,nlohmann::json{{"kind","spawn"},{"mod",RC::to_string(spawn.ModName)},
                {"definition",RC::to_string(spawn.EntryId)},{"actor",RC::to_string(actor->GetPathName())},
                {"lifecycle","active"}}.dump());
        }

        if (spawn.bUseNativeRespawn)
        {
            LoadNativeRespawnState();
            if (m_placedNativeRespawnActors.insert(spawn.PersistentPlacementKey).second)
            {
                std::string error;
                if (!SaveNativeRespawnState(error))
                    PS::Log<LogLevel::Warning>(
                        STR("Could not record native-respawn placement '{}': {}\n"),
                        spawn.EntryId, PS::ToWideSafe(error.c_str()));
            }
        }

        spawn.bExistsInWorld = true;
        PS::Log<LogLevel::Verbose>(STR("Spawned actor '{}' ({}) at {} {} {}\n"), spawn.EntryId,
            actor->GetClassPrivate()->GetName(), spawn.Location.X(), spawn.Location.Y(), spawn.Location.Z());
    }

    void DragonWildsSpawnLoader::ReconcileTimedBuildingProps(float deltaSeconds)
    {
        if(!m_readyWorld || !GetGameMode(m_readyWorld))return;
        if(std::none_of(m_spawns.begin(),m_spawns.end(),[](const auto& spawn){
            return spawn.Type!=ESpawnEntryType::RemoveActor && (spawn.Time!=TimeOfDay::Requirement::Any || !spawn.QuestCompleted.empty());
        }))return;
        m_buildingTimeElapsed+=deltaSeconds;
        if(m_buildingTimeElapsed<1.0)return;
        m_buildingTimeElapsed=0;
        TimeOfDay::Requirement current=TimeOfDay::Requirement::Any;
        const bool needsTime=std::any_of(m_spawns.begin(),m_spawns.end(),[](const auto& spawn){return spawn.Time!=TimeOfDay::Requirement::Any;});
        if(needsTime)try {current=TimeOfDay::Current(m_readyWorld);} catch(...) {return;}
        for(auto& spawn:m_spawns) {
            if(spawn.Type==ESpawnEntryType::RemoveActor || (spawn.Time==TimeOfDay::Requirement::Any && spawn.QuestCompleted.empty()) || !spawn.bCellActivated)continue;
            auto* actor=spawn.LiveActor.Get();
            if(!actor)actor=FindActorByStableId(m_readyWorld,spawn.StableId);
            if(actor && (actor->GetWorld()!=m_readyWorld
                || actor->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))))actor=nullptr;
            if(actor && !spawn.QuestCompleted.empty() && spawn.bPersistAfterCondition)spawn.bConditionLatched=true;
            bool questAllowed=spawn.QuestCompleted.empty() || spawn.bConditionLatched;
            if(!questAllowed)try {questAllowed=IsQuestCompleted && IsQuestCompleted(m_readyWorld,spawn.QuestCompleted);} catch(...) {questAllowed=false;}
            if(questAllowed && !spawn.QuestCompleted.empty() && spawn.bPersistAfterCondition)spawn.bConditionLatched=true;
            const bool allowed=(spawn.Time==TimeOfDay::Requirement::Any || spawn.Time==current) && questAllowed;
            if(allowed!=spawn.bTimeAllowed) {
                spawn.bTimeAllowed=allowed;
                if(allowed)spawn.bDeconstructed=false;
                PS::Log<LogLevel::Verbose>(STR("Conditional spawn '{}' entering {} state.\n"),
                    spawn.EntryId,allowed?STR("active"):STR("inactive"));
            }
            if(!allowed) {
                try {
                    if(spawn.Type==ESpawnEntryType::AISpawnPoint)DestroyLiveSpawnedAI(m_readyWorld,spawn);
                    if(actor && actor->IsA<AActor>())RetireTimedActor(static_cast<AActor*>(actor));
                } catch(const std::exception& error) {
                    if(!spawn.bSpawnFailed)PS::Log<LogLevel::Error>(STR("Timed spawn '{}' cleanup failed safely: {}\n"),
                        spawn.EntryId,PS::ToWideSafe(error.what()));
                    spawn.bSpawnFailed=true;
                }
                spawn.LiveActor={};spawn.bExistsInWorld=false;
                continue;
            }
            if(!actor && spawn.Type==ESpawnEntryType::Actor && !spawn.QuestCompleted.empty()
                && HasEquivalentActorNear(m_readyWorld,spawn)) {
                spawn.bSatisfiedByExisting=true;spawn.bExistsInWorld=true;continue;
            }
            if(spawn.bSatisfiedByExisting) {
                if(spawn.Type==ESpawnEntryType::Actor && HasEquivalentActorNear(m_readyWorld,spawn))continue;
                spawn.bSatisfiedByExisting=false;spawn.bExistsInWorld=false;
            }
            if(actor) {spawn.LiveActor=PS::WeakObjectHandle(actor);spawn.bExistsInWorld=true;continue;}
            if(spawn.bBuildingProp && spawn.bExistsInWorld && spawn.bAllowDeconstruction) {
                spawn.bDeconstructed=true;
            }
            if(spawn.bDeconstructed)continue;
            spawn.bExistsInWorld=false;spawn.bSpawnFailed=false;
            try {
                if(spawn.Type==ESpawnEntryType::AISpawnPoint)ProcessAISpawnPointEntry(m_readyWorld,spawn);
                else ProcessActorEntry(m_readyWorld,spawn);
            }
            catch(const std::exception& error) {
                spawn.bSpawnFailed=true;
                PS::Log<LogLevel::Error>(STR("Timed spawn '{}' failed safely: {}\n"),
                    spawn.EntryId,PS::ToWideSafe(error.what()));
            }
        }
    }

    fs::path DragonWildsSpawnLoader::GetNativeRespawnStatePath()
    {
        return PS::HostServices::ExportsDirectory()
            / "native-respawn-placements.json";
    }

    void DragonWildsSpawnLoader::LoadNativeRespawnState()
    {
        if (m_nativeRespawnStateLoaded) return;
        m_nativeRespawnStateLoaded = true;
        const auto path = GetNativeRespawnStatePath();
        if (!fs::is_regular_file(path)) return;
        try
        {
            std::ifstream input(path, std::ios::binary);
            const auto document = nlohmann::json::parse(input, nullptr, true, true);
            if (!document.is_object() || document.value("SchemaVersion", 0) != 1
                || !document.contains("Placements")
                || !document.at("Placements").is_array())
                throw std::runtime_error("native respawn state has an invalid schema");
            for (const auto& value : document.at("Placements"))
                if (value.is_string() && !value.get_ref<const std::string&>().empty())
                    m_placedNativeRespawnActors.insert(value.get<std::string>());
        }
        catch (const std::exception& exception)
        {
            m_placedNativeRespawnActors.clear();
            PS::Log<LogLevel::Warning>(
                STR("Native-respawn placement state was ignored safely: {}\n"),
                PS::ToWideSafe(exception.what()));
        }
    }

    bool DragonWildsSpawnLoader::SaveNativeRespawnState(std::string& error)
    {
        const auto path = GetNativeRespawnStatePath();
        const auto temporary = fs::path(path.string() + ".tmp");
        try
        {
            std::vector<std::string> placements(
                m_placedNativeRespawnActors.begin(),
                m_placedNativeRespawnActors.end());
            std::sort(placements.begin(), placements.end());
            fs::create_directories(path.parent_path());
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) throw std::runtime_error("temporary state file could not be opened");
                output << nlohmann::json{{"SchemaVersion", 1},
                    {"Placements", placements}}.dump(2) << '\n';
                if (!output.good()) throw std::runtime_error("native respawn state write failed");
            }
            fs::copy_file(temporary, path, fs::copy_options::overwrite_existing);
            std::error_code ignored;
            fs::remove(temporary, ignored);
            return true;
        }
        catch (const std::exception& exception)
        {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            error = exception.what();
            return false;
        }
    }

}
