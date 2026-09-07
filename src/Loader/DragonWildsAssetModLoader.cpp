#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <string_view>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Helpers/String.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/ConsumeQueue.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsAssetModLoader.h"
#include "Core/JsonPatchDirective.h"
#include "Loader/PlayerGhost.h"

using namespace RC;
using namespace RC::Unreal;

namespace
{
    bool ReadRequiredString(const nlohmann::json& body, const char* name,
        std::string& out)
    {
        if (!body.contains(name) || !body.at(name).is_string()) return false;
        out = body.at(name).get<std::string>();
        return !out.empty();
    }

    bool IsCanonicalPersistenceId(std::string_view value)
    {
        if (value.size() != 22) return false;
        for (const auto character : value)
        {
            const auto byte = static_cast<unsigned char>(character);
            if (!std::isalnum(byte) && character != '-' && character != '_')
                return false;
        }
        const auto last = value.back();
        return last == 'A' || last == 'Q' || last == 'g' || last == 'w';
    }

    std::string SanitizePackageSegment(std::string_view value,
        std::string_view fallback)
    {
        std::string result;
        result.reserve(value.size());
        bool previousUnderscore = false;
        for (const auto character : value)
        {
            const auto byte = static_cast<unsigned char>(character);
            const auto normalized = (std::isalnum(byte) || character == '_')
                ? character : '_';
            if (normalized == '_' && previousUnderscore) continue;
            result.push_back(normalized);
            previousUnderscore = normalized == '_';
        }
        while (!result.empty() && result.front() == '_') result.erase(result.begin());
        while (!result.empty() && result.back() == '_') result.pop_back();
        if (result.empty()) result.assign(fallback);
        return result;
    }

    void ClearItemIdentity(UObject* object, UClass* objectClass)
    {
        if (!object || !objectClass) return;
        for (const auto* name : { TEXT("PersistenceID"), TEXT("InternalName") })
        {
            auto* property = DragonWilds::PropertyHelper::CastProperty<FStrProperty>(
                DragonWilds::PropertyHelper::GetPropertyByName(objectClass, name));
            if (property)
                property->SetPropertyValue(
                    property->ContainerPtrToValuePtr<void>(object), FString{});
        }
    }

    bool MapContainsOther(FMapProperty* mapProperty, UObject* subsystem,
        const FString& key, UObject* item)
    {
        if (!mapProperty || !subsystem || key.GetCharArray().Num() <= 1)
            return false;
        bool collision = false;
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        map.ForEachPair([&](void* keyPointer, void* valuePointer) {
            if (collision || *static_cast<FString*>(keyPointer) != key) return;
            UObject* mapped = nullptr;
            std::memcpy(&mapped, valuePointer, sizeof(mapped));
            collision = mapped && mapped != item;
        });
        return collision;
    }

    void AddMapEntry(FMapProperty* mapProperty, UObject* subsystem,
        const FString& key, UObject* item)
    {
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        UECustom::FManagedValue pair;
        map.InitializePair(pair);
        *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
        std::memcpy(map.GetValuePtr(pair.GetData()), &item, sizeof(item));
        map.Add(pair);
        map.Rehash();
    }
}

namespace DragonWilds {
    DragonWildsAssetModLoader::DragonWildsAssetModLoader() : DragonWildsModLoaderBase("assets")
    {
        SetDisplayName(TEXT("Asset Mod Loader"));
    }

    DragonWildsAssetModLoader::~DragonWildsAssetModLoader()
    {
        std::scoped_lock lock{m_mutex};
        m_pendingAssets.clear();
        m_pendingPatches.clear();
        m_createdAssetsByTarget.clear();
        m_createdAssets.clear();
    }

