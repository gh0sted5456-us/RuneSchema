#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <map>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Helpers/String.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/IdentityOverride.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Utility/ObjectPath.h"
#include "Loader/DragonWildsAssetModLoader.h"
#include "Loader/AssetCreateSchema.h"
#include "Loader/AssetConsumablePackSchema.h"
#include "Loader/JsonPatchDirective.h"
#include "Loader/PersistenceId.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        std::string StablePersistenceId(const std::string& identity)
        {
            // Two independent FNV-1a lanes yield the 16 bytes expected by
            // Dragonwilds' unpadded base64url persistence format.
            std::array<std::uint64_t, 2> lanes{
                1469598103934665603ULL, 1099511628211ULL ^ 0x9E3779B97F4A7C15ULL};
            for (const auto character : identity)
            {
                const auto byte = static_cast<unsigned char>(character);
                lanes[0] = (lanes[0] ^ byte) * 1099511628211ULL;
                lanes[1] = (lanes[1] ^ static_cast<unsigned char>(byte + 0x5D))
                    * 1469598103934665603ULL;
            }
            std::array<std::uint8_t, 16> bytes{};
            std::memcpy(bytes.data(), lanes.data(), bytes.size());
            constexpr char alphabet[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string encoded;
            encoded.reserve(22);
            std::uint32_t accumulator = 0;
            int bits = 0;
            for (const auto byte : bytes)
            {
                accumulator = (accumulator << 8) | byte;
                bits += 8;
                while (bits >= 6)
                {
                    bits -= 6;
                    encoded.push_back(alphabet[(accumulator >> bits) & 0x3F]);
                }
            }
            if (bits > 0)
                encoded.push_back(alphabet[(accumulator << (6 - bits)) & 0x3F]);
            return encoded;
        }

        std::string TemporaryPersistenceId(const std::string& identity)
        {
            static const auto sessionNonce = std::to_string(
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch().count());
            return StablePersistenceId("temporary:" + sessionNonce + ":" + identity);
        }

        bool HasNonBlankString(const nlohmann::json& object, const char* key)
        {
            if (!object.contains(key) || !object.at(key).is_string())
                return false;
            const auto& value = object.at(key).get_ref<const std::string&>();
            return !value.empty() && std::any_of(value.begin(), value.end(),
                [](unsigned char character) { return std::isspace(character) == 0; });
        }

        void ClearItemIdentity(UObject* object, UClass* objectClass)
        {
            if (!object || !objectClass) return;
            for (const auto* propertyName : {TEXT("PersistenceID"), TEXT("InternalName")})
            {
                if (auto* property = PropertyHelper::CastProperty<FStrProperty>(
                        PropertyHelper::GetPropertyByName(objectClass, propertyName)))
                {
                    property->SetPropertyValue(
                        property->ContainerPtrToValuePtr<void>(object), FString{});
                }
            }
        }

        bool IsSafeTransientName(const std::string& value)
        {
            return !value.empty() && value.size() <= 200
                && std::all_of(value.begin(), value.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '_';
                });
        }

        bool IsSafeRuntimeObjectPath(const std::string& value)
        {
            constexpr std::array<std::string_view, 2> prefixes{
                "/Game/Mods/", "/Game/RuneSchema/"};
            const auto prefix = std::find_if(prefixes.begin(), prefixes.end(),
                [&](const auto candidate) { return value.starts_with(candidate); });
            if (prefix == prefixes.end() || value.size() > 512)
                return false;
            const auto dot = value.rfind('.');
            if (dot == std::string::npos || dot + 1 >= value.size())
                return false;
            const auto objectName = value.substr(dot + 1);
            const auto packagePath = value.substr(0, dot);
            if (!IsSafeTransientName(objectName)
                || !packagePath.ends_with('/' + objectName))
                return false;
            return std::all_of(value.begin() + prefix->size(), value.end(),
                [](unsigned char c) {
                    return std::isalnum(c) || c == '_' || c == '/' || c == '.';
                });
        }
    }

    DragonWildsAssetModLoader::DragonWildsAssetModLoader() : DragonWildsModLoaderBase("assets")
    {
        SetDisplayName(TEXT("Asset Mod Loader"));
    }

    DragonWildsAssetModLoader::~DragonWildsAssetModLoader()
    {
        std::scoped_lock lock{m_mutex};
        m_pendingAssets.clear();
        m_createdAssets.clear();
        m_createdAssetsByTarget.clear();
    }

    void DragonWildsAssetModLoader::SetRuntimeAssetSink(
        std::function<void(const std::string&, UObject*)> sink)
    {
        std::scoped_lock lock{m_mutex};
        m_runtimeAssetSink = std::move(sink);
    }

    UObject* DragonWildsAssetModLoader::FindCreatedAssetByAuthoringPath(
        const RC::StringType& authoredPath)
    {
        std::scoped_lock lock{m_mutex};
        const auto found = m_createdAssetsByTarget.find(authoredPath);
        return found != m_createdAssetsByTarget.end() ? found->second : nullptr;
    }

    void DragonWildsAssetModLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                QueueData(data, modName);
            });

            // Construct any assets whose reflected classes/templates are already
            // available before the raw DataTable callback runs. This lets /raw
            // rows reference a custom /assets object by its authoring alias in
            // the same deterministic load pass. Assets that are not ready remain
            // pending and receive the existing GameInstanceInit retry below.
            TryApplyPending();
        }
        else if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            TryApplyPending();
            ReportUnresolvedAssets();
        }
    }

    void DragonWildsAssetModLoader::OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            QueueData(data, modName);
        });

        TryApplyPending();
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

            auto normalizedProperties = properties;
            auto effectiveTarget = target;
            try
            {
                static constexpr std::array<std::string_view, 2>
                    protectedIdentity{"PersistenceID", "InternalName"};
                if (const auto patch = JsonPatchDirective::Parse(
                        normalizedProperties, protectedIdentity, "asset"))
                {
                    effectiveTarget = patch->Reference;
                    normalizedProperties = patch->Changes;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("Asset patch '{}': {}. Skipping.\n"),
                    RC::to_generic_string(target),
                    PS::ToWideSafe(error.what()));
                continue;
            }
            if (normalizedProperties.contains("$Create")
                || normalizedProperties.contains("$Clone")
                || normalizedProperties.contains("$CloneFrom")
                || normalizedProperties.contains("$clone"))
            {
                try
                {
                    NormalizeAssetCreateDirective(normalizedProperties);
                }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Custom asset '{}': invalid creation directive: {}. Skipping.\n"),
                        RC::to_generic_string(target),
                        PS::ToWideSafe(error.what()));
                    continue;
                }
            }
            if (normalizedProperties.contains("$ConsumablePack"))
            {
                try
                {
                    if (!normalizedProperties.contains("$Create")
                        && !normalizedProperties.contains("$Clone"))
                    {
                        throw std::runtime_error(
                            "patching a baked consumable emitter's live drop array is quarantined after repeatable heap-corruption crashes; create or clone a separate pack instead");
                    }
                    ParseConsumablePackDrops(
                        normalizedProperties.at("$ConsumablePack"));
                    if (normalizedProperties.contains("ItemsToDrop")
                        || normalizedProperties.contains("Items to Drop"))
                        throw std::runtime_error(
                            "use $ConsumablePack.Drops or ItemsToDrop, not both");
                }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Consumable pack '{}': {}. Skipping.\n"),
                        RC::to_generic_string(target), PS::ToWideSafe(error.what()));
                    continue;
                }
            }
            if (normalizedProperties.contains("$Create")
                || normalizedProperties.contains("$Clone"))
            {
                if (!IsSafeTransientName(target)
                    && !IsSafeRuntimeObjectPath(target))
                {
                    PS::Log<LogLevel::Error>(STR("Custom asset '{}': a created asset key must be a simple letters/numbers/underscores name or a /Game/Mods/.../Name.Name destination path. Skipping.\n"),
                        RC::to_generic_string(target));
                    continue;
                }
            }

            auto targetWide = RC::to_generic_string(effectiveTarget);
            auto objectPath = NormalizeObjectPath(targetWide);
            m_pendingAssets.push_back({
                targetWide,
                std::move(objectPath),
                modName,
                std::move(normalizedProperties)
            });
        }
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

        auto effectiveProperties = pendingAsset.Properties;
        RC::StringType consumablePackPropertyName;
        std::vector<std::pair<UObject*, bool>> consumablePackItems;
        const bool isCreatedItem = (effectiveProperties.contains("$Create")
                || effectiveProperties.contains("$Clone"))
            && m_itemDataClass && object->IsA(m_itemDataClass);
        if (isCreatedItem)
        {
            const bool hasAuthoredPersistence = HasNonBlankString(
                effectiveProperties, "PersistenceID");
            const bool hasAuthoredInternalName = HasNonBlankString(
                effectiveProperties, "InternalName");
            if (!hasAuthoredPersistence || !hasAuthoredInternalName)
            {
                ClearItemIdentity(object, objectClass);
                outResult.ErrorCount++;
                outResult.Disabled = true;
                PS::Log<LogLevel::Error>(STR("Custom item '{}': new $Create/$Clone items require both PersistenceID and InternalName because temporary item identities are disabled in Advanced settings. Plain /assets patches are unaffected.\n"),
                    pendingAsset.Target);
                return;
            }
            if (!effectiveProperties.at("PersistenceID").is_string()
                || !IsCanonicalPersistenceId(effectiveProperties.at("PersistenceID").get<std::string>()))
            {
                ClearItemIdentity(object, objectClass);
                outResult.ErrorCount++;
                outResult.Disabled = true;
                PS::Log<LogLevel::Error>(STR("Custom item '{}': PersistenceID must be the canonical unpadded base64url encoding of 16 bytes (22 characters ending in A, Q, g, or w).\n"), pendingAsset.Target);
                return;
            }
            if (!effectiveProperties.at("InternalName").is_string()
                || effectiveProperties.at("InternalName").get<std::string>().empty())
            {
                ClearItemIdentity(object, objectClass);
                outResult.ErrorCount++;
                outResult.Disabled = true;
                PS::Log<LogLevel::Error>(STR("Custom item '{}': InternalName must be a non-empty string.\n"), pendingAsset.Target);
                return;
            }
        }
        if (effectiveProperties.contains("$ConsumablePack"))
        {
            try
            {
                // Blueprint-authored variables may retain their editor display
                // name in reflection. Dragonwilds' cooked
                // BP_Consumables_ItemEmitter exposes "Items to Drop", while
                // native/generated variants may expose "ItemsToDrop".
                RC::StringType itemsPropertyName;
                if (PropertyHelper::GetPropertyByName(
                        objectClass, TEXT("ItemsToDrop")))
                    itemsPropertyName = TEXT("ItemsToDrop");
                else if (PropertyHelper::GetPropertyByName(
                             objectClass, TEXT("Items to Drop")))
                    itemsPropertyName = TEXT("Items to Drop");
                else
                    throw std::runtime_error(
                        "the target is not a native consumable item emitter");
                const auto drops = ParseConsumablePackDrops(
                    effectiveProperties.at("$ConsumablePack"));
                effectiveProperties[RC::to_string(itemsPropertyName)] =
                    BuildNativeConsumablePackDrops(
                    drops, [&](const std::string& identifier) {
                        const auto path = ResolveItemObjectPath(identifier);
                        UObject* item = nullptr;
                        if (path)
                            item = UECustom::UObjectGlobals::StaticFindObject(
                                nullptr, nullptr,
                                RC::to_generic_string(*path).c_str(), false);
                        consumablePackItems.emplace_back(item,
                            item && std::find(m_createdAssets.begin(),
                                m_createdAssets.end(), item)
                                != m_createdAssets.end());
                        return path;
                    });
                consumablePackPropertyName = itemsPropertyName;
            }
            catch (const std::exception& error)
            {
                if (isCreatedItem)
                {
                    ClearItemIdentity(object, objectClass);
                    outResult.Disabled = true;
                }
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(
                    STR("Consumable pack '{}': disabled safely: {}.\n"),
                    pendingAsset.Target, PS::ToWideSafe(error.what()));
                return;
            }
        }

        static auto* followerDataAssetClass =
            UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Dominion.FollowerDataAsset"),
                false);
        const bool isFollowerDataAsset = followerDataAssetClass
            && object->IsA(followerDataAssetClass);
        if (isFollowerDataAsset
            && effectiveProperties.contains("PersistenceID")
            && (!effectiveProperties.at("PersistenceID").is_string()
                || !IsCanonicalPersistenceId(
                    effectiveProperties.at("PersistenceID").get<std::string>())))
        {
            outResult.ErrorCount++;
            PS::Log<LogLevel::Error>(
                STR("Follower asset '{}': PersistenceID must be a unique canonical 16-byte Dragonwilds base64url ID (22 characters ending in A, Q, g, or w).\n"),
                pendingAsset.Target);
            return;
        }

        // FollowerDataAsset owns these strings but has no Item/Recipe/Journal
        // registry. Apply them as ordinary reflected fields so an addressable
        // follower clone receives an independent native identity without being
        // incorrectly rejected by registry-aware item handling.
        auto registryProperties = effectiveProperties;
        if (isFollowerDataAsset)
        {
            registryProperties.erase("PersistenceID");
            registryProperties.erase("InternalName");
        }
        auto identityResult = IdentityOverride::Apply(
            object, registryProperties, pendingAsset.Target);
        outResult.PropertiesWritten += identityResult.Written;
        outResult.ErrorCount += identityResult.Rejected;
        if (isCreatedItem
            && identityResult.Rejected > 0)
        {
            // A clone must never remain discoverable with the template item's
            // identity. Clear both copied values so the registrar cannot alias
            // this failed object to vanilla content.
            ClearItemIdentity(object, objectClass);
            outResult.Disabled = true;
            PS::Log<LogLevel::Error>(STR("Custom item '{}': authoritative identity registration prerequisites were rejected; optional fields were not applied and the created item remains unavailable.\n"),
                pendingAsset.Target);
            return;
        }

        if (isCreatedItem)
        {
            bool identityMatches = true;
            for (const auto& [jsonName, propertyName] : {
                    std::pair{ "PersistenceID", TEXT("PersistenceID") },
                    std::pair{ "InternalName", TEXT("InternalName") } })
            {
                auto* property = PropertyHelper::CastProperty<FStrProperty>(
                    PropertyHelper::GetPropertyByName(objectClass, propertyName));
                const auto expected = RC::to_generic_string(
                    effectiveProperties.at(jsonName).get<std::string>());
                const auto actual = property
                    ? property->GetPropertyValue(
                        property->ContainerPtrToValuePtr<void>(object))
                    : FString{};
                if (!property || RC::StringType(*actual) != expected)
                {
                    identityMatches = false;
                    break;
                }
            }
            if (!identityMatches)
            {
                ClearItemIdentity(object, objectClass);
                outResult.ErrorCount++;
                outResult.Disabled = true;
                PS::Log<LogLevel::Error>(STR("Custom item '{}': identity read-back did not match the authored PersistenceID/InternalName; the clone was disabled before registration.\n"),
                    pendingAsset.Target);
                return;
            }
        }

        for (auto& [propertyName, propertyValue] : effectiveProperties.items())
        {
            if (propertyName == "$Append" || propertyName == "$Create"
                || propertyName == "$Clone"
                || propertyName == "$ConsumablePack"
                || (!isFollowerDataAsset
                    && IdentityOverride::IsIdentityProperty(propertyName)))
            {
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
                if (!consumablePackPropertyName.empty()
                    && propertyName
                        == RC::to_string(consumablePackPropertyName))
                {
                    if (std::any_of(consumablePackItems.begin(),
                            consumablePackItems.end(),
                            [](const auto& item) { return item.second; }))
                    {
                        // CopyJsonValueToContainer's FSoftObjectProperty path
                        // resolves the authoring alias to the canonical live
                        // /Game/RuneSchema object and calls SetResolvedObject.
                        // Do not rewrite FSoftObjectPtr internals manually:
                        // doing so corrupted the process heap during the first
                        // live binder test.
                        PS::Log<LogLevel::Normal>(STR("Consumable pack '{}': resolved registry-created contents through the standard reflected soft-object writer.\n"),
                            pendingAsset.Target);
                    }
                }
                outResult.PropertiesWritten++;
            }
            catch (const std::exception& e)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("[{}] Failed writing '{}': {}\n"),
                    object->GetName(), propertyNameWide, PS::ToWideSafe(e.what()));
            }
        }

        if (effectiveProperties.contains("$Append"))
        {
            AppendProperties(object, objectClass, effectiveProperties.at("$Append"), outResult);
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
            int PropertiesWritten = 0;
            int ErrorCount = 0;
        };

        std::map<RC::StringType, BatchResult> batchResults;
        std::scoped_lock lock{m_mutex};

        auto it = m_pendingAssets.begin();
        while (it != m_pendingAssets.end())
        {
            UObject* object = nullptr;
            const bool constructsAsset = it->Properties.contains("$Create")
                || it->Properties.contains("$Clone");
            try
            {
                object = Resolve(*it);
                if (object && constructsAsset
                    && std::find(m_createdAssets.begin(), m_createdAssets.end(), object)
                        == m_createdAssets.end())
                {
                    throw std::runtime_error(
                        "the custom item key collides with an existing loaded asset name");
                }
                if (!object && constructsAsset)
                    object = it->Properties.contains("$Clone")
                        ? CreateFromClone(*it)
                        : CreateFromClass(*it);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Custom asset '{}' from {} was disabled safely: {}\n"),
                    it->Target, it->ModName, PS::ToWideSafe(error.what()));
                batchResults[it->ModName].ErrorCount++;
                it = m_pendingAssets.erase(it);
                continue;
            }
            if (!object || !IsReadyForPatch(object))
            {
                ++it;
                continue;
            }

            // A baked consumable pack may depend on an item created by an
            // earlier /assets definition.  At PostEngineInit that item is
            // intentionally waiting for ItemSubsystem, so keep the pack patch
            // pending as well instead of consuming it as an unresolved error.
            if (it->Properties.contains("$ConsumablePack"))
            {
                bool dependenciesReady = true;
                for (const auto& drop : ParseConsumablePackDrops(
                         it->Properties.at("$ConsumablePack")))
                {
                    if (!ResolveItemObjectPath(drop.Identifier))
                    {
                        dependenciesReady = false;
                        break;
                    }
                }
                if (!dependenciesReady)
                {
                    ++it;
                    continue;
                }
            }

            if (!IsSupportedTarget(object))
            {
                auto* objectClass = object->GetClassPrivate();
                auto className = objectClass ? objectClass->GetName() : TEXT("<unknown>");
                PS::Log<LogLevel::Error>(STR("'{}' resolved to '{}' (class '{}'), which is not a DataAsset, a Curve, or a subobject owned by one. Skipping.\n"),
                    it->Target, object->GetName(), className);
                batchResults[it->ModName].ErrorCount++;
                it = m_pendingAssets.erase(it);
                continue;
            }

            LoadResult result{};
            Apply(object, *it, result);
            if (m_runtimeAssetSink && constructsAsset && !result.Disabled)
                m_runtimeAssetSink(RC::to_string(object->GetPathName()), object);

            auto& batchResult = batchResults[it->ModName];
            batchResult.TargetsUpdated++;
            batchResult.PropertiesWritten += result.PropertiesWritten;
            batchResult.ErrorCount += result.ErrorCount;

            it = m_pendingAssets.erase(it);
        }

        for (auto& [modName, result] : batchResults)
        {
            if (result.ErrorCount > 0)
            {
                PS::Log<LogLevel::Warning>(STR("{}: {} targets updated, {} properties updated, {} errors.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten, result.ErrorCount);
            }
            else
            {
                PS::Log<LogLevel::Normal>(STR("{}: {} targets updated, {} properties updated, 0 errors.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten);
            }
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
            // A $Create/$Clone target is an authoring alias, not a cooked
            // package RuneSchema expects Unreal to load from disk. Trying to
            // load it emits a misleading missing-package warning before the
            // canonical /Game/RuneSchema/... runtime object can be built.
            const bool constructsAsset = pendingAsset.Properties.contains("$Create")
                || pendingAsset.Properties.contains("$Clone");
            if (!found && !constructsAsset)
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
        if (found)
            return found;

        // A bare, non-path Target (no leading '/') that doesn't match any
        // live object's engine name still might identify a previously
        // $Create/$Clone-d item by its authored InternalName or
        // PersistenceID instead - e.g. a second mod's /assets patch
        // targeting "ITEM_RS_GuildedIronLegs" where the item was actually
        // created under a full "/Game/Mods/.../ITEM_RS_GuildedIronLegs.
        // ITEM_RS_GuildedIronLegs" authoring key. m_createdAssetsByTarget
        // above only matches that exact original key string. Confirmed live:
        // this is exactly why AddWindstepToLegs/AddHuntertoHelm-style
        // override mods (targeting a sibling mod's item by its short
        // InternalName) got "gave up resolving" instead of ever applying.
        // ResolveItemObjectPath already does the identifier-flexible lookup
        // $Clone's own source resolution and $ConsumablePack.Drops rely on;
        // reuse it here as a last resort rather than duplicating it.
        if (const auto resolvedPath = ResolveItemObjectPath(RC::to_string(pendingAsset.Target)))
        {
            found = UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, RC::to_generic_string(*resolvedPath).c_str(), false);
        }

        return found;
    }

    UObject* DragonWildsAssetModLoader::CreateFromClass(
        const PendingAsset& pendingAsset)
    {
        const auto classText = pendingAsset.Properties.at("$Create")
            .at("Class").get<std::string>();
        const auto classPath = RC::to_generic_string(classText);
        auto* assetClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, classPath.c_str(), false);
        if (!assetClass && !classPath.starts_with(TEXT("/Script/")))
        {
            auto soft = UECustom::TSoftObjectPtr<UObject>(
                UECustom::FSoftObjectPath(classPath));
            assetClass = Cast<UClass>(
                UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft));
        }
        if (!assetClass)
            throw std::runtime_error("$Create.Class could not be resolved");
        if ((!m_dataAssetClass || !assetClass->IsChildOf(m_dataAssetClass))
            && (!m_curveBaseClass || !assetClass->IsChildOf(m_curveBaseClass)))
            throw std::runtime_error(
                "$Create.Class must derive from DataAsset or CurveBase; Blueprint classes belong in /blueprints");

        UObject* outerPackage = nullptr;
        const bool isItemData = m_itemDataClass
            && assetClass->IsChildOf(m_itemDataClass);
        EObjectFlags createdFlags = static_cast<EObjectFlags>(
            RF_Public | RF_Standalone | RF_Transactional);
        RC::StringType runtimeObjectPath;
        RC::StringType createdObjectName = pendingAsset.Target;
        if (isItemData)
        {
            // Give every generated item a deterministic, real in-memory path.
            // The JSON target remains an authoring alias, while engine tools,
            // LootMenu-style object enumeration, and soft references can use:
            // /Game/RuneSchema/<ModName>/Items/<InternalName>.<InternalName>.
            // This package is runtime-only; it is not advertised as a cooked
            // disk package and persistence remains owned by ItemSubsystem IDs.
            std::string internalName;
            if (HasNonBlankString(pendingAsset.Properties, "InternalName"))
                internalName = pendingAsset.Properties.at("InternalName")
                    .get<std::string>();
            else
            {
                const auto target = RC::to_string(pendingAsset.Target);
                const auto dot = target.rfind('.');
                const auto slash = target.rfind('/');
                internalName = dot != std::string::npos
                    ? target.substr(dot + 1)
                    : (slash != std::string::npos
                        ? target.substr(slash + 1) : target);
            }

            runtimeObjectPath = RC::to_generic_string(
                BuildRuntimeItemObjectPath(
                    RC::to_string(pendingAsset.ModName), internalName));
            const auto dot = runtimeObjectPath.rfind(TEXT('.'));
            if (dot == RC::StringType::npos)
                throw std::runtime_error(
                    "RuneSchema generated an invalid runtime item path");
            const auto packagePath = runtimeObjectPath.substr(0, dot);
            createdObjectName = runtimeObjectPath.substr(dot + 1);

            if (UECustom::UObjectGlobals::StaticFindObject(
                    nullptr, nullptr, runtimeObjectPath.c_str(), false))
                throw std::runtime_error(
                    "the canonical RuneSchema item path is already occupied");

            outerPackage = UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, packagePath.c_str(), false);
            if (!outerPackage)
            {
                auto* packageClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, TEXT("/Script/CoreUObject.Package"), false);
                if (!packageClass)
                    throw std::runtime_error("the Unreal package class is unavailable");
                FStaticConstructObjectParameters packageParams(packageClass, nullptr);
                packageParams.Name = FName(packagePath, FNAME_Add);
                packageParams.SetFlags = static_cast<EObjectFlags>(
                    RF_Public | RF_Standalone | RF_Transactional);
                outerPackage = UObjectGlobals::StaticConstructObject<UObject*>(
                    packageParams);
            }
            if (!outerPackage)
                throw std::runtime_error(
                    "RuneSchema could not construct the runtime item package");
            outerPackage->SetRootSet();
            if (std::find(m_createdAssets.begin(), m_createdAssets.end(), outerPackage)
                == m_createdAssets.end())
                m_createdAssets.push_back(outerPackage);

            // The item still registers with ItemSubsystem after its authored
            // identity is applied; package ownership exists for discovery and
            // path resolution, not as a substitute for native registration.
            if (!FindItemSubsystem())
                return nullptr;
        }
        else if (!pendingAsset.ObjectPath.empty())
        {
            runtimeObjectPath = pendingAsset.ObjectPath;
            const auto dot = runtimeObjectPath.rfind(TEXT('.'));
            if (runtimeObjectPath.empty() || dot == RC::StringType::npos)
                throw std::runtime_error("the $Create destination path could not be normalized");
            const auto packagePath = runtimeObjectPath.substr(0, dot);
            createdObjectName = runtimeObjectPath.substr(dot + 1);
            outerPackage = UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, packagePath.c_str(), false);
            if (!outerPackage)
            {
                auto* packageClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, TEXT("/Script/CoreUObject.Package"), false);
                if (!packageClass)
                    throw std::runtime_error("the Unreal package class is unavailable");
                FStaticConstructObjectParameters packageParams(packageClass, nullptr);
                packageParams.Name = FName(packagePath, FNAME_Add);
                packageParams.SetFlags = static_cast<EObjectFlags>(
                    RF_Public | RF_Standalone | RF_Transactional
                    | RF_WasLoaded | RF_LoadCompleted);
                outerPackage = UObjectGlobals::StaticConstructObject<UObject*>(
                    packageParams);
            }
            if (!outerPackage)
                throw std::runtime_error("RuneSchema could not construct the $Create destination package");
            outerPackage->SetRootSet();
            m_createdAssets.push_back(outerPackage);
            createdFlags = static_cast<EObjectFlags>(createdFlags
                | RF_WasLoaded | RF_LoadCompleted);
        }
        else
        {
            outerPackage = UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, TEXT("/Engine/Transient"), false);
            if (!outerPackage)
                throw std::runtime_error("the transient package is unavailable");
        }

        // No Template is supplied here. Unreal initializes the object from the
        // reflected class defaults; every authored JSON field is then applied
        // by Apply().
        FStaticConstructObjectParameters params(assetClass, outerPackage);
        params.Name = createdObjectName.empty()
            ? NAME_None : FName(createdObjectName, FNAME_Add);
        params.SetFlags = createdFlags;
        auto* created = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!created)
            throw std::runtime_error("Dragonwilds refused to construct the new asset");
        created->SetRootSet();
        m_createdAssets.push_back(created);
        m_createdAssetsByTarget[pendingAsset.Target] = created;
        if (!runtimeObjectPath.empty())
            m_createdAssetsByTarget[runtimeObjectPath] = created;
        PS::Log<LogLevel::Normal>(
                runtimeObjectPath.empty()
                    ? STR("{}: created independent asset '{}' from class '{}'; authored fields will now be applied.\n")
                    : STR("{}: created addressable runtime asset '{}' from class '{}' at the requested path; authored fields will now be applied.\n"),
                pendingAsset.ModName,
                runtimeObjectPath.empty() ? pendingAsset.Target : runtimeObjectPath,
                classPath);
        return created;
    }

    UObject* DragonWildsAssetModLoader::FindItemSubsystem() const
    {
        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemSubsystem"), false);
        if (!subsystemClass) return nullptr;

        TArray<UObject*> subsystems;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            subsystemClass, subsystems, true);
        for (auto* subsystem : subsystems)
        {
            if (subsystem && !subsystem->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject)))
                return subsystem;
        }
        return nullptr;
    }

    UObject* DragonWildsAssetModLoader::CreateFromClone(
        const PendingAsset& pendingAsset)
    {
        const auto sourceReference = pendingAsset.Properties.at("$Clone")
            .get<std::string>();
        const auto sourcePath = ResolveItemObjectPath(sourceReference);
        if (!sourcePath)
            throw std::runtime_error(
                "$Clone source could not be resolved by object path or InternalName");

        const auto sourcePathWide = RC::to_generic_string(*sourcePath);
        auto* source = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, sourcePathWide.c_str(), false);
        if (!source)
        {
            auto soft = UECustom::TSoftObjectPtr<UObject>(
                UECustom::FSoftObjectPath(sourcePathWide));
            source = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
        }
        if (!source || !source->GetClassPrivate())
            throw std::runtime_error("$Clone source asset could not be loaded");
        if (!m_itemDataClass || !source->IsA(m_itemDataClass))
            throw std::runtime_error(
                "$Clone is restricted to ItemData-derived assets; runtime-cloning "
                "complex DataAssets or Curves is unsafe. Patch a cooked object or "
                "use $Create with explicit fields instead");
        if (!IsSupportedTarget(source))
            throw std::runtime_error(
                "$Clone source must be a DataAsset or Curve; Blueprint classes belong in /blueprints");

        auto construction = pendingAsset;
        construction.Properties.erase("$Clone");
        construction.Properties["$Create"] = nlohmann::json{
            {"Class", RC::to_string(source->GetClassPrivate()->GetPathName())}};
        auto* created = CreateFromClass(construction);
        if (!created)
            return nullptr;

        constexpr std::uint64_t unsafeFlags =
            CPF_Transient | CPF_DuplicateTransient
            | CPF_NonPIEDuplicateTransient | CPF_InstancedReference
            | CPF_ContainsInstancedReference | CPF_Deprecated
            | CPF_EditorOnly;
        std::size_t copied = 0;
        for (auto* property : TFieldRange<FProperty>(
                 source->GetClassPrivate(), EFieldIterationFlags::Default))
        {
            if (!property || property->HasAnyPropertyFlags(unsafeFlags))
                continue;
            try
            {
                property->CopyCompleteValue_InContainer(created, source);
                ++copied;
            }
            catch (...)
            {
                ClearItemIdentity(created, created->GetClassPrivate());
                throw std::runtime_error(
                    "$Clone failed while copying a reflected source property");
            }
        }

        // Identity is never inherited. Apply() will write the new authored
        // identity (or the explicitly enabled temporary test identity) before
        // registering this object with Dragonwilds.
        ClearItemIdentity(created, created->GetClassPrivate());
        PS::Log<LogLevel::Normal>(
                STR("{}: cloned '{}' into independent asset '{}' using {} safe reflected properties; authored overrides will now be applied.\n"),
                pendingAsset.ModName, source->GetPathName(), pendingAsset.Target,
                copied);
        return created;
    }

    std::optional<std::string> DragonWildsAssetModLoader::ResolveItemObjectPath(
        const std::string& identifier) const
    {
        if (identifier.starts_with("/"))
        {
            const auto normalized = NormalizeObjectPath(
                RC::to_generic_string(identifier));
            if (normalized.empty()) return std::nullopt;

            // Older mods commonly use /Game/Mods/... as their JSON key. Keep
            // that key as an alias, but return the object's actual canonical
            // /Game/RuneSchema/... path once it has been created.
            if (const auto created = m_createdAssetsByTarget.find(normalized);
                created != m_createdAssetsByTarget.end() && created->second)
                return RC::to_string(created->second->GetPathName());

            // /Game/Mods and /Game/RuneSchema are runtime authoring aliases,
            // not cooked packages. Do not report them as resolved merely
            // because their path syntax is valid: an existing consumable-pack
            // patch must wait until the corresponding $Create/$Clone object
            // has actually been constructed at GameInstanceInit.
            const auto normalizedUtf8 = RC::to_string(normalized);
            if (normalizedUtf8.starts_with("/Game/Mods/")
                || normalizedUtf8.starts_with("/Game/RuneSchema/"))
            {
                auto* existing = UECustom::UObjectGlobals::StaticFindObject(
                    nullptr, nullptr, normalized.c_str(), false);
                return existing
                    ? std::optional<std::string>{
                        RC::to_string(existing->GetPathName())}
                    : std::nullopt;
            }
            return RC::to_string(normalized);
        }

        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemSubsystem"));
        if (!subsystemClass) return std::nullopt;
        TArray<UObject*> subsystems;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, subsystems, true);
        const auto identifierWide = RC::to_generic_string(identifier);
        for (auto* subsystem : subsystems)
        {
            if (!subsystem || subsystem->HasAnyFlags(
                static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                continue;
            auto* mapProperty = PropertyHelper::CastProperty<FMapProperty>(PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("InternalNameToDataMap")));
            if (!mapProperty) continue;
            UObject* match = nullptr;
            UECustom::FScriptMapHelper map(
                mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
            map.ForEachPair([&](void* keyPointer, void* valuePointer) {
                if (match) return;
                const auto* key = static_cast<FString*>(keyPointer);
                auto* candidate = *static_cast<UObject**>(valuePointer);
                if (key && key->GetCharArray().Num() > 1
                    && RC::StringType(**key) == identifierWide)
                {
                    match = candidate;
                    return;
                }
                if (!candidate) return;

                // Keep consumable-pack references aligned with the runtime spawner:
                // accept the object name, internal_name, PersistenceID, or display
                // text in addition to the ItemSubsystem map key.
                if (candidate->GetName() == identifierWide)
                {
                    match = candidate;
                    return;
                }
                for (const auto propertyName : {TEXT("InternalName"),
                        TEXT("PersistenceID")})
                {
                    auto* property = PropertyHelper::CastProperty<FStrProperty>(
                        PropertyHelper::GetPropertyByName(
                            candidate->GetClassPrivate(), propertyName));
                    if (!property) continue;
                    const auto value = property->GetPropertyValue(
                        property->ContainerPtrToValuePtr<void>(candidate));
                    if (value.GetCharArray().Num() > 1
                        && RC::StringType(*value) == identifierWide)
                    {
                        match = candidate;
                        return;
                    }
                }
                for (auto* property = candidate->GetClassPrivate()->GetPropertyLink();
                    property != nullptr; property = property->GetPropertyLinkNext())
                {
                    auto* textProperty = CastField<FTextProperty>(property);
                    if (!textProperty) continue;
                    const auto displayName = textProperty->GetPropertyValue(
                        textProperty->ContainerPtrToValuePtr<void>(candidate));
                    if (PropertyHelper::GetTextAsString(displayName) == identifierWide)
                    {
                        match = candidate;
                        return;
                    }
                }
            });
            if (match) return RC::to_string(match->GetPathName());
        }
        return std::nullopt;
    }

    RC::StringType DragonWildsAssetModLoader::NormalizeObjectPath(const RC::StringType& target) const
    {
        if (!target.starts_with(TEXT("/")))
        {
            return TEXT("");
        }

        return PS::ObjectPath::Normalize(target);
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
