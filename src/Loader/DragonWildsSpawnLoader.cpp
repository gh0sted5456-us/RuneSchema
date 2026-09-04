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
#include "Loader/PlayerAttributeNames.h"
#include "UE4SSProgram.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    const std::unordered_map<std::string, std::pair<std::string, std::string>>& AppearanceFields()
    {
        static const std::unordered_map<std::string, std::pair<std::string, std::string>> fields{
            {"BodyType", {"BodyType", "/Game/Gameplay/Character/Player/Customization/DT_Customization_BodyType.DT_Customization_BodyType"}},
            {"FaceType", {"FaceType", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FaceType.DT_Customization_FaceType"}},
            {"Head", {"FaceType", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FaceType.DT_Customization_FaceType"}},
            {"HairPreset", {"HairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets"}},
            {"HairStyle", {"HairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets"}},
            {"FacialHairPreset", {"FacialHairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FacialHairPresets.DT_Customization_FacialHairPresets"}},
            {"BeardStyle", {"FacialHairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FacialHairPresets.DT_Customization_FacialHairPresets"}},
            {"SkinTone", {"SkinTone", "/Game/Gameplay/Character/Player/Customization/DT_Customization_SkinTone.DT_Customization_SkinTone"}},
            {"SkinColor", {"SkinTone", "/Game/Gameplay/Character/Player/Customization/DT_Customization_SkinTone.DT_Customization_SkinTone"}},
            {"HairColor", {"HairColor", "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairColor.DT_Customization_HairColor"}},
            {"EyeColor", {"EyeColor", "/Game/Gameplay/Character/Player/Customization/DT_Customization_EyeColor.DT_Customization_EyeColor"}},
            {"EyebrowColor", {"EyebrowColor", "/Game/Gameplay/Character/Player/Customization/DT_Customization_EyebrowColor.DT_Customization_EyebrowColor"}},
        };
        return fields;
    }

    const std::unordered_map<std::string, const TCHAR*>& AppearanceHandleFields()
    {
        static const std::unordered_map<std::string, const TCHAR*> fields{
            {"BodyType", TEXT("BodyTypeDataHandle")},
            {"FaceType", TEXT("FaceDataHandle")},
            {"HairPreset", TEXT("HairPresetDataHandle")},
            {"FacialHairPreset", TEXT("FacialHairPresetDataHandle")},
            {"HairColor", TEXT("HairColorPrimitiveDataHandle")},
            {"SkinTone", TEXT("SkinTonePrimitiveDataHandle")},
            {"EyeColor", TEXT("EyeColorPrimitiveDataHandle")},
            {"EyebrowColor", TEXT("EyebrowColorPrimitiveDataHandle")},
        };
        return fields;
    }

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
        if (PS::JsonHelpers::FieldExists(value, "DisplayName"))
        {
            PS::JsonHelpers::ParseString(value, "DisplayName", spawn.DisplayName);
            if (spawn.DisplayName.empty() || spawn.DisplayName.size() > 128)
            {
                throw std::runtime_error("DisplayName must contain between 1 and 128 characters");
            }
        }
        if (PS::JsonHelpers::FieldExists(value, "LootRow"))
        {
            PS::JsonHelpers::ParseString(value, "LootRow", spawn.LootRow);
            if (spawn.LootRow.empty() || spawn.LootRow.size() > 256)
                throw std::runtime_error("LootRow must contain between 1 and 256 characters");
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
            ApplyAIDisplayName(character, spawn->DisplayName);
            ApplyAILootRow(character, spawn->LootRow);
            ApplyAIProperties(character, spawn->CharacterProperties,
                spawn->ComponentProperties);
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
                PS::Log<LogLevel::Warning>(STR("AI component '{}' was unavailable on {}.\n"),
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
            PS::Log<LogLevel::Normal>(STR("Applied enemy loot row '{}' to {}.\n"),
                RC::to_generic_string(lootRow), character->GetClassPrivate()->GetName());
    }

    void DragonWildsSpawnLoader::ApplyAIDisplayName(
        UObject* character, const std::string& displayName)
    {
        if (!character || displayName.empty()) return;

        auto* property = PropertyHelper::GetPropertyByName(
            character->GetClassPrivate(), TEXT("AIName"));
        if (!property)
        {
            PS::Log<LogLevel::Warning>(
                STR("Custom spawn name '{}' could not be applied: AIName is unavailable on {}.\n"),
                PS::ToWideSafe(displayName.c_str()), character->GetClassPrivate()->GetName());
            return;
        }

        try
        {
            PropertyHelper::CopyJsonValueToContainer(character, property, displayName);
            PS::Log<LogLevel::Verbose>(STR("Applied custom spawn name '{}' to {}.\n"),
                PS::ToWideSafe(displayName.c_str()), character->GetClassPrivate()->GetName());
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(STR("Custom spawn name '{}' failed on {}: {}\n"),
                PS::ToWideSafe(displayName.c_str()), character->GetClassPrivate()->GetName(),
                PS::ToWideSafe(error.what()));
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
                m_playerRuleTickAccumulator += deltaSeconds;
                if (m_playerRuleTickAccumulator >= 1.0)
                {
                    m_playerRuleTickAccumulator = 0.0;
                    ApplyPlayerRules();
                }
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

    void DragonWildsSpawnLoader::LoadPlayerRules(
        const fs::path& loaderPath, const RC::StringType& modName, bool replaceExisting)
    {
        if (replaceExisting)
        {
            std::erase_if(m_playerRules, [&](const PlayerRule& rule) { return rule.ModName == modName; });
            m_reportedPlayerRuleFailures.clear();
            m_reportedPlayerRuleApplications.clear();
            m_reportedAppearanceNoOps.clear();
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            if (!data.is_array()) throw std::runtime_error("players JSON root must be an array");
            for (const auto& value : data)
            {
                if (!value.is_object()) throw std::runtime_error("each players entry must be an object");
                PlayerRule rule;
                rule.ModName = modName;
                const auto addPlayerSelector = [&](std::string name, const char* field) {
                    if (name == "*")
                    {
                        rule.AllPlayers = true;
                        return;
                    }
                    if (name.size() > 1 && name.front() == '*')
                    {
                        const auto digits = name.substr(1);
                        if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) {
                            return std::isdigit(c) != 0;
                        }))
                            throw std::runtime_error(std::string(field)
                                + " wildcard must be *, *1, *2, and so on");
                        const auto slot = std::stoull(digits);
                        if (slot == 0 || slot > 9999)
                            throw std::runtime_error(std::string(field)
                                + " numbered wildcard must be between *1 and *9999");
                        rule.PlayerLoadSlots.push_back(static_cast<std::size_t>(slot));
                        return;
                    }
                    if (!name.empty()) rule.PlayerNames.push_back(std::move(name));
                };
                if (value.contains("PlayerName"))
                {
                    if (!value.at("PlayerName").is_string())
                        throw std::runtime_error("PlayerName must be a string");
                    auto name = value.at("PlayerName").get<std::string>();
                    addPlayerSelector(std::move(name), "PlayerName");
                }
                if (value.contains("PlayerNames"))
                {
                    if (!value.at("PlayerNames").is_array())
                        throw std::runtime_error("PlayerNames must be an array of strings");
                    for (const auto& nameValue : value.at("PlayerNames"))
                    {
                        if (!nameValue.is_string())
                            throw std::runtime_error("PlayerNames must contain only strings");
                        auto name = nameValue.get<std::string>();
                        addPlayerSelector(std::move(name), "PlayerNames");
                    }
                }
                if (value.contains("PlayerGuid"))
                {
                    if (!value.at("PlayerGuid").is_string())
                        throw std::runtime_error("PlayerGuid must be a string");
                    auto guid = value.at("PlayerGuid").get<std::string>();
                    if (!guid.empty()) rule.PlayerGuids.push_back(std::move(guid));
                }
                if (value.contains("PlayerGuids"))
                {
                    if (!value.at("PlayerGuids").is_array())
                        throw std::runtime_error("PlayerGuids must be an array of strings");
                    for (const auto& guidValue : value.at("PlayerGuids"))
                    {
                        if (!guidValue.is_string())
                            throw std::runtime_error("PlayerGuids must contain only strings");
                        auto guid = guidValue.get<std::string>();
                        if (!guid.empty()) rule.PlayerGuids.push_back(std::move(guid));
                    }
                }
                if (!rule.AllPlayers && rule.PlayerNames.empty() && rule.PlayerGuids.empty()
                    && rule.PlayerLoadSlots.empty())
                    throw std::runtime_error(
                        "a players entry requires PlayerName, PlayerNames, PlayerGuid, or PlayerGuids");

                const auto parse = [&](const char* field, bool& specified, double& target,
                    double minimum, double maximum = 100.0) {
                    if (!value.contains(field)) return;
                    if (!value.at(field).is_number())
                        throw std::runtime_error(std::string(field) + " must be a number");
                    target = value.at(field).get<double>();
                    if (!std::isfinite(target) || target < minimum || target > maximum)
                        throw std::runtime_error(std::string(field) + " is out of range");
                    specified = true;
                };
                parse("Scale", rule.SetScale, rule.ScaleMultiplier, 0.25, 3.0);
                parse("HealthMultiplier", rule.SetHealth, rule.HealthMultiplier, 0.1);
                parse("MaxHealth", rule.SetMaxHealth, rule.MaxHealth, 1.0, 1000000.0);
                parse("BaseHealth", rule.SetMaxHealth, rule.MaxHealth, 1.0, 1000000.0);
                parse("DefenseMultiplier", rule.SetDefense, rule.DefenseMultiplier, 0.1);
                parse("DamageMultiplier", rule.SetDamage, rule.DamageMultiplier, 0.1);
                parse("StaminaMultiplier", rule.SetStamina, rule.StaminaMultiplier, 0.1);
                parse("MaxStamina", rule.SetMaxStamina, rule.MaxStamina, 1.0, 1000000.0);
                parse("WalkSpeedMultiplier", rule.SetWalkSpeed, rule.WalkSpeedMultiplier, 0.1, 10.0);
                parse("RunSpeedMultiplier", rule.SetRunSpeed, rule.RunSpeedMultiplier, 0.1, 10.0);
                parse("CarryWeightMultiplier", rule.SetCarryWeight,
                    rule.CarryWeightMultiplier, 0.1, 100.0);
                parse("MaxCarryWeight", rule.SetMaxCarryWeight,
                    rule.MaxCarryWeight, 1.0, 1000000.0);
                parse("PoisonResistanceMultiplier", rule.SetPoisonResistance,
                    rule.PoisonResistanceMultiplier, 0.0, 100.0);
                parse("StaminaRecoveryMultiplier", rule.SetStaminaRecovery,
                    rule.StaminaRecoveryMultiplier, 0.0, 100.0);
                parse("PhysicalAttackMultiplier", rule.SetPhysicalAttack,
                    rule.PhysicalAttackMultiplier, 0.0);
                parse("MagicalAttackMultiplier", rule.SetMagicalAttack,
                    rule.MagicalAttackMultiplier, 0.0);
                parse("MagicAttackMultiplier", rule.SetMagicalAttack,
                    rule.MagicalAttackMultiplier, 0.0);
                parse("RangedAttackMultiplier", rule.SetRangedAttack,
                    rule.RangedAttackMultiplier, 0.0);
                parse("RangeAttackMultiplier", rule.SetRangedAttack,
                    rule.RangedAttackMultiplier, 0.0);
                parse("PhysicalDefenseMultiplier", rule.SetPhysicalDefense,
                    rule.PhysicalDefenseMultiplier, 0.0);
                parse("MagicalDefenseMultiplier", rule.SetMagicalDefense,
                    rule.MagicalDefenseMultiplier, 0.0);
                parse("MagicDefenseMultiplier", rule.SetMagicalDefense,
                    rule.MagicalDefenseMultiplier, 0.0);
                parse("RangedDefenseMultiplier", rule.SetRangedDefense,
                    rule.RangedDefenseMultiplier, 0.0);
                parse("RangeDefenseMultiplier", rule.SetRangedDefense,
                    rule.RangedDefenseMultiplier, 0.0);
                if (value.contains("AttributeMultipliers"))
                {
                    const auto& attributes = value.at("AttributeMultipliers");
                    if (!attributes.is_object())
                        throw std::runtime_error("AttributeMultipliers must be an object");
                    for (const auto& [identifier, multiplierValue] : attributes.items())
                    {
                        if (identifier.empty() || !multiplierValue.is_number())
                            throw std::runtime_error(
                                "AttributeMultipliers requires non-empty names and numeric values");
                        const double multiplier = multiplierValue.get<double>();
                        if (!std::isfinite(multiplier) || multiplier < 0.0 || multiplier > 100.0)
                            throw std::runtime_error(
                                "AttributeMultipliers values must be between 0 and 100");
                        rule.AttributeMultipliers.push_back({identifier, multiplier});
                    }
                }
                if (value.contains("Attributes"))
                {
                    const auto& attributes = value.at("Attributes");
                    if (!attributes.is_object())
                        throw std::runtime_error("Attributes must be an object");
                    for (const auto& [identifier, editValue] : attributes.items())
                    {
                        if (identifier.empty() || !editValue.is_object())
                            throw std::runtime_error(
                                "Attributes requires non-empty names and operation objects");
                        const std::array<std::pair<const char*, EPlayerAttributeEditOperation>, 3>
                            operations{{
                                {"Set", EPlayerAttributeEditOperation::Set},
                                {"Add", EPlayerAttributeEditOperation::Add},
                                {"Multiply", EPlayerAttributeEditOperation::Multiply},
                            }};
                        const char* selectedName = nullptr;
                        EPlayerAttributeEditOperation selectedOperation{};
                        double selectedValue = 0.0;
                        for (const auto& [operationName, operation] : operations)
                        {
                            if (!editValue.contains(operationName)) continue;
                            if (selectedName)
                                throw std::runtime_error(
                                    "Attributes entries must contain exactly one of Set, Add, or Multiply");
                            if (!editValue.at(operationName).is_number())
                                throw std::runtime_error(
                                    std::string("Attributes ") + operationName + " must be a number");
                            selectedName = operationName;
                            selectedOperation = operation;
                            selectedValue = editValue.at(operationName).get<double>();
                        }
                        if (!selectedName)
                            throw std::runtime_error(
                                "Attributes entries require Set, Add, or Multiply");
                        if (!std::isfinite(selectedValue)
                            || (selectedOperation == EPlayerAttributeEditOperation::Multiply
                                && (selectedValue < 0.0 || selectedValue > 100.0))
                            || (selectedOperation != EPlayerAttributeEditOperation::Multiply
                                && (selectedValue < -1000000.0 || selectedValue > 1000000.0)))
                            throw std::runtime_error(
                                "Attributes operation value is out of range");
                        rule.Attributes.push_back(
                            {identifier, selectedOperation, selectedValue});
                    }
                }
                if (value.contains("Appearance"))
                {
                    const auto& appearance = value.at("Appearance");
                    if (!appearance.is_object())
                        throw std::runtime_error("Appearance must be an object");

                    for (const auto& [inputField, selectionValue] : appearance.items())
                    {
                        const auto known = AppearanceFields().find(inputField);
                        if (known == AppearanceFields().end())
                            throw std::runtime_error("unsupported Appearance field: " + inputField);
                        std::string tablePath = known->second.second;
                        std::string rowName;
                        std::string sourceName;
                        std::string fallbackTablePath;
                        std::string fallbackRowName;
                        bool hasFallback = false;
                        if (selectionValue.is_string())
                        {
                            rowName = selectionValue.get<std::string>();
                        }
                        else if (selectionValue.is_object())
                        {
                            const char* rowKey = selectionValue.contains("Name") ? "Name"
                                : selectionValue.contains("Row") ? "Row" : "RowName";
                            if (!selectionValue.contains(rowKey)
                                || !selectionValue.at(rowKey).is_string())
                                throw std::runtime_error(inputField + " requires a string Name, Row, or RowName");
                            rowName = selectionValue.at(rowKey).get<std::string>();
                            if (selectionValue.contains("Source"))
                            {
                                if (!selectionValue.at("Source").is_string())
                                    throw std::runtime_error(inputField + " Source must be a string");
                                sourceName = selectionValue.at("Source").get<std::string>();
                                const auto source = m_appearanceSources.find(sourceName);
                                if (source == m_appearanceSources.end())
                                    throw std::runtime_error(inputField + " references an unavailable or disabled appearance source: " + sourceName);
                                const auto table = source->second.Tables.find(known->second.first);
                                if (table == source->second.Tables.end())
                                    throw std::runtime_error("appearance source " + sourceName + " has no table for " + known->second.first);
                                tablePath = table->second;
                                const auto fallback = source->second.FallbackRows.find(known->second.first);
                                if (fallback != source->second.FallbackRows.end())
                                {
                                    fallbackTablePath = known->second.second;
                                    fallbackRowName = fallback->second;
                                    hasFallback = true;
                                }
                            }
                            if (selectionValue.contains("DataTable"))
                            {
                                if (!selectionValue.at("DataTable").is_string())
                                    throw std::runtime_error(inputField + " DataTable must be a string");
                                tablePath = selectionValue.at("DataTable").get<std::string>();
                            }
                            if (selectionValue.contains("Fallback"))
                            {
                                const auto& fallback = selectionValue.at("Fallback");
                                fallbackTablePath = known->second.second;
                                if (fallback.is_string()) fallbackRowName = fallback.get<std::string>();
                                else if (fallback.is_object())
                                {
                                    if (!fallback.contains("RowName") || !fallback.at("RowName").is_string())
                                        throw std::runtime_error(inputField + " Fallback requires a string RowName");
                                    fallbackRowName = fallback.at("RowName").get<std::string>();
                                    if (fallback.contains("DataTable"))
                                    {
                                        if (!fallback.at("DataTable").is_string())
                                            throw std::runtime_error(inputField + " Fallback DataTable must be a string");
                                        fallbackTablePath = fallback.at("DataTable").get<std::string>();
                                    }
                                }
                                else throw std::runtime_error(inputField + " Fallback must be a row string or object");
                                hasFallback = true;
                            }
                        }
                        else
                        {
                            throw std::runtime_error(inputField
                                + " must be a row-name string or a DataTable/RowName object");
                        }
                        if (rowName.empty() || tablePath.empty())
                            throw std::runtime_error(inputField + " has an empty table or row name");
                        if (!sourceName.empty() && !hasFallback)
                            throw std::runtime_error(inputField + " uses Source '" + sourceName
                                + "' but has no safe vanilla Fallback in the selection or source manifest");
                        std::erase_if(rule.Appearance, [&](const PlayerAppearanceSelection& existing) {
                            return existing.Field == known->second.first;
                        });
                        rule.Appearance.push_back({known->second.first, std::move(tablePath),
                            std::move(rowName), std::move(sourceName),
                            std::move(fallbackTablePath), std::move(fallbackRowName), hasFallback});
                    }
                }
                if (rule.SetHealth && rule.SetMaxHealth)
                    throw std::runtime_error("HealthMultiplier cannot be combined with MaxHealth or BaseHealth");
                if (rule.SetStamina && rule.SetMaxStamina)
                    throw std::runtime_error("StaminaMultiplier cannot be combined with MaxStamina");
                if (rule.SetCarryWeight && rule.SetMaxCarryWeight)
                    throw std::runtime_error(
                        "CarryWeightMultiplier cannot be combined with MaxCarryWeight");
                if (!rule.SetScale && !rule.SetHealth && !rule.SetMaxHealth && !rule.SetDefense
                    && !rule.SetDamage && !rule.SetStamina && !rule.SetMaxStamina
                    && !rule.SetWalkSpeed && !rule.SetRunSpeed
                    && !rule.SetCarryWeight && !rule.SetMaxCarryWeight
                    && !rule.SetPoisonResistance && !rule.SetStaminaRecovery
                    && !rule.SetPhysicalAttack && !rule.SetMagicalAttack
                    && !rule.SetRangedAttack && !rule.SetPhysicalDefense
                    && !rule.SetMagicalDefense && !rule.SetRangedDefense
                    && rule.AttributeMultipliers.empty() && rule.Attributes.empty()
                    && rule.Appearance.empty())
                    throw std::runtime_error("a players entry requires at least one adjustment field");
                m_playerRules.push_back(std::move(rule));
            }
        });
        PS::Log<LogLevel::Normal>(STR("Loaded persistent player rules for {} from /players.\n"), modName);
    }

    UObject* DragonWildsSpawnLoader::FindLocalPlayerController()
    {
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return nullptr;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->HasAnyFlags(static_cast<EObjectFlags>(
                RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            auto local = ActorHelper::FunctionCall(
                controller, STR("/Script/Engine.Controller:IsLocalController"));
            local.Invoke();
            if (local.Result<bool>()) return controller;
        }
        return nullptr;
    }

    UObject* DragonWildsSpawnLoader::FindPlayerControllerByName(
        const RC::StringType& playerName, bool& ambiguous)
    {
        ambiguous = false;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return nullptr;

        const auto equalIgnoringCase = [](const RC::StringType& left, const RC::StringType& right) {
            if (left.size() != right.size()) return false;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                if (std::towlower(left[index]) != std::towlower(right[index])) return false;
            }
            return true;
        };

        UObject* match = nullptr;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(
                    static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }

            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
            auto* playerState = statePointer ? statePointer->Get() : nullptr;
            if (!playerState) continue;

            auto getName = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
            getName.Invoke();
            const auto actualName = getName.Result<FString>();
            if (actualName.GetCharArray().Num() <= 1
                || !equalIgnoringCase(RC::StringType(*actualName), playerName))
            {
                continue;
            }
            if (match)
            {
                ambiguous = true;
                return nullptr;
            }
            match = controller;
        }
        return match;
    }

    std::string DragonWildsSpawnLoader::GetPlayerControllerName(UObject* controller)
    {
        if (!controller) return {};
        try
        {
            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
            auto* playerState = statePointer ? statePointer->Get() : nullptr;
            if (!playerState) return {};
            auto getName = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
            getName.Invoke();
            const auto actualName = getName.Result<FString>();
            return actualName.GetCharArray().Num() > 1
                ? RC::to_string(RC::StringType(*actualName)) : std::string{};
        }
        catch (...) { return {}; }
    }

    std::string DragonWildsSpawnLoader::GetPlayerCharacterGuid(UObject* controller)
    {
        if (!controller) return {};
        try
        {
            auto getGuid = ActorHelper::FunctionCall(
                controller, STR("/Script/Dominion.DominionPlayerControllerBase:GetCharacterGuid"));
            getGuid.Invoke();
            FGuid guid{};
            getGuid.MoveResult(&guid, sizeof(guid));
            uint32 lanes[4]{};
            std::memcpy(lanes, &guid, sizeof(lanes));
            if (lanes[0] == 0 && lanes[1] == 0 && lanes[2] == 0 && lanes[3] == 0) return {};
            return std::format("{:08X}{:08X}{:08X}{:08X}",
                lanes[0], lanes[1], lanes[2], lanes[3]);
        }
        catch (...) { return {}; }
    }

    UObject* DragonWildsSpawnLoader::FindPlayerControllerByGuid(
        const std::string& playerGuid, bool& ambiguous)
    {
        ambiguous = false;
        const auto normalize = [](std::string_view value) {
            std::string normalized;
            normalized.reserve(32);
            for (const unsigned char character : value)
            {
                if (std::isxdigit(character))
                    normalized.push_back(static_cast<char>(std::toupper(character)));
            }
            return normalized;
        };
        const auto expected = normalize(playerGuid);
        if (expected.size() != 32) return nullptr;

        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return nullptr;
        UObject* match = nullptr;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            if (normalize(GetPlayerCharacterGuid(controller)) != expected) continue;
            if (match)
            {
                ambiguous = true;
                return nullptr;
            }
            match = controller;
        }
        return match;
    }

    std::vector<std::string> DragonWildsSpawnLoader::GetConnectedPlayerNames()
    {
        std::vector<std::string> names;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return names;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
            auto* playerState = statePointer ? statePointer->Get() : nullptr;
            if (!playerState) continue;
            try
            {
                auto getName = ActorHelper::FunctionCall(
                    playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
                getName.Invoke();
                const auto actualName = getName.Result<FString>();
                if (actualName.GetCharArray().Num() > 1)
                    names.push_back(RC::to_string(RC::StringType(*actualName)));
            }
            catch (...) {}
        }
        return names;
    }

    std::vector<DragonWildsSpawnLoader::PlayerLoadOrderEntry>
    DragonWildsSpawnLoader::GetConnectedPlayersInLoadOrder()
    {
        std::vector<PlayerLoadOrderEntry> connected;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return connected;

        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            const auto name = GetPlayerControllerName(controller);
            const auto guid = GetPlayerCharacterGuid(controller);
            if (name.empty() && guid.empty()) continue;
            const auto key = !guid.empty() ? "guid:" + guid : "name:" + name;
            const auto known = std::find_if(m_playerLoadOrder.begin(),
                m_playerLoadOrder.end(), [&](const PlayerLoadOrderEntry& entry) {
                    return entry.Key == key;
                });
            if (known == m_playerLoadOrder.end())
                m_playerLoadOrder.push_back({key, name, guid});
            else
            {
                known->Name = name;
                if (!guid.empty()) known->Guid = guid;
            }
        }

        for (const auto& ordered : m_playerLoadOrder)
        {
            const auto active = std::find_if(controllers.begin(), controllers.end(),
                [&](UObject* controller) {
                    if (!controller || controller->GetWorld() != m_readyWorld) return false;
                    const auto guid = GetPlayerCharacterGuid(controller);
                    const auto name = GetPlayerControllerName(controller);
                    return (!ordered.Guid.empty() && ordered.Guid == guid)
                        || (ordered.Guid.empty() && ordered.Name == name);
                });
            if (active != controllers.end()) connected.push_back(ordered);
        }
        return connected;
    }


    fs::path DragonWildsSpawnLoader::GetAppearanceProvenancePath()
    {
        return fs::path(UE4SSProgram::get_program().get_working_directory())
            / "Mods" / "RuneSchema" / "player-data" / "appearance-fallbacks.json";
    }

    void DragonWildsSpawnLoader::LoadAppearanceProvenance()
    {
        if (m_appearanceProvenanceLoaded) return;
        m_appearanceProvenanceLoaded = true;
        const auto path = GetAppearanceProvenancePath();
        const auto temporary = fs::path(path.string() + ".tmp");
        std::error_code staleTemporaryError;
        if (fs::remove(temporary, staleTemporaryError))
        {
            PS::Log<LogLevel::Normal>(
                STR("Removed stale RuneSchema appearance-state temporary file.\n"));
        }
        if (!fs::is_regular_file(path)) return;
        try
        {
            std::ifstream input(path, std::ios::binary);
            const auto document = nlohmann::json::parse(input, nullptr, true, true);
            if (!document.is_object() || document.value("SchemaVersion", 0) != 1
                || !document.contains("Fields") || !document.at("Fields").is_array())
                throw std::runtime_error("appearance fallback file has an invalid schema");
            for (const auto& value : document.at("Fields"))
            {
                AppearanceProvenance record;
                record.PlayerGuid = value.at("PlayerGuid").get<std::string>();
                record.Field = value.at("Field").get<std::string>();
                record.OwnerMod = value.at("OwnerMod").get<std::string>();
                record.Source = value.value("Source", std::string{});
                record.AppliedDataTablePath = value.at("AppliedDataTable").get<std::string>();
                record.AppliedRowName = value.at("AppliedRowName").get<std::string>();
                record.FallbackDataTablePath = value.at("FallbackDataTable").get<std::string>();
                record.FallbackRowName = value.at("FallbackRowName").get<std::string>();
                if (record.PlayerGuid.empty() || record.Field.empty()
                    || record.FallbackDataTablePath.empty() || record.FallbackRowName.empty())
                    throw std::runtime_error("appearance fallback file contains an incomplete field");
                m_appearanceProvenance.push_back(std::move(record));
            }
        }
        catch (const std::exception& error)
        {
            m_appearanceProvenance.clear();
            PS::Log<LogLevel::Error>(
                STR("Appearance fallback state was ignored safely: {}\n"),
                PS::ToWideSafe(error.what()));
        }
    }

    bool DragonWildsSpawnLoader::SaveAppearanceProvenance(std::string& error)
    {
        const auto path = GetAppearanceProvenancePath();
        const auto temporary = fs::path(path.string() + ".tmp");
        try
        {
            nlohmann::json fields = nlohmann::json::array();
            for (const auto& record : m_appearanceProvenance)
            {
                fields.push_back({
                    {"PlayerGuid", record.PlayerGuid}, {"Field", record.Field},
                    {"OwnerMod", record.OwnerMod}, {"Source", record.Source},
                    {"AppliedDataTable", record.AppliedDataTablePath},
                    {"AppliedRowName", record.AppliedRowName},
                    {"FallbackDataTable", record.FallbackDataTablePath},
                    {"FallbackRowName", record.FallbackRowName},
                });
            }
            fs::create_directories(path.parent_path());
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) throw std::runtime_error("appearance fallback temporary file could not be opened");
                output << nlohmann::json{{"SchemaVersion", 1}, {"Fields", fields}}.dump(2) << '\n';
                if (!output.good()) throw std::runtime_error("appearance fallback write failed");
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

    bool DragonWildsSpawnLoader::ReadPlayerAppearance(
        UObject* pawn, const std::string& field, std::string& dataTablePath,
        std::string& rowName, UObject** customizationOut, std::string& error)
    {
        try
        {
            auto getCustomization = ActorHelper::FunctionCall(
                pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetPlayerCustomizationComponent"));
            getCustomization.Invoke();
            auto* customization = getCustomization.Result<UObject*>();
            if (customizationOut) *customizationOut = customization;
            auto* saveProperty = customization ? CastField<FStructProperty>(
                PropertyHelper::GetPropertyByName(
                    customization->GetClassPrivate(), TEXT("CustomizationSaveData"))) : nullptr;
            auto* saveStruct = saveProperty ? saveProperty->GetStruct().Get() : nullptr;
            auto* saveData = saveProperty
                ? saveProperty->ContainerPtrToValuePtr<void>(customization) : nullptr;
            const auto handleName = AppearanceHandleFields().find(field);
            auto* handleProperty = handleName != AppearanceHandleFields().end() && saveStruct
                ? CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
                    saveStruct, handleName->second)) : nullptr;
            auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
            auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
            auto* rowProperty = handleStruct ? CastField<FNameProperty>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
            if (!saveData || !handleProperty || !tableProperty || !rowProperty)
                throw std::runtime_error("customization field was unavailable");
            auto* handle = handleProperty->ContainerPtrToValuePtr<void>(saveData);
            UObject* table = nullptr;
            std::memcpy(&table, tableProperty->ContainerPtrToValuePtr<void>(handle), sizeof(table));
            const auto row = rowProperty->GetPropertyValue(
                rowProperty->ContainerPtrToValuePtr<void>(handle));
            if (!table || row == NAME_None)
                throw std::runtime_error("customization handle is empty");
            dataTablePath = RC::to_string(table->GetPathName());
            rowName = RC::to_string(row.ToString());
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        catch (...)
        {
            error = "unknown appearance read error";
            return false;
        }
    }

    bool DragonWildsSpawnLoader::WritePlayerAppearance(
        UObject* pawn, const std::string& field, const std::string& dataTablePath,
        const std::string& rowName, bool& changed, UObject** customizationOut,
        std::string& error)
    {
        changed = false;
        std::string currentTable;
        std::string currentRow;
        UObject* customization = nullptr;
        if (!ReadPlayerAppearance(pawn, field, currentTable, currentRow, &customization, error))
            return false;
        if (customizationOut) *customizationOut = customization;

        auto normalizedCurrent = ActorHelper::NormalizeObjectPath(RC::to_generic_string(currentTable));
        auto normalizedTarget = ActorHelper::NormalizeObjectPath(RC::to_generic_string(dataTablePath));
        if (normalizedCurrent == normalizedTarget && currentRow == rowName)
            return true; // Already native in the save: deliberately do not rewrite or replicate.

        try
        {
            auto* saveProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
                customization->GetClassPrivate(), TEXT("CustomizationSaveData")));
            auto* saveStruct = saveProperty ? saveProperty->GetStruct().Get() : nullptr;
            auto* saveData = saveProperty ? saveProperty->ContainerPtrToValuePtr<void>(customization) : nullptr;
            const auto handleName = AppearanceHandleFields().find(field);
            auto* handleProperty = handleName != AppearanceHandleFields().end() && saveStruct
                ? CastField<FStructProperty>(PropertyHelper::GetPropertyByName(saveStruct, handleName->second)) : nullptr;
            auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
            auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
            auto* rowProperty = handleStruct ? CastField<FNameProperty>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
            auto* resolved = ActorHelper::ResolveObject(normalizedTarget);
            auto* dataTableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Engine.DataTable"));
            auto* table = resolved && dataTableClass && resolved->IsA(dataTableClass)
                ? static_cast<UDataTable*>(resolved) : nullptr;
            const FName targetRow(RC::to_generic_string(rowName), FNAME_Add);
            if (!saveData || !handleProperty || !tableProperty || !rowProperty
                || !table || !table->FindRowUnchecked(targetRow))
                throw std::runtime_error("table or row was unavailable");
            auto* handle = handleProperty->ContainerPtrToValuePtr<void>(saveData);
            std::memcpy(tableProperty->ContainerPtrToValuePtr<void>(handle), &table, sizeof(table));
            rowProperty->SetPropertyValue(
                rowProperty->ContainerPtrToValuePtr<void>(handle), targetRow);
            changed = true;
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        catch (...)
        {
            error = "unknown appearance write error";
            return false;
        }
    }

    void DragonWildsSpawnLoader::ClearAppearanceSources()
    {
        m_appearanceSources.clear();
    }

    void DragonWildsSpawnLoader::RegisterAppearanceSource(
        const fs::path& modPath, const RC::StringType& modName)
    {
        const auto manifestPath = modPath / "appearance" / "manifest.json";
        if (!fs::is_regular_file(manifestPath)) return;

        std::ifstream input(manifestPath, std::ios::binary);
        if (!input) throw std::runtime_error("appearance manifest could not be opened");
        const auto document = nlohmann::json::parse(input, nullptr, true, true);
        if (!document.is_object())
            throw std::runtime_error("appearance/manifest.json must contain an object");
        if (document.value("SchemaVersion", 1) != 1)
            throw std::runtime_error("appearance manifest SchemaVersion must be 1");
        if (!document.contains("Tables") || !document.at("Tables").is_object())
            throw std::runtime_error("appearance manifest requires a Tables object");

        AppearanceSource source;
        for (const auto& [inputField, path] : document.at("Tables").items())
        {
            const auto known = AppearanceFields().find(inputField);
            if (known == AppearanceFields().end() || !path.is_string() || path.get<std::string>().empty())
                throw std::runtime_error("appearance manifest contains an invalid table mapping for " + inputField);
            source.Tables[known->second.first] = path.get<std::string>();
        }
        if (document.contains("Fallbacks"))
        {
            if (!document.at("Fallbacks").is_object())
                throw std::runtime_error("appearance manifest Fallbacks must be an object");
            for (const auto& [inputField, row] : document.at("Fallbacks").items())
            {
                const auto known = AppearanceFields().find(inputField);
                if (known == AppearanceFields().end() || !row.is_string() || row.get<std::string>().empty())
                    throw std::runtime_error("appearance manifest contains an invalid fallback for " + inputField);
                source.FallbackRows[known->second.first] = row.get<std::string>();
            }
        }
        m_appearanceSources[RC::to_string(modName)] = std::move(source);
        PS::Log<LogLevel::Normal>(
            STR("Registered appearance source '{}' from appearance/manifest.json.\n"), modName);
    }


    void DragonWildsSpawnLoader::ApplyPlayerRules()
    {
        if (!IsWorldStillLoaded(m_readyWorld)) return;
        LoadAppearanceProvenance();
        std::unordered_map<std::string, std::string> activeAppearanceOwners;
        const auto connected = GetConnectedPlayerNames();
        const auto connectedInLoadOrder = GetConnectedPlayersInLoadOrder();
        for (const auto& rule : m_playerRules)
        {
            struct PlayerRuleTarget {
                std::string Name;
                std::string Guid;
            };
            std::vector<PlayerRuleTarget> targets;
            if (rule.AllPlayers)
            {
                for (const auto& name : connected) targets.push_back({name, {}});
            }
            else if (!rule.PlayerGuids.empty())
            {
                // A stable character GUID is authoritative whenever supplied. Names
                // beside it are human-readable documentation, not an additional OR
                // selector that could accidentally affect a different character.
                for (const auto& guid : rule.PlayerGuids)
                {
                    bool ambiguous = false;
                    if (auto* controller = FindPlayerControllerByGuid(guid, ambiguous))
                    {
                        auto name = GetPlayerControllerName(controller);
                        if (std::none_of(targets.begin(), targets.end(), [&](const auto& target) {
                            return target.Guid == guid;
                        })) targets.push_back({std::move(name), guid});
                    }
                }
            }
            else if (!rule.PlayerLoadSlots.empty())
            {
                for (const auto slot : rule.PlayerLoadSlots)
                {
                    if (slot > connectedInLoadOrder.size())
                    {
                        const auto key = RC::to_string(rule.ModName) + "\n*"
                            + std::to_string(slot);
                        if (m_reportedPlayerRuleFailures.insert(key).second)
                            PS::Log<LogLevel::Warning>(
                                STR("Player rule from '{}' targets *{}, but that player has not loaded in this world yet.\n"),
                                rule.ModName, slot);
                        continue;
                    }
                    const auto& player = connectedInLoadOrder.at(slot - 1);
                    if (std::none_of(targets.begin(), targets.end(), [&](const auto& target) {
                        return (!player.Guid.empty() && target.Guid == player.Guid)
                            || (player.Guid.empty() && target.Name == player.Name);
                    })) targets.push_back({player.Name, player.Guid});
                }
            }
            else
            {
                for (const auto& name : rule.PlayerNames) targets.push_back({name, {}});
            }
            for (const auto& target : targets)
            {
                bool targetAmbiguous = false;
                auto* targetController = !target.Guid.empty()
                    ? FindPlayerControllerByGuid(target.Guid, targetAmbiguous)
                    : FindPlayerControllerByName(RC::to_generic_string(target.Name), targetAmbiguous);
                const auto actualGuid = targetController
                    ? GetPlayerCharacterGuid(targetController) : target.Guid;
                if (!actualGuid.empty())
                {
                    for (const auto& appearance : rule.Appearance)
                        activeAppearanceOwners[actualGuid + "\n" + appearance.Field]
                            = RC::to_string(rule.ModName);
                }
                std::string ignored;
                const bool applied = AdjustRuntimePlayerRule(
                    target.Name, rule, ignored, false, target.Guid);
                if (!applied)
                {
                    const auto label = target.Name.empty() ? target.Guid : target.Name;
                    const auto key = RC::to_string(rule.ModName) + "\n" + label
                        + "\n" + target.Guid + "\n" + ignored;
                    if (m_reportedPlayerRuleFailures.insert(key).second)
                    {
                        PS::Log<LogLevel::Error>(
                            STR("Player rule from '{}' for '{}' failed safely: {}\n"),
                            rule.ModName, PS::ToWideSafe(label.c_str()), PS::ToWideSafe(ignored.c_str()));
                    }
                }
                else
                {
                    const auto label = target.Name.empty() ? target.Guid : target.Name;
                    const auto auditGuid = actualGuid.empty() ? target.Guid : actualGuid;
                    const auto key = RC::to_string(rule.ModName) + "\n" + label
                        + "\n" + auditGuid + "\n" + ignored;
                    if (m_reportedPlayerRuleApplications.insert(key).second)
                    {
                        PS::Log<LogLevel::Normal>(
                            STR("Applied player rule from '{}' to '{}' (GUID {}): {}\n"),
                            rule.ModName, PS::ToWideSafe(label.c_str()),
                            PS::ToWideSafe(auditGuid.c_str()), PS::ToWideSafe(ignored.c_str()));
                    }
                }
            }
        }
        ReconcileAppearanceFallbacks(activeAppearanceOwners);
    }

    void DragonWildsSpawnLoader::ReconcileAppearanceFallbacks(
        const std::unordered_map<std::string, std::string>& activeOwners)
    {
        bool stateChanged = false;
        for (auto record = m_appearanceProvenance.begin();
            record != m_appearanceProvenance.end();)
        {
            const auto active = activeOwners.find(record->PlayerGuid + "\n" + record->Field);
            if (active != activeOwners.end() && active->second == record->OwnerMod)
            {
                ++record;
                continue;
            }

            bool ambiguous = false;
            auto* controller = FindPlayerControllerByGuid(record->PlayerGuid, ambiguous);
            if (!controller)
            {
                ++record; // Restore when that character is next connected.
                continue;
            }
            try
            {
                auto pawnCall = ActorHelper::FunctionCall(
                    controller, STR("/Script/Engine.Controller:K2_GetPawn"));
                pawnCall.Invoke();
                auto* pawn = pawnCall.Result<UObject*>();
                bool changed = false;
                UObject* customization = nullptr;
                std::string error;
                if (!WritePlayerAppearance(pawn, record->Field,
                    record->FallbackDataTablePath, record->FallbackRowName,
                    changed, &customization, error))
                {
                    PS::Log<LogLevel::Error>(
                        STR("Appearance fallback for player {} field {} failed safely: {}\n"),
                        PS::ToWideSafe(record->PlayerGuid.c_str()),
                        PS::ToWideSafe(record->Field.c_str()), PS::ToWideSafe(error.c_str()));
                    ++record;
                    continue;
                }
                if (changed && customization)
                {
                    auto refresh = ActorHelper::FunctionCall(customization,
                        STR("/Script/Dominion.PlayerCustomizationComponent:OnRep_PlayerCustomization"));
                    refresh.Invoke();
                }
                PS::Log<LogLevel::Normal>(
                    STR("Restored safe appearance fallback for player {} field {} after mod '{}' became inactive.\n"),
                    PS::ToWideSafe(record->PlayerGuid.c_str()),
                    PS::ToWideSafe(record->Field.c_str()), PS::ToWideSafe(record->OwnerMod.c_str()));
                record = m_appearanceProvenance.erase(record);
                stateChanged = true;
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("Appearance fallback failed safely: {}\n"), PS::ToWideSafe(error.what()));
                ++record;
            }
            catch (...)
            {
                ++record;
            }
        }
        if (stateChanged)
        {
            std::string error;
            if (!SaveAppearanceProvenance(error))
                PS::Log<LogLevel::Error>(STR("Could not save appearance fallback state: {}\n"),
                    PS::ToWideSafe(error.c_str()));
        }
    }


    bool DragonWildsSpawnLoader::AdjustRuntimePlayerRule(
        const std::string& targetPlayerNameUtf8,
        const PlayerRule& rule,
        std::string& result,
        bool requireRuntimeSpawning,
        const std::string& targetPlayerGuid)
    {
        try
        {
            const bool setScale = rule.SetScale;
            const double scaleMultiplier = rule.ScaleMultiplier;
            const bool setHealth = rule.SetHealth;
            const double healthMultiplier = rule.HealthMultiplier;
            const bool setMaxHealth = rule.SetMaxHealth;
            const double maxHealth = rule.MaxHealth;
            const bool setDefense = rule.SetDefense;
            const double defenseMultiplier = rule.DefenseMultiplier;
            const bool setDamage = rule.SetDamage;
            const double damageMultiplier = rule.DamageMultiplier;
            const bool setStamina = rule.SetStamina;
            const double staminaMultiplier = rule.StaminaMultiplier;
            const bool setMaxStamina = rule.SetMaxStamina;
            const double maxStamina = rule.MaxStamina;
            if (requireRuntimeSpawning)
            {
                result = "runtime player adjustment commands are not included in 0.6.2";
                return false;
            }
            if (!setScale && !setHealth && !setMaxHealth && !setDefense && !setDamage
                && !setStamina && !setMaxStamina
                && !rule.SetWalkSpeed && !rule.SetRunSpeed
                && !rule.SetCarryWeight && !rule.SetMaxCarryWeight
                && !rule.SetPoisonResistance && !rule.SetStaminaRecovery
                && !rule.SetPhysicalAttack && !rule.SetMagicalAttack
                && !rule.SetRangedAttack && !rule.SetPhysicalDefense
                && !rule.SetMagicalDefense && !rule.SetRangedDefense
                && rule.AttributeMultipliers.empty() && rule.Attributes.empty()
                && rule.Appearance.empty())
            {
                result = "no player adjustment was requested";
                return false;
            }
            if ((setScale && (!std::isfinite(scaleMultiplier) || scaleMultiplier < 0.25 || scaleMultiplier > 3.0))
                || (setHealth && (!std::isfinite(healthMultiplier) || healthMultiplier < 0.1 || healthMultiplier > 100.0))
                || (setMaxHealth && (!std::isfinite(maxHealth) || maxHealth < 1.0 || maxHealth > 1000000.0))
                || (setDefense && (!std::isfinite(defenseMultiplier) || defenseMultiplier < 0.1 || defenseMultiplier > 100.0))
                || (setDamage && (!std::isfinite(damageMultiplier) || damageMultiplier < 0.1 || damageMultiplier > 100.0))
                || (setStamina && (!std::isfinite(staminaMultiplier) || staminaMultiplier < 0.1 || staminaMultiplier > 100.0))
                || (setMaxStamina && (!std::isfinite(maxStamina)
                    || maxStamina < 1.0 || maxStamina > 1000000.0))
                || (rule.SetWalkSpeed && (!std::isfinite(rule.WalkSpeedMultiplier)
                    || rule.WalkSpeedMultiplier < 0.1 || rule.WalkSpeedMultiplier > 10.0))
                || (rule.SetRunSpeed && (!std::isfinite(rule.RunSpeedMultiplier)
                    || rule.RunSpeedMultiplier < 0.1 || rule.RunSpeedMultiplier > 10.0))
                || (rule.SetCarryWeight && (!std::isfinite(rule.CarryWeightMultiplier)
                    || rule.CarryWeightMultiplier < 0.1
                    || rule.CarryWeightMultiplier > 100.0))
                || (rule.SetMaxCarryWeight && (!std::isfinite(rule.MaxCarryWeight)
                    || rule.MaxCarryWeight < 1.0 || rule.MaxCarryWeight > 1000000.0)))
            {
                result = "scale must be 0.25-3, health/defense/damage/stamina multipliers must be 0.1-100, and absolute health/stamina must be 1-1000000";
                return false;
            }
            if (setHealth && setMaxHealth)
            {
                result = "health multiplier and absolute max health cannot be combined";
                return false;
            }
            if (setStamina && setMaxStamina)
            {
                result = "stamina multiplier and absolute max stamina cannot be combined";
                return false;
            }
            if (rule.SetCarryWeight && rule.SetMaxCarryWeight)
            {
                result = "carry-weight multiplier and absolute max carry weight cannot be combined";
                return false;
            }
            if (!IsWorldStillLoaded(m_readyWorld) || !GetGameMode(m_readyWorld))
            {
                result = "an authoritative game world is not ready";
                return false;
            }

            bool ambiguous = false;
            auto* controller = !targetPlayerGuid.empty()
                ? FindPlayerControllerByGuid(targetPlayerGuid, ambiguous)
                : targetPlayerNameUtf8.empty()
                ? FindLocalPlayerController()
                : FindPlayerControllerByName(RC::to_generic_string(targetPlayerNameUtf8), ambiguous);
            if (!controller)
            {
                result = ambiguous ? "player name matched more than one connected player"
                    : targetPlayerNameUtf8.empty() && targetPlayerGuid.empty()
                    ? "no local player exists; a headless server must specify 'player <name>'"
                    : !targetPlayerGuid.empty()
                    ? "no connected player matched that character GUID"
                    : "no connected player matched that name";
                return false;
            }
            const auto resolvedPlayerGuid = GetPlayerCharacterGuid(controller);

            auto pawnCall = ActorHelper::FunctionCall(
                controller, STR("/Script/Engine.Controller:K2_GetPawn"));
            pawnCall.Invoke();
            auto* pawn = pawnCall.Result<UObject*>();
            if (!pawn || pawn->GetWorld() != m_readyWorld)
            {
                result = "the selected player has no active pawn";
                return false;
            }

            auto state = std::find_if(m_playerAdjustments.begin(), m_playerAdjustments.end(),
                [&](const PlayerAdjustmentState& value) { return value.Pawn == pawn; });
            if (state == m_playerAdjustments.end())
            {
                PlayerAdjustmentState initial;
                initial.Pawn = pawn;
                m_playerAdjustments.push_back(std::move(initial));
                state = std::prev(m_playerAdjustments.end());
            }

            UObject* playerAttributes = nullptr;
            try
            {
                auto getAttributes = ActorHelper::FunctionCall(
                    pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetPlayerAttributesComponent"));
                getAttributes.Invoke();
                playerAttributes = getAttributes.Result<UObject*>();
            }
            catch (...)
            {
                auto* address = PropertyHelper::GetValuePtrByPropertyNameInChain<
                    TObjectPtr<UObject>>(pawn, TEXT("AttributesComponent"));
                playerAttributes = address ? address->Get() : nullptr;
            }

            const auto captureAttribute = [&](PlayerAttributeBaseline& baseline,
                UObject* attributes,
                const std::function<bool(const std::string&)>& matches) {
                if (baseline.Valid || !attributes) return;
                const auto captureFromPair = [&](const TCHAR* attributesName,
                    const TCHAR* valuesName) {
                    auto* attributesProperty = CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), attributesName));
                    auto* valuesProperty = CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), valuesName));
                    auto* valueInner = valuesProperty
                        ? CastField<FNumericProperty>(valuesProperty->GetInner()) : nullptr;
                    auto* attributeArray = attributesProperty
                        ? attributesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    auto* valueArray = valuesProperty
                        ? valuesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    if (!attributesProperty || !valuesProperty || !valueInner
                        || !valueInner->IsFloatingPoint() || !attributeArray || !valueArray
                        || attributeArray->Num() != valueArray->Num()) return;
                    const int32 attributeSize = attributesProperty->GetInner()->GetElementSize();
                    const int32 valueSize = valueInner->GetElementSize();
                    auto* attributeData = static_cast<uint8*>(attributeArray->GetData());
                    auto* valueData = static_cast<uint8*>(valueArray->GetData());
                    for (int32 index = 0; index < attributeArray->Num(); ++index)
                    {
                        UObject* attribute = nullptr;
                        std::memcpy(&attribute,
                            attributeData + index * attributeSize, sizeof(attribute));
                        if (!attribute) continue;
                        std::array<std::string, 2> names{
                            RC::to_string(attribute->GetName()),
                            attribute->GetClassPrivate()
                                ? RC::to_string(attribute->GetClassPrivate()->GetName())
                                : std::string{},
                        };
                        const bool matched = std::any_of(names.begin(), names.end(),
                            [&](std::string name) {
                                std::transform(name.begin(), name.end(), name.begin(),
                                    [](unsigned char character) {
                                        return static_cast<char>(std::tolower(character));
                                    });
                                return matches(name);
                            });
                        if (!matched) continue;
                        baseline.Attributes = attributes;
                        baseline.ValuesProperty = valuesProperty;
                        baseline.Attribute = attribute;
                        baseline.Index = index;
                        baseline.Value = valueInner->GetFloatingPointPropertyValue(
                            valueData + index * valueSize);
                        baseline.Valid = std::isfinite(baseline.Value);
                        return;
                    }
                };

                captureFromPair(TEXT("FloatAttributes"), TEXT("AttributeValues"));
                if (!baseline.Valid)
                    captureFromPair(TEXT("SharedFloatAttributes"),
                        TEXT("SharedAttributeValues"));
            };
            const auto applyAttributeValue = [&](PlayerAttributeBaseline& baseline, double value) {
                if (!baseline.Valid || !baseline.Attributes || !baseline.ValuesProperty
                    || !baseline.Attribute || baseline.Index < 0 || !std::isfinite(value))
                    return false;
                auto* numeric = CastField<FNumericProperty>(baseline.ValuesProperty->GetInner());
                auto* values = baseline.ValuesProperty->ContainerPtrToValuePtr<FScriptArray>(
                    baseline.Attributes);
                if (!numeric || !numeric->IsFloatingPoint() || !values
                    || !values->IsValidIndex(baseline.Index)) return false;
                auto* address = static_cast<uint8*>(values->GetData())
                    + baseline.Index * numeric->GetElementSize();
                numeric->SetFloatingPointPropertyValue(address, value);
                auto changed = ActorHelper::FunctionCall(
                    baseline.Attributes,
                    STR("/Script/Dominion.DominionAttributesComponent:OnAttributeChanged"));
                changed.Arg(STR("Attribute"), baseline.Attribute).Invoke();
                return true;
            };
            const auto applyAttribute = [&](PlayerAttributeBaseline& baseline, double multiplier) {
                return applyAttributeValue(baseline, baseline.Value * multiplier);
            };

            const auto normalizeAttributeIdentifier = [](std::string identifier) {
                return NormalizePlayerAttributeIdentifier(std::move(identifier));
            };

            bool scaleApplied = !setScale;
            bool healthApplied = !setHealth && !setMaxHealth;
            bool defenseApplied = !setDefense;
            bool damageApplied = !setDamage;
            bool staminaApplied = !setStamina && !setMaxStamina;
            bool walkSpeedApplied = !rule.SetWalkSpeed;
            bool runSpeedApplied = !rule.SetRunSpeed;
            bool carryWeightApplied = !rule.SetCarryWeight && !rule.SetMaxCarryWeight;
            bool poisonResistanceApplied = !rule.SetPoisonResistance;
            bool staminaRecoveryApplied = !rule.SetStaminaRecovery;
            bool physicalAttackApplied = !rule.SetPhysicalAttack;
            bool magicalAttackApplied = !rule.SetMagicalAttack;
            bool rangedAttackApplied = !rule.SetRangedAttack;
            bool physicalDefenseApplied = !rule.SetPhysicalDefense;
            bool magicalDefenseApplied = !rule.SetMagicalDefense;
            bool rangedDefenseApplied = !rule.SetRangedDefense;
            bool namedAttributesApplied = true;
            std::vector<std::string> unsupportedNamedAttributes;
            bool appearanceApplied = true;
            std::vector<std::string> unsupportedAppearance;

            if (setScale)
            {
                static_cast<AActor*>(pawn)->SetActorScale3D(FVector(
                    state->BaseScale.X() * scaleMultiplier,
                    state->BaseScale.Y() * scaleMultiplier,
                    state->BaseScale.Z() * scaleMultiplier));
                scaleApplied = true;
            }

            if (setHealth || setMaxHealth)
            {
                UObject* health = nullptr;
                std::vector<UObject*> healthCandidates;
                const auto addHealthCandidate = [&](UObject* candidate) {
                    if (candidate && std::find(healthCandidates.begin(),
                        healthCandidates.end(), candidate) == healthCandidates.end())
                        healthCandidates.push_back(candidate);
                };
                for (const auto* propertyName : {
                    TEXT("HealthComponent"), TEXT("BP_Components_Health")})
                {
                    auto* address = PropertyHelper::GetValuePtrByPropertyNameInChain<
                        TObjectPtr<UObject>>(pawn, propertyName);
                    addHealthCandidate(address ? address->Get() : nullptr);
                }
                try
                {
                    auto getPlayerHealth = ActorHelper::FunctionCall(
                        pawn,
                        STR("/Script/Dominion.DominionPlayerCharacter:GetHealthComponent"));
                    getPlayerHealth.Invoke();
                    addHealthCandidate(getPlayerHealth.Result<UObject*>());
                }
                catch (...)
                {
                }
                try
                {
                    auto getComponent = ActorHelper::FunctionCall(
                        pawn, STR("/Script/Dominion.HealthInterface:GetBaseHealthComponent"));
                    getComponent.Invoke();
                    addHealthCandidate(getComponent.Result<UObject*>());
                }
                catch (...)
                {
                }
                // Several player Blueprints expose both a template health
                // component and the authoritative BP_Components_Health
                // instance. Select the first candidate whose reflected API
                // reports a real positive maximum instead of trusting the
                // first non-null pointer.
                for (auto* candidate : healthCandidates)
                {
                    try
                    {
                        auto getMax = ActorHelper::FunctionCall(
                            candidate, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                        getMax.Invoke();
                        const double candidateMaximum = getMax.NumericResult();
                        if (!std::isfinite(candidateMaximum) || candidateMaximum <= 0.0)
                            continue;
                        health = candidate;
                        if (!state->HasBaseHealth)
                        {
                            state->BaseMaxHealth = candidateMaximum;
                            state->HasBaseHealth = true;
                        }
                        break;
                    }
                    catch (...)
                    {
                    }
                }
                if (!health)
                    PS::Log<LogLevel::Warning>(
                        STR("No authoritative positive player health component was available.\n"));
                // Health's private attribute component can terminate the game if its
                // change delegate is invoked directly. Only use the player's public
                // attribute pool here and fall back to HealthComponent's reflected API.
                captureAttribute(state->HealthAttribute, playerAttributes,
                    [](const std::string& name) {
                        return name == "da_attribute_maxhealth"
                            || name == "maxhealthattribute"
                            || (name.find("health") != std::string::npos
                                && name.find("max") != std::string::npos);
                    });
                const double desired = setMaxHealth ? maxHealth
                    : state->HealthAttribute.Valid
                    ? state->HealthAttribute.Value * healthMultiplier
                    : state->HasBaseHealth ? state->BaseMaxHealth * healthMultiplier : 0.0;
                if (health)
                {
                    if (state->HasBaseHealth)
                    {
                        const double desiredMaximum = desired;
                        auto verifyMax = ActorHelper::FunctionCall(
                            health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                        verifyMax.Invoke();
                        double currentMaximum = verifyMax.NumericResult();
                        healthApplied = std::isfinite(currentMaximum)
                            && std::abs(currentMaximum - desiredMaximum) < 0.01;
                        if (!healthApplied && std::isfinite(currentMaximum) && currentMaximum > 0.0)
                        {
                            // The live Dominion signature is
                            // ModifyMaxHealth(float NewMaxHealth): pass the requested
                            // absolute maximum, despite the backing gameplay effect's
                            // AmountToModify field name.
                            auto modifyMax = ActorHelper::FunctionCall(
                                health, STR("/Script/Dominion.HealthComponent:ModifyMaxHealth"));
                            modifyMax.FirstNumericArg(desiredMaximum).Invoke();
                            auto verifyFallback = ActorHelper::FunctionCall(
                                health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                            verifyFallback.Invoke();
                            currentMaximum = verifyFallback.NumericResult();
                            healthApplied = std::isfinite(currentMaximum)
                                && std::abs(currentMaximum - desiredMaximum) < 0.01;
                            if (!healthApplied && state->HealthAttribute.Valid
                                && state->HealthAttribute.Value > 0.0)
                            {
                                // DA_Attribute_MaxHealth owns the authoritative
                                // MaxHealthAttribute slot in the player's public
                                // DominionAttributesComponent. Health.CurrentValue
                                // in the character save only seeds current health;
                                // the engine rebuilds this maximum from the live
                                // attribute pool. Notify the normal attribute-change
                                // path after updating that slot so HealthComponent,
                                // replication, and the HUD can refresh naturally.
                                const double absoluteMultiplier =
                                    desiredMaximum / state->HealthAttribute.Value;
                                if (std::isfinite(absoluteMultiplier)
                                    && applyAttribute(state->HealthAttribute,
                                        absoluteMultiplier))
                                {
                                    auto verifyAttribute = ActorHelper::FunctionCall(
                                        health,
                                        STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                                    verifyAttribute.Invoke();
                                    currentMaximum = verifyAttribute.NumericResult();
                                    healthApplied = std::isfinite(currentMaximum)
                                        && std::abs(currentMaximum - desiredMaximum) < 0.01;
                                }
                            }
                            if (!healthApplied)
                            {
                                // Max health is attribute-derived. Apply the same
                                // GE_ModifyMaxHealth data object used by the game's
                                // permanent-health reward path. AddGameplayEffect is
                                // the native one-shot wrapper around Instantiate+Apply,
                                // so no weak gameplay-effect handle crosses this code.
                                try
                                {
                                    auto* effectClass = ActorHelper::ResolveClass(STR(
                                        "/Game/Gameplay/GameplayEffects/Effects/GE_ModifyMaxHealth.GE_ModifyMaxHealth_C"));
                                    auto effectObject = effectClass
                                        ? effectClass->GetClassDefaultObject() : TObjectPtr<UObject>{};
                                    auto* effect = effectObject.Get();
                                    auto* dataProperty = effect ? CastField<FStructProperty>(
                                        PropertyHelper::GetPropertyByName(
                                            effect->GetClassPrivate(), TEXT("Data"))) : nullptr;
                                    auto* dataStruct = dataProperty ? dataProperty->GetStruct().Get() : nullptr;
                                    auto* amountProperty = dataStruct ? CastField<FStructProperty>(
                                        PropertyHelper::GetPropertyByName(
                                            dataStruct, TEXT("AmountToModify"))) : nullptr;
                                    auto* amountStruct = amountProperty
                                        ? amountProperty->GetStruct().Get() : nullptr;
                                    auto* valueProperty = amountStruct ? CastField<FNumericProperty>(
                                        PropertyHelper::GetPropertyByName(
                                            amountStruct, TEXT("Value"))) : nullptr;
                                    if (!effect || !dataProperty || !amountProperty
                                        || !valueProperty || !valueProperty->IsFloatingPoint())
                                        throw std::runtime_error(
                                            "GE_ModifyMaxHealth magnitude was unavailable");
                                    auto* data = dataProperty->ContainerPtrToValuePtr<void>(effect);
                                    auto* amount = amountProperty->ContainerPtrToValuePtr<void>(data);
                                    auto* magnitude = valueProperty->ContainerPtrToValuePtr<void>(amount);
                                    const double originalMagnitude =
                                        valueProperty->GetFloatingPointPropertyValue(magnitude);
                                    const double delta = desiredMaximum - currentMaximum;
                                    valueProperty->SetFloatingPointPropertyValue(magnitude, delta);
                                    try
                                    {
                                        auto applyEffect = ActorHelper::FunctionCall(pawn,
                                            STR("/Script/Dominion.DominionPlayerCharacter:AddGameplayEffect"));
                                        applyEffect.Arg(STR("GameplayEffectData"), effect)
                                            .Arg(STR("EffectInstigator"), static_cast<AActor*>(pawn))
                                            .Arg(STR("EffectSource"), pawn)
                                            .Invoke();
                                    }
                                    catch (...)
                                    {
                                        valueProperty->SetFloatingPointPropertyValue(
                                            magnitude, originalMagnitude);
                                        throw;
                                    }
                                    valueProperty->SetFloatingPointPropertyValue(
                                        magnitude, originalMagnitude);
                                    auto verifyEffect = ActorHelper::FunctionCall(
                                        health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                                    verifyEffect.Invoke();
                                    currentMaximum = verifyEffect.NumericResult();
                                    healthApplied = std::isfinite(currentMaximum)
                                        && std::abs(currentMaximum - desiredMaximum) < 0.01;
                                    if (!healthApplied)
                                    {
                                        // The high-level player wrapper may decline an
                                        // effect during early pawn initialization. C++
                                        // can safely preserve Dominion's 24-byte handle,
                                        // so retry through the component's native
                                        // Instantiate+Apply path without Lua's weak-
                                        // object marshaling limitation.
                                        UObject* gameplayEffects = nullptr;
                                        try
                                        {
                                            auto getEffects = ActorHelper::FunctionCall(pawn,
                                                STR("/Script/Dominion.DominionPlayerCharacter:GetPlayerGameplayEffectsComponent"));
                                            getEffects.Invoke();
                                            gameplayEffects = getEffects.Result<UObject*>();
                                        }
                                        catch (...)
                                        {
                                            auto* address = PropertyHelper::GetValuePtrByPropertyNameInChain<
                                                TObjectPtr<UObject>>(pawn, TEXT("GameplayEffectsComponent"));
                                            gameplayEffects = address ? address->Get() : nullptr;
                                        }
                                        if (!gameplayEffects)
                                            throw std::runtime_error(
                                                "player gameplay-effects component was unavailable");

                                        valueProperty->SetFloatingPointPropertyValue(magnitude, delta);
                                        std::array<uint8_t, 24> effectHandle{};
                                        try
                                        {
                                            auto instantiate = ActorHelper::FunctionCall(gameplayEffects,
                                                STR("/Script/Dominion.DominionGameplayEffectsComponent:InstantiateGameplayEffect"));
                                            instantiate.Arg(STR("DataClass"), effectClass)
                                                .Arg(STR("InstigatingGE"), static_cast<UObject*>(nullptr))
                                                .Arg(STR("bForceNewInstance"), true)
                                                .Invoke();
                                            instantiate.MoveResult(effectHandle.data(), effectHandle.size());
                                        }
                                        catch (...)
                                        {
                                            valueProperty->SetFloatingPointPropertyValue(
                                                magnitude, originalMagnitude);
                                            throw;
                                        }
                                        valueProperty->SetFloatingPointPropertyValue(
                                            magnitude, originalMagnitude);
                                        auto apply = ActorHelper::FunctionCall(gameplayEffects,
                                            STR("/Script/Dominion.DominionGameplayEffectsComponent:ApplyGameplayEffect"));
                                        apply.Arg(STR("Instigator"), static_cast<AActor*>(pawn))
                                            .Arg(STR("Source"), pawn)
                                            .Arg(STR("Handle"), effectHandle)
                                            .Arg(STR("bIgnoreChanceToApply"), true)
                                            .Invoke();
                                        auto verifyNative = ActorHelper::FunctionCall(
                                            health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                                        verifyNative.Invoke();
                                        currentMaximum = verifyNative.NumericResult();
                                        healthApplied = std::isfinite(currentMaximum)
                                            && std::abs(currentMaximum - desiredMaximum) < 0.01;
                                    }
                                    if (!healthApplied)
                                    {
                                        const auto diagnosticKey = std::format(
                                            "max-health-effect\n{}\n{}\n{}",
                                            resolvedPlayerGuid, currentMaximum, desiredMaximum);
                                        if (m_reportedPlayerRuleFailures.insert(diagnosticKey).second)
                                            PS::Log<LogLevel::Warning>(
                                                STR("GE_ModifyMaxHealth dispatched but verification returned {} (requested {}).\n"),
                                                currentMaximum, desiredMaximum);
                                    }
                                }
                                catch (const std::exception& error)
                                {
                                    PS::Log<LogLevel::Warning>(
                                        STR("Max-health gameplay effect failed safely: {}\n"),
                                        PS::ToWideSafe(error.what()));
                                }
                            }
                        }
                        if (healthApplied)
                        {
                            auto setHealthCall = ActorHelper::FunctionCall(
                                health, STR("/Script/Dominion.HealthComponent:SetHealth"));
                            setHealthCall.FirstNumericArg(desiredMaximum).Invoke();
                        }
                    }
                }
            }

            if (setStamina || setMaxStamina)
            {
                auto getStamina = ActorHelper::FunctionCall(
                    pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetStaminaComponent"));
                getStamina.Invoke();
                auto* stamina = getStamina.Result<UObject*>();
                if (stamina && !state->HasBaseStamina)
                {
                    auto getMaximum = ActorHelper::FunctionCall(
                        stamina, STR("/Script/Dominion.StaminaComponent:GetMaxStamina"));
                    getMaximum.Invoke();
                    const double baseMaximum = getMaximum.NumericResult();
                    auto* attributesAddress = PropertyHelper::GetValuePtrByPropertyNameInChain<
                        TObjectPtr<UObject>>(stamina, TEXT("AttributesComponent"));
                    auto* attributes = attributesAddress ? attributesAddress->Get() : nullptr;
                    auto* attributesProperty = attributes ? CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), TEXT("FloatAttributes"))) : nullptr;
                    auto* valuesProperty = attributes ? CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), TEXT("AttributeValues"))) : nullptr;
                    auto* attributeInner = attributesProperty
                        ? CastField<FObjectPropertyBase>(attributesProperty->GetInner()) : nullptr;
                    auto* valueInner = valuesProperty
                        ? CastField<FNumericProperty>(valuesProperty->GetInner()) : nullptr;
                    auto* attributeArray = attributesProperty
                        ? attributesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    auto* valueArray = valuesProperty
                        ? valuesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    if (attributes && attributesProperty && valuesProperty && attributeInner && valueInner
                        && valueInner->IsFloatingPoint() && attributeArray && valueArray
                        && attributeArray->Num() == valueArray->Num()
                        && std::isfinite(baseMaximum) && baseMaximum > 0.0)
                    {
                        const int32 attributeSize = attributesProperty->GetInner()->GetElementSize();
                        auto* attributeData = static_cast<uint8*>(attributeArray->GetData());
                        for (int32 index = 0; index < attributeArray->Num(); ++index)
                        {
                            UObject* attribute = nullptr;
                            std::memcpy(&attribute, attributeData + index * attributeSize, sizeof(attribute));
                            if (!attribute) continue;
                            std::array<std::string, 2> names{
                                RC::to_string(attribute->GetName()),
                                attribute->GetClassPrivate()
                                    ? RC::to_string(attribute->GetClassPrivate()->GetName())
                                    : std::string{},
                            };
                            const bool isMaximumStamina = std::any_of(
                                names.begin(), names.end(), [](std::string name) {
                                    std::transform(name.begin(), name.end(), name.begin(),
                                        [](unsigned char character) {
                                            return static_cast<char>(std::tolower(character));
                                        });
                                    return name.find("stamina") != std::string::npos
                                        && name.find("max") != std::string::npos;
                                });
                            if (!isMaximumStamina) continue;
                            state->StaminaAttributes = attributes;
                            state->StaminaValuesProperty = valuesProperty;
                            state->MaxStaminaAttribute = attribute;
                            state->MaxStaminaIndex = index;
                            state->BaseMaxStamina = baseMaximum;
                            state->HasBaseStamina = true;
                            break;
                        }
                    }
                }
                if (stamina && state->HasBaseStamina && state->StaminaAttributes
                    && state->StaminaValuesProperty && state->MaxStaminaAttribute
                    && state->MaxStaminaIndex >= 0)
                {
                    auto* valueInner = CastField<FNumericProperty>(
                        state->StaminaValuesProperty->GetInner());
                    auto* values = state->StaminaValuesProperty->ContainerPtrToValuePtr<FScriptArray>(
                        state->StaminaAttributes);
                    if (valueInner && valueInner->IsFloatingPoint() && values
                        && values->IsValidIndex(state->MaxStaminaIndex))
                    {
                        auto* valueAddress = static_cast<uint8*>(values->GetData())
                            + state->MaxStaminaIndex * valueInner->GetElementSize();
                        const double desired = setMaxStamina
                            ? maxStamina : state->BaseMaxStamina * staminaMultiplier;
                        valueInner->SetFloatingPointPropertyValue(valueAddress, desired);
                        auto changed = ActorHelper::FunctionCall(
                            state->StaminaAttributes,
                            STR("/Script/Dominion.DominionAttributesComponent:OnAttributeChanged"));
                        changed.Arg(STR("Attribute"), state->MaxStaminaAttribute).Invoke();
                        auto maximumChanged = ActorHelper::FunctionCall(
                            stamina, STR("/Script/Dominion.StaminaComponent:MaxStaminaChanged"));
                        maximumChanged.FirstNumericArg(desired).Invoke();
                        staminaApplied = true;
                    }
                }
            }

            const auto captureFields = [&](std::vector<PlayerNumericBaseline>& fields,
                const std::vector<const TCHAR*>& directNames,
                const std::vector<const TCHAR*>& inverseNames) {
                if (!fields.empty()) return;
                std::vector<UObject*> objects{pawn};
                for (auto* property = pawn->GetClassPrivate()->GetPropertyLink(); property;
                    property = property->GetPropertyLinkNext())
                {
                    auto* objectProperty = CastField<FObjectProperty>(property);
                    if (!objectProperty) continue;
                    auto* address = objectProperty->ContainerPtrToValuePtr<void>(pawn);
                    auto* object = address ? *reinterpret_cast<UObject**>(address) : nullptr;
                    if (object) objects.push_back(object);
                }
                for (auto* object : objects)
                {
                    const auto add = [&](const TCHAR* name, bool inverse) {
                        auto* numeric = CastField<FNumericProperty>(
                            PropertyHelper::GetPropertyByName(object->GetClassPrivate(), name));
                        if (!numeric || !numeric->IsFloatingPoint()) return;
                        auto* address = numeric->ContainerPtrToValuePtr<void>(object);
                        fields.push_back({object, numeric,
                            numeric->GetFloatingPointPropertyValue(address), inverse});
                    };
                    for (auto* name : directNames) add(name, false);
                    for (auto* name : inverseNames) add(name, true);
                }
            };
            const auto applyFields = [](std::vector<PlayerNumericBaseline>& fields, double multiplier) {
                for (auto& field : fields)
                {
                    auto* address = field.Property->ContainerPtrToValuePtr<void>(field.Object);
                    field.Property->SetFloatingPointPropertyValue(
                        address, field.Value * (field.Inverse ? 1.0 / multiplier : multiplier));
                }
                return !fields.empty();
            };

            if (setDamage)
            {
                captureAttribute(state->DamageAttribute, playerAttributes, [](const std::string& name) {
                    return name.find("damage") != std::string::npos
                        && name.find("taken") == std::string::npos
                        && (name.find("mult") != std::string::npos
                            || name.find("output") != std::string::npos
                            || name.find("attack") != std::string::npos);
                });
                captureFields(state->DamageFields,
                    {TEXT("DamageMultiplier"), TEXT("DamageScale"),
                     TEXT("OutgoingDamageMultiplier"), TEXT("AttackDamageMultiplier")}, {});
                damageApplied = applyAttribute(state->DamageAttribute, damageMultiplier)
                    || applyFields(state->DamageFields, damageMultiplier);
            }
            if (setDefense)
            {
                captureAttribute(state->DefenseAttribute, playerAttributes, [](const std::string& name) {
                    return name.find("defense") != std::string::npos
                        || name.find("defence") != std::string::npos
                        || name.find("armor") != std::string::npos
                        || name.find("armour") != std::string::npos;
                });
                captureFields(state->DefenseFields,
                    {TEXT("DefenseMultiplier"), TEXT("DefenceMultiplier"),
                     TEXT("ArmorMultiplier"), TEXT("ArmourMultiplier")},
                    {TEXT("DamageTakenMultiplier"), TEXT("IncomingDamageMultiplier")});
                defenseApplied = applyAttribute(state->DefenseAttribute, defenseMultiplier)
                    || applyFields(state->DefenseFields, defenseMultiplier);
            }

            if (rule.SetWalkSpeed)
            {
                captureFields(state->WalkSpeedFields,
                    {TEXT("MaxWalkSpeed")}, {});
                walkSpeedApplied = applyFields(
                    state->WalkSpeedFields, rule.WalkSpeedMultiplier);
            }
            if (rule.SetRunSpeed)
            {
                captureAttribute(state->RunSpeedAttribute, playerAttributes,
                    [](const std::string& name) {
                        return (name.find("run") != std::string::npos
                                || name.find("sprint") != std::string::npos)
                            && name.find("speed") != std::string::npos;
                    });
                captureFields(state->RunSpeedFields,
                    {TEXT("RunSpeed"), TEXT("SprintSpeed"),
                     TEXT("RunSpeedMultiplier"), TEXT("SprintSpeedMultiplier")}, {});
                runSpeedApplied = applyAttribute(
                    state->RunSpeedAttribute, rule.RunSpeedMultiplier)
                    || applyFields(state->RunSpeedFields, rule.RunSpeedMultiplier);
            }
            if (rule.SetCarryWeight || rule.SetMaxCarryWeight)
            {
                captureAttribute(state->CarryWeightAttribute, playerAttributes,
                    [&](const std::string& name) {
                        return NormalizePlayerAttributeIdentifier(name) == "carryweightmax";
                    });
                carryWeightApplied = rule.SetMaxCarryWeight
                    ? applyAttributeValue(
                        state->CarryWeightAttribute, rule.MaxCarryWeight)
                    : applyAttribute(
                        state->CarryWeightAttribute, rule.CarryWeightMultiplier);
            }
            if (rule.SetPoisonResistance)
            {
                captureAttribute(state->PoisonResistanceAttribute, playerAttributes,
                    [](const std::string& name) {
                        return name.find("poison") != std::string::npos
                            && (name.find("resist") != std::string::npos
                                || name.find("defense") != std::string::npos
                                || name.find("defence") != std::string::npos);
                    });
                poisonResistanceApplied = applyAttribute(
                    state->PoisonResistanceAttribute,
                    rule.PoisonResistanceMultiplier);
            }
            if (rule.SetStaminaRecovery)
            {
                auto getStamina = ActorHelper::FunctionCall(
                    pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetStaminaComponent"));
                getStamina.Invoke();
                auto* stamina = getStamina.Result<UObject*>();
                auto* attributesAddress = stamina
                    ? PropertyHelper::GetValuePtrByPropertyNameInChain<TObjectPtr<UObject>>(
                        stamina, TEXT("AttributesComponent"))
                    : nullptr;
                auto* staminaAttributes = attributesAddress ? attributesAddress->Get() : nullptr;
                captureAttribute(state->StaminaRecoveryAttribute, staminaAttributes,
                    [](const std::string& name) {
                        return name.find("stamina") != std::string::npos
                            && (name.find("regen") != std::string::npos
                                || name.find("recovery") != std::string::npos
                                || name.find("recover") != std::string::npos);
                    });
                staminaRecoveryApplied = applyAttribute(
                    state->StaminaRecoveryAttribute,
                    rule.StaminaRecoveryMultiplier);
                if (staminaRecoveryApplied && stamina)
                {
                    auto resetRegen = ActorHelper::FunctionCall(
                        stamina, STR("/Script/Dominion.StaminaComponent:ResetRegen"));
                    resetRegen.Invoke();
                }
            }

            const auto attackMatcher = [](const std::string& name, const char* category) {
                const bool categoryMatch = std::string_view(category) == "physical"
                    ? name.find("physical") != std::string::npos
                        || name.find("melee") != std::string::npos
                    : std::string_view(category) == "magical"
                    ? name.find("magic") != std::string::npos
                    : name.find("range") != std::string::npos;
                return categoryMatch
                    && (name.find("attack") != std::string::npos
                        || name.find("damage") != std::string::npos)
                    && name.find("taken") == std::string::npos
                    && name.find("resist") == std::string::npos;
            };
            const auto defenseMatcher = [](const std::string& name, const char* category) {
                const bool categoryMatch = std::string_view(category) == "physical"
                    ? name.find("physical") != std::string::npos
                        || name.find("melee") != std::string::npos
                    : std::string_view(category) == "magical"
                    ? name.find("magic") != std::string::npos
                    : name.find("range") != std::string::npos;
                return categoryMatch
                    && (name.find("resist") != std::string::npos
                        || name.find("defense") != std::string::npos
                        || name.find("defence") != std::string::npos
                        || name.find("armor") != std::string::npos
                        || name.find("armour") != std::string::npos
                        || name.find("taken") != std::string::npos);
            };
            if (rule.SetPhysicalAttack)
            {
                captureAttribute(state->PhysicalAttackAttribute, playerAttributes,
                    [&](const std::string& name) { return attackMatcher(name, "physical"); });
                physicalAttackApplied = applyAttribute(
                    state->PhysicalAttackAttribute,
                    rule.PhysicalAttackMultiplier * (setDamage ? damageMultiplier : 1.0));
            }
            if (rule.SetMagicalAttack)
            {
                captureAttribute(state->MagicalAttackAttribute, playerAttributes,
                    [&](const std::string& name) { return attackMatcher(name, "magical"); });
                magicalAttackApplied = applyAttribute(
                    state->MagicalAttackAttribute,
                    rule.MagicalAttackMultiplier * (setDamage ? damageMultiplier : 1.0));
            }
            if (rule.SetRangedAttack)
            {
                captureAttribute(state->RangedAttackAttribute, playerAttributes,
                    [&](const std::string& name) { return attackMatcher(name, "ranged"); });
                rangedAttackApplied = applyAttribute(
                    state->RangedAttackAttribute,
                    rule.RangedAttackMultiplier * (setDamage ? damageMultiplier : 1.0));
            }
            if (rule.SetPhysicalDefense)
            {
                captureAttribute(state->PhysicalDefenseAttribute, playerAttributes,
                    [&](const std::string& name) { return defenseMatcher(name, "physical"); });
                physicalDefenseApplied = applyAttribute(
                    state->PhysicalDefenseAttribute,
                    rule.PhysicalDefenseMultiplier * (setDefense ? defenseMultiplier : 1.0));
            }
            if (rule.SetMagicalDefense)
            {
                captureAttribute(state->MagicalDefenseAttribute, playerAttributes,
                    [&](const std::string& name) { return defenseMatcher(name, "magical"); });
                magicalDefenseApplied = applyAttribute(
                    state->MagicalDefenseAttribute,
                    rule.MagicalDefenseMultiplier * (setDefense ? defenseMultiplier : 1.0));
            }
            if (rule.SetRangedDefense)
            {
                captureAttribute(state->RangedDefenseAttribute, playerAttributes,
                    [&](const std::string& name) { return defenseMatcher(name, "ranged"); });
                rangedDefenseApplied = applyAttribute(
                    state->RangedDefenseAttribute,
                    rule.RangedDefenseMultiplier * (setDefense ? defenseMultiplier : 1.0));
            }

            for (const auto& requested : rule.AttributeMultipliers)
            {
                const auto normalized = normalizeAttributeIdentifier(requested.Identifier);
                // Current/max vitals need their dedicated component notifications.
                // Keep them out of the generic path even if a data asset has that name.
                if (normalized == "health" || normalized == "maxhealth"
                    || normalized == "stamina" || normalized == "maxstamina")
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(requested.Identifier + " (reserved vital)");
                    continue;
                }
                auto named = std::find_if(state->NamedAttributes.begin(), state->NamedAttributes.end(),
                    [&](const NamedPlayerAttributeBaseline& entry) {
                        return entry.Identifier == normalized;
                    });
                if (named == state->NamedAttributes.end())
                {
                    state->NamedAttributes.push_back({normalized, {}});
                    named = std::prev(state->NamedAttributes.end());
                }
                captureAttribute(named->Baseline, playerAttributes,
                    [&](const std::string& loadedName) {
                        return normalizeAttributeIdentifier(loadedName) == normalized;
                    });
                if (!applyAttribute(named->Baseline, requested.Multiplier))
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(requested.Identifier);
                }
            }

            for (const auto& requested : rule.Attributes)
            {
                const auto normalized = normalizeAttributeIdentifier(requested.Identifier);
                if (normalized == "health" || normalized == "maxhealth"
                    || normalized == "stamina" || normalized == "maxstamina")
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(
                        requested.Identifier + " (use a dedicated vital field)");
                    continue;
                }
                auto named = std::find_if(state->NamedAttributes.begin(),
                    state->NamedAttributes.end(),
                    [&](const NamedPlayerAttributeBaseline& entry) {
                        return entry.Identifier == normalized;
                    });
                if (named == state->NamedAttributes.end())
                {
                    state->NamedAttributes.push_back({normalized, {}});
                    named = std::prev(state->NamedAttributes.end());
                }
                captureAttribute(named->Baseline, playerAttributes,
                    [&](const std::string& loadedName) {
                        return normalizeAttributeIdentifier(loadedName) == normalized;
                    });
                double desired = requested.Value;
                if (requested.Operation == EPlayerAttributeEditOperation::Add)
                    desired = named->Baseline.Value + requested.Value;
                else if (requested.Operation == EPlayerAttributeEditOperation::Multiply)
                    desired = named->Baseline.Value * requested.Value;
                if (!applyAttributeValue(named->Baseline, desired))
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(requested.Identifier);
                }
            }

            std::string namedAttributeStatus = "applied";
            if (!namedAttributesApplied)
            {
                namedAttributeStatus = "unsupported [";
                for (size_t index = 0; index < unsupportedNamedAttributes.size(); ++index)
                {
                    if (index) namedAttributeStatus += ", ";
                    namedAttributeStatus += unsupportedNamedAttributes[index];
                }
                namedAttributeStatus += "]";
            }

            if (!rule.Appearance.empty())
            {
                bool changed = false;
                bool provenanceChanged = false;
                UObject* customization = nullptr;
                LoadAppearanceProvenance();
                for (const auto& selection : rule.Appearance)
                {
                    try
                    {
                        std::string currentTable;
                        std::string currentRow;
                        std::string error;
                        if (!ReadPlayerAppearance(pawn, selection.Field, currentTable,
                            currentRow, &customization, error))
                            throw std::runtime_error(error);

                        auto provenance = std::find_if(
                            m_appearanceProvenance.begin(), m_appearanceProvenance.end(),
                            [&](const AppearanceProvenance& value) {
                                return value.PlayerGuid == resolvedPlayerGuid
                                    && value.Field == selection.Field;
                            });
                        const auto normalizedCurrent = ActorHelper::NormalizeObjectPath(
                            RC::to_generic_string(currentTable));
                        const auto normalizedTarget = ActorHelper::NormalizeObjectPath(
                            RC::to_generic_string(selection.DataTablePath));
                        const bool alreadyApplied = normalizedCurrent == normalizedTarget
                            && currentRow == selection.RowName;
                        if (alreadyApplied)
                        {
                            const auto noOpKey = resolvedPlayerGuid + "\n"
                                + selection.Field + "\n" + selection.DataTablePath
                                + "\n" + selection.RowName;
                            if (m_reportedAppearanceNoOps.insert(noOpKey).second)
                            {
                                PS::Log<LogLevel::Normal>(
                                    STR("Player appearance already matches the native save; skipped rewrite for player {} field {}.\n"),
                                    PS::ToWideSafe(resolvedPlayerGuid.c_str()),
                                    PS::ToWideSafe(selection.Field.c_str()));
                            }
                        }

                        // Preserve the very first safe value across multiple overriding
                        // mods. If the native save already contains the requested value,
                        // only explicit fallback metadata is safe to record.
                        if (provenance == m_appearanceProvenance.end()
                            && (!alreadyApplied || selection.HasFallback)
                            && !resolvedPlayerGuid.empty())
                        {
                            AppearanceProvenance record;
                            record.PlayerGuid = resolvedPlayerGuid;
                            record.Field = selection.Field;
                            record.OwnerMod = RC::to_string(rule.ModName);
                            record.Source = selection.Source;
                            record.AppliedDataTablePath = selection.DataTablePath;
                            record.AppliedRowName = selection.RowName;
                            // When RuneSchema is changing a field, the value currently
                            // loaded from the character is the authoritative saved
                            // preset and must win over an author-supplied generic
                            // fallback. An explicit fallback is only needed when the
                            // requested value was already present before provenance
                            // existed, because the prior value can no longer be
                            // observed in that case.
                            const bool useExplicitFallback = alreadyApplied
                                && selection.HasFallback;
                            record.FallbackDataTablePath = useExplicitFallback
                                ? selection.FallbackDataTablePath : currentTable;
                            record.FallbackRowName = useExplicitFallback
                                ? selection.FallbackRowName : currentRow;
                            m_appearanceProvenance.push_back(std::move(record));
                            provenance = std::prev(m_appearanceProvenance.end());
                            provenanceChanged = true;
                        }
                        else if (provenance != m_appearanceProvenance.end()
                            && (provenance->OwnerMod != RC::to_string(rule.ModName)
                                || provenance->AppliedDataTablePath != selection.DataTablePath
                                || provenance->AppliedRowName != selection.RowName))
                        {
                            provenance->OwnerMod = RC::to_string(rule.ModName);
                            provenance->Source = selection.Source;
                            provenance->AppliedDataTablePath = selection.DataTablePath;
                            provenance->AppliedRowName = selection.RowName;
                            provenanceChanged = true;
                        }

                        bool fieldChanged = false;
                        if (!WritePlayerAppearance(pawn, selection.Field,
                            selection.DataTablePath, selection.RowName,
                            fieldChanged, &customization, error))
                            throw std::runtime_error(error);
                        changed = changed || fieldChanged;
                    }
                    catch (const std::exception& error)
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back(selection.Field + "="
                            + selection.RowName + " (" + error.what() + ")");
                    }
                    catch (...)
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back(
                            selection.Field + "=" + selection.RowName);
                    }
                }
                if (provenanceChanged)
                {
                    std::string error;
                    if (!SaveAppearanceProvenance(error))
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back("fallback state (" + error + ")");
                    }
                }
                if (changed && customization)
                {
                    try
                    {
                        auto refresh = ActorHelper::FunctionCall(
                            customization,
                            STR("/Script/Dominion.PlayerCustomizationComponent:OnRep_PlayerCustomization"));
                        refresh.Invoke();
                    }
                    catch (...)
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back("visual refresh");
                    }
                }
            }

            std::string appearanceStatus = "applied";
            if (!appearanceApplied)
            {
                appearanceStatus = "unsupported [";
                for (size_t index = 0; index < unsupportedAppearance.size(); ++index)
                {
                    if (index) appearanceStatus += ", ";
                    appearanceStatus += unsupportedAppearance[index];
                }
                appearanceStatus += "]";
            }

            std::vector<std::string> requestedStatuses;
            const auto appendStatus = [&](const char* label, bool requested,
                bool applied, const std::string& detail = {}) {
                if (!requested) return;
                requestedStatuses.push_back(std::format("{}: {}", label,
                    detail.empty() ? (applied ? "applied" : "unsupported") : detail));
            };
            appendStatus("scale", setScale, scaleApplied);
            appendStatus("health", setHealth || setMaxHealth, healthApplied);
            appendStatus("defense", setDefense, defenseApplied);
            appendStatus("damage", setDamage, damageApplied);
            appendStatus("stamina", setStamina || setMaxStamina, staminaApplied);
            appendStatus("walk speed", rule.SetWalkSpeed, walkSpeedApplied);
            appendStatus("run speed", rule.SetRunSpeed, runSpeedApplied);
            appendStatus("carry weight", rule.SetCarryWeight || rule.SetMaxCarryWeight,
                carryWeightApplied);
            appendStatus("poison resistance", rule.SetPoisonResistance,
                poisonResistanceApplied);
            appendStatus("stamina recovery", rule.SetStaminaRecovery,
                staminaRecoveryApplied);
            appendStatus("physical attack", rule.SetPhysicalAttack, physicalAttackApplied);
            appendStatus("magical attack", rule.SetMagicalAttack, magicalAttackApplied);
            appendStatus("ranged attack", rule.SetRangedAttack, rangedAttackApplied);
            appendStatus("physical defense", rule.SetPhysicalDefense, physicalDefenseApplied);
            appendStatus("magical defense", rule.SetMagicalDefense, magicalDefenseApplied);
            appendStatus("ranged defense", rule.SetRangedDefense, rangedDefenseApplied);
            appendStatus("named attributes",
                !rule.AttributeMultipliers.empty() || !rule.Attributes.empty(),
                namedAttributesApplied, namedAttributeStatus);
            appendStatus("appearance", !rule.Appearance.empty(),
                appearanceApplied, appearanceStatus);

            std::string statusList;
            for (size_t index = 0; index < requestedStatuses.size(); ++index)
            {
                if (index) statusList += ", ";
                statusList += requestedStatuses[index];
            }
            result = "player adjustment completed (" + statusList + ")";
            return scaleApplied && healthApplied && defenseApplied && damageApplied && staminaApplied
                && walkSpeedApplied && runSpeedApplied && carryWeightApplied && poisonResistanceApplied
                && staminaRecoveryApplied && physicalAttackApplied && magicalAttackApplied
                && rangedAttackApplied && physicalDefenseApplied && magicalDefenseApplied
                && rangedDefenseApplied && namedAttributesApplied && appearanceApplied;
        }
        catch (const std::exception& error)
        {
            result = std::string("player adjustment failed safely: ") + error.what();
            return false;
        }
        catch (...)
        {
            result = "player adjustment failed safely with an unknown exception";
            return false;
        }
    }

}
