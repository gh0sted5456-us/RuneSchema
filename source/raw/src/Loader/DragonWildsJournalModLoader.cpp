#include "Utility/NativeFunctionHook.h"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <safetyhook.hpp>
#include <vector>
#include <Windows.h>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectArray.hpp"
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
#include "SDK/Helper/ActorHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsJournalModLoader.h"
#include "Loader/JournalPlayerAccess.h"
#include "Loader/JournalSaveOwnership.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Core/JsonPatchDirective.h"
#include "Core/JournalPlacement.h"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Core/HAL/UnrealMemory.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Loader/JournalNativeContract.h"
#include "Loader/JournalPersistenceContract.h"
#include "Loader/JournalJsonFieldContract.h"
#include "Generator/NativeCallResolver.h"
#include "Loader/QuestNativeRegistry.h"
#include "Loader/ModLoadOrder.h"
#include "Loader/OwnedContentLedger.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    #include "JournalJsonBridge.inl"
    #include "JournalPersistence.inl"
    #include "JournalLorePresentation.inl"
    DragonWildsJournalModLoader::EntryHandle::EntryHandle(UObject* object):Object(object) {
        auto* slot=object?FUObjectArray::IndexToObject(object->GetInternalIndex()):nullptr;
        if(!slot || slot->GetUObject()!=object || !slot->IsValid(false) || !slot->IsRootSet())
            throw std::runtime_error("Journal cache requires a live rooted entry");
        Index=object->GetInternalIndex();Serial=slot->GetSerialNumber();Path=object->GetPathName();
    }
    UObject* DragonWildsJournalModLoader::EntryHandle::Get() const {
        auto* slot=Index<0?nullptr:FUObjectArray::IndexToObject(Index);
        if(!slot || slot->GetUObject()!=Object || !slot->IsValid(false) || !slot->IsRootSet())return nullptr;
        const auto current=slot->GetSerialNumber();
        if(current<0 || Serial<0 || (Serial!=0 && Serial!=current) || Object->GetPathName()!=Path
            || Object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return nullptr;
        // A continuously rooted runtime asset can acquire its first serial later.
        Serial=current;
        return Object;
    }
    #include "JournalGroupPlacement.inl"
    #include "JournalHierarchy.inl"
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
            UObject* match = nullptr;
            const FName targetName(name,FNAME_Add);
            for (auto* object : objects)
            {
                if (object && object->GetFName() == targetName
                    && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                {
                    if (match && match != object)
                        throw std::runtime_error("Ambiguous journal reference; use the full asset path: " + RC::to_string(name));
                    match = object;
                }
            }

            return match;
        }

        UObject* ResolveSoftReference(const TCHAR* classPath, const RC::StringType& reference)
        {
            if (reference.starts_with(TEXT("/")))
            {
                UECustom::TSoftObjectPtr<UObject> soft{ UECustom::FSoftObjectPath(reference) };
                auto* target = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
                auto* expected = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, classPath);
                if (target && (!expected || !target->IsA(expected)))
                    throw std::runtime_error("Journal reference has the wrong asset class: " + RC::to_string(reference));
                return target;
            }
            return FindObjectByClassAndName(classPath, reference);
        }

        void SetLiveSoftReference(UObject* owner, FProperty* property, UObject* target)
        {
            auto* reference = CastField<FSoftObjectProperty>(property);
            if (!reference || reference->GetArrayDim() != 1
                || reference->GetElementSize() != sizeof(UECustom::FSoftObjectPtr)
                || !target || !reference->GetPropertyClass()
                || !target->IsA(reference->GetPropertyClass().Get()))
                throw std::runtime_error("Journal soft reference does not match the live property contract");
            auto* soft = static_cast<UECustom::FSoftObjectPtr*>(property->ContainerPtrToValuePtr<void>(owner));
            soft->ObjectID = UECustom::FSoftObjectPath(target->GetPathName());
        }
    }

    DragonWildsJournalModLoader::DragonWildsJournalModLoader(bool loreOnly)
        : DragonWildsModLoaderBase(loreOnly ? "lore" : "journal"), m_loreOnly(loreOnly)
    {
        SetDisplayName(loreOnly ? TEXT("Lore Loader") : TEXT("Journal Loader"));
    }

    void DragonWildsJournalModLoader::OnLoad(const std::filesystem::path& loaderPath,
        const RC::StringType& modName, const EEngineLifecyclePhase& phase)
    {
        if (phase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath,
                [&](const nlohmann::json& data) { QueueData(data, modName); });
        }
    }

    void DragonWildsJournalModLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase)
    {
        if (phase == EEngineLifecyclePhase::GameInstanceInit && !m_initialJournalApplied)
        {
            ApplyPendingPatches();
            ApplyAll();
        }
    }

    void DragonWildsJournalModLoader::OnAutoReload(const RC::StringType& modName,
        const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath,
            [&](const nlohmann::json& data) { QueueData(data, modName); });
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
        m_journalComponentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalComponent"));
        m_journalSubsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.JournalSubsystem"));

        if (!m_baseEntryClass || !m_noBiomeSubCategoryClass
            || !m_journalComponentClass || !m_journalSubsystemClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, required Dominion journal classes were not found.\n"),
                GetDisplayName());
            return false;
        }

        return true;
    }

    void DragonWildsJournalModLoader::QueueData(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (!data.is_object())
        {
            PS::Log<LogLevel::Error>(STR("Journal JSON root must be an object.\n"));
            return;
        }

        try {
            const auto kind=m_loreOnly?std::string("Lore"):std::string("Journal");
            for(const auto& record:OwnedContent::Declarations(data,RC::to_string(modName),kind)) {
                const auto key=RC::to_generic_string(record.Source);
                JournalDef replacement{key,nlohmann::json::object(),modName,true,
                    record.PersistenceID,record.InternalName};
                replacement.DeclaredInternalNameAsserted=record.InternalNameAsserted;
                auto found=std::find_if(m_defs.begin(),m_defs.end(),[&](const auto& value){return value.Key==key;});
                if(found==m_defs.end())m_defs.push_back(std::move(replacement));
                else *found=std::move(replacement);
                m_rejectedEntries.erase(key);
            }
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR("[SAVE-CLEANER][DECLARATION][LOADER:{}][MOD:{}] Declaration rejected; other records continue: {}.\n"),
                m_loreOnly?TEXT("lore"):TEXT("journal"),modName,PS::ToWideSafe(error.what()));
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
            try
            {
                static constexpr std::array<std::string_view, 1> protectedIdentity{"PersistenceID"};
                if (const auto patch = JsonPatchDirective::Parse(body, protectedIdentity, "journal"))
                {
                    m_pendingPatches.push_back({patch->Reference, patch->Changes, modName});
                    continue;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Journal patch '{}': {}. Skipping.\n"),
                    wideKey, PS::ToWideSafe(error.what()));
                continue;
            }
            auto found = std::find_if(m_defs.begin(), m_defs.end(),
                [&](const JournalDef& def) { return def.Key == wideKey; });
            if(found!=m_defs.end() && !wideKey.starts_with(TEXT("/")) && found->Owner!=modName) {
                PS::Log<LogLevel::Error>(STR("Journal '{}' is already owned by '{}'; duplicate from '{}' rejected.\n"),wideKey,found->Owner,modName);
                continue;
            }
            JournalDef replacement{ wideKey, body, modName };
            m_rejectedEntries.erase(wideKey);
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

    void DragonWildsJournalModLoader::ApplyPendingPatches()
    {
        size_t updated = 0, errors = 0;
        for (const auto& patch : m_pendingPatches)
        {
            auto key = patch.Reference;
            if (const auto colon = key.find(':'); colon != std::string::npos) key = key.substr(colon + 1);
            const auto wideKey = RC::to_generic_string(key);
            auto found = std::find_if(m_defs.begin(), m_defs.end(),
                [&](const JournalDef& def) { return def.Key == wideKey; });
            if (found == m_defs.end())
            {
                PS::Log<LogLevel::Error>(STR("Journal $Patch target '{}' was not loaded; no entry was created.\n"), wideKey);
                ++errors;
                continue;
            }
            JsonPatchDirective::Directive directive{patch.Reference, patch.Changes};
            m_rejectedEntries.erase(wideKey);
            const auto stats = JsonPatchDirective::Apply(found->Body, directive, true);
            WarnPatchConflicts(m_patchConflicts, "journal:" + key, patch.Changes, RC::to_string(patch.Owner));
            ++updated;
            PS::Log<LogLevel::Verbose>( STR("Patched journal entry '{}' ({} fields overwritten).\n"),
                found->Key, stats.FieldsOverwritten);
        }
        if (updated || errors) PS::RoutineLog("patches", STR("Journal $Patch: {} updated, {} errors.\n"), updated, errors);
        m_pendingPatches.clear();
    }

    DragonWildsJournalModLoader::LoadResult DragonWildsJournalModLoader::ApplyAll()
    {
        LoadResult result{};

        for (auto& def : m_defs)
        {
            if (m_rejectedEntries.contains(def.Key)) continue;
            try
            {
                if (def.Body.contains("AddTo"))
                    PS::JournalPlacement::Parse(def.Body.at("AddTo"), RC::to_string(def.Key));
                else if (!def.Key.starts_with(TEXT("/")))
                    throw std::runtime_error("New journal entries require AddTo");
                auto* entry = ResolveOrCreate(def);
                if (!entry)
                {
                    result.ErrorCount++;
                    continue;
                }

                if(def.Declared) {
                    auto* id=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("PersistenceID")));
                    auto* name=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("InternalName")));
                    const auto actualId=id?RC::to_string(*id->GetPropertyValue(id->ContainerPtrToValuePtr<void>(entry))):std::string{};
                    const auto actualName=name?RC::to_string(*name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(entry))):std::string{};
                    if(actualId!=def.DeclaredPersistenceID || actualName.empty()
                        || (def.DeclaredInternalNameAsserted && actualName!=def.DeclaredInternalName))
                        throw std::runtime_error("Cooked journal/lore declaration does not match its PersistenceID/InternalName");
                }
                ApplyProperties(entry, def.Body);
                RegisterEntry(entry,def.Owner);
                if(def.Declared) {
                    auto* id=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("PersistenceID")));
                    TrackOwnedId(entry,id->GetPropertyValue(id->ContainerPtrToValuePtr<void>(entry)),def.Owner,true);
                    auto* name=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("InternalName")));
                    const auto actualName=RC::to_string(*name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(entry)));
                    OwnedContent::Merge(OwnedContent::LedgerPath(PS::HostServices::StateDirectory()),
                        {{m_loreOnly?"Lore":"Journal",RC::to_string(def.Owner),def.DeclaredPersistenceID,
                            actualName,RC::to_string(def.Key)}});
                }
                if (def.Body.contains("AddTo")) {
                    if (Place(entry, def)) ++result.Placements;
                } else if (m_createdEntries.contains(entry))
                    throw std::runtime_error("New journal entries require AddTo");

                if (ReadBool(def.Body, "Unlock", !def.Declared))
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
                m_rejectedEntries.insert(def.Key);
                m_unlock.erase(def.Key);
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
                PS::RoutineLog("journal", STR("Journal: {} entries ready, {} placements, 0 errors.\n"),
                    result.EntriesReady, result.Placements);
            }
        }
        // Native persistence cleanup is an optional safety adapter. A
        // storefront-specific routine mismatch must not roll back journal
        // registration, placement, or unlock delivery.
        if (!m_ownedIds.empty()) {
            try {
                InstallNativePersistence();
            } catch (const std::exception& error) {
                PS::Log<LogLevel::Warning>(STR("[FEATURE:journal-save-cleanup][UNAVAILABLE] {}. Journal/lore content remains active.\n"),
                    PS::ToWideSafe(error.what()));
            } catch (...) {
                PS::Log<LogLevel::Warning>(STR("[FEATURE:journal-save-cleanup][UNAVAILABLE] Native adapter initialization failed. Journal/lore content remains active.\n"));
            }
        }
        RegisterHooks();
        m_initialJournalApplied = true;
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
            auto* entry = found->second.Get();
            if (!entry) throw std::runtime_error("Cached journal entry is no longer live; skipping until definitions are reloaded");
            return entry;
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
            m_entries.emplace(def.Key, EntryHandle(entry));
            return entry;
        }

        auto* entryClass = ResolveEntryClass(def.Body);
        auto* transientPackage = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, TEXT("/Engine/Transient"), false);
        if (!entryClass || !transientPackage)
        {
            throw std::runtime_error("entry class or transient package was unavailable");
        }
        if (UECustom::UObjectGlobals::StaticFindObject(nullptr, transientPackage, def.Key.c_str(), false))
            throw std::runtime_error("Journal entry ID is already in use; journal and lore must use distinct IDs");

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
        m_entries.emplace(def.Key, EntryHandle(entry));
        return entry;
    }

    void DragonWildsJournalModLoader::ApplyProperties(UObject* entry, const nlohmann::json& body)
    {
        if (body.contains("Type")) {
            auto* expected = ResolveEntryClass(body);
            if (!expected || !entry->IsA(expected))
                throw std::runtime_error("Type cannot change the class of an existing journal entry");
        }
        if (m_loreOnly) {
            auto* loreClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, EntryClassPaths[0]);
            if (!loreClass || !entry->IsA(loreClass))
                throw std::runtime_error("The lore loader accepts only Lore journal entries");
        }

        struct PropertyRollback {
            UObject* entry;
            std::vector<std::unique_ptr<JournalFieldCopy>> fields;
            bool committed = false;
            ~PropertyRollback() {
                if (!committed) for (auto& field : fields)
                    field->Commit(field->property->ContainerPtrToValuePtr<void>(entry));
            }
        } rollback{entry};
        std::unordered_set<FProperty*> saved;
        auto preserve = [&](const RC::StringType& name) {
            auto* field = PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), name);
            if (field && saved.insert(field).second)
                rollback.fields.push_back(std::make_unique<JournalFieldCopy>(field, field->ContainerPtrToValuePtr<void>(entry)));
        };
        for (const auto& [name, unused] : body.items()) preserve(RC::to_generic_string(name));
        for (const auto* name : {TEXT("ItemData"), TEXT("RecipeData"), TEXT("Image"), TEXT("JournalItemData")}) preserve(name);
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
                }
                else
                {
                    throw std::runtime_error("RecipeData '" + RC::to_string(reference)
                        + "' could not be resolved; define that /recipes key or use a valid cooked RecipeData path.");
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

                auto* table = ResolveSoftReference(DataTableClassPath, tableName);
                if (!table)
                {
                    throw std::runtime_error(std::format("Data table '{}' was not loaded.",
                        RC::to_string(tableName)));
                }

                auto* dataTable = static_cast<UDataTable*>(table);
                auto* rowType = dataTable->GetRowStruct().Get();
                if (!rowType || rowType->GetPathName() != TEXT("/Script/Dominion.CraftingStationDataTableRow"))
                    throw std::runtime_error("Journal recipe station requires a CraftingStationDataTableRow table");
                const FName row(rowName, FNAME_Add);
                if (!dataTable->FindRowUnchecked(row))
                    throw std::runtime_error("Journal recipe station row was not found: " + RC::to_string(rowName));
                auto* handle = CastField<FStructProperty>(property);
                auto* type = handle ? handle->GetStruct().Get() : nullptr;
                auto* tableField = type ? CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type, TEXT("DataTable"))) : nullptr;
                auto* rowField = type ? CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type, TEXT("RowName"))) : nullptr;
                if (!type || type->GetPathName() != TEXT("/Script/Engine.DataTableRowHandle")
                    || !tableField || tableField->GetSize() != sizeof(table) || !rowField)
                    throw std::runtime_error("Journal station row handle does not match the live property contract");
                auto* destination = property->ContainerPtrToValuePtr<void>(entry);
                std::memcpy(tableField->ContainerPtrToValuePtr<void>(destination), &table, sizeof(table));
                rowField->SetPropertyValue(rowField->ContainerPtrToValuePtr<void>(destination), row);
            }
            else
            {
                PropertyHelper::CopyJsonValueToContainer(entry, property, value);
            }
        }

        auto resolveField = [&](const TCHAR* name, bool required) -> UObject* {
            auto* field = CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), name));
            if (!field || field->GetSize() != sizeof(UECustom::FSoftObjectPtr))
                throw std::runtime_error("Journal reference property layout changed: " + RC::to_string(name));
            auto* reference = field->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(entry);
            if (reference->ObjectID.GetAssetFName() == NAME_None) {
                if (required) throw std::runtime_error("Journal entry requires " + RC::to_string(name));
                return nullptr;
            }
            UECustom::TSoftObjectPtr<UObject> soft{reference->ObjectID};
            auto* target = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            if (!target) throw std::runtime_error("Journal asset could not be loaded: " + RC::to_string(name));
            SetLiveSoftReference(entry, field, target);
            return target;
        };
        resolveField(TEXT("Image"), false);
        resolveField(TEXT("JournalItemData"), false);
        auto* recipeClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, EntryClassPaths[5]);
        if (recipeClass && entry->IsA(recipeClass)) {
            auto* recipe = resolveField(TEXT("RecipeData"), true);
            auto* item = resolveField(TEXT("ItemData"), false);
            auto* outputs = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(recipe->GetClassPrivate(), TEXT("ItemsCreated")));
            auto* output = outputs ? CastField<FStructProperty>(outputs->GetInner()) : nullptr;
            auto* itemField = output && output->GetStruct() ? CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(output->GetStruct().Get(), TEXT("ItemData"))) : nullptr;
            if (!outputs || !itemField || itemField->GetSize() != sizeof(UObject*))
                throw std::runtime_error("Recipe ItemsCreated contract is unavailable");
            std::unordered_set<UObject*> created;
            UECustom::FScriptArrayHelper items(outputs->ContainerPtrToValuePtr<FScriptArray>(recipe), outputs);
            items.ForEachElement([&](void* value) {
                UObject* target{}; std::memcpy(&target, itemField->ContainerPtrToValuePtr<void>(value), sizeof(target));
                if (target) created.insert(target);
            });
            if (!item && created.size() == 1) {
                item = *created.begin();
                SetLiveSoftReference(entry, PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), TEXT("ItemData")), item);
            }
            if (!item || !created.contains(item))
                throw std::runtime_error("Journal ItemData must identify a recipe output; specify it for multi-output recipes");
            auto* handle = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), TEXT("StationTableRowHandle")));
            auto* type = handle ? handle->GetStruct().Get() : nullptr;
            auto* tableField = type ? CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type, TEXT("DataTable"))) : nullptr;
            auto* rowField = type ? CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type, TEXT("RowName"))) : nullptr;
            if (!handle || !tableField || tableField->GetSize() != sizeof(UObject*) || !rowField)
                throw std::runtime_error("Journal recipe station handle is unavailable");
            auto* data = handle->ContainerPtrToValuePtr<void>(entry);
            UObject* table{}; std::memcpy(&table, tableField->ContainerPtrToValuePtr<void>(data), sizeof(table));
            auto* expected = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, DataTableClassPath);
            auto* station = table && expected && table->IsA(expected) ? static_cast<UDataTable*>(table) : nullptr;
            if (!station || !station->GetRowStruct()
                || station->GetRowStruct()->GetPathName() != TEXT("/Script/Dominion.CraftingStationDataTableRow")
                || !station->FindRowUnchecked(rowField->GetPropertyValue(rowField->ContainerPtrToValuePtr<void>(data))))
                throw std::runtime_error("Recipe journal requires a valid StationTableRowHandle (crafting-station table and row)");
        }
        rollback.committed = true;
    }

    bool DragonWildsJournalModLoader::Place(UObject* entry, const JournalDef& def)
    {
        if (!def.Body.contains("AddTo") || !def.Body.at("AddTo").is_object())
        {
            throw std::runtime_error("AddTo object is required");
        }
        const auto& addTo = def.Body.at("AddTo");
        const auto placement=PS::JournalPlacement::Parse(addTo,RC::to_string(def.Key));
        auto path = RC::to_generic_string(placement.SubCategory);
        auto keyString = RC::to_generic_string(placement.Key);
        if (path.empty())
        {
            throw std::runtime_error("AddTo.SubCategory is required.");
        }
        if (keyString.empty())
        {
            keyString = def.Key;
        }

        UObject* subCategory=nullptr;
        if(path.starts_with(TEXT("/"))) {
            UECustom::TSoftObjectPtr<UObject> softSubCategory{ UECustom::FSoftObjectPath(path) };
            subCategory=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softSubCategory);
        } else {
            auto* categoryClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.JournalSubCategoryData"));
            if(!categoryClass)throw std::runtime_error("Journal category class unavailable");
            auto normalize=[](RC::StringType value) {
                std::transform(value.begin(),value.end(),value.begin(),[](auto c){return static_cast<TCHAR>(std::towlower(c));});return value;
            };
            const auto wanted=normalize(path);
            TArray<UObject*> categories;UECustom::UObjectGlobals::GetObjectsOfClass(categoryClass,categories,true);
            for(auto* candidate:categories) {
                if(!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))continue;
                bool matches=normalize(candidate->GetName())==wanted;
                auto* name=CastField<FTextProperty>(PropertyHelper::GetPropertyByName(candidate->GetClassPrivate(),TEXT("Name")));
                if(name)matches=matches || normalize(PropertyHelper::GetTextAsString(name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(candidate))))==wanted;
                auto* internal=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(candidate->GetClassPrivate(),TEXT("InternalName")));
                if(internal)matches=matches || normalize(RC::StringType(**internal->ContainerPtrToValuePtr<FString>(candidate)))==wanted;
                if(!matches)continue;
                if(subCategory && subCategory!=candidate)throw std::runtime_error("Journal category name is ambiguous; use its full asset path");
                subCategory=candidate;
            }
        }
        if (!subCategory)
        {
            throw std::runtime_error(std::format("AddTo.SubCategory could not be loaded: {}", RC::to_string(path)));
        }

        FMapProperty* dataMapProperty = nullptr;
        void* dataMap = nullptr;
        const auto hierarchy = PrepareJournalHierarchy(FindJournalSubsystem(), subCategory,
            FName(keyString, FNAME_Add), placement.TargetGroup
                ? FName(RC::to_generic_string(placement.TargetGroup->Id), FNAME_Add) : FName());
        if(subCategory->GetClassPrivate()->GetPathName()==TEXT("/Script/Dominion.JournalSubCategoryByGroupData"))
        {
            const bool changed = PlaceJournalGroup(subCategory,entry,placement);
            hierarchy.Publish(entry);
            return changed;
        }
        if (subCategory->IsA(m_noBiomeSubCategoryClass))
        {
            if(placement.TargetGroup)throw std::runtime_error("This journal category does not support groups");
            if (addTo.contains("Biome"))
                throw std::runtime_error("AddTo.Biome cannot be applied to a non-biome journal subcategory; entry was not placed.");
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
        else
        {
            throw std::runtime_error(std::format(
                "Unsupported journal subcategory '{}' (class '{}'). Entry was not placed.",
                RC::to_string(path), RC::to_string(subCategory->GetClassPrivate()->GetPathName())));
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
        });
        if (alreadyPresent)
        {
            hierarchy.Publish(entry);
            return false;
        }

        UECustom::FManagedValue pair;
        map.InitializePair(pair);
        *static_cast<FName*>(map.GetKeyPtr(pair.GetData())) = key;
        auto* soft = static_cast<UECustom::FSoftObjectPtr*>(map.GetValuePtr(pair.GetData()));
        soft->ObjectID = UECustom::FSoftObjectPath(entry->GetPathName());
        map.Add(pair);
        map.Rehash();
        hierarchy.Publish(entry);
        return true;
    }

    UObject* DragonWildsJournalModLoader::FindJournalSubsystem()
    {
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_journalSubsystemClass, objects, true);
        UObject* selected=nullptr;
        for (auto* object : objects)
        {
            if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))continue;
            if(selected)throw std::runtime_error("Journal subsystem is ambiguous; registration refused");
            selected=object;
        }
        return selected;
    }

    void DragonWildsJournalModLoader::RegisterEntry(UObject* entry,const RC::StringType& owner)
    {
        auto* subsystem = FindJournalSubsystem();
        if (!subsystem)
        {
            throw std::runtime_error("Journal subsystem was unavailable.");
        }
        auto* persistenceProperty = CastField<FStrProperty>(
            PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), TEXT("PersistenceID")));
        if(!persistenceProperty || persistenceProperty->GetArrayDim()!=1)
            throw std::runtime_error("Journal persistence identity field changed");
        const auto& persistenceId = persistenceProperty->GetPropertyValue(
            persistenceProperty->ContainerPtrToValuePtr<void>(entry));
        try {
            QuestRegistry::NativeRegistry::RegisterJournal(subsystem,subsystem->GetOuterPrivate(),entry);
        } catch(const std::exception& error) {
            throw std::runtime_error(std::string("Journal registry: ")+error.what());
        }
        TrackOwnedId(entry, persistenceId,owner);
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
            PS::Log<LogLevel::Error>(STR("Journal persistence event was not found; custom entry unlocks are disabled.\n"));
            return;
        }
        size_t parameterCount=0;
        bool valid=function->GetParmsSize()==32 && !function->GetReturnProperty();
        for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default)) {
            if(!field->HasAnyPropertyFlags(CPF_Parm))continue;
            ++parameterCount;
            const auto name=field->GetName();
            const int offset=name==TEXT("InUnlockedEntries")?0:name==TEXT("InUnreadEntries")?16:-1;
            auto* array=CastField<FArrayProperty>(field);
            auto* inner=array?CastField<FStructProperty>(array->GetInner()):nullptr;
            auto* type=inner?inner->GetStruct().Get():nullptr;
            auto* netId=type?CastField<FUInt16Property>(PropertyHelper::GetPropertyByName(type,TEXT("NetId"))):nullptr;
            valid=valid && offset>=0 && field->GetOffset_Internal()==offset
                && field->GetElementSize()==16 && field->GetArrayDim()==1
                && type && type->GetPathName()==TEXT("/Script/Dominion.DominionDataAssetNetId")
                && inner->GetElementSize()==2 && inner->GetArrayDim()==1 && type->GetStructureSize()==2
                && netId && netId->GetOffset_Internal()==0 && netId->GetElementSize()==2 && netId->GetArrayDim()==1;
        }
        if(!valid || parameterCount!=2) {
            PS::Log<LogLevel::Error>(STR("Journal persistence event layout changed; unlock hook was not installed.\n"));
            return;
        }
        const auto hookId=PS::RegisterNativePostHook(function, [this](UnrealScriptFunctionCallableContext& context, void*) {
            try {UnlockEntries(context.Context);}
            catch(const std::exception& error) {
                PS::Log<LogLevel::Error>(STR("Journal persistence unlock failed: {}\n"),PS::ToWideSafe(error.what()));
            }
            catch(...) {PS::Log<LogLevel::Error>(STR("Journal persistence unlock failed with an unknown exception.\n"));}
        });
        if(!hookId) {
            PS::Log<LogLevel::Error>(STR("Journal persistence hook registration failed; custom unlock delivery is unavailable.\n"));
            return;
        }
        m_hooksActive = true;
    }

    void DragonWildsJournalModLoader::UnlockEntries(UObject* journalComponent)
    {
        if (!journalComponent || !journalComponent->IsA(m_journalComponentClass))
        {
            PS::Log<LogLevel::Error>(STR("Journal persistence event supplied an invalid component.\n"));
            return;
        }

        size_t added=0,unavailable=0;
        for (auto& key : m_unlock)
        {
            auto found = m_entries.find(key);
            if (found == m_entries.end())
            {
                ++unavailable;
                continue;
            }

            UObject* entry = found->second.Get();
            if (!entry) {++unavailable;continue;}
            try {
                if(JournalPlayerAccess::EnsureUnlocked(journalComponent,entry))++added;
            }catch(const std::exception& error) {
                ++unavailable;
                PS::Log<LogLevel::Error>(STR("Journal unlock '{}': {}\n"),key,PS::ToWideSafe(error.what()));
            }
        }
        if(added)PS::Log<LogLevel::Verbose>(STR("Journal unlock: {} entries added to '{}'. UI visibility still depends on category placement.\n"),added,journalComponent->GetPathName());
        if(unavailable)PS::Log<LogLevel::Warning>(STR("Journal unlock: {} requested entries unavailable; asset placement does not confirm player unlock.\n"),unavailable);
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

    void DragonWildsJournalModLoader::TrackOwnedId(UObject* entry, const FString& persistenceId,const RC::StringType& owner,bool declared)
    {
        if ((!m_createdEntries.contains(entry) && !declared) || persistenceId.GetCharArray().Num() <= 1)
        {
            return;
        }

        const auto id=RC::to_string(RC::StringType(*persistenceId)),mod=RC::to_string(owner);
        const auto [found,added]=m_ownedIds.emplace(id,mod);
        if(!added && found->second!=mod)throw std::runtime_error("Journal persistence ownership transfer refused");
    }

    void DragonWildsJournalModLoader::InstallNativePersistence()
    {
        // The journal JSON ABI is native and build-specific. The WinGDK
        // reader/writer pair is known, but its JSON helper ABI is not yet a
        // complete verified contract. Never run the Steam adapter in that
        // process; unlock delivery below remains storefront-neutral.
        if (PS::Storefront::CurrentNativeLane() == PS::Storefront::NativeLane::GamePassNative)
            throw std::runtime_error("WinGDK journal save-cleanup adapter is not verified for this build");
        JournalPersistence::Install(this, JournalSave::Owners(m_ownedIds.begin(),m_ownedIds.end()), m_journalComponentClass);
    }
}