    void DragonWildsAssetModLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                QueueData(data, modName);
            });
        }
        else if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            TryApplyPending();
            ApplyPendingPatches();
            ReportUnresolvedAssets();
        }
    }

    void DragonWildsAssetModLoader::OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            QueueData(data, modName);
        });

        TryApplyPending();
        ApplyPendingPatches();
        ReportUnresolvedAssets();
    }

    bool DragonWildsAssetModLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit;
    }

    bool DragonWildsAssetModLoader::OnInitialize()
    {
        m_dataAssetClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.DataAsset"), false);

        if (!m_dataAssetClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Engine.DataAsset.\n"), GetDisplayName());
            return false;
        }

        m_curveBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.CurveBase"), false);

        if (!m_curveBaseClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Engine.CurveBase.\n"), GetDisplayName());
            return false;
        }

        m_itemDataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemData"), false);

        if (!m_itemDataClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Dominion.ItemData.\n"), GetDisplayName());
            return false;
        }

        return true;
    }

    void DragonWildsAssetModLoader::QueueData(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (!data.is_object())
        {
            PS::Log<LogLevel::Error>(STR("JSON root must be an object.\n"));
            return;
        }

        std::scoped_lock lock{m_mutex};
        for (auto& [target, properties] : data.items())
        {
            if (target.starts_with("$"))
            {
                continue;
            }

            if (!properties.is_object())
            {
                PS::Log<LogLevel::Error>(STR("Target '{}' must contain an object of properties. Skipping.\n"),
                    RC::to_generic_string(target));
                continue;
            }

            nlohmann::json normalizedProperties = properties;
            std::string effectiveTarget = target;
            bool isPatch = false;
            try
            {
                static constexpr std::array<std::string_view, 2> protectedIdentity{
                    "PersistenceID", "InternalName"};
                if (const auto patch = JsonPatchDirective::Parse(
                        properties, protectedIdentity, "asset"))
                {
                    effectiveTarget = patch->Reference;
                    normalizedProperties = patch->Changes;
                    isPatch = true;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Asset patch '{}': {}. Skipping.\n"),
                    RC::to_generic_string(target), PS::ToWideSafe(error.what()));
                continue;
            }

            if (normalizedProperties.contains("$clone") || normalizedProperties.contains("$CloneFrom"))
            {
                PS::Log<LogLevel::Error>(STR("Target '{}': use the exact case-sensitive $Clone directive. Skipping.\n"),
                    RC::to_generic_string(target));
                continue;
            }
            if (normalizedProperties.contains("$Clone")
                && (!normalizedProperties.at("$Clone").is_string()
                    || normalizedProperties.at("$Clone").get_ref<const std::string&>().empty()))
            {
                PS::Log<LogLevel::Error>(STR("Target '{}': $Clone must contain a baked ItemData object path. Skipping.\n"),
                    RC::to_generic_string(target));
                continue;
            }

            auto targetWide = RC::to_generic_string(effectiveTarget);
            auto pending = PendingAsset{
                targetWide,
                NormalizeObjectPath(targetWide),
                modName,
                std::move(normalizedProperties), isPatch
            };
            (isPatch ? m_pendingPatches : m_pendingAssets).push_back(std::move(pending));
        }
    }

    void DragonWildsAssetModLoader::ApplyPendingPatches()
    {
        {
            std::scoped_lock lock{m_mutex};
            if (m_pendingPatches.empty()) return;
            PS::Log<LogLevel::Verbose>(STR("Applying {} deferred asset $Patch document(s).\n"),
                m_pendingPatches.size());
            m_pendingAssets.insert(m_pendingAssets.end(),
                std::make_move_iterator(m_pendingPatches.begin()),
                std::make_move_iterator(m_pendingPatches.end()));
            m_pendingPatches.clear();
        }
        TryApplyPending();
    }

    void DragonWildsAssetModLoader::Apply(UObject* object, const PendingAsset& pendingAsset, LoadResult& outResult)
    {
        if (!object)
        {
            outResult.ErrorCount++;
            return;
        }

        auto* objectClass = object->GetClassPrivate();
        if (!objectClass)
        {
            outResult.ErrorCount++;
            return;
        }

        for (auto& [propertyName, propertyValue] : pendingAsset.Properties.items())
        {
            if (propertyName == "$Append" || propertyName == "$Clone")
            {
                continue;
            }

            if(propertyName == "$VisualEffect") {
                try {
                    const auto sourceHint = pendingAsset.Properties.contains("$Clone")
                        ? pendingAsset.Properties.at("$Clone").get<std::string>()
                        : RC::to_string(object->GetPathName());
                    PlayerGhost::SetItemEffect(object,propertyValue,sourceHint);
                    ++outResult.PropertiesWritten;
                }
                catch(const std::exception& error) {
                    ++outResult.ErrorCount;
                    PS::Log<LogLevel::Error>(TEXT("[{}] Invalid equipment visual: {}\n"),object->GetName(),PS::ToWideSafe(error.what()));
                }
                continue;
            }
            auto propertyNameWide = RC::to_generic_string(propertyName);
            auto* property = PropertyHelper::GetPropertyByName(objectClass, propertyNameWide);
            if (!property)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("[{}] Property '{}' was not found.\n"),
                    object->GetName(), propertyNameWide);
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(object, property, propertyValue);
                outResult.PropertiesWritten++;
            }
            catch (const std::exception& e)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("[{}] Failed writing '{}': {}\n"),
                    object->GetName(), propertyNameWide, PS::ToWideSafe(e.what()));
            }
        }

        if (pendingAsset.Properties.contains("$Append"))
        {
            AppendProperties(object, objectClass, pendingAsset.Properties.at("$Append"), outResult);
        }
    }

    void DragonWildsAssetModLoader::AppendProperties(UObject* object, UClass* objectClass, const nlohmann::json& appendData, LoadResult& outResult)
    {
        if (!appendData.is_object())
        {
            outResult.ErrorCount++;
            PS::Log<LogLevel::Error>(STR("[{}] $Append must be an object.\n"), object->GetName());
            return;
        }

        for (auto& [propertyName, items] : appendData.items())
        {
            auto propertyNameWide = RC::to_generic_string(propertyName);
            if (!items.is_array())
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("[{}] $Append.{} must be an array.\n"),
                    object->GetName(), propertyNameWide);
                continue;
            }

            auto* property = PropertyHelper::GetPropertyByName(objectClass, propertyNameWide);
            if (!property)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("[{}] Append target '{}' was not found.\n"),
                    object->GetName(), propertyNameWide);
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(object, property, PropertyHelper::BuildAppendValue(property, items));
                outResult.PropertiesWritten++;
            }
            catch (const std::exception& e)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("[{}] Failed appending to '{}': {}\n"),
                    object->GetName(), propertyNameWide, PS::ToWideSafe(e.what()));
            }
        }
    }

    void DragonWildsAssetModLoader::TryApplyPending()
    {
        struct BatchResult {
            int TargetsUpdated = 0;
            int Created = 0;
            int ClonesUpdated = 0;
            int Patched = 0;
            int PropertiesWritten = 0;
            int ErrorCount = 0;
        };

        std::map<RC::StringType, BatchResult> batchResults;
        std::scoped_lock lock{m_mutex};

        UObject* itemSubsystem = nullptr;
        bool subsystemSearched = false;
        PS::ConsumeQueue(m_pendingAssets, [&](PendingAsset& pending)
        {
            auto* it = &pending;
            UObject* object = nullptr;
            const bool isClone = it->Properties.contains("$Clone");
            bool createdNow = false;
            if (isClone && !subsystemSearched) {
                itemSubsystem = FindItemSubsystem();
                subsystemSearched = true;
            }
            try
            {
                object = Resolve(*it);
                if (object && isClone
                    && std::find(m_createdAssets.begin(), m_createdAssets.end(), object)
                        == m_createdAssets.end())
                    throw std::runtime_error(
                        "the clone target collides with an existing loaded asset");
                if (!object && isClone) {
                    object = CreateFromClone(*it, itemSubsystem);
                    createdNow = object != nullptr;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Clone '{}' from {} was rejected safely: {}\n"),
                    it->Target, it->ModName, PS::ToWideSafe(error.what()));
                batchResults[it->ModName].ErrorCount++;
                return true;
            }
            if (!object || !IsReadyForPatch(object))
            {
                return false;
            }

            if (!IsSupportedTarget(object))
            {
                auto* objectClass = object->GetClassPrivate();
                auto className = objectClass ? objectClass->GetName() : TEXT("<unknown>");
                PS::Log<LogLevel::Error>(STR("'{}' resolved to '{}' (class '{}'), which is not a DataAsset, a Curve, or a subobject owned by one. Skipping.\n"),
                    it->Target, object->GetName(), className);
                batchResults[it->ModName].ErrorCount++;
                return true;
            }

            LoadResult result{};
            Apply(object, *it, result);
            if (isClone && !RegisterCreatedItem(object, *it, itemSubsystem))
                result.ErrorCount++;

            auto& batchResult = batchResults[it->ModName];
            if (isClone) {
                if (createdNow) ++batchResult.Created;
                else ++batchResult.ClonesUpdated;
            } else if (it->IsPatch) ++batchResult.Patched;
            else ++batchResult.TargetsUpdated;
            batchResult.PropertiesWritten += result.PropertiesWritten;
            batchResult.ErrorCount += result.ErrorCount;

            return true;
        });

        for (auto& [modName, result] : batchResults)
        {
            if (result.Created || result.ClonesUpdated)
                PS::RoutineLog("assets", STR("{} $Clone: {} new, {} updated.\n"),
                    modName, result.Created, result.ClonesUpdated);
            if (result.Patched)
                PS::RoutineLog("patches", STR("{} $Patch: {} updated.\n"), modName, result.Patched);
            if (result.ErrorCount)
                PS::Log<LogLevel::Warning>(STR("{} assets: {} updated, {} properties written, {} errors.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten, result.ErrorCount);
            else if (result.TargetsUpdated)
                PS::RoutineLog("assets", STR("{} assets: {} updated, 0 errors.\n"), modName, result.TargetsUpdated);
        }
    }

    void DragonWildsAssetModLoader::ReportUnresolvedAssets()
    {
        std::scoped_lock lock{m_mutex};
        for (auto& pendingAsset : m_pendingAssets)
        {
            PS::Log<LogLevel::Error>(STR("Gave up resolving '{}'; the asset never loaded.\n"),
                pendingAsset.Target);
        }

        m_pendingAssets.clear();
    }

    UObject* DragonWildsAssetModLoader::Resolve(const PendingAsset& pendingAsset)
    {
        if (const auto created = m_createdAssetsByTarget.find(pendingAsset.Target);
            created != m_createdAssetsByTarget.end())
            return created->second;

        if (!pendingAsset.ObjectPath.empty())
        {
            auto* found = UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, pendingAsset.ObjectPath.c_str(), false);
            if (!found && !pendingAsset.Properties.contains("$Clone"))
            {
                auto soft = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(pendingAsset.ObjectPath));
                found = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            return found;
        }

        UObject* found = nullptr;
        UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
            if (object && object->GetName() == pendingAsset.Target && IsSupportedTarget(object))
            {
                found = object;
                return LoopAction::Break;
            }

            return LoopAction::Continue;
        });

        return found;
    }

    UObject* DragonWildsAssetModLoader::FindItemSubsystem() const
    {
        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemSubsystem"), false);
        if (!subsystemClass) return nullptr;

        TArray<UObject*> subsystems;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, subsystems, true);
        for (auto* subsystem : subsystems)
        {
            if (subsystem && !subsystem->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject)))
                return subsystem;
        }
        return nullptr;
    }

    UObject* DragonWildsAssetModLoader::CreateFromClone(
        const PendingAsset& pendingAsset, UObject* subsystem)
    {
        std::string internalName;
        std::string persistenceId;
        if (!ReadRequiredString(pendingAsset.Properties, "InternalName", internalName))
            throw std::runtime_error("$Clone requires a non-empty InternalName");
        if (!ReadRequiredString(pendingAsset.Properties, "PersistenceID", persistenceId)
            || !IsCanonicalPersistenceId(persistenceId))
            throw std::runtime_error(
                "$Clone requires a unique canonical 22-character PersistenceID");
        if (!subsystem)
            throw std::runtime_error("ItemSubsystem is not ready for clone registration");

        const auto sourceText = pendingAsset.Properties.at("$Clone").get<std::string>();
        const auto sourcePath = NormalizeObjectPath(
            RC::to_generic_string(sourceText));
        if (sourcePath.empty())
            throw std::runtime_error("$Clone source must be a full baked object path");

        auto* source = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, sourcePath.c_str(), false);
        if (!source)
        {
            auto soft = UECustom::TSoftObjectPtr<UObject>(
                UECustom::FSoftObjectPath(sourcePath));
            source = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
        }
        if (!source || !source->GetClassPrivate())
            throw std::runtime_error("$Clone source baked asset could not be loaded");
        if (!m_itemDataClass || !source->IsA(m_itemDataClass))
            throw std::runtime_error("$Clone source must derive from ItemData");

        const auto runtimePath = RC::to_generic_string(
            "/Game/RuneSchema/" + SanitizePackageSegment(
                RC::to_string(pendingAsset.ModName), "UnnamedMod")
            + "/Items/" + SanitizePackageSegment(internalName, "ITEM_RS_Unnamed")
            + "." + SanitizePackageSegment(internalName, "ITEM_RS_Unnamed"));
        const auto dot = runtimePath.rfind(TEXT('.'));
        if (dot == RC::StringType::npos)
            throw std::runtime_error("failed to form the runtime clone path");
        const auto packagePath = runtimePath.substr(0, dot);
        const auto objectName = runtimePath.substr(dot + 1);

        if (UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, runtimePath.c_str(), false))
            throw std::runtime_error("the runtime clone path is already occupied");

        auto* package = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, packagePath.c_str(), false);
        if (!package)
        {
            auto* packageClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/CoreUObject.Package"), false);
            if (!packageClass)
                throw std::runtime_error("the Unreal package class is unavailable");
            FStaticConstructObjectParameters packageParams(packageClass, nullptr);
            packageParams.Name = FName(packagePath, FNAME_Add);
            packageParams.SetFlags = static_cast<EObjectFlags>(
                RF_Public | RF_Standalone | RF_Transactional);
            package = UObjectGlobals::StaticConstructObject<UObject*>(packageParams);
        }
        if (!package)
            throw std::runtime_error("failed to create the runtime clone package");
        package->SetRootSet();
        m_createdAssets.push_back(package);

        FStaticConstructObjectParameters params(source->GetClassPrivate(), package);
        params.Name = FName(objectName, FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(
            RF_Public | RF_Standalone | RF_Transactional);
        auto* created = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!created)
            throw std::runtime_error("Dragonwilds refused to construct the clone");

        constexpr std::uint64_t unsafeFlags =
            CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient
            | CPF_InstancedReference | CPF_ContainsInstancedReference
            | CPF_Deprecated | CPF_EditorOnly;
        std::size_t copied = 0;
        for (auto* property : TFieldRange<FProperty>(
                 source->GetClassPrivate(), EFieldIterationFlags::Default))
        {
            if (!property || property->HasAnyPropertyFlags(unsafeFlags)) continue;
            property->CopyCompleteValue_InContainer(created, source);
            ++copied;
        }

        ClearItemIdentity(created, created->GetClassPrivate());

        // Use the clone InternalName as its stats row; preserve the source table.
        if (auto* rowHandle = PropertyHelper::GetPropertyByName(
                created->GetClassPrivate(),
                TEXT("WearableEquipmentDataTableRowHandle")))
        {
            PropertyHelper::CopyJsonValueToContainer(
                created, rowHandle,
                nlohmann::json{{"RowName", internalName}});
        }

        created->SetRootSet();
        m_createdAssets.push_back(created);
        m_createdAssetsByTarget[pendingAsset.Target] = created;
        m_createdAssetsByTarget[runtimePath] = created;

        PS::Log<LogLevel::Verbose>(
            STR("{}: cloned baked item '{}' to new runtime item '{}' using {} reflected properties.\n"),
            pendingAsset.ModName, source->GetPathName(), runtimePath, copied);
        return created;
    }

    bool DragonWildsAssetModLoader::RegisterCreatedItem(
        UObject* item, const PendingAsset& pendingAsset, UObject* subsystem)
    {
        if (!item || !subsystem) return false;

        auto* itemClass = item->GetClassPrivate();
        auto* persistenceProperty = PropertyHelper::CastProperty<FStrProperty>(
            PropertyHelper::GetPropertyByName(itemClass, TEXT("PersistenceID")));
        auto* internalProperty = PropertyHelper::CastProperty<FStrProperty>(
            PropertyHelper::GetPropertyByName(itemClass, TEXT("InternalName")));
        auto* persistenceMap = PropertyHelper::CastProperty<FMapProperty>(
            PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
        auto* internalMap = PropertyHelper::CastProperty<FMapProperty>(
            PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("InternalNameToDataMap")));
        if (!persistenceProperty || !internalProperty
            || !persistenceMap || !internalMap)
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': ItemSubsystem identity maps were unavailable; item was not registered.\n"),
                pendingAsset.Target);
            return false;
        }

        const auto persistenceId = persistenceProperty->GetPropertyValue(
            persistenceProperty->ContainerPtrToValuePtr<void>(item));
        const auto internalName = internalProperty->GetPropertyValue(
            internalProperty->ContainerPtrToValuePtr<void>(item));
        if (persistenceId.GetCharArray().Num() <= 1
            || internalName.GetCharArray().Num() <= 1)
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': identity fields were not written; item was not registered.\n"),
                pendingAsset.Target);
            return false;
        }
        if (MapContainsOther(persistenceMap, subsystem, persistenceId, item)
            || MapContainsOther(internalMap, subsystem, persistenceId, item)
            || MapContainsOther(internalMap, subsystem, internalName, item))
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': PersistenceID or InternalName collides with another item; clone was not registered.\n"),
                pendingAsset.Target);
            return false;
        }

        AddMapEntry(persistenceMap, subsystem, persistenceId, item);
        AddMapEntry(internalMap, subsystem, persistenceId, item);
        if (internalName != persistenceId)
            AddMapEntry(internalMap, subsystem, internalName, item);

        PS::Log<LogLevel::Verbose>(STR("Clone '{}': registered new item identity '{}' / '{}'.\n"),
            pendingAsset.Target, RC::StringType(*persistenceId),
            RC::StringType(*internalName));
        return true;
    }

    RC::StringType DragonWildsAssetModLoader::NormalizeObjectPath(const RC::StringType& target) const
    {
        if (!target.starts_with(TEXT("/")))
        {
            return TEXT("");
        }

        auto slash = target.find_last_of(TEXT('/'));
        auto dot = target.find(TEXT('.'), slash == RC::StringType::npos ? 0 : slash + 1);
        if (dot != RC::StringType::npos)
        {
            return target;
        }

        auto assetName = target.substr(slash + 1);
        return std::format(TEXT("{}.{}"), target, assetName);
    }

    bool DragonWildsAssetModLoader::IsSupportedTarget(UObject* object) const
    {
        if (!object || !m_dataAssetClass || !m_curveBaseClass)
        {
            return false;
        }

        return object->IsA(m_dataAssetClass)
            || object->IsA(m_curveBaseClass)
            || object->GetTypedOuter(m_dataAssetClass) != nullptr;
    }

    bool DragonWildsAssetModLoader::IsReadyForPatch(UObject* object) const
    {
        if (!object)
        {
            return false;
        }

        return !object->HasAnyFlags(RF_NeedInitialization)
            && !object->HasAnyFlags(RF_NeedLoad)
            && !object->HasAnyFlags(RF_NeedPostLoad)
            && !object->HasAnyFlags(RF_NeedPostLoadSubobjects)
            && !object->HasAnyFlags(RF_BeginDestroyed)
            && !object->HasAnyFlags(RF_FinishDestroyed);
    }
}
