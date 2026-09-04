#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/AGameModeBase.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsBuildingModLoader.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        constexpr const TCHAR* BuildingPieceClassPath =
            TEXT("/Script/Dominion.BuildingPieceData");
        constexpr const TCHAR* BuildingPieceSubsystemClassPath =
            TEXT("/Script/Dominion.BuildingPieceSubsystem");
        constexpr const TCHAR* ItemDataClassPath =
            TEXT("/Script/Dominion.ItemData");
        constexpr const TCHAR* ProgressComponentClassPath =
            TEXT("/Script/Dominion.ProgressComponent");
        constexpr const TCHAR* CataloguePath =
            TEXT("/Game/Gameplay/BaseBuilding_New/BuildingPieces/"
                 "DA_BuildPieceCatalogue_Default.DA_BuildPieceCatalogue_Default");
        constexpr const TCHAR* StabilityProfilePath =
            TEXT("/Game/Gameplay/BaseBuilding_New/"
                 "DT_StabilityProfile.DT_StabilityProfile");
        constexpr const TCHAR* RetiredStabilityProfileRow = TEXT("FarmPlot");
        constexpr int BuildingManifestVersion = 1;

        std::string OrderedFingerprint(const std::vector<UObject*>& objects)
        {
            uint64_t hash = 14695981039346656037ull;
            const auto append = [&](const std::string& value, uint8_t separator) {
                for (const auto byte : value)
                {
                    hash ^= static_cast<uint8_t>(byte);
                    hash *= 1099511628211ull;
                }
                hash ^= separator;
                hash *= 1099511628211ull;
            };
            for (auto* object : objects)
            {
                auto* property = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), TEXT("PersistenceID")));
                if (!property) return {};
                const auto id = property->GetPropertyValue(
                    property->ContainerPtrToValuePtr<void>(object));
                append(RC::to_string(*id), 0x1f);
                append(RC::to_string(object->GetPathName()), 0x1e);
            }
            std::ostringstream output;
            output << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
            return output.str();
        }

        RC::StringType GuidString(const void* guidData)
        {
            uint32 words[4]{};
            std::memcpy(words, guidData, sizeof(words));
            return std::format(STR("{:08X}-{:08X}-{:08X}-{:08X}"),
                words[0], words[1], words[2], words[3]);
        }

        constexpr const TCHAR* UnlockHookPaths[] = {
            TEXT("/Script/Dominion.ProgressComponent:"
                 "Client_HandleNewBuildingPiecesLoadedFromPersistence"),
            TEXT("/Script/Dominion.ProgressComponent:Client_OnBuildingsUnlocked"),
        };
        bool SameSoftObject(const UECustom::FSoftObjectPtr& soft, UObject* object)
        {
            if (!object)
            {
                return false;
            }

            if (soft.WeakPtr.Get() == object)
            {
                return true;
            }

            const auto target = UECustom::FSoftObjectPath(object->GetPathName());
            return soft.ObjectID.AssetPath.GetPackageName() == target.AssetPath.GetPackageName()
                && soft.ObjectID.AssetPath.GetAssetName() == target.AssetPath.GetAssetName();
        }

        void InitializeSoftObject(void* destination, UObject* object)
        {
            auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(destination);
            soft->ObjectID = UECustom::FSoftObjectPath(object->GetPathName());
            soft->WeakPtr = FWeakObjectPtr(object);
        }

        UObject* ResolveItem(const RC::StringType& reference)
        {
            if (reference.starts_with(TEXT("/")))
            {
                UECustom::TSoftObjectPtr<UObject> soft{
                    UECustom::FSoftObjectPath(reference) };
                return UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }

            auto* itemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, ItemDataClassPath);
            if (!itemClass)
            {
                return nullptr;
            }

            TArray<UObject*> items;
            UECustom::UObjectGlobals::GetObjectsOfClass(itemClass, items, true);
            for (auto* item : items)
            {
                if (item && item->GetName() == reference
                    && !item->HasAnyFlags(static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject)))
                {
                    return item;
                }
            }

            return nullptr;
        }

    }

    DragonWildsBuildingModLoader::DragonWildsBuildingModLoader()
        : DragonWildsModLoaderBase("buildings")
    {
        SetDisplayName(TEXT("Building Loader"));
    }

    void DragonWildsBuildingModLoader::OnLoad(const std::filesystem::path& loaderPath,
        const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                ReadDefinitions(data, modName);
            });
            return;
        }

        if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            ApplyDefinitions();
        }
    }

    void DragonWildsBuildingModLoader::OnAutoReload(const RC::StringType& modName,
        const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            ReadDefinitions(data, modName);
        });
        ApplyDefinitions();
    }

    bool DragonWildsBuildingModLoader::CanInitialize(
        const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit;
    }

    bool DragonWildsBuildingModLoader::OnInitialize()
    {
        m_buildingPieceClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, BuildingPieceClassPath);
        m_buildingPieceSubsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, BuildingPieceSubsystemClassPath);
        m_progressComponentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, ProgressComponentClassPath);

        if (!m_buildingPieceClass || !m_buildingPieceSubsystemClass
            || !m_progressComponentClass)
        {
            PS::Log<LogLevel::Error>(
                STR("Unable to initialize Building Loader: required Dominion types were not found.\n"));
            return false;
        }

        return true;
    }

    void DragonWildsBuildingModLoader::ActivateWorldRegistration()
    {
        RegisterHooks();
    }

    void DragonWildsBuildingModLoader::ReadDefinitions(
        const nlohmann::json& data, const RC::StringType& modName)
    {
        if (!data.is_object())
        {
            PS::Log<LogLevel::Error>(
                STR("{}: building file must contain a JSON object.\n"), modName);
            return;
        }

        for (const auto& [key, body] : data.items())
        {
            if (key.starts_with("$"))
            {
                continue;
            }

            if (!body.is_object())
            {
                PS::Log<LogLevel::Error>(
                    STR("{}: Building '{}' must be a JSON object.\n"),
                    modName, RC::to_generic_string(key));
                continue;
            }

            if (!body.contains("Asset") || !body.at("Asset").is_string())
            {
                PS::Log<LogLevel::Error>(
                    STR("{}: Building '{}' requires a cooked 'Asset' path.\n"),
                    modName, RC::to_generic_string(key));
                continue;
            }

            BuildingDefinition definition{};
            definition.Owner = modName;
            definition.Key = RC::to_generic_string(key);
            definition.AssetPath =
                RC::to_generic_string(body.at("Asset").get<std::string>());

            if (body.contains("Properties"))
            {
                if (!body.at("Properties").is_object())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Properties' must be a JSON object.\n"),
                        modName, definition.Key);
                    continue;
                }
                definition.Properties = body.at("Properties");
            }

            if (body.contains("Requirements"))
            {
                const auto& requirements = body.at("Requirements");
                if (!requirements.is_array())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Requirements' must be an array.\n"),
                        modName, definition.Key);
                    continue;
                }

                bool valid = true;
                for (const auto& requirement : requirements)
                {
                    if (!requirement.is_object()
                        || !requirement.contains("ItemData")
                        || !requirement.at("ItemData").is_string()
                        || !requirement.contains("Amount")
                        || !requirement.at("Amount").is_number_integer()
                        || requirement.at("Amount").get<int64_t>() <= 0)
                    {
                        valid = false;
                        break;
                    }
                }

                if (!valid)
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Requirements' entries require an ItemData string and positive Amount.\n"),
                        modName, definition.Key);
                    continue;
                }

                definition.Requirements = requirements;
            }

            if (body.contains("Unlock"))
            {
                if (!body.at("Unlock").is_boolean())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Unlock' must be true or false.\n"),
                        modName, definition.Key);
                    continue;
                }
                definition.Unlock = body.at("Unlock").get<bool>();
            }

            if (body.contains("AddTo"))
            {
                const auto& addTo = body.at("AddTo");
                if (!addTo.is_object()
                    || !addTo.contains("Collection")
                    || !addTo.at("Collection").is_string())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.AddTo' requires a 'Collection' string.\n"),
                        modName, definition.Key);
                    continue;
                }

                definition.Target.Collection =
                    RC::to_generic_string(addTo.at("Collection").get<std::string>());

                if (addTo.contains("PageIndex"))
                {
                    if (!addTo.at("PageIndex").is_number_integer())
                    {
                        PS::Log<LogLevel::Error>(
                            STR("{}: Building '{}.AddTo.PageIndex' must be an integer.\n"),
                            modName, definition.Key);
                        continue;
                    }
                    definition.Target.PageIndex = addTo.at("PageIndex").get<int32>();
                }
            }
            else
            {
                definition.Target.Collection = TEXT("Modded Buildings");
            }

            auto existing = std::find_if(
                m_definitions.begin(), m_definitions.end(),
                [&](const BuildingDefinition& value) {
                    return value.Owner == definition.Owner
                        && value.Key == definition.Key;
                });

            if (existing == m_definitions.end())
            {
                m_definitions.push_back(std::move(definition));
            }
            else
            {
                *existing = std::move(definition);
            }

            m_applied.erase(Identity(modName, RC::to_generic_string(key)));
        }
    }

    void DragonWildsBuildingModLoader::ApplyDefinitions()
    {
        if (!m_catalogue)
        {
            m_catalogue = LoadObject(CataloguePath);
        }

        if (!m_catalogue)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings cannot be loaded because the default build catalogue is unavailable.\n"));
            return;
        }

        LoadResult result{};
        for (const auto& definition : m_definitions)
        {
            const auto identity = Identity(definition.Owner, definition.Key);
            if (m_applied.contains(identity))
            {
                continue;
            }

            auto* building = LoadBuilding(definition, result);
            if (!building)
            {
                continue;
            }

            ApplyProperties(building, definition, result);
            if (!ApplyRequirements(building, definition))
            {
                result.Errors++;
                continue;
            }

            if (!EnsureStabilityProfile(building))
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': stability profile is unavailable.\n"),
                    definition.Key);
                result.Errors++;
                continue;
            }

            if (!AddPersistenceIdentity(building))
            {
                result.Errors++;
                continue;
            }

            if (!AddToMenu(building, definition.Target))
            {
                result.Errors++;
                continue;
            }

            if (definition.Unlock)
            {
                m_unlocks.insert(identity);
            }
            else
            {
                m_unlocks.erase(identity);
            }

            m_applied.insert(identity);
            result.Loaded++;
        }

        RegisterHooks();
        if (auto* progress = FindProgressComponent())
        {
            ApplyUnlocks(progress);
        }

        if (result.Loaded || result.Errors)
        {
            PS::Log<LogLevel::Normal>(
                STR("Buildings: {} loaded, {} error{}.\n"),
                result.Loaded, result.Errors, result.Errors == 1 ? STR("") : STR("s"));
        }
    }

    UObject* DragonWildsBuildingModLoader::LoadBuilding(
        const BuildingDefinition& definition, LoadResult& result)
    {
        const auto identity = Identity(definition.Owner, definition.Key);
        if (auto found = m_buildings.find(identity);
            found != m_buildings.end())
        {
            return found->second;
        }

        auto* building = LoadObject(definition.AssetPath);
        if (!building || !building->IsA(m_buildingPieceClass))
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}' asset '{}' is not a BuildingPieceData asset.\n"),
                definition.Key, definition.AssetPath);
            result.Errors++;
            return nullptr;
        }

        building->SetRootSet();
        m_buildings.emplace(identity, building);
        return building;
    }

    void DragonWildsBuildingModLoader::ApplyProperties(
        UObject* building, const BuildingDefinition& definition, LoadResult& result)
    {
        for (const auto& [name, value] : definition.Properties.items())
        {
            auto propertyName = RC::to_generic_string(name);
            auto* property =
                PropertyHelper::GetPropertyByName(building->GetClassPrivate(), propertyName);
            if (!property)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': property '{}' was not found.\n"),
                    definition.Key, propertyName);
                result.Errors++;
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(building, property, value);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': failed to set '{}': {}\n"),
                    definition.Key, propertyName, PS::ToWideSafe(error.what()));
                result.Errors++;
            }
        }
    }

    bool DragonWildsBuildingModLoader::ApplyRequirements(
        UObject* building, const BuildingDefinition& definition)
    {
        if (definition.Requirements.is_null())
        {
            return true;
        }

        auto* arrayProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                building->GetClassPrivate(), TEXT("Requirements")));
        auto* structProperty = arrayProperty
            ? CastField<FStructProperty>(arrayProperty->GetInner()) : nullptr;
        auto* requirementStruct = structProperty ? structProperty->GetStruct().Get() : nullptr;
        auto* amountProperty = requirementStruct ? CastField<FNumericProperty>(
            PropertyHelper::GetPropertyByName(requirementStruct, TEXT("Amount"))) : nullptr;
        auto* itemProperty = requirementStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(requirementStruct, TEXT("ItemData"))) : nullptr;
        if (!arrayProperty || !structProperty || !amountProperty || !itemProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': Requirements layout is incompatible with RuneSchema.\n"),
                definition.Key);
            return false;
        }

        struct ResolvedRequirement
        {
            UObject* Item = nullptr;
            int64_t Amount = 0;
        };
        std::vector<ResolvedRequirement> resolved;
        resolved.reserve(definition.Requirements.size());

        for (const auto& requirement : definition.Requirements)
        {
            const auto reference = RC::to_generic_string(
                requirement.at("ItemData").get<std::string>());
            auto* item = ResolveItem(reference);
            if (!item)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': requirement item '{}' could not be resolved.\n"),
                    definition.Key, reference);
                return false;
            }

            resolved.push_back({
                item,
                requirement.at("Amount").get<int64_t>()
            });
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(building);
        UECustom::FScriptArrayHelper helper(array, arrayProperty);
        helper.Empty();

        for (const auto& requirement : resolved)
        {
            UECustom::FManagedValue value;
            helper.InitializeValue(value);
            amountProperty->SetIntPropertyValue(
                amountProperty->ContainerPtrToValuePtr<void>(value.GetData()),
                requirement.Amount);
            auto* itemAddress = itemProperty->ContainerPtrToValuePtr<void>(value.GetData());
            std::memcpy(itemAddress, &requirement.Item, sizeof(requirement.Item));
            helper.Add(value);
        }

        return true;
    }

    bool DragonWildsBuildingModLoader::EnsureStabilityProfile(UObject* building)
    {
        auto* handleProperty = CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                building->GetClassPrivate(), TEXT("BuildingStabilityProfileRowHandle")));
        auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
        auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
        if (!handleProperty || !tableProperty)
        {
            return false;
        }

        auto* handle = handleProperty->ContainerPtrToValuePtr<void>(building);
        auto* tableAddress = tableProperty->ContainerPtrToValuePtr<void>(handle);
        UObject* currentTable = nullptr;
        std::memcpy(&currentTable, tableAddress, sizeof(currentTable));
        if (currentTable)
        {
            return true;
        }

        auto* table = LoadObject(StabilityProfilePath);
        if (!table)
        {
            return false;
        }

        std::memcpy(tableAddress, &table, sizeof(table));
        currentTable = nullptr;
        std::memcpy(&currentTable, tableAddress, sizeof(currentTable));
        return currentTable == table;
    }

    bool DragonWildsBuildingModLoader::AddPersistenceIdentity(UObject* building)
    {
        auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
            building->GetClassPrivate(), TEXT("PersistenceID")));
        auto* setProperty = m_catalogue ? CastField<FSetProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("AllPiecesInCatalogue"))) : nullptr;
        if (!idProperty || !setProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}' has no usable persistence identity.\n"),
                building->GetName());
            return false;
        }

        auto persistenceId = idProperty->GetPropertyValue(
            idProperty->ContainerPtrToValuePtr<void>(building));
        if (persistenceId.GetCharArray().Num() <= 1)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}' has an empty PersistenceID.\n"),
                building->GetName());
            return false;
        }

        UECustom::FScriptSetHelper set(
            setProperty, setProperty->ContainerPtrToValuePtr<void>(m_catalogue));
        set.Add(&persistenceId);
        return true;
    }

    bool DragonWildsBuildingModLoader::ResolveWorldRegistryPath(AGameModeBase* gameMode)
    {
        auto* persistenceClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.PersistenceSubsystem"));
        if (!gameMode || !persistenceClass)
        {
            return false;
        }

        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(persistenceClass, candidates, true);
        UObject* persistence = nullptr;
        for (auto* candidate : candidates)
        {
            if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }
            if (candidate->GetWorld() == gameMode->GetWorld())
            {
                if (persistence)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building registry protection found multiple persistence subsystems for one world.\n"));
                    return false;
                }
                persistence = candidate;
            }
        }
        if (!persistence)
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry protection could not resolve this world's persistence subsystem.\n"));
            return false;
        }

        auto* settingsProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            persistence->GetClassPrivate(), TEXT("WorldSaveSettings")));
        auto* guidProperty = settingsProperty ? CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                settingsProperty->GetStruct().Get(), TEXT("WorldSaveGuid"))) : nullptr;
        if (!settingsProperty || !guidProperty || guidProperty->GetElementSize() != 16)
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry protection cannot read WorldSaveGuid.\n"));
            return false;
        }

        auto* settings = settingsProperty->ContainerPtrToValuePtr<void>(persistence);
        auto* guid = guidProperty->ContainerPtrToValuePtr<void>(settings);
        uint8 guidBytes[16]{};
        std::memcpy(guidBytes, guid, sizeof(guidBytes));
        if (std::all_of(std::begin(guidBytes), std::end(guidBytes),
                [](uint8 value) { return value == 0; }))
        {
            m_worldManifestPath.clear();
            return false;
        }

        const auto guidString = GuidString(guid);
        auto* systemLibrary = ActorHelper::ResolveObject(
            TEXT("/Script/Engine.Default__KismetSystemLibrary"));
        if (!systemLibrary)
        {
            return false;
        }
        auto savedDirectoryCall = ActorHelper::FunctionCall(systemLibrary,
            TEXT("/Script/Engine.KismetSystemLibrary:GetProjectSavedDirectory"));
        savedDirectoryCall.Invoke();
        const auto savedDirectory = savedDirectoryCall.Result<FString>();
        if (savedDirectory.GetCharArray().Num() <= 1)
        {
            return false;
        }

        m_worldManifestPath = std::filesystem::path(*savedDirectory)
            / "RuneSchema" / RC::to_string(guidString) / "CustomBuildingData.json";
        return true;
    }

    bool DragonWildsBuildingModLoader::ProtectWorldRegistry(UObject* subsystem)
    {
        auto* subsystemClass = subsystem->GetClassPrivate();
        auto* arrayProperty = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("NetIdToData")));
        auto* reverseProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("DataToNetIdMap")));
        auto* persistenceMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("PersistenceIDToDataMap")));
        auto* internalMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("InternalNameToDataMap")));
        if (!arrayProperty || !reverseProperty || !persistenceMapProperty || !internalMapProperty)
        {
            return false;
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        const auto elementSize = arrayProperty->GetInner()->GetElementSize();
        struct ActiveDefinition {
            const BuildingDefinition* Definition = nullptr;
            UObject* Object = nullptr;
            std::string PersistenceId;
            std::string InternalName;
            std::string AssetPath;
        };
        std::unordered_map<std::string, ActiveDefinition> activeById;
        for (const auto& definition : m_definitions)
        {
            const auto identity = Identity(definition.Owner, definition.Key);
            if (!m_applied.contains(identity)) continue;

            const auto loaded = m_buildings.find(identity);
            if (loaded == m_buildings.end() || !loaded->second) continue;
            auto* object = loaded->second;
            auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("PersistenceID")));
            auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("InternalName")));
            if (!idProperty || !nameProperty)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{} / {}' resolved as '{}' but is missing required identity field(s): PersistenceID={}, InternalName={}.\n"),
                    definition.Owner, definition.Key,
                    object->GetClassPrivate()->GetPathName(),
                    idProperty ? STR("yes") : STR("no"),
                    nameProperty ? STR("yes") : STR("no"));
                return false;
            }
            const auto idValue = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object));
            const auto nameValue = nameProperty->GetPropertyValue(
                nameProperty->ContainerPtrToValuePtr<void>(object));
            ActiveDefinition active{ &definition, object, RC::to_string(*idValue),
                RC::to_string(*nameValue), RC::to_string(object->GetPathName()) };
            if (active.PersistenceId.empty()
                || !activeById.emplace(active.PersistenceId, active).second)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry protection found a duplicate/empty custom PersistenceID.\n"));
                return false;
            }
        }
        std::unordered_map<UObject*, std::string> activeIdsByObject;
        for (const auto& [id, active] : activeById)
        {
            activeIdsByObject.emplace(active.Object, id);
        }

        std::vector<UObject*> current;
        std::vector<std::string> currentIds;
        std::unordered_map<std::string, int32> currentIndexById;
        for (int32 index = 0; index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object,
                static_cast<uint8*>(array->GetData()) + index * elementSize,
                sizeof(object));
            if (!object) return false;
            auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("PersistenceID")));
            if (!idProperty) return false;
            const auto idValue = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object));
            const auto id = RC::to_string(*idValue);
            if (id.empty() || !currentIndexById.emplace(id, index).second)
            {
                PS::Log<LogLevel::Error>(
                    STR("The live Building registry contains a duplicate/empty PersistenceID; protection aborted.\n"));
                return false;
            }
            current.push_back(object);
            currentIds.push_back(id);
        }
        nlohmann::json manifest;
        bool created = false;
        if (std::filesystem::exists(m_worldManifestPath))
        {
            try
            {
                std::ifstream input(m_worldManifestPath);
                manifest = nlohmann::json::parse(input, nullptr, true, true);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json is corrupt; registry protection was not applied: {}\n"),
                    PS::ToWideSafe(error.what()));
                return false;
            }
        }
        else
        {
            if (activeById.empty()) return true;
            created = true;
            manifest = {
                { "FormatVersion", BuildingManifestVersion },
                { "VanillaDefinitionCount", 0 },
                { "OrderedVanillaFingerprint", "" },
                { "Records", nlohmann::json::array() },
            };
        }

        if (!manifest.is_object()
            || manifest.value("FormatVersion", 0) != BuildingManifestVersion
            || !manifest.contains("VanillaDefinitionCount")
            || (!manifest["VanillaDefinitionCount"].is_number_unsigned()
                && (!manifest["VanillaDefinitionCount"].is_number_integer()
                    || manifest["VanillaDefinitionCount"].get<int64_t>() < 0))
            || !manifest.contains("OrderedVanillaFingerprint")
            || !manifest["OrderedVanillaFingerprint"].is_string()
            || !manifest.contains("Records") || !manifest["Records"].is_array())
        {
            PS::Log<LogLevel::Error>(
                STR("CustomBuildingData.json has an unsupported or invalid schema; registry untouched.\n"));
            return false;
        }

        std::vector<UObject*> vanilla;
        for (int32 index = 0; index < static_cast<int32>(current.size()); ++index)
        {
            if (!activeById.contains(currentIds[index])) vanilla.push_back(current[index]);
        }
        const auto vanillaFingerprint = OrderedFingerprint(vanilla);
        if (vanillaFingerprint.empty())
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry fingerprint could not be generated; registry untouched.\n"));
            return false;
        }

        if (created)
        {
            manifest["VanillaDefinitionCount"] = vanilla.size();
            manifest["OrderedVanillaFingerprint"] = vanillaFingerprint;
            std::unordered_set<int32> usedIndices;
            int32 nextIndex = static_cast<int32>(current.size());
            for (const auto& definition : m_definitions)
            {
                const auto identity = Identity(definition.Owner, definition.Key);
                if (!m_applied.contains(identity)) continue;

                const auto loaded = m_buildings.find(identity);
                if (loaded == m_buildings.end() || !loaded->second) continue;
                auto* object = loaded->second;
                const auto& active = activeById.at(activeIdsByObject.at(object));
                auto foundIndex = currentIndexById.find(active.PersistenceId);
                int32 index = foundIndex == currentIndexById.end() ? nextIndex++ : foundIndex->second;
                if (!usedIndices.insert(index).second)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building registry history produced a duplicate custom index; registry untouched.\n"));
                    return false;
                }
                manifest["Records"].push_back({
                    { "Owner", RC::to_string(definition.Owner) },
                    { "Key", RC::to_string(definition.Key) },
                    { "PersistenceID", active.PersistenceId },
                    { "InternalName", active.InternalName },
                    { "AssetPath", active.AssetPath },
                    { "HistoricalIndex", index },
                    { "State", "active" },
                });
            }
        }
        else if (manifest["VanillaDefinitionCount"].get<size_t>() != vanilla.size()
            || manifest["OrderedVanillaFingerprint"].get<std::string>() != vanillaFingerprint)
        {
            PS::Log<LogLevel::Error>(
                STR("Incompatible vanilla Building registry detected; world registry reconstruction aborted.\n"));
            return false;
        }

        std::unordered_set<std::string> recordIds;
        std::unordered_set<std::string> recordOwners;
        std::unordered_set<int32> recordIndices;
        auto& records = manifest["Records"];
        for (auto& record : records)
        {
            if (!record.is_object() || !record.contains("Owner") || !record["Owner"].is_string()
                || !record.contains("Key") || !record["Key"].is_string()
                || !record.contains("PersistenceID") || !record["PersistenceID"].is_string()
                || !record.contains("InternalName") || !record["InternalName"].is_string()
                || !record.contains("AssetPath") || !record["AssetPath"].is_string()
                || !record.contains("HistoricalIndex") || !record["HistoricalIndex"].is_number_integer()
                || !record.contains("State") || !record["State"].is_string())
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json contains an invalid record; registry untouched.\n"));
                return false;
            }
            const auto id = record["PersistenceID"].get<std::string>();
            const auto ownerKey = record["Owner"].get<std::string>() + "\n"
                + record["Key"].get<std::string>();
            const auto index = record["HistoricalIndex"].get<int32>();
            const auto state = record["State"].get<std::string>();
            if (id.empty() || index < 0 || (state != "active" && state != "retired")
                || !recordIds.insert(id).second || !recordOwners.insert(ownerKey).second
                || !recordIndices.insert(index).second)
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json contains duplicate or invalid records; registry untouched.\n"));
                return false;
            }
        }
        const auto historicalSize = static_cast<int32>(vanilla.size() + records.size());
        if (std::any_of(recordIndices.begin(), recordIndices.end(),
                [&](int32 index) { return index >= historicalSize; }))
        {
            PS::Log<LogLevel::Error>(
                STR("CustomBuildingData.json contains an out-of-range historical index; registry untouched.\n"));
            return false;
        }

        for (const auto& [id, active] : activeById)
        {
            if (recordIds.contains(id)) continue;

            const auto ownerKey = RC::to_string(active.Definition->Owner) + "\n"
                + RC::to_string(active.Definition->Key);
            if (recordOwners.contains(ownerKey))
            {
                PS::Log<LogLevel::Error>(
                    STR("Custom Building '{} / {}' conflicts with an existing historical identity; registry untouched.\n"),
                    active.Definition->Owner, active.Definition->Key);
                return false;
            }

            const auto nextIndex = static_cast<int32>(vanilla.size() + records.size());
            records.push_back({
                { "Owner", RC::to_string(active.Definition->Owner) },
                { "Key", RC::to_string(active.Definition->Key) },
                { "PersistenceID", id }, { "InternalName", active.InternalName },
                { "AssetPath", active.AssetPath }, { "HistoricalIndex", nextIndex },
                { "State", "active" },
            });
            recordIds.insert(id);
            recordOwners.insert(ownerKey);
            recordIndices.insert(nextIndex);
        }

        const auto totalSize = static_cast<int32>(vanilla.size() + records.size());
        if (totalSize > std::numeric_limits<uint16>::max())
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry exceeds the native index limit; registry untouched.\n"));
            return false;
        }
        for (const auto& record : records)
        {
            const auto id = record["PersistenceID"].get<std::string>();
            if (auto active = activeById.find(id); active != activeById.end())
            {
                const auto& value = active->second;
                if (record["Owner"].get<std::string>() != RC::to_string(value.Definition->Owner)
                    || record["Key"].get<std::string>() != RC::to_string(value.Definition->Key)
                    || record["AssetPath"].get<std::string>() != value.AssetPath
                    || record["InternalName"].get<std::string>() != value.InternalName)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Returning custom Building '{}' conflicts with its historical record; replacement aborted.\n"),
                        RC::to_generic_string(id));
                    return false;
                }
            }
        }
        std::vector<UObject*> desired(totalSize, nullptr);
        m_retiredBuildings.clear();
        int retired = 0;
        int reclaimed = 0;
        for (auto& record : records)
        {
            const auto id = record["PersistenceID"].get<std::string>();
            const auto index = record["HistoricalIndex"].get<int32>();
            if (index < 0 || index >= totalSize || desired[index])
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json contains a conflicting historical index; registry untouched.\n"));
                return false;
            }
            UObject* object = nullptr;
            if (auto active = activeById.find(id); active != activeById.end())
            {
                const auto& value = active->second;
                object = value.Object;
                if (record["State"].get<std::string>() == "retired") ++reclaimed;
                record["State"] = "active";
            }
            else
            {
                record["State"] = "retired";
                ++retired;
                object = CreateRetiredBuilding(record, index);
                if (!object)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Retired Building creation failed; registry untouched.\n"));
                    return false;
                }
                object->SetRootSet();
                m_retiredBuildings.push_back(object);
                PS::Log<LogLevel::Verbose>(STR("Created retired Building '{}' at historical index {}.\n"),
                    RC::to_generic_string(id), index);
            }
            desired[index] = object;
        }
        auto vanillaIterator = vanilla.begin();
        for (auto& slot : desired)
        {
            if (!slot)
            {
                if (vanillaIterator == vanilla.end())
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building registry history does not match the vanilla registry; reconstruction aborted.\n"));
                    return false;
                }
                slot = *vanillaIterator++;
            }
        }
        if (vanillaIterator != vanilla.end())
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry history does not match the vanilla registry; reconstruction aborted.\n"));
            return false;
        }

        if (!CaptureNativeRegistry(subsystem))
        {
            return false;
        }

        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        arrayHelper.Empty();
        for (auto* object : desired)
        {
            UECustom::FManagedValue value;
            arrayHelper.InitializeValue(value);
            std::memcpy(value.GetData(), &object, sizeof(object));
            arrayHelper.Add(value);
        }

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        std::vector<UObject*> reverseKeys;
        reverse.ForEachPair([&](void* key, void*) {
            UObject* object = nullptr;
            std::memcpy(&object, key, sizeof(object));
            reverseKeys.push_back(object);
        });
        // Remove backwards because Unreal maps retain sparse slots.
        for (auto iterator = reverseKeys.rbegin(); iterator != reverseKeys.rend(); ++iterator)
        {
            auto* object = *iterator;
            reverse.Remove(&object);
        }

        const auto addStringMap = [&](FMapProperty* property, const FString& key, UObject* object) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            UECustom::FManagedValue pair;
            map.InitializePair(pair);
            *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
            std::memcpy(map.GetValuePtr(pair.GetData()), &object, sizeof(object));
            map.Add(pair);
            map.Rehash();
        };

        for (int32 index = 0; index < static_cast<int32>(desired.size()); ++index)
        {
            auto* object = desired[index];
            const auto netIndex = static_cast<uint16>(index);
            UECustom::FManagedValue pair;
            reverse.InitializePair(pair);
            std::memcpy(reverse.GetKeyPtr(pair.GetData()), &object, sizeof(object));
            std::memcpy(reverse.GetValuePtr(pair.GetData()), &netIndex, sizeof(netIndex));
            reverse.Add(pair);

            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("PersistenceID")));
            auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("InternalName")));
            if (!indexProperty || !idProperty || !nameProperty) return false;
            indexProperty->SetIntPropertyValue(
                indexProperty->ContainerPtrToValuePtr<void>(object),
                static_cast<int64>(index));
            if (recordIndices.contains(index))
            {
                const auto id = idProperty->GetPropertyValue(
                    idProperty->ContainerPtrToValuePtr<void>(object));
                const auto name = nameProperty->GetPropertyValue(
                    nameProperty->ContainerPtrToValuePtr<void>(object));
                addStringMap(persistenceMapProperty, id, object);
                addStringMap(internalMapProperty, id, object);
                if (name.GetCharArray().Num() > 1) addStringMap(internalMapProperty, name, object);
            }
        }
        reverse.Rehash();

        bool valid = array->Num() == static_cast<int32>(desired.size());
        for (int32 index = 0; index < array->Num(); ++index)
        {
            auto* object = desired[index];
            int32 reverseIndex = -1;
            reverse.ForEachPair([&](void* key, void* value) {
                UObject* candidate = nullptr;
                std::memcpy(&candidate, key, sizeof(candidate));
                if (candidate == object)
                {
                    uint16 found = 0;
                    std::memcpy(&found, value, sizeof(found));
                    reverseIndex = static_cast<int32>(found);
                }
            });
            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            const auto reported = static_cast<int32>(indexProperty->GetSignedIntPropertyValue(
                indexProperty->ContainerPtrToValuePtr<void>(object)));
            UObject* forward = nullptr;
            std::memcpy(&forward,
                static_cast<uint8*>(array->GetData()) + index * elementSize,
                sizeof(forward));
            valid = valid && forward == object && reverseIndex == index && reported == index;
        }
        const auto stringMapMatches = [&](FMapProperty* property,
                const FString& key, UObject* expected) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            int matches = 0;
            map.ForEachPair([&](void* mapKey, void* mapValue) {
                if (static_cast<FString*>(mapKey)->Equals(key))
                {
                    UObject* value = nullptr;
                    std::memcpy(&value, mapValue, sizeof(value));
                    if (value == expected) ++matches;
                }
            });
            return matches == 1;
        };
        for (const auto& record : records)
        {
            const auto index = record["HistoricalIndex"].get<int32>();
            auto* object = desired[index];
            const FString id(RC::to_generic_string(
                record["PersistenceID"].get<std::string>()).c_str());
            const FString name(RC::to_generic_string(
                record["InternalName"].get<std::string>()).c_str());
            valid = valid && stringMapMatches(persistenceMapProperty, id, object)
                && stringMapMatches(internalMapProperty, id, object)
                && (name.GetCharArray().Num() <= 1
                    || stringMapMatches(internalMapProperty, name, object));
        }
        for (const auto& [id, active] : activeById)
        {
            int32 matches = 0;
            int32 installedIndex = -1;
            for (int32 index = 0; index < array->Num(); ++index)
            {
                UObject* candidate = nullptr;
                std::memcpy(&candidate,
                    static_cast<uint8*>(array->GetData()) + index * elementSize,
                    sizeof(candidate));
                if (candidate == active.Object)
                {
                    ++matches;
                    installedIndex = index;
                }
            }
            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                active.Object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            const auto reported = indexProperty ? static_cast<int32>(
                indexProperty->GetSignedIntPropertyValue(
                    indexProperty->ContainerPtrToValuePtr<void>(active.Object))) : -1;
            int32 reverseIndex = -1;
            reverse.ForEachPair([&](void* key, void* value) {
                UObject* candidate = nullptr;
                std::memcpy(&candidate, key, sizeof(candidate));
                if (candidate == active.Object)
                {
                    uint16 found = 0; std::memcpy(&found, value, sizeof(found));
                    reverseIndex = found;
                }
            });
            const bool returningValid = matches == 1 && installedIndex == reported
                && installedIndex == reverseIndex;
            valid = valid && returningValid;
        }
        if (!valid)
        {
            PS::Log<LogLevel::Error>(
                STR("Custom Building registry reconstruction validation failed.\n"));
            return false;
        }

        std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
            return left["HistoricalIndex"].template get<int32>()
                < right["HistoricalIndex"].template get<int32>();
        });
        try
        {
            std::filesystem::create_directories(m_worldManifestPath.parent_path());
            const auto temporary = m_worldManifestPath.string() + ".tmp";
            std::ofstream output(temporary, std::ios::trunc);
            output << manifest.dump(2);
            output.close();
            if (!output.good()) throw std::runtime_error("write failed");
            std::filesystem::copy_file(temporary, m_worldManifestPath,
                std::filesystem::copy_options::overwrite_existing);
            std::filesystem::remove(temporary);
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("Could not persist CustomBuildingData.json: {}\n"), PS::ToWideSafe(error.what()));
            return false;
        }

        if (reclaimed)
        {
            PS::Log<LogLevel::Normal>(STR("Buildings: {} active, {} retired, {} reclaimed, 0 errors.\n"),
                activeById.size(), retired, reclaimed);
        }
        else
        {
            PS::Log<LogLevel::Normal>(STR("Buildings: {} active, {} retired, 0 errors.\n"),
                activeById.size(), retired);
        }
        return valid;
    }

    bool DragonWildsBuildingModLoader::AddToMenu(
        UObject* building, const Placement& placement)
    {
        auto* pagesProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("Pages")));
        auto* pageProperty =
            pagesProperty ? CastField<FStructProperty>(pagesProperty->GetInner()) : nullptr;
        if (!pagesProperty || !pageProperty || !pageProperty->GetStruct())
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue Pages layout is incompatible with RuneSchema.\n"));
            return false;
        }

        auto* pages =
            pagesProperty->ContainerPtrToValuePtr<FScriptArray>(m_catalogue);
        if (!pages->IsValidIndex(placement.PageIndex))
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': menu page {} does not exist.\n"),
                building->GetName(), placement.PageIndex);
            return false;
        }

        auto* pageData = static_cast<uint8*>(pages->GetData())
            + placement.PageIndex * pageProperty->GetElementSize();
        auto* collectionsProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                pageProperty->GetStruct().Get(), TEXT("Collection")));
        auto* collectionProperty = collectionsProperty
            ? CastField<FStructProperty>(collectionsProperty->GetInner())
            : nullptr;
        if (!collectionsProperty || !collectionProperty || !collectionProperty->GetStruct())
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue Collection layout is incompatible with RuneSchema.\n"));
            return false;
        }

        auto* labelProperty = PropertyHelper::GetPropertyByName(
            collectionProperty->GetStruct().Get(), TEXT("Label"));
        auto* piecesProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                collectionProperty->GetStruct().Get(), TEXT("Collection")));
        if (!labelProperty || !piecesProperty
            || !CastField<FSoftObjectProperty>(piecesProperty->GetInner())
            || piecesProperty->GetInner()->GetElementSize()
                != sizeof(UECustom::FSoftObjectPtr))
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue item layout is incompatible with RuneSchema.\n"));
            return false;
        }

        auto* collections =
            collectionsProperty->ContainerPtrToValuePtr<FScriptArray>(pageData);
        const auto collectionSize = collectionProperty->GetElementSize();
        int32 collectionIndex = -1;

        for (int32 index = 0; index < collections->Num(); ++index)
        {
            auto* collection =
                static_cast<uint8*>(collections->GetData()) + index * collectionSize;
            auto* label = labelProperty->ContainerPtrToValuePtr<FText>(collection);
            if (label
                && PropertyHelper::GetTextAsString(*label) == placement.Collection)
            {
                collectionIndex = index;
                break;
            }
        }

        if (collectionIndex < 0)
        {
            UECustom::FScriptArrayHelper helper(collections, collectionsProperty);
            UECustom::FManagedValue value;
            helper.InitializeValue(value);
            PropertyHelper::CopyJsonValueToContainer(
                value.GetData(), labelProperty, RC::to_string(placement.Collection));
            collectionIndex = collections->Num();
            helper.Add(value);
        }

        auto* collection =
            static_cast<uint8*>(collections->GetData()) + collectionIndex * collectionSize;
        auto* pieces =
            piecesProperty->ContainerPtrToValuePtr<FScriptArray>(collection);
        const auto elementSize = piecesProperty->GetInner()->GetElementSize();

        for (int32 index = 0; index < pieces->Num(); ++index)
        {
            auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(
                static_cast<uint8*>(pieces->GetData()) + index * elementSize);
            if (SameSoftObject(*soft, building))
            {
                return true;
            }
        }

        UECustom::FScriptArrayHelper helper(pieces, piecesProperty);
        UECustom::FManagedValue value;
        helper.InitializeValue(value);
        InitializeSoftObject(value.GetData(), building);
        helper.Add(value);
        return true;
    }

    void DragonWildsBuildingModLoader::RegisterHooks()
    {
        if (m_hooksRegistered)
        {
            return;
        }

        for (auto* path : UnlockHookPaths)
        {
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr, path);
            if (!function)
            {
                PS::Log<LogLevel::Warning>(
                    STR("Building unlock hook '{}' was not found.\n"), path);
                continue;
            }

            function->RegisterPostHook(
                [](UnrealScriptFunctionCallableContext& context, void* customData) {
                    static_cast<DragonWildsBuildingModLoader*>(customData)
                        ->ApplyUnlocks(context.Context);
                },
                this);
        }

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("BuildingLoaderInitGameState");
        Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase* gameMode) {
                PrepareWorldState(gameMode);
            },
            options);

        options.HookName = TEXT("BuildingRegistryWorldTeardown");
        Hook::RegisterLoadMapPreCallback(
            [this](Hook::TCallbackIterationData<bool>&, UEngine*, FWorldContext&,
                FURL, UPendingNetGame*, FString&) {
                if (!m_nativeRegistrySnapshot.Subsystem)
                {
                    return;
                }
                RestoreNativeRegistry();
            }, options);

        m_hooksRegistered = true;
    }

    void DragonWildsBuildingModLoader::PrepareWorldState(AGameModeBase* gameMode)
    {
        if (!m_catalogue)
        {
            m_catalogue = LoadObject(CataloguePath);
        }

        auto* subsystem = FindBuildingSubsystem(gameMode);
        auto* arrayProperty = subsystem ? CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("NetIdToData"))) : nullptr;
        auto* setProperty = m_catalogue ? CastField<FSetProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("AllPiecesInCatalogue"))) : nullptr;
        if (!subsystem || !arrayProperty || !setProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings cannot be registered because the native registry is unavailable.\n"));
            return;
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        auto* catalogueIds = setProperty->ContainerPtrToValuePtr<FScriptSet>(m_catalogue);
        if (array->Num() < 700 || catalogueIds->Num() < 700)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings were not registered because the native registry is incomplete.\n"));
            return;
        }

        if (!ResolveWorldRegistryPath(gameMode)) return;

        bool protectedRegistry = false;
        try
        {
            protectedRegistry = ProtectWorldRegistry(subsystem);
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("Custom Building registry protection raised an error: {}\n"),
                PS::ToWideSafe(error.what()));
        }
        if (!protectedRegistry)
        {
            if (m_nativeRegistrySnapshot.Subsystem)
            {
                if (!RestoreNativeRegistry())
                {
                    PS::Log<LogLevel::Error>(
                        STR("Custom Building registry rollback failed; transient state was retained.\n"));
                }
            }
            else
            {
                ClearWorldRegistryState();
            }

            PS::Log<LogLevel::Error>(
                STR("Custom Building registry protection failed; Building registration was aborted.\n"));
            return;
        }

        if (auto* progress = FindProgressComponent())
        {
            ApplyUnlocks(progress);
        }
    }

    void DragonWildsBuildingModLoader::ApplyUnlocks(UObject* progressComponent)
    {
        if (!progressComponent || m_unlocks.empty())
        {
            return;
        }

        std::vector<UObject*> buildings;
        for (const auto& key : m_unlocks)
        {
            auto found = m_buildings.find(key);
            if (found != m_buildings.end() && found->second)
            {
                buildings.push_back(found->second);
            }
        }

        auto* unlockedProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                progressComponent->GetClassPrivate(), TEXT("BuildingsUnlocked")));
        if (unlockedProperty
            && CastField<FObjectProperty>(unlockedProperty->GetInner()))
        {
            auto* unlocked =
                unlockedProperty->ContainerPtrToValuePtr<FScriptArray>(progressComponent);
            const auto elementSize = unlockedProperty->GetInner()->GetElementSize();
            UECustom::FScriptArrayHelper helper(unlocked, unlockedProperty);

            for (auto* building : buildings)
            {
                bool exists = false;
                for (int32 index = 0; index < unlocked->Num(); ++index)
                {
                    UObject* current = nullptr;
                    std::memcpy(
                        &current,
                        static_cast<uint8*>(unlocked->GetData()) + index * elementSize,
                        sizeof(current));
                    if (current == building)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    UECustom::FManagedValue value;
                    helper.InitializeValue(value);
                    std::memcpy(value.GetData(), &building, sizeof(building));
                    helper.Add(value);
                }
            }
        }

        auto* sessionOnlyProperty = CastField<FSetProperty>(
            PropertyHelper::GetPropertyByName(
                progressComponent->GetClassPrivate(),
                TEXT("BuildingsUnlockedThatShouldNotPersist")));
        if (sessionOnlyProperty)
        {
            UECustom::FScriptSetHelper helper(
                sessionOnlyProperty,
                sessionOnlyProperty->ContainerPtrToValuePtr<void>(progressComponent));
            for (auto* building : buildings)
            {
                helper.Add(&building);
            }
        }
    }

    UObject* DragonWildsBuildingModLoader::FindProgressComponent() const
    {
        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            m_progressComponentClass, candidates, true);

        for (auto* candidate : candidates)
        {
            if (candidate
                && !candidate->HasAnyFlags(
                    static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                return candidate;
            }
        }

        return nullptr;
    }

    UObject* DragonWildsBuildingModLoader::FindBuildingSubsystem(
        UObject* worldContext) const
    {
        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            m_buildingPieceSubsystemClass, candidates, true);

        UObject* fallback = nullptr;
        int32 usable = 0;
        for (auto* candidate : candidates)
        {
            if (candidate && !candidate->HasAnyFlags(
                static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                fallback = candidate;
                usable++;
                if (worldContext && candidate->GetWorld() == worldContext->GetWorld())
                {
                    return candidate;
                }
            }
        }

        return usable == 1 ? fallback : nullptr;
    }

    UObject* DragonWildsBuildingModLoader::CreateRetiredBuilding(
        const nlohmann::json& record, int32 historicalIndex)
    {
        auto* stabilityTable = static_cast<UDataTable*>(LoadObject(StabilityProfilePath));
        if (!stabilityTable || !stabilityTable->GetRowStruct())
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building could not load the native stability table.\n"));
            return nullptr;
        }

        const FName stabilityRow(RetiredStabilityProfileRow);
        if (!stabilityTable->FindRowUnchecked(stabilityRow))
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building stability profile row '{}' is unavailable.\n"),
                RetiredStabilityProfileRow);
            return nullptr;
        }

        auto* object = ActorHelper::ConstructTransientObject(m_buildingPieceClass,
            std::format(STR("RuneSchema_RetiredBuilding_{}"), historicalIndex));
        if (!object)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building construction failed.\n"));
            return nullptr;
        }
        auto* objectClass = object->GetClassPrivate();
        auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("PersistenceID")));
        auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("InternalName")));
        auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildingPieceDataIndex")));
        auto* stabilityProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildingStabilityProfileRowHandle")));
        auto* pieceTagProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("PieceTag")));
        auto* representationProperty = PropertyHelper::GetPropertyByName(
            objectClass, TEXT("RepresentationCategory"));
        auto* actorProperty = CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildableActor")));
        auto* proxyProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildingPieceProxyData")));
        auto* farAwayProperty = CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("bShouldBeVisibleFromFarAway")));
        if (!idProperty || !nameProperty || !indexProperty || !stabilityProperty
            || !pieceTagProperty || !representationProperty || !actorProperty
            || !proxyProperty || !farAwayProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building property layout is incompatible with RuneSchema.\n"));
            return nullptr;
        }

        auto* stabilityStruct = stabilityProperty->GetStruct().Get();
        auto* tableProperty = stabilityStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(stabilityStruct, TEXT("DataTable"))) : nullptr;
        auto* rowProperty = stabilityStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(stabilityStruct, TEXT("RowName"))) : nullptr;
        auto* tagStruct = pieceTagProperty->GetStruct().Get();
        auto* tagNameProperty = tagStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(tagStruct, TEXT("TagName"))) : nullptr;
        auto* proxyStruct = proxyProperty->GetStruct().Get();
        auto* proxyMeshProperty = proxyStruct ? CastField<FSoftObjectProperty>(
            PropertyHelper::GetPropertyByName(proxyStruct, TEXT("ProxyMesh"))) : nullptr;
        if (!tableProperty || !rowProperty || !tagNameProperty || !proxyMeshProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building nested property layout is incompatible with RuneSchema.\n"));
            return nullptr;
        }

        const FString historicalId(RC::to_generic_string(
            record["PersistenceID"].get<std::string>()).c_str());
        const FString historicalName(RC::to_generic_string(
            record["InternalName"].get<std::string>()).c_str());
        idProperty->SetPropertyValue(idProperty->ContainerPtrToValuePtr<void>(object), historicalId);
        nameProperty->SetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(object), historicalName);
        indexProperty->SetIntPropertyValue(
            indexProperty->ContainerPtrToValuePtr<void>(object),
            static_cast<int64>(historicalIndex));

        auto* stability = stabilityProperty->ContainerPtrToValuePtr<void>(object);
        std::memcpy(tableProperty->ContainerPtrToValuePtr<void>(stability),
            &stabilityTable, sizeof(stabilityTable));
        rowProperty->SetPropertyValue(rowProperty->ContainerPtrToValuePtr<void>(stability),
            stabilityRow);
        PropertyHelper::CopyJsonValueToContainer(
            object, representationProperty, "ManagedActor");
        PropertyHelper::CopyJsonValueToContainer(object, farAwayProperty, false);

        const auto& actor = *actorProperty->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(object);
        auto* proxy = proxyProperty->ContainerPtrToValuePtr<void>(object);
        const auto& proxyMesh = *proxyMeshProperty->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(proxy);
        auto* tag = pieceTagProperty->ContainerPtrToValuePtr<void>(object);
        const auto tagName = tagNameProperty->GetPropertyValue(
            tagNameProperty->ContainerPtrToValuePtr<void>(tag));
        UObject* verifiedTable = nullptr;
        std::memcpy(&verifiedTable, tableProperty->ContainerPtrToValuePtr<void>(stability),
            sizeof(verifiedTable));
        const auto verifiedRow = rowProperty->GetPropertyValue(
            rowProperty->ContainerPtrToValuePtr<void>(stability));
        const auto actorPathEmpty = actor.ObjectID.AssetPath.GetPackageName() == NAME_None
            && actor.ObjectID.AssetPath.GetAssetName() == NAME_None;
        const auto proxyPathEmpty = proxyMesh.ObjectID.AssetPath.GetPackageName() == NAME_None
            && proxyMesh.ObjectID.AssetPath.GetAssetName() == NAME_None;
        const auto farAway = farAwayProperty->GetPropertyValue(
            farAwayProperty->ContainerPtrToValuePtr<void>(object));
        auto* representationEnum = CastField<FEnumProperty>(representationProperty);
        auto* representationNumeric = representationEnum
            ? representationEnum->GetUnderlyingProperty()
            : CastField<FNumericProperty>(representationProperty);
        const auto representation = representationNumeric
            ? representationNumeric->GetSignedIntPropertyValue(
                representationProperty->ContainerPtrToValuePtr<void>(object))
            : int64{-1};
        if (verifiedTable != stabilityTable || verifiedRow != stabilityRow
            || !stabilityTable->FindRowUnchecked(verifiedRow)
            || !actorPathEmpty || !proxyPathEmpty || !tagName.IsNone() || farAway
            || representation != 1)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building critical-field validation failed.\n"));
            return nullptr;
        }

        return object;
    }

    bool DragonWildsBuildingModLoader::CaptureNativeRegistry(UObject* subsystem)
    {
        if (m_nativeRegistrySnapshot.Subsystem)
        {
            if (m_nativeRegistrySnapshot.Subsystem == subsystem)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry protection refused to snapshot an already reconstructed subsystem.\n"));
            }
            else
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry protection found stale world state from another subsystem.\n"));
            }
            return false;
        }

        auto* cls = subsystem ? subsystem->GetClassPrivate() : nullptr;
        auto* arrayProperty = cls ? CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("NetIdToData"))) : nullptr;
        auto* reverseProperty = cls ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("DataToNetIdMap"))) : nullptr;
        auto* persistenceProperty = cls ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("PersistenceIDToDataMap"))) : nullptr;
        auto* internalProperty = cls ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("InternalNameToDataMap"))) : nullptr;
        if (!arrayProperty || !reverseProperty || !persistenceProperty || !internalProperty)
        {
            return false;
        }

        NativeRegistrySnapshot snapshot;
        snapshot.Subsystem = subsystem;
        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        const auto elementSize = arrayProperty->GetInner()->GetElementSize();
        for (int32 index = 0; index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object, static_cast<uint8*>(array->GetData()) + index * elementSize,
                sizeof(object));
            if (!object) return false;
            snapshot.NetIdToData.push_back(object);
            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            if (!indexProperty) return false;
            snapshot.BuildingPieceDataIndices.emplace(object, static_cast<int32>(
                indexProperty->GetSignedIntPropertyValue(
                    indexProperty->ContainerPtrToValuePtr<void>(object))));
        }

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        reverse.ForEachPair([&](void* key, void* value) {
            UObject* object = nullptr;
            uint16 index = 0;
            std::memcpy(&object, key, sizeof(object));
            std::memcpy(&index, value, sizeof(index));
            snapshot.DataToNetIdMap.emplace_back(object, index);
        });
        const auto captureStringMap = [&](FMapProperty* property,
                std::vector<RegistryStringMapEntry>& entries) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            map.ForEachPair([&](void* key, void* value) {
                UObject* object = nullptr;
                std::memcpy(&object, value, sizeof(object));
                entries.push_back({ *static_cast<FString*>(key), object });
            });
        };
        captureStringMap(persistenceProperty, snapshot.PersistenceIDToDataMap);
        captureStringMap(internalProperty, snapshot.InternalNameToDataMap);
        m_nativeRegistrySnapshot = std::move(snapshot);
        return true;
    }

    bool DragonWildsBuildingModLoader::RestoreNativeRegistry()
    {
        auto* subsystem = m_nativeRegistrySnapshot.Subsystem;
        if (!subsystem) return true;
        auto* cls = subsystem->GetClassPrivate();
        auto* arrayProperty = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("NetIdToData")));
        auto* reverseProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("DataToNetIdMap")));
        auto* persistenceProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("PersistenceIDToDataMap")));
        auto* internalProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("InternalNameToDataMap")));
        if (!arrayProperty || !reverseProperty || !persistenceProperty || !internalProperty)
            return false;

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        arrayHelper.Empty();
        for (auto* object : m_nativeRegistrySnapshot.NetIdToData)
        {
            UECustom::FManagedValue value;
            arrayHelper.InitializeValue(value);
            std::memcpy(value.GetData(), &object, sizeof(object));
            arrayHelper.Add(value);
        }

        const auto clearObjectMap = [&](FMapProperty* property) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            std::vector<UObject*> keys;
            map.ForEachPair([&](void* key, void*) {
                UObject* object = nullptr; std::memcpy(&object, key, sizeof(object));
                keys.push_back(object);
            });
            // Remove backwards because Unreal maps retain sparse slots.
            for (auto iterator = keys.rbegin(); iterator != keys.rend(); ++iterator)
            {
                auto* key = *iterator;
                map.Remove(&key);
            }
        };
        const auto clearStringMap = [&](FMapProperty* property) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            std::vector<FString> keys;
            map.ForEachPair([&](void* key, void*) { keys.push_back(*static_cast<FString*>(key)); });
            for (auto iterator = keys.rbegin(); iterator != keys.rend(); ++iterator)
            {
                map.Remove(&*iterator);
            }
        };
        clearObjectMap(reverseProperty);
        clearStringMap(persistenceProperty);
        clearStringMap(internalProperty);

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        for (const auto& [object, index] : m_nativeRegistrySnapshot.DataToNetIdMap)
        {
            UECustom::FManagedValue pair; reverse.InitializePair(pair);
            std::memcpy(reverse.GetKeyPtr(pair.GetData()), &object, sizeof(object));
            std::memcpy(reverse.GetValuePtr(pair.GetData()), &index, sizeof(index));
            reverse.Add(pair);
        }
        reverse.Rehash();
        const auto restoreStringMap = [&](FMapProperty* property,
                const std::vector<RegistryStringMapEntry>& entries) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            for (const auto& entry : entries)
            {
                UECustom::FManagedValue pair; map.InitializePair(pair);
                *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = entry.Key;
                std::memcpy(map.GetValuePtr(pair.GetData()), &entry.Value, sizeof(entry.Value));
                map.Add(pair);
            }
            map.Rehash();
        };
        restoreStringMap(persistenceProperty, m_nativeRegistrySnapshot.PersistenceIDToDataMap);
        restoreStringMap(internalProperty, m_nativeRegistrySnapshot.InternalNameToDataMap);
        for (const auto& [object, index] : m_nativeRegistrySnapshot.BuildingPieceDataIndices)
        {
            auto* property = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            property->SetIntPropertyValue(property->ContainerPtrToValuePtr<void>(object),
                static_cast<int64>(index));
        }

        size_t reverseCount = 0;
        size_t persistenceCount = 0;
        size_t internalCount = 0;
        reverse.ForEachPair([&](void*, void*) { ++reverseCount; });
        UECustom::FScriptMapHelper restoredPersistence(
            persistenceProperty, persistenceProperty->ContainerPtrToValuePtr<void>(subsystem));
        restoredPersistence.ForEachPair([&](void*, void*) { ++persistenceCount; });
        UECustom::FScriptMapHelper restoredInternal(
            internalProperty, internalProperty->ContainerPtrToValuePtr<void>(subsystem));
        restoredInternal.ForEachPair([&](void*, void*) { ++internalCount; });
        bool valid = array->Num() == static_cast<int32>(m_nativeRegistrySnapshot.NetIdToData.size())
            && reverseCount == m_nativeRegistrySnapshot.DataToNetIdMap.size()
            && persistenceCount == m_nativeRegistrySnapshot.PersistenceIDToDataMap.size()
            && internalCount == m_nativeRegistrySnapshot.InternalNameToDataMap.size();
        for (int32 index = 0; valid && index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object,
                static_cast<uint8*>(array->GetData())
                    + index * arrayProperty->GetInner()->GetElementSize(),
                sizeof(object));
            valid = object == m_nativeRegistrySnapshot.NetIdToData[index];
        }

        if (!valid)
        {
            PS::Log<LogLevel::Error>(
                STR("Native Building registry restoration audit failed; "
                    "retired Buildings and snapshot retained for safety.\n"));
            return false;
        }
        ClearWorldRegistryState();
        return true;
    }

    void DragonWildsBuildingModLoader::ClearWorldRegistryState()
    {
        const auto retired = m_retiredBuildings.size();
        for (auto* building : m_retiredBuildings)
        {
            if (building && building->IsRootSet()) building->ClearRootSet();
        }
        m_retiredBuildings.clear();
        m_nativeRegistrySnapshot = {};
        m_worldManifestPath.clear();
        if (retired)
        {
            PS::Log<LogLevel::Verbose>(
                STR("Released {} retired Building{} and cleared world registry state.\n"),
                retired, retired == 1 ? STR("") : STR("s"));
        }
    }

    UObject* DragonWildsBuildingModLoader::LoadObject(
        const RC::StringType& path) const
    {
        if (auto* object = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, path.c_str(), false))
        {
            return object;
        }

        UECustom::TSoftObjectPtr<UObject> soft{
            UECustom::FSoftObjectPath(path)
        };
        return UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
    }

    RC::StringType DragonWildsBuildingModLoader::Identity(
        const RC::StringType& owner, const RC::StringType& key)
    {
        return owner + TEXT("\n") + key;
    }
}
