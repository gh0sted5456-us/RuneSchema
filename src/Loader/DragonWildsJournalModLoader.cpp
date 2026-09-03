#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>
#include <Windows.h>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsJournalModLoader.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        constexpr const TCHAR* EntryClassPaths[] = {
            TEXT("/Script/Dominion.JournalEntryKnowLoreData"),
            TEXT("/Script/Dominion.JournalEntryKnowPeopleData"),
            TEXT("/Script/Dominion.JournalEntryKnowPlaceData"),
            TEXT("/Script/Dominion.JournalEntryKnowTreasureData"),
            TEXT("/Script/Dominion.JournalEntryWorldData"),
            TEXT("/Script/Dominion.JournalEntryRecipeData"),
        };

        constexpr const TCHAR* PersistenceLoadedPath =
            TEXT("/Script/Dominion.JournalComponent:Client_HandleJournalEntriesLoadedFromPersistence");

        constexpr const TCHAR* RecipeDataClassPath = TEXT("/Script/Dominion.RecipeData");
        constexpr const TCHAR* ItemDataClassPath = TEXT("/Script/Dominion.ItemData");
        constexpr const TCHAR* DataTableClassPath = TEXT("/Script/Engine.DataTable");

        constexpr const char* JournalSaveLists[] = { "UnlockedEntries", "UnreadEntries" };


        RC::StringType ReadString(const nlohmann::json& value, const char* key)
        {
            if (!value.contains(key) || !value.at(key).is_string())
            {
                return {};
            }

            return RC::to_generic_string(value.at(key).get<std::string>());
        }

        bool ReadBool(const nlohmann::json& value, const char* key, bool fallback)
        {
            if (!value.contains(key) || !value.at(key).is_boolean())
            {
                return fallback;
            }

            return value.at(key).get<bool>();
        }

        UObject* FindObjectByClassAndName(const TCHAR* classPath, const RC::StringType& name)
        {
            auto* objectClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, classPath);
            if (!objectClass)
            {
                return nullptr;
            }

            TArray<UObject*> objects;
            UECustom::UObjectGlobals::GetObjectsOfClass(objectClass, objects, true);
            for (auto* object : objects)
            {
                if (object && object->GetName() == name
                    && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                {
                    return object;
                }
            }

            return nullptr;
        }

        UObject* ResolveSoftReference(const TCHAR* classPath, const RC::StringType& reference)
        {
            if (reference.starts_with(TEXT("/")))
            {
                UECustom::TSoftObjectPtr<UObject> soft{ UECustom::FSoftObjectPath(reference) };
                return UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            return FindObjectByClassAndName(classPath, reference);
        }

        void SetLiveSoftReference(UObject* owner, FProperty* property, UObject* target)
        {
            auto* soft = static_cast<UECustom::FSoftObjectPtr*>(property->ContainerPtrToValuePtr<void>(owner));
            soft->ObjectID = UECustom::FSoftObjectPath(target->GetPathName());
            soft->WeakPtr = FWeakObjectPtr(target);
        }
    }

    DragonWildsJournalModLoader::DragonWildsJournalModLoader()
        : DragonWildsModLoaderBase("journal")
    {
        SetDisplayName(TEXT("Journal Loader"));
    }

    void DragonWildsJournalModLoader::OnLoad(const std::filesystem::path& loaderPath,
        const RC::StringType&, const EEngineLifecyclePhase& phase)
    {
        if (phase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath,
                [this](const nlohmann::json& data) { QueueData(data); });
        }
        else if (phase == EEngineLifecyclePhase::GameInstanceInit)
        {
            ApplyAll();
        }
    }

    void DragonWildsJournalModLoader::OnAutoReload(const RC::StringType&,
        const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath,
            [this](const nlohmann::json& data) { QueueData(data); });
        ApplyAll();
        if (auto* component = FindJournalComponent())
        {
            UnlockEntries(component);
        }
    }

    bool DragonWildsJournalModLoader::CanInitialize(const EEngineLifecyclePhase& phase)
    {
        return phase == EEngineLifecyclePhase::PostEngineInit;
    }

    bool DragonWildsJournalModLoader::OnInitialize()
    {
        m_baseEntryClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalEntryData"));
        m_noBiomeSubCategoryClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalSubCategoryNoBiomeData"));
        m_byBiomeSubCategoryClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalSubCategoryByBiomeData"));
        m_journalComponentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalComponent"));
        m_journalSubsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalSubsystem"));

        if (!m_baseEntryClass || !m_noBiomeSubCategoryClass || !m_byBiomeSubCategoryClass
            || !m_journalComponentClass || !m_journalSubsystemClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, required Dominion journal classes were not found.\n"),
                GetDisplayName());
            return false;
        }

        return true;
    }

    void DragonWildsJournalModLoader::QueueData(const nlohmann::json& data)
    {
        if (!data.is_object())
        {
            PS::Log<LogLevel::Error>(STR("Journal JSON root must be an object.\n"));
            return;
        }

        for (auto& [key, body] : data.items())
        {
            if (key.starts_with("$"))
            {
                continue;
            }

            if (!body.is_object())
            {
                PS::Log<LogLevel::Error>(STR("Journal entry '{}' must be an object. Skipping.\n"),
                    RC::to_generic_string(key));
                continue;
            }

            auto wideKey = RC::to_generic_string(key);
            auto found = std::find_if(m_defs.begin(), m_defs.end(),
                [&](const JournalDef& def) { return def.Key == wideKey; });
            JournalDef replacement{ wideKey, body };
            if (found == m_defs.end())
            {
                m_defs.push_back(std::move(replacement));
            }
            else
            {
                *found = std::move(replacement);
            }
        }
    }

    DragonWildsJournalModLoader::LoadResult DragonWildsJournalModLoader::ApplyAll()
    {
        LoadResult result{};

        for (auto& def : m_defs)
        {
            try
            {
                auto* entry = ResolveOrCreate(def);
                if (!entry)
                {
                    result.ErrorCount++;
                    continue;
                }

                ApplyProperties(entry, def.Body);
                RegisterEntry(entry);
                if (Place(entry, def))
                {
                    result.Placements++;
                }

                if (ReadBool(def.Body, "Unlock", true))
                {
                    m_unlock.insert(def.Key);
                }
                else
                {
                    m_unlock.erase(def.Key);
                }

                result.EntriesReady++;
            }
            catch (const std::exception& e)
            {
                result.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("[{}] {}\n"), def.Key, PS::ToWideSafe(e.what()));
            }
        }

        if (!m_defs.empty())
        {
            if (result.ErrorCount > 0)
            {
                PS::Log<LogLevel::Warning>(STR("Journal: {} entries ready, {} placements, {} errors.\n"),
                    result.EntriesReady, result.Placements, result.ErrorCount);
            }
            else
            {
                PS::Log<LogLevel::Normal>(STR("Journal: {} entries ready, {} placements, 0 errors.\n"),
                    result.EntriesReady, result.Placements);
            }
        }
        RegisterHooks();
        SnapshotRegistryIds();
        InstallSaveGuard();
        return result;
    }

    UClass* DragonWildsJournalModLoader::ResolveEntryClass(const nlohmann::json& body) const
    {
        auto type = ReadString(body, "Type");
        const TCHAR* classPath = EntryClassPaths[0];
        if (type == TEXT("People"))
        {
            classPath = EntryClassPaths[1];
        }
        else if (type == TEXT("Place"))
        {
            classPath = EntryClassPaths[2];
        }
        else if (type == TEXT("Treasure"))
        {
            classPath = EntryClassPaths[3];
        }
        else if (type == TEXT("World"))
        {
            classPath = EntryClassPaths[4];
        }
        else if (type == TEXT("Recipe"))
        {
            classPath = EntryClassPaths[5];
        }
        else if (!type.empty() && type != TEXT("Lore"))
        {
            throw std::runtime_error("Type must be Lore, People, Place, Treasure, World, or Recipe");
        }

        return UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, classPath);
    }

    UObject* DragonWildsJournalModLoader::ResolveOrCreate(const JournalDef& def)
    {
        if (auto found = m_entries.find(def.Key); found != m_entries.end())
        {
            return found->second;
        }

        if (def.Key.starts_with(TEXT("/")))
        {
            auto* entry = UECustom::UObjectGlobals::StaticFindObject(nullptr, nullptr, def.Key.c_str(), false);
            if (!entry)
            {
                UECustom::TSoftObjectPtr<UObject> soft{ UECustom::FSoftObjectPath(def.Key) };
                entry = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            if (!entry || !entry->IsA(m_baseEntryClass))
            {
                throw std::runtime_error("cooked journal entry was not found or had the wrong class");
            }
            entry->SetRootSet();
            m_entries.emplace(def.Key, entry);
            return entry;
        }

        auto* entryClass = ResolveEntryClass(def.Body);
        auto* transientPackage = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, TEXT("/Engine/Transient"), false);
        if (!entryClass || !transientPackage)
        {
            throw std::runtime_error("entry class or transient package was unavailable");
        }

        FStaticConstructObjectParameters params(entryClass, transientPackage);
        params.Name = FName(def.Key, FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(RF_Public | RF_Standalone | RF_Transactional);
        auto* entry = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!entry)
        {
            throw std::runtime_error("failed to construct journal entry");
        }
        entry->SetRootSet();

        for (auto* name : { TEXT("PersistenceID"), TEXT("InternalName") })
        {
            if (auto* property = PropertyHelper::GetPropertyByName(entryClass, name))
            {
                PropertyHelper::CopyJsonValueToContainer(entry, property, RC::to_string(def.Key));
            }
        }

        m_createdEntries.insert(entry);
        m_entries.emplace(def.Key, entry);
        return entry;
    }

    void DragonWildsJournalModLoader::ApplyProperties(UObject* entry, const nlohmann::json& body)
    {
        m_pendingRecipeReferences.erase(entry);

        for (auto& [name, value] : body.items())
        {
            if (name == "Type" || name == "AddTo" || name == "Unlock")
            {
                continue;
            }

            auto propertyName = RC::to_generic_string(name);
            auto* property = PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), propertyName);
            if (!property)
            {
                throw std::runtime_error(std::format("Property '{}' was not found on {}.",
                    name, RC::to_string(entry->GetClassPrivate()->GetName())));
            }

            if (name == "RecipeData" && value.is_string())
            {
                auto reference = RC::to_generic_string(value.get<std::string>());
                auto* recipe = ResolveSoftReference(RecipeDataClassPath, reference);
                if (recipe)
                {
                    SetLiveSoftReference(entry, property, recipe);
                    m_pendingRecipeReferences.erase(entry);
                }
                else if (!reference.starts_with(TEXT("/")))
                {
                    m_pendingRecipeReferences[entry] = reference;
                }
                else
                {
                    throw std::runtime_error("RecipeData cooked asset path could not be loaded.");
                }
            }
            else if (name == "ItemData" && value.is_string())
            {
                auto reference = RC::to_generic_string(value.get<std::string>());
                auto* item = ResolveSoftReference(ItemDataClassPath, reference);
                if (!item)
                {
                    throw std::runtime_error("ItemData could not be resolved; check the cooked item asset path.");
                }
                SetLiveSoftReference(entry, property, item);
            }
            else if (name == "StationTableRowHandle" && value.is_object())
            {
                auto tableName = ReadString(value, "DataTable");
                auto rowName = ReadString(value, "RowName");
                if (tableName.empty() || rowName.empty())
                {
                    throw std::runtime_error("StationTableRowHandle requires DataTable and RowName.");
                }

                auto* table = FindObjectByClassAndName(DataTableClassPath, tableName);
                if (!table)
                {
                    throw std::runtime_error(std::format("Data table '{}' was not loaded.",
                        RC::to_string(tableName)));
                }

                auto* rowHandle = property->ContainerPtrToValuePtr<uint8>(entry);
                std::memcpy(rowHandle, &table, sizeof(table));
                *reinterpret_cast<FName*>(rowHandle + sizeof(table)) = FName(rowName, FNAME_Add);
            }
            else
            {
                PropertyHelper::CopyJsonValueToContainer(entry, property, value);
            }
        }

        auto imagePath = ReadString(body, "Image");
        if (!imagePath.empty())
        {
            UECustom::TSoftObjectPtr<UObject> softImage{ UECustom::FSoftObjectPath(imagePath) };
            auto* image = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softImage);
            if (!image)
            {
                throw std::runtime_error("journal Image could not be loaded; check the cooked asset path and container mount");
            }
        }
    }

    bool DragonWildsJournalModLoader::Place(UObject* entry, const JournalDef& def)
    {
        if (!def.Body.contains("AddTo") || !def.Body.at("AddTo").is_object())
        {
            throw std::runtime_error("AddTo object is required");
        }
        const auto& addTo = def.Body.at("AddTo");
        auto path = ReadString(addTo, "SubCategory");
        auto keyString = ReadString(addTo, "Key");
        if (path.empty())
        {
            throw std::runtime_error("AddTo.SubCategory is required.");
        }
        if (keyString.empty())
        {
            keyString = def.Key;
        }

        UECustom::TSoftObjectPtr<UObject> softSubCategory{ UECustom::FSoftObjectPath(path) };
        auto* subCategory = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softSubCategory);
        if (!subCategory)
        {
            throw std::runtime_error(std::format("AddTo.SubCategory could not be loaded: {}", RC::to_string(path)));
        }

        FMapProperty* dataMapProperty = nullptr;
        void* dataMap = nullptr;
        if (subCategory->IsA(m_noBiomeSubCategoryClass))
        {
            auto* dataProperty = CastField<FStructProperty>(
                PropertyHelper::GetPropertyByName(subCategory->GetClassPrivate(), TEXT("Data")));
            if (!dataProperty || !dataProperty->GetStruct())
            {
                throw std::runtime_error("Subcategory Data layout was invalid.");
            }
            dataMapProperty = CastField<FMapProperty>(
                PropertyHelper::GetPropertyByName(dataProperty->GetStruct().Get(), TEXT("DataMap")));
            dataMap = dataMapProperty ? dataMapProperty->ContainerPtrToValuePtr<void>(
                dataProperty->ContainerPtrToValuePtr<void>(subCategory)) : nullptr;
        }
        else if (subCategory->IsA(m_byBiomeSubCategoryClass))
        {
            auto biomeName = ReadString(addTo, "Biome");
            uint8 biome = 1;
            if (biomeName == TEXT("NotSet")) biome = 0;
            else if (biomeName.empty() || biomeName == TEXT("Ghornfell")) biome = 1;
            else if (biomeName == TEXT("DowdunReach")) biome = 2;
            else if (biomeName == TEXT("Fellhollow")) biome = 3;
            else if (biomeName == TEXT("Brynmoor")) biome = 4;
            else if (biomeName == TEXT("UmbralSands")) biome = 5;
            else
            {
                throw std::runtime_error("AddTo.Biome must be NotSet, Ghornfell, DowdunReach, Fellhollow, Brynmoor, or UmbralSands.");
            }

            auto* outerProperty = CastField<FMapProperty>(
                PropertyHelper::GetPropertyByName(subCategory->GetClassPrivate(), TEXT("DataByBiome")));
            auto* containerProperty = outerProperty
                ? CastField<FStructProperty>(const_cast<FProperty*>(outerProperty->GetValueProp())) : nullptr;
            if (!outerProperty || !containerProperty || !containerProperty->GetStruct())
            {
                throw std::runtime_error("Subcategory DataByBiome layout was invalid.");
            }

            dataMapProperty = CastField<FMapProperty>(
                PropertyHelper::GetPropertyByName(containerProperty->GetStruct().Get(), TEXT("DataMap")));
            UECustom::FScriptMapHelper outer(outerProperty,
                outerProperty->ContainerPtrToValuePtr<void>(subCategory));
            outer.ForEachPair([&](void* biomePtr, void* containerPtr) {
                if (*static_cast<uint8*>(biomePtr) == biome && dataMapProperty)
                {
                    dataMap = dataMapProperty->ContainerPtrToValuePtr<void>(containerPtr);
                }
            });
            if (!dataMap)
            {
                throw std::runtime_error("Requested biome was not present in the journal subcategory.");
            }
        }
        else
        {
            throw std::runtime_error("Target was not a supported journal subcategory.");
        }

        if (!dataMapProperty || !dataMap)
        {
            throw std::runtime_error("Subcategory DataMap was unavailable.");
        }
        UECustom::FScriptMapHelper map(dataMapProperty, dataMap);
        FName key(keyString, FNAME_Add);
        bool alreadyPresent = false;
        map.ForEachPair([&](void* keyPtr, void* valuePtr) {
            if (*static_cast<FName*>(keyPtr) != key)
            {
                return;
            }
            alreadyPresent = true;
            auto* soft = static_cast<UECustom::FSoftObjectPtr*>(valuePtr);
            soft->ObjectID = UECustom::FSoftObjectPath(entry->GetPathName());
            soft->WeakPtr = FWeakObjectPtr(entry);
        });
        if (alreadyPresent)
        {
            return false;
        }

        UECustom::FManagedValue pair;
        map.InitializePair(pair);
        *static_cast<FName*>(map.GetKeyPtr(pair.GetData())) = key;
        auto* soft = static_cast<UECustom::FSoftObjectPtr*>(map.GetValuePtr(pair.GetData()));
        soft->ObjectID = UECustom::FSoftObjectPath(entry->GetPathName());
        soft->WeakPtr = FWeakObjectPtr(entry);
        map.Add(pair);
        map.Rehash();
        return true;
    }

    UObject* DragonWildsJournalModLoader::FindJournalSubsystem()
    {
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_journalSubsystemClass, objects, true);
        for (auto* object : objects)
        {
            if (object && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                return object;
        }
        return nullptr;
    }

    void DragonWildsJournalModLoader::RegisterEntry(UObject* entry)
    {
        auto* subsystem = FindJournalSubsystem();
        if (!subsystem)
        {
            throw std::runtime_error("Journal subsystem was unavailable.");
        }
        auto* subsystemClass = subsystem->GetClassPrivate();

        auto* persistenceProperty = CastField<FStrProperty>(
            PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), TEXT("PersistenceID")));
        auto* internalProperty = CastField<FStrProperty>(
            PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), TEXT("InternalName")));
        if (!persistenceProperty || !internalProperty)
        {
            throw std::runtime_error("Journal identity properties were unavailable.");
        }
        auto persistenceId = persistenceProperty->GetPropertyValue(
            persistenceProperty->ContainerPtrToValuePtr<void>(entry));
        auto internalName = internalProperty->GetPropertyValue(
            internalProperty->ContainerPtrToValuePtr<void>(entry));
        TrackOwnedId(entry, persistenceId);

        auto addStringMap = [&](const TCHAR* propertyName, const FString& key) {
            auto* property = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystemClass, propertyName));
            if (!property)
            {
                throw std::runtime_error(std::format("Journal registry '{}' was unavailable.",
                    RC::to_string(propertyName)));
            }
            UECustom::FScriptMapHelper map(property, property->ContainerPtrToValuePtr<void>(subsystem));
            bool found = false;
            map.ForEachPair([&](void* keyPtr, void*) { if (*static_cast<FString*>(keyPtr) == key) found = true; });
            if (found)
            {
                return;
            }
            UECustom::FManagedValue pair;
            map.InitializePair(pair);
            *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
            std::memcpy(map.GetValuePtr(pair.GetData()), &entry, sizeof(entry));
            map.Add(pair);
            map.Rehash();
        };
        addStringMap(TEXT("PersistenceIDToDataMap"), persistenceId);
        addStringMap(TEXT("InternalNameToDataMap"), internalName);

        auto* reverseProperty = CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(subsystemClass, TEXT("DataToNetIdMap")));
        auto* arrayProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(subsystemClass, TEXT("NetIdToData")));
        if (!reverseProperty || !arrayProperty)
        {
            throw std::runtime_error("Journal network registry was unavailable.");
        }

        UECustom::FScriptMapHelper reverse(reverseProperty,
            reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        bool hasNetId = false;
        reverse.ForEachPair([&](void* keyPtr, void*) {
            UObject* existing = nullptr;
            std::memcpy(&existing, keyPtr, sizeof(existing));
            if (existing == entry) hasNetId = true;
        });
        if (hasNetId)
        {
            return;
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        if (array->Num() >= std::numeric_limits<uint16>::max())
        {
            throw std::runtime_error("Journal network registry exhausted its uint16 IDs.");
        }
        uint16 netId = static_cast<uint16>(array->Num());

        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        UECustom::FManagedValue arrayValue;
        arrayHelper.InitializeValue(arrayValue);
        std::memcpy(arrayValue.GetData(), &entry, sizeof(entry));
        arrayHelper.Add(arrayValue);

        UECustom::FManagedValue reversePair;
        reverse.InitializePair(reversePair);
        std::memcpy(reverse.GetKeyPtr(reversePair.GetData()), &entry, sizeof(entry));
        std::memcpy(reverse.GetValuePtr(reversePair.GetData()), &netId, sizeof(netId));
        reverse.Add(reversePair);
        reverse.Rehash();
    }

    void DragonWildsJournalModLoader::RegisterHooks()
    {
        if (m_hooksActive || m_defs.empty())
        {
            return;
        }
        auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, PersistenceLoadedPath);
        if (!function)
        {
            PS::Log<LogLevel::Error>(STR("Journal persistence event was not found; custom entries cannot be unlocked safely.\n"));
            return;
        }
        function->RegisterPostHook([this](UnrealScriptFunctionCallableContext& context, void*) {
            UnlockEntries(context.Context);
        });
        m_hooksActive = true;
    }

    void DragonWildsJournalModLoader::ResolvePendingRecipeReferences()
    {
        for (auto it = m_pendingRecipeReferences.begin(); it != m_pendingRecipeReferences.end();)
        {
            auto* recipe = FindObjectByClassAndName(RecipeDataClassPath, it->second);
            auto* property = it->first
                ? PropertyHelper::GetPropertyByName(it->first->GetClassPrivate(), TEXT("RecipeData")) : nullptr;
            if (!recipe || !property)
            {
                auto reference = it->second;
                it = m_pendingRecipeReferences.erase(it);
                PS::Log<LogLevel::Error>(STR("Journal RecipeData '{}' could not be resolved after content loading completed.\n"),
                    reference);
                continue;
            }

            SetLiveSoftReference(it->first, property, recipe);
            it = m_pendingRecipeReferences.erase(it);
        }
    }

    void DragonWildsJournalModLoader::UnlockEntries(UObject* journalComponent)
    {
        ResolvePendingRecipeReferences();
        if (!journalComponent || !journalComponent->IsA(m_journalComponentClass))
        {
            PS::Log<LogLevel::Error>(STR("Journal persistence event supplied an invalid component.\n"));
            return;
        }

        auto* componentClass = journalComponent->GetClassPrivate();
        auto* unlockedProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(componentClass, TEXT("UnlockedJournalEntries")));
        auto* unreadProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(componentClass, TEXT("UnreadJournalEntries")));
        if (!unlockedProperty || !unreadProperty)
        {
            PS::Log<LogLevel::Error>(STR("Journal component unlock arrays were unavailable.\n"));
            return;
        }

        auto ensureInArray = [&](FArrayProperty* property, UObject* entry) {
            auto* array = property->ContainerPtrToValuePtr<FScriptArray>(journalComponent);
            UECustom::FScriptArrayHelper helper(array, property);
            bool found = false;
            helper.ForEachElement([&](void* valuePtr) {
                UObject* value = nullptr;
                std::memcpy(&value, valuePtr, sizeof(value));
                if (value == entry)
                {
                    found = true;
                }
            });

            if (!found)
            {
                UECustom::FManagedValue value;
                helper.InitializeValue(value);
                std::memcpy(value.GetData(), &entry, sizeof(entry));
                helper.Add(value);
            }
        };

        for (auto& key : m_unlock)
        {
            auto found = m_entries.find(key);
            if (found == m_entries.end() || !found->second)
            {
                continue;
            }

            UObject* entry = found->second;
            ensureInArray(unlockedProperty, entry);
            ensureInArray(unreadProperty, entry);
        }
    }

    UObject* DragonWildsJournalModLoader::FindJournalComponent()
    {
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_journalComponentClass, objects, true);
        for (auto* object : objects)
        {
            if (object && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                return object;
        }
        return nullptr;
    }

    void DragonWildsJournalModLoader::TrackOwnedId(UObject* entry, const FString& persistenceId)
    {
        if (!m_createdEntries.contains(entry) || persistenceId.GetCharArray().Num() <= 1)
        {
            return;
        }

        m_ownedIds.insert(RC::to_string(RC::StringType(*persistenceId)));
    }

    void DragonWildsJournalModLoader::SnapshotRegistryIds()
    {
        auto* subsystem = FindJournalSubsystem();
        auto* idMapProperty = subsystem ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap"))) : nullptr;
        if (!idMapProperty)
        {
            return;
        }

        std::unordered_set<std::string> known;
        UECustom::FScriptMapHelper idMap(idMapProperty, idMapProperty->ContainerPtrToValuePtr<void>(subsystem));
        idMap.ForEachPair([&](void* keyPtr, void*) {
            auto* key = static_cast<FString*>(keyPtr);
            if (key->GetCharArray().Num() > 1)
            {
                known.insert(RC::to_string(RC::StringType(**key)));
            }
        });

        if (known.size() < 500)
        {
            PS::Log<LogLevel::Warning>(STR("Journal registry looks incomplete ({} ids); leaving orphaned save ids alone.\n"),
                known.size());
            return;
        }

        m_knownIds = std::move(known);
        m_knownIdsValid = true;
    }

    std::filesystem::path DragonWildsJournalModLoader::GetCharacterSaveDirectory()
    {
        auto* localAppData = std::getenv("LOCALAPPDATA");
        if (!localAppData)
        {
            return {};
        }

        return std::filesystem::path(localAppData) / "RSDragonwilds" / "Saved" / "SaveCharacters";
    }


    void DragonWildsJournalModLoader::InstallSaveGuard()
    {
        if (m_saveGuardActive || m_ownedIds.empty())
        {
            return;
        }

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("JournalSaveGuard");

        auto callbackId = Hook::RegisterLoadMapPreCallback(
            [this](Hook::TCallbackIterationData<bool>&, UEngine*, FWorldContext&, FURL, UPendingNetGame*, FString&) {
                try
                {
                    StripUnusableIdsFromCharacterSaves();
                }
                catch (const std::exception& e)
                {
                    PS::Log<LogLevel::Error>(STR("Journal save guard failed: {}\n"), PS::ToWideSafe(e.what()));
                }
            }, options);

        if (callbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(STR("Journal save guard could not be installed; custom journal ids will stay in the profile and block the next login.\n"));
            return;
        }

        m_saveGuardActive = true;
    }

    void DragonWildsJournalModLoader::StripUnusableIdsFromCharacterSaves()
    {
        auto saveDir = GetCharacterSaveDirectory();
        if (!std::filesystem::is_directory(saveDir))
        {
            PS::Log<LogLevel::Verbose>(STR("No character save folder here; the journal id sweep stays idle.\n"));
            return;
        }

        for (auto& entry : std::filesystem::directory_iterator(saveDir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            try
            {
                StripUnusableIdsFromCharacterSave(entry.path());
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed cleaning journal ids from '{}': {}\n"),
                    entry.path().filename().wstring(), PS::ToWideSafe(e.what()));
            }
        }
    }

    bool DragonWildsJournalModLoader::StripUnusableIdsFromCharacterSave(const std::filesystem::path& savePath)
    {
        std::ifstream in(savePath, std::ios::binary);
        if (!in)
        {
            return false;
        }
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        bool isUtf16 = bytes.size() >= 2
            && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE;
        if (isUtf16)
        {
            std::wstring wide((bytes.size() - 2) / sizeof(wchar_t), L'\0');
            std::memcpy(wide.data(), bytes.data() + 2, wide.size() * sizeof(wchar_t));

            auto utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            std::string utf8(utf8Length, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), utf8Length, nullptr, nullptr);
            bytes = std::move(utf8);
        }

        auto save = nlohmann::json::parse(bytes);
        if (!save.contains("GameProgress") || !save["GameProgress"].is_object())
        {
            return false;
        }

        auto& progress = save["GameProgress"];
        if (!progress.contains("Journal") || !progress["Journal"].is_object())
        {
            return false;
        }

        auto& journal = progress["Journal"];
        int removed = 0;
        int orphans = 0;
        for (auto* listName : JournalSaveLists)
        {
            if (!journal.contains(listName) || !journal[listName].is_array())
            {
                continue;
            }

            auto& list = journal[listName];
            for (auto it = list.begin(); it != list.end();)
            {
                if (!it->is_string())
                {
                    ++it;
                    continue;
                }

                auto id = it->get<std::string>();
                bool isOurs = m_ownedIds.contains(id);
                bool isOrphan = m_knownIdsValid && !m_knownIds.contains(id);
                if (!isOurs && !isOrphan)
                {
                    ++it;
                    continue;
                }

                it = list.erase(it);
                removed++;
                orphans += isOurs ? 0 : 1;
            }
        }

        if (removed == 0)
        {
            return false;
        }
        auto serialized = save.dump(1, '\t', false, nlohmann::json::error_handler_t::replace);

        if (isUtf16)
        {
            auto wideLength = MultiByteToWideChar(CP_UTF8, 0, serialized.data(), static_cast<int>(serialized.size()), nullptr, 0);
            std::wstring wide(wideLength, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, serialized.data(), static_cast<int>(serialized.size()), wide.data(), wideLength);

            std::string encoded;
            encoded.reserve(2 + wide.size() * sizeof(wchar_t));
            encoded.push_back('\xFF');
            encoded.push_back('\xFE');
            encoded.append(reinterpret_cast<const char*>(wide.data()), wide.size() * sizeof(wchar_t));
            serialized = std::move(encoded);
        }

        auto stagingPath = savePath;
        stagingPath += ".runeschema_tmp";
        {
            std::ofstream out(stagingPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                PS::Log<LogLevel::Error>(STR("Could not stage the cleaned save for '{}'; leaving it untouched.\n"),
                    savePath.filename().wstring());
                return false;
            }
            out << serialized;
        }
        std::filesystem::rename(stagingPath, savePath);

        PS::Log<LogLevel::Verbose>(STR("Removed {} journal entr{} the profile could not load from '{}'.\n"),
            removed, removed == 1 ? STR("y") : STR("ies"), savePath.filename().wstring());
        if (orphans > 0)
        {
            PS::Log<LogLevel::Warning>(STR("Dropped {} journal entr{} from '{}' that no journal entry answers to any more.\n"),
                orphans, orphans == 1 ? STR("y") : STR("ies"), savePath.filename().wstring());
        }
        return true;
    }
}
