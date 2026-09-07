#include "Loader/Spawn/RuntimeSupport.h"
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
#include <sstream>
#include <vector>
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
#include "Loader/PlayerGhost.h"
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
    DragonWildsSpawnLoader::DragonWildsSpawnLoader() : DragonWildsModLoaderBase("spawns")
    {
        SetDisplayName(TEXT("Spawn Loader"));
    }

    DragonWildsSpawnLoader::~DragonWildsSpawnLoader()
    {
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
        if (m_playerPawnStateFunction && m_playerPawnStateCallbackId != 0)
        {
            m_playerPawnStateFunction->UnregisterHook(m_playerPawnStateCallbackId);
        }
        if (m_spawnTickCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_spawnTickCallbackId);
        }
        PlayerGhost::Clear();
        for (auto* material : m_rootedVisualEffectMaterials)
            if (material && material->IsRootSet()) material->ClearRootSet();
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
        return SetupWorldReadyHook() && SetupSpawnTick();
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
                RegisterSpawn(value, modName);
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
        PS::JsonHelpers::ValidateFieldExists(value, "Type");
        PS::JsonHelpers::ValidateFieldExists(value, "Location");

        SpawnInfo spawn{};
        spawn.ModName = modName;
        auto locationInput = value;
        const auto& authoredLocation = value.at("Location");
        if (!authoredLocation.is_object() || !authoredLocation.contains("Z"))
            throw std::runtime_error("Location must contain X, Y, and Z");
        if (authoredLocation.at("Z").is_string())
        {
            const auto shorthand = authoredLocation.at("Z").get<std::string>();
            if (shorthand.empty() || shorthand.front() != '$')
                throw std::runtime_error(
                    "Location.Z string must use $, $+offset, or $-offset");
            double offset = 0.0;
            if (shorthand.size() > 1)
            {
                std::size_t consumed = 0;
                try { offset = std::stod(shorthand.substr(1), &consumed); }
                catch (...) { throw std::runtime_error("Location.Z $ offset must be numeric"); }
                if (consumed != shorthand.size() - 1 || !std::isfinite(offset)
                    || offset < -100000.0 || offset > 100000.0)
                    throw std::runtime_error(
                        "Location.Z $ offset must be finite and between -100000 and 100000");
            }
            spawn.bGroundToSurface = true;
            spawn.bGroundingResolved = false;
            spawn.GroundZOffset = offset;
            locationInput["Location"]["Z"] = 0.0;
        }
        PS::JsonHelpers::ParseVector(locationInput, "Location", spawn.Location);
        spawn.AuthoredLocation = spawn.Location;

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
        if (PS::JsonHelpers::FieldExists(value, "VisualEffect"))
            spawn.VisualEffect = ValidateVisualEffect(value.at("VisualEffect"));
        std::string type;
        PS::JsonHelpers::ParseString(value, "Type", type);
        if (type == "AISpawnPoint")
        {
            RegisterAISpawnPoint(spawn, value);
        }
        else if (type == "Actor")
        {
            RegisterActor(spawn, value);
        }
        else if (type == "RemoveActor")
        {
            RegisterRemoveActor(spawn, value);
        }
        else
        {
            throw std::runtime_error("Type must be 'AISpawnPoint', 'Actor' or 'RemoveActor'");
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
            spawn.Location.X(), spawn.Location.Y(), spawn.Location.Z()));

        std::string aiClass;
        PS::JsonHelpers::ParseString(value, "AIClass", aiClass);
        spawn.AIClassPath = RC::to_generic_string(aiClass);
        spawn.Properties["AIClass"] = aiClass;

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

        copy("PowerLevel", "PowerLevel");
        copy("Mandatory", "bMandatorySpawn");
        copy("Respawn", "bShouldRespawn");
        copy("RespawnDuration", "RespawnDuration");
        copy("DespawnBehaviour", "DespawnBehaviour");
        copy("RequiresActivation", "bRequiresActivation");
        copy("IgnoreNavmeshRequirement", "bIgnoreNavmeshRequirement");
        copy("RoamGoalQueryType", "RoamGoalQueryTypeOverride");

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

        const bool hasMinDistance = PS::JsonHelpers::FieldExists(value, "MinSpawnDistance");
        const bool hasMaxDistance = PS::JsonHelpers::FieldExists(value, "MaxSpawnDistance");
        if (hasMinDistance || hasMaxDistance)
        {
            spawn.Properties["bOverrideSpawnRadius"] = true;
            if (hasMinDistance) copy("MinSpawnDistance", "MinDistanceForSpawnPointToSpawn");
            if (hasMaxDistance) copy("MaxSpawnDistance", "MaxDistanceForSpawnPointToSpawn");
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

        m_onLevelShownCallbackId = m_onLevelShownFunction->RegisterPostHook(
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

        m_aiScaleCallbackId = m_aiScaleFunction->RegisterPostHook(
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
            m_healthBarSetTextCallbackId = m_healthBarSetTextFunction->RegisterPostHook(
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
        m_playerPostLoginFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.GameModeBase:K2_PostLogin"));
        if (m_playerPostLoginFunction)
        {
            m_playerPostLoginCallbackId = m_playerPostLoginFunction->RegisterPostHook(
                [this](UnrealScriptFunctionCallableContext&, void*) {
                    ApplyPlayerRules();
                });
        }

        m_playerClientRestartFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.PlayerController:ClientRestart"));
        if (m_playerClientRestartFunction)
        {
            m_playerClientRestartCallbackId = m_playerClientRestartFunction->RegisterPostHook(
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    ApplyPlayerRules();
                    try
                    {
                        auto getPawn = ActorHelper::FunctionCall(context.Context,
                            STR("/Script/Engine.Controller:K2_GetPawn"));
                        getPawn.Invoke();
                        ApplyClientPlayerVisualRules(getPawn.Result<UObject*>());
                    }
                    catch (...) {}
                });
        }

        m_playerPawnStateFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.Pawn:OnRep_PlayerState"));
        if (m_playerPawnStateFunction)
        {
            m_playerPawnStateCallbackId = m_playerPawnStateFunction->RegisterPostHook(
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    ApplyClientPlayerVisualRules(context.Context);
                });
        }

        if (m_playerPostLoginCallbackId == 0 && m_playerClientRestartCallbackId == 0
            && m_playerPawnStateCallbackId == 0)
        {
            PS::Log<LogLevel::Error>(
                STR("Unable to register native player join hooks; /players will only apply during initial world setup.\n"));
            return;
        }

        PS::Log<LogLevel::Verbose>(
            STR("/players is event-driven (PostLogin={}, ClientRestart={}, "
                "OnRep_PlayerState={}); periodic player scanning is disabled.\n"),
            m_playerPostLoginCallbackId != 0, m_playerClientRestartCallbackId != 0,
            m_playerPawnStateCallbackId != 0);
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
            if (binding.Actor.Get() == character) return findById(binding.SpawnId);
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
                m_liveAIBindings.push_back({FWeakObjectPtr(character), exact->StableId});
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
            m_liveAIBindings.push_back({FWeakObjectPtr(character), nearest->StableId});
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

    void DragonWildsSpawnLoader::ApplyAILootRow(
        UObject* character, const std::string& lootRow)
    {
        if (!character || lootRow.empty()) return;
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
            return;
        }

        auto* handleProperty = CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                loot->GetClassPrivate(), TEXT("EnemyTableRowHandle")));
        auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
        auto* tableProperty = handleStruct ? CastField<FObjectProperty>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
        auto* rowProperty = handleStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
        if (!handleProperty || !tableProperty || !rowProperty)
        {
            if (m_lootRowWarningActors.insert(character).second)
                PS::Log<LogLevel::Warning>(
                    STR("AI EnemyTableRowHandle was unavailable on {}.\n"),
                    character->GetClassPrivate()->GetName());
            return;
        }

        auto* handle = handleProperty->ContainerPtrToValuePtr<void>(loot);
        if (auto* commonObject = ActorHelper::ResolveObject(TEXT(
                "/Game/Gameplay/Items/LootDropTables/DT_EnemyLootDropTable."
                "DT_EnemyLootDropTable")))
        {
            auto* commonTable = static_cast<UDataTable*>(commonObject);
            const FName requestedRow(RC::to_generic_string(lootRow), FNAME_Add);
            if (commonTable->FindRowUnchecked(requestedRow))
            {
                auto* tableAddress = tableProperty->ContainerPtrToValuePtr<void>(handle);
                UObject* tableValue = commonTable;
                std::memcpy(tableAddress, &tableValue, sizeof(tableValue));
            }
        }
        rowProperty->SetPropertyValue(
            rowProperty->ContainerPtrToValuePtr<void>(handle),
            FName(RC::to_generic_string(lootRow), FNAME_Add));
        if (m_lootRowConfiguredActors.insert(character).second)
            PS::Log<LogLevel::Verbose>(STR("Applied enemy loot row '{}' to {}.\n"),
                RC::to_generic_string(lootRow), character->GetClassPrivate()->GetName());
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
            const auto spawn = std::find_if(m_spawns.begin(), m_spawns.end(),
                [&](const SpawnInfo& entry) {
                    return entry.Type == ESpawnEntryType::AISpawnPoint
                        && std::memcmp(&entry.StableId, &binding.SpawnId,
                            sizeof(binding.SpawnId)) == 0;
                });
            if (spawn == m_spawns.end()
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
            const auto spawn = std::find_if(m_spawns.begin(), m_spawns.end(),
                [&](const SpawnInfo& entry) {
                    return entry.Type == ESpawnEntryType::AISpawnPoint
                        && std::memcmp(&entry.StableId, &binding.SpawnId,
                            sizeof(binding.SpawnId)) == 0;
                });
            if (spawn != m_spawns.end())
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

        m_spawnTickCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float deltaSeconds, bool) {
                PlayerGhost::Flush();
                RetryPendingAINames(deltaSeconds);
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
            if (!object || object->GetWorld() != world)
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

    void DragonWildsSpawnLoader::CleanupOrphanedActors(UWorld* world)
    {
        auto* actorClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.Actor"));
        if (!actorClass)
        {
            return;
        }

        TArray<UObject*> instances;
        UECustom::UObjectGlobals::GetObjectsOfClass(actorClass, instances, true, static_cast<EObjectFlags>(0));
        for (auto* object : instances)
        {
            if (!object || object->GetWorld() != world)
            {
                continue;
            }

            auto* guidProperty = PropertyHelper::GetPropertyByName(object->GetClassPrivate(), TEXT("SpudGuid"));
            if (!guidProperty || guidProperty->GetSize() != sizeof(FGuid))
            {
                continue;
            }

            const auto guid = ReadGuidProperty(object, guidProperty);
            if (!HasActorGuidMagic(guid))
            {
                continue;
            }

            const bool registered = std::any_of(m_spawns.begin(), m_spawns.end(), [&](const SpawnInfo& entry) {
                return entry.Type == ESpawnEntryType::Actor
                    && std::memcmp(&entry.StableId, &guid, sizeof(guid)) == 0;
            });
            if (registered)
            {
                continue;
            }

            ActorHelper::DestroyActor(static_cast<AActor*>(object));
            PS::Log<LogLevel::Verbose>(STR("Removed orphaned {} spawned by a removed RuneSchema entry.\n"),
                object->GetClassPrivate()->GetName());
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
            if (GetGameMode(world))
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
                    PlayerGhost::Clear();
                    m_visualEffectAppliedActors.clear();
                    m_sharedSpawnVisuals.clear();
                    for (auto* material : m_rootedVisualEffectMaterials)
                        if (material && material->IsRootSet()) material->ClearRootSet();
                    m_rootedVisualEffectMaterials.clear();
                    for (auto& spawn : m_spawns)
                    {
                        spawn.bExistsInWorld = false;
                        spawn.bSpawnFailed = false;
                        spawn.Location = spawn.AuthoredLocation;
                        spawn.bGroundingResolved = !spawn.bGroundToSurface;
                    }
                    m_readyWorld = world;
                    ApplyPlayerRules();

                    if (!m_spawns.empty())
                    {
                        PS::Log<LogLevel::Verbose>(STR("World ready ({}), processing {} entries.\n"), trigger, m_spawns.size());
                    }
                }

                const bool hasPending = std::any_of(m_spawns.begin(), m_spawns.end(), [](const SpawnInfo& spawn) {
                    return !spawn.bExistsInWorld && !spawn.bSpawnFailed;
                });
                if (hasPending && GetAIDirector(world))
                {
                    ProcessSpawns(world, bounds);
                }

                CleanupOrphanedActors(world);
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
            }
            else if (spawn.Type != ESpawnEntryType::AISpawnPoint)
            {
                continue;
            }

            try
            {
                ResolveGroundedLocation(world, spawn);
                switch (spawn.Type)
                {
                case ESpawnEntryType::AISpawnPoint:
                    ProcessAISpawnPointEntry(world, spawn);
                    break;
                case ESpawnEntryType::Actor:
                    ProcessActorEntry(world, spawn);
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
            impact.Z() + spawn.GroundZOffset);
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

    void DragonWildsSpawnLoader::CreateActor(UWorld* world, SpawnInfo& spawn)
    {
        auto* actorClass = ResolveClass(spawn.ClassPath);
        if (!actorClass)
        {
            throw std::runtime_error(std::format("Class '{}' is no longer available", RC::to_string(spawn.ClassPath)));
        }

        auto* actor = ActorHelper::SpawnActor(world, actorClass, spawn.Location, spawn.Rotation,
            [&](AActor* spawned) {
                SetGuidProperty(spawned, TEXT("SpudGuid"), spawn.StableId);
                ApplyEntryProperties(spawned, spawn.Properties);
            }, ESpawnActorScaleMethod::OverrideRootScale);
        actor->SetActorScale3D(spawn.Scale);
        ApplyActorDisplayName(actor, spawn.DisplayName);
        ApplyAIProperties(actor, nlohmann::json::object(),
            spawn.ComponentProperties);
        ApplyVisualEffect(actor, spawn.VisualEffect,
            STR("Spawn from '") + spawn.ModName + STR("'"));
        ApplyDropMultiplier(actor, spawn.DropMultiplier);

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

    fs::path DragonWildsSpawnLoader::GetNativeRespawnStatePath()
    {
        return fs::path(PS::HostServices::WorkingDirectory())
            / "Mods" / "RuneSchema" / "runtime" / "native-respawn-placements.json";
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
