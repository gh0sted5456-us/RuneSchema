#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
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

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    UObject* CallWorldContextGetter(const TCHAR* functionPath, const TCHAR* objectPath, UObject* worldContext)
    {
        auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, functionPath);
        auto* self = UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, objectPath);
        if (!function || !self)
        {
            throw std::runtime_error(std::format("{} was unavailable", RC::to_string(functionPath)));
        }

        std::vector<uint8> params(function->GetParmsSize(), 0);
        auto* contextProperty = function->FindProperty(FName(TEXT("WorldContextObject"), FNAME_Find));
        auto* returnProperty = function->GetReturnProperty();
        if (!contextProperty || !returnProperty
            || contextProperty->GetOffset_Internal() < 0
            || returnProperty->GetOffset_Internal() < 0
            || static_cast<size_t>(contextProperty->GetOffset_Internal()) + sizeof(worldContext) > params.size()
            || static_cast<size_t>(returnProperty->GetOffset_Internal()) + sizeof(UObject*) > params.size())
        {
            throw std::runtime_error(std::format("{} metadata was invalid", RC::to_string(functionPath)));
        }

        std::memcpy(params.data() + contextProperty->GetOffset_Internal(), &worldContext, sizeof(worldContext));
        self->ProcessEvent(function, params.data());
        return *reinterpret_cast<UObject**>(params.data() + returnProperty->GetOffset_Internal());
    }

    UObject* GetGameMode(UObject* worldContext)
    {
        return CallWorldContextGetter(TEXT("/Script/Engine.GameplayStatics:GetGameMode"),
            TEXT("/Script/Engine.Default__GameplayStatics"), worldContext);
    }

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

    void ApplyEntryProperties(AActor* actor, const nlohmann::json& properties)
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
        if (m_spawnTickCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_spawnTickCallbackId);
        }
    }

    void DragonWildsSpawnLoader::OnLoad(const fs::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::GameInstanceInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            LoadSpawns(data, modName);
        });

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
        PS::JsonHelpers::ParseVector(value, "Location", spawn.Location);

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

        PS::Log<LogLevel::Normal>(STR("Added {} entry for {} at {} {} {}\n"), RC::to_generic_string(type), modName,
            m_spawns.back().Location.X(), m_spawns.back().Location.Y(), m_spawns.back().Location.Z());
    }

    void DragonWildsSpawnLoader::RegisterAISpawnPoint(SpawnInfo& spawn, const nlohmann::json& value)
    {
        PS::JsonHelpers::ValidateFieldExists(value, "AIClass");
        spawn.Type = ESpawnEntryType::AISpawnPoint;
        spawn.ClassPath = TEXT("/Script/Dominion.AISpawnPoint");
        spawn.Properties = nlohmann::json::object();

        spawn.StableId = StableGuid(std::format("{}|{}|{:.17g}|{:.17g}|{:.17g}",
            RC::to_string(spawn.ModName), RC::to_string(spawn.ClassPath),
            spawn.Location.X(), spawn.Location.Y(), spawn.Location.Z()));

        std::string aiClass;
        PS::JsonHelpers::ParseString(value, "AIClass", aiClass);
        spawn.Properties["AIClass"] = aiClass;

        auto* aiClassObject = ResolveClass(RC::to_generic_string(aiClass));
        auto* aiBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        if (!aiClassObject || !aiBaseClass || !aiClassObject->IsChildOf(aiBaseClass))
        {
            throw std::runtime_error("AIClass must resolve to a DominionAICharacter class");
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
        copy("AmbientBehaviour", "AmbientBehaviour");
        copy("DespawnBehaviour", "DespawnBehaviour");
        copy("RequiresActivation", "bRequiresActivation");
        copy("IgnoreNavmeshRequirement", "bIgnoreNavmeshRequirement");
        copy("RoamMaxZTolerance", "RoamMaxZTolerance");
        copy("RoamGoalQueryType", "RoamGoalQueryTypeOverride");

        if (PS::JsonHelpers::FieldExists(value, "RoamRadius"))
        {
            spawn.Properties["bOverrideRoamBehaviour"] = true;
            spawn.Properties["RoamDistance"] = value.at("RoamRadius");
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

    void DragonWildsSpawnLoader::ApplyAIScale(UObject* character)
    {
        auto* aiBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionAICharacter"));
        if (!character || !aiBaseClass || !character->IsA(aiBaseClass))
        {
            return;
        }

        auto* spawnInfoProperty = PropertyHelper::GetPropertyByName<FStructProperty>(aiBaseClass, TEXT("SpawnInfo"));
        auto* spawnInfoStruct = spawnInfoProperty ? spawnInfoProperty->GetStruct().Get() : nullptr;
        auto* sourceIdProperty = spawnInfoStruct
            ? PropertyHelper::GetPropertyByName(spawnInfoStruct, TEXT("SpawnSourceId")) : nullptr;
        if (!spawnInfoProperty || !sourceIdProperty || sourceIdProperty->GetSize() != sizeof(FGuid))
        {
            return;
        }

        FGuid sourceId{};
        auto* data = reinterpret_cast<uint8*>(character)
            + spawnInfoProperty->GetOffset_Internal() + sourceIdProperty->GetOffset_Internal();
        std::memcpy(&sourceId, data, sizeof(sourceId));
        const auto spawn = std::find_if(m_spawns.begin(), m_spawns.end(), [&](const SpawnInfo& entry) {
            return entry.Type == ESpawnEntryType::AISpawnPoint
                && std::memcmp(&entry.StableId, &sourceId, sizeof(sourceId)) == 0;
        });
        if (spawn != m_spawns.end())
        {
            auto* actor = static_cast<AActor*>(character);
            actor->SetActorScale3D(spawn->Scale);
            ApplyDropMultiplier(actor, spawn->DropMultiplier);
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
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
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
                auto* actor = static_cast<AActor*>(object);
                actor->SetActorScale3D(spawn.Scale);
                ApplyDropMultiplier(actor, spawn.DropMultiplier);
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
                    for (auto& spawn : m_spawns)
                    {
                        spawn.bExistsInWorld = false;
                        spawn.bSpawnFailed = false;
                    }
                    m_readyWorld = world;

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
        PS::Log<LogLevel::Normal>(STR("RemoveActor for {}: removed {} runtime-spawned actor(s), left {} level-placed alone.\n"),
            spawn.ModName, removed, keptLevelPlaced);
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
            });
        actor->SetActorScale3D(spawn.Scale);

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
        ApplyDropMultiplier(actor, spawn.DropMultiplier);

        spawn.bExistsInWorld = true;
        PS::Log<LogLevel::Verbose>(STR("Spawned actor '{}' ({}) at {} {} {}\n"), spawn.EntryId,
            actor->GetClassPrivate()->GetName(), spawn.Location.X(), spawn.Location.Y(), spawn.Location.Z());
    }
}
