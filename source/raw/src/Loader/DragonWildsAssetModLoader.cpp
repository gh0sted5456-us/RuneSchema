#include "Loader/HelpyDependencyOrder.h"
#include "Generator/HelpyStatKey.h"
#include "Generator/HelpyPropertyValue.h"
#include "Generator/QuickMenuDecorations.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "Runtime/AuthoredFile.h"
#include "Runtime/HelpyBundlePublish.h"
#include "Loader/AssetAuthoringMetadata.h"
#include "Generator/ClonePresentation.h"
#include "SDK/Helper/CookedAssetLookup.h"
#include "SDK/Helper/ItemAppearanceMetadata.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include <memory>
#include <set>
#include <algorithm>
#include "Loader/AssetProvenance.h"
#include "Loader/OwnedContentLedger.h"
#include "Runtime/HostServices.h"
#include "Utility/AssetAliases.h"
#include "Loader/ItemIdentity.h"
#include <cctype>
#include <cmath>
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

    using DragonWilds::IsCanonicalPersistenceId;

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

    bool IsUnlockableAssetField(std::string_view name)
    {
        return name == "RecipesToUnlock"
            || name == "BuildingPieceToUnlock";
    }

    void ValidateDominionSpheres(const nlohmann::json& definitions)
    {
        if (!definitions.is_object() || definitions.empty() || definitions.size() > 32)
            throw std::runtime_error("$DominionSpheres requires 1..32 named subobjects");
        for (const auto& [path, body] : definitions.items())
        {
            if (path.empty() || path.size() > 512)
                throw std::runtime_error("Dominion sphere subobject path length is invalid");
            bool segmentStart = true;
            for (const auto character : path)
            {
                const auto byte = static_cast<unsigned char>(character);
                if (character == '.')
                {
                    if (segmentStart) throw std::runtime_error("Dominion sphere subobject path has an empty segment");
                    segmentStart = true;
                }
                else
                {
                    if (!std::isalnum(byte) && character != '_')
                        throw std::runtime_error("Dominion sphere subobject path contains an invalid character");
                    segmentStart = false;
                }
            }
            if (segmentStart) throw std::runtime_error("Dominion sphere subobject path has an empty segment");
            if (!body.is_object() || body.size() != 1 || !body.contains("Radius")
                || !body.at("Radius").is_number())
                throw std::runtime_error("Dominion sphere entry requires only numeric Radius");
            const auto radius = body.at("Radius").get<double>();
            if (!std::isfinite(radius) || radius <= 0.0 || radius > 100000.0)
                throw std::runtime_error("Dominion sphere Radius must be greater than 0 and at most 100000 cm");
        }
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
        AuthoringInstance=this;
    }

    DragonWildsAssetModLoader::~DragonWildsAssetModLoader()
    {
        if(AuthoringInstance==this)AuthoringInstance=nullptr;
        std::scoped_lock lock{m_mutex};
        m_pendingAssets.clear();
        m_pendingPatches.clear();
        m_createdAssetsByTarget.clear();
        PS::AssetAliases::Clear();
        m_createdAssets.clear();
        PS::AssetProvenance::Clear();
        PS::AssetMetadata::Clear();
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
        // A live authoring transaction has already applied this exact file.
        // Reapplying it could mutate an inventory item during a watcher callback.
        {
            std::scoped_lock lock{m_mutex};
            if(m_toolAssetFiles.contains(modFilePath.lexically_normal())) {
                PS::Log<LogLevel::Warning>(TEXT("Live-authored asset changes require restart: {}\n"),modFilePath.wstring());return;
            }
        }
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

        m_recipeDataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.RecipeData"), false);
        if (!m_recipeDataClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Dominion.RecipeData.\n"), GetDisplayName());
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

        try { RegisterDeclarations(data,modName); }
        catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR("[SAVE-CLEANER][DECLARATION][MOD:{}] Declaration rejected; ordinary asset entries continue: {}.\n"),
                modName,PS::ToWideSafe(error.what()));
        }

        PS::AssetMetadata::Declaration documentMetadata;
        try { documentMetadata=PS::AssetMetadata::Read(data); }
        catch(const std::exception& e) {
            PS::Log<LogLevel::Error>(STR("Asset file metadata from {}: {}. File skipped.\n"),modName,PS::ToWideSafe(e.what()));
            return;
        }
        std::scoped_lock lock{m_mutex};
        for (auto& [target, properties] : data.items())
        {
            if (target.starts_with("$") || PS::AssetMetadata::IsKey(target))
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
            auto metadata=documentMetadata;
            try
            {
                metadata=PS::AssetMetadata::Merge(metadata,PS::AssetMetadata::Read(properties));
                static constexpr std::array<std::string_view, 2> protectedIdentity{
                    "PersistenceID", "InternalName"};
                if (const auto patch = JsonPatchDirective::Parse(
                        properties, protectedIdentity, "asset"))
                {
                    effectiveTarget = patch->Reference;
                    normalizedProperties = patch->Changes;
                    isPatch = true;
                    metadata=PS::AssetMetadata::Merge(metadata,PS::AssetMetadata::Read(normalizedProperties));
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Asset patch '{}': {}. Skipping.\n"),
                    RC::to_generic_string(target), PS::ToWideSafe(error.what()));
                continue;
            }

            normalizedProperties.erase("Modded");normalizedProperties.erase("RuneSchema");
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
            try
            {
                if (normalizedProperties.contains("$DominionSpheres"))
                {
                    if (normalizedProperties.contains("$Clone"))
                        throw std::runtime_error("$DominionSpheres cannot be combined with $Clone");
                    ValidateDominionSpheres(normalizedProperties.at("$DominionSpheres"));
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Target '{}': {}. Skipping.\n"),
                    RC::to_generic_string(target), PS::ToWideSafe(error.what()));
                continue;
            }

            auto targetWide = RC::to_generic_string(effectiveTarget);
            auto pending = PendingAsset{
                targetWide,
                NormalizeObjectPath(targetWide),
                modName,
                std::move(normalizedProperties), isPatch,metadata,true
            };
            if (isPatch) WarnPatchConflicts(m_patchConflicts, "assets:" + RC::to_string(pending.ObjectPath),
                pending.Properties, RC::to_string(modName), false);
            (isPatch ? m_pendingPatches : m_pendingAssets).push_back(std::move(pending));
        }
    }

    void DragonWildsAssetModLoader::RegisterDeclarations(const nlohmann::json& data,const RC::StringType& modName)
    {
        auto declarations=OwnedContent::Declarations(data,RC::to_string(modName));
        std::vector<UObject*> verifiedObjects;
        verifiedObjects.reserve(declarations.size());
        for(auto& declaration:declarations)
        {
            const auto path=RC::to_generic_string(declaration.Source);
            auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path.c_str(),false);
            if(!object) {
                auto soft=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(path));
                object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            auto* expected=declaration.Kind=="Item"?m_itemDataClass:m_recipeDataClass;
            if(!object || !expected || !object->IsA(expected))
                throw std::runtime_error(declaration.Kind+" declaration did not resolve to the expected cooked asset class: "+declaration.Source);
            auto* idProperty=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("PersistenceID")));
            auto* nameProperty=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("InternalName")));
            if(!idProperty || !nameProperty)throw std::runtime_error("Declared cooked asset is missing PersistenceID or InternalName");
            const auto actualId=RC::to_string(*idProperty->GetPropertyValue(idProperty->ContainerPtrToValuePtr<void>(object)));
            const auto actualName=RC::to_string(*nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(object)));
            if(actualId!=declaration.PersistenceID)
                throw std::runtime_error("Declared PersistenceID does not match the cooked asset: "+declaration.Source);
            if(actualName.empty())throw std::runtime_error("Declared cooked asset has an empty InternalName: "+declaration.Source);
            // An omitted InternalName is represented by the path-derived object name.
            // Accept that convenience value, but reject any explicit conflicting value.
            const auto& raw=data.at("$declaration");
            bool explicitName=false;
            if(raw.is_object())explicitName=raw.contains("InternalName");
            else for(const auto& row:raw)if(row.is_object() && row.value("Path",std::string{})==declaration.Source)explicitName=row.contains("InternalName");
            if(explicitName && declaration.InternalName!=actualName)
                throw std::runtime_error("Declared InternalName does not match the cooked asset: "+declaration.Source);
            declaration.InternalName=actualName;
            verifiedObjects.push_back(object);
        }
        if(!declarations.empty()) {
            OwnedContent::Merge(PS::HostServices::SettingsDirectory()/"OwnedContentLedger.json",declarations);
            for(std::size_t index=0;index<declarations.size();++index) {
                verifiedObjects[index]->SetRootSet();
                OwnedContent::RegisterActiveDeclarationPath(declarations[index].Source);
            }
            PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][DECLARATION][MOD:{}] Verified and recorded {} cooked persistent asset declaration(s).\n"),
                modName,declarations.size());
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

        const auto errorsBefore = outResult.ErrorCount;
        const auto writesBefore = outResult.PropertiesWritten;
        for (auto& [propertyName, propertyValue] : pendingAsset.Properties.items())
        {
            if (propertyName == "$Append" || propertyName == "$Clone" || propertyName == "$InheritEquipmentStats" || PS::AssetMetadata::IsKey(propertyName))
            {
                continue;
            }

            if (propertyName == "$DominionSpheres")
            {
                ApplyDominionSpheres(object, propertyValue, outResult);
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
                if (IsUnlockableAssetField(propertyName))
                    PS::Log<LogLevel::Verbose>(STR("[{}] Applied unlockable asset field '{}'.\n"),
                        object->GetName(), propertyNameWide);
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
        // Metadata is recorded only against an object actually handled by this loader.
        // A native field-write failure cannot grant new clone permissions.
        if(outResult.ErrorCount==errorsBefore) {
            try {PS::AssetMetadata::Record(object,pendingAsset.Metadata,RC::to_string(pendingAsset.ModName),pendingAsset.InstalledDefinition);}
            catch(const std::exception& e){++outResult.ErrorCount;PS::Log<LogLevel::Error>(STR("Asset metadata: {}\n"),PS::ToWideSafe(e.what()));}
        }
        if (outResult.ErrorCount > errorsBefore) {
            PS::Log<LogLevel::Error>(STR("[{}] Asset edit incomplete: {} successful field writes, {} errors. Changes are not rolled back; inspect field errors and restart after correcting the mod.\n"),
                object->GetPathName(), outResult.PropertiesWritten - writesBefore,
                outResult.ErrorCount - errorsBefore);
        }
    }

    void DragonWildsAssetModLoader::ApplyDominionSpheres(UObject* owner,
        const nlohmann::json& definitions, LoadResult& outResult)
    {
        ValidateDominionSpheres(definitions);
        auto* sphereClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionShape_Sphere"), false);
        if (!sphereClass) throw std::runtime_error("DominionShape_Sphere class is unavailable");

        for (const auto& [path, body] : definitions.items())
        {
            UObject* current = owner;
            size_t begin = 0;
            while (begin < path.size())
            {
                const auto end = path.find('.', begin);
                const auto segment = path.substr(begin, end == std::string::npos ? path.size() - begin : end - begin);
                const FName wanted(RC::to_generic_string(segment), FNAME_Add);
                UObject* found = nullptr;
                UObjectGlobals::ForEachUObject([&](UObject* candidate, int32_t, int32_t) -> LoopAction {
                    if (!candidate || candidate->GetOuterPrivate() != current || candidate->GetFName() != wanted
                        || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
                        return LoopAction::Continue;
                    if (found && found != candidate)
                        throw std::runtime_error("Dominion sphere subobject path is ambiguous: " + path);
                    found = candidate;
                    return LoopAction::Continue;
                });
                if (!found) throw std::runtime_error("Dominion sphere subobject was not found: " + path);
                current = found;
                if (end == std::string::npos) break;
                begin = end + 1;
            }
            if (!current || !current->IsA(sphereClass))
                throw std::runtime_error("Dominion sphere target has the wrong class: " + path);
            auto* radius = CastField<FFloatProperty>(
                PropertyHelper::GetPropertyByName(current->GetClassPrivate(), TEXT("Radius")));
            if (!radius || radius->GetArrayDim() != 1 || radius->GetElementSize() != sizeof(float))
                throw std::runtime_error("DominionShape_Sphere Radius layout changed");
            const auto requested = body.at("Radius").get<double>();
            PropertyHelper::CopyJsonValueToContainer(current, radius, body.at("Radius"));
            const auto written = radius->GetFloatingPointPropertyValue(
                radius->ContainerPtrToValuePtr<void>(current));
            if (!std::isfinite(written) || std::abs(written - requested) > 0.001)
                throw std::runtime_error("DominionShape_Sphere Radius did not match after update");
            ++outResult.PropertiesWritten;
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

        // Helpy may clone another installed runtime item. Its parent must be
        // created/registered first even when JSON filenames put the child first.
        std::vector<PS::HelpyDependencies::Node> dependencies;
        dependencies.reserve(m_pendingAssets.size());
        for(const auto& pending:m_pendingAssets) {
            PS::HelpyDependencies::Node node;
            if(pending.Properties.contains("$Clone")) {
                node.source=RC::to_string(NormalizeObjectPath(RC::to_generic_string(pending.Properties.at("$Clone").get<std::string>())));
                node.aliases.push_back(RC::to_string(pending.ObjectPath));
                std::string internal;
                if(ReadRequiredString(pending.Properties,"InternalName",internal)) {
                    const auto name=SanitizePackageSegment(internal,"ITEM_RS_Unnamed");
                    node.aliases.push_back("/Game/RuneSchema/"+SanitizePackageSegment(RC::to_string(pending.ModName),"UnnamedMod")+"/Items/"+name+"."+name);
                }
            }
            dependencies.push_back(std::move(node));
        }
        const auto order=PS::HelpyDependencies::Order(dependencies);
        for(const auto i:order.blocked) {
            const auto& pending=m_pendingAssets[i];++batchResults[pending.ModName].ErrorCount;
            PS::Log<LogLevel::Error>(STR("Clone '{}' has a cyclic clone-source dependency; skipped without changing unrelated definitions.\n"),pending.Target);
        }
        std::vector<PendingAsset> sorted;sorted.reserve(order.ordered.size());
        for(const auto i:order.ordered)sorted.push_back(std::move(m_pendingAssets[i]));
        m_pendingAssets=std::move(sorted);

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
            const bool registered=!isClone || (result.ErrorCount==0 && RegisterCreatedItem(object, *it, itemSubsystem));
            if (!registered) result.ErrorCount++;
            if(isClone && registered && result.ErrorCount==0) {
                try {
                    const auto authored=NormalizeObjectPath(it->Target);
                    if(!authored.empty() && authored.front()==TEXT('/'))
                        PS::AssetAliases::Add(authored,object->GetPathName());
                }catch(const std::exception& error) {
                    ++result.ErrorCount;
                    PS::Log<LogLevel::Error>(STR("Clone alias '{}': {}\n"),it->Target,PS::ToWideSafe(error.what()));
                }
            }
            if (isClone) PS::AssetProvenance::Record(object,it->Properties.at("$Clone").get<std::string>(),
                RC::to_string(it->ModName),createdNow,registered,result.ErrorCount,it->Properties);

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

        PS::AssetProvenance::Flush();
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
        const FName targetName(pendingAsset.Target,FNAME_Add);
        UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
            if (object && object->GetFName() == targetName && IsSupportedTarget(object))
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

        UObject* source=nullptr;
        if(const auto known=m_createdAssetsByTarget.find(sourcePath);known!=m_createdAssetsByTarget.end())source=known->second;
        if(!source)source=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,sourcePath.c_str(),false);
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
        if(!IsReadyForPatch(source)||PS::AssetMetadata::IsIncomplete(source))
            throw std::runtime_error("$Clone source is not ready or has incomplete authored files");
        const auto sourceOrigin=PS::AssetProvenance::Lookup(source);
        if(sourceOrigin.is_object()&&!sourceOrigin.value("Registered",false))
            throw std::runtime_error("$Clone runtime source has not completed registration");

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

        // Establish ownership at the creation site, including assets created by JSON loaders.
        // Do not wait for item registration to allocate a weak-reference serial.
        PS::AssetProvenance::Record(created,pendingAsset.Properties.at("$Clone").get<std::string>(),
            RC::to_string(pendingAsset.ModName),true,false,0,pendingAsset.Properties);

        constexpr std::uint64_t unsafeFlags =
            CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient
            | CPF_InstancedReference | CPF_ContainsInstancedReference
            | CPF_Deprecated | CPF_EditorOnly;
        std::size_t copied = 0;
        RC::StringType inheritedUnlockFields;
        for (auto* property : TFieldRange<FProperty>(
                 source->GetClassPrivate(), EFieldIterationFlags::Default))
        {
            if (!property || property->HasAnyPropertyFlags(unsafeFlags)) continue;
            property->CopyCompleteValue_InContainer(created, source);
            ++copied;
            if (IsUnlockableAssetField(RC::to_string(property->GetName()))) {
                if (!inheritedUnlockFields.empty()) inheritedUnlockFields += TEXT(", ");
                inheritedUnlockFields += property->GetName();
            }
        }

        ClearItemIdentity(created, created->GetClassPrivate());

        if (!inheritedUnlockFields.empty())
            PS::Log<LogLevel::Verbose>(STR("{}: clone inherited unlockable field(s): {}. Explicit asset fields can replace them.\n"),
                pendingAsset.ModName, inheritedUnlockFields);

        // Existing mods keep their prior row-name policy. Live authored clones
        // opt in to inheriting the original row and never create a dangling row.
        if (!pendingAsset.Properties.value("$InheritEquipmentStats",false))
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

#include "LiveCloneAuthoring.inl"
