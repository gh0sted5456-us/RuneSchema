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
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsJournalModLoader.h"
#include "Loader/DragonWildsRecipeModLoader.h"
#include "Loader/JournalPlayerAccess.h"
#include "Loader/JournalSaveOwnership.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Core/JsonPatchDirective.h"
#include "Core/SaveRegistrySnapshot.h"
#include "Core/JournalPlacement.h"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Core/HAL/UnrealMemory.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Loader/JournalNativeContract.h"
#include "Loader/JournalWinGDKContract.h"
#include "Loader/JournalPersistenceContract.h"
#include "Loader/JournalJsonFieldContract.h"
#include "Generator/NativeCallResolver.h"
#include "Loader/QuestNativeRegistry.h"

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
        bool RegisteredJournalEntry(UObject* entry)
        {
            if (!entry) return false;
            const auto registry = PS::SaveCleanup::ReadRegistry();
            if (!registry || !registry->JournalsComplete) return false;
            auto* idProperty = CastField<FStrProperty>(
                PropertyHelper::GetPropertyByName(
                    entry->GetClassPrivate(), TEXT("PersistenceID")));
            if (!idProperty) return false;
            const auto id = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(entry));
            return id.GetCharArray().Num() > 1
                && registry->Journals.contains(
                    RC::to_string(RC::StringType(*id)));
        }

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

        RC::StringType NormalizeReferenceKey(RC::StringType value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](auto character) {
                return static_cast<TCHAR>(std::towlower(character));
            });
            return value;
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

    DragonWildsJournalModLoader::~DragonWildsJournalModLoader()
    {
        if(m_acquisitionCallbackId!=Hook::ERROR_ID)
            Hook::UnregisterCallback(m_acquisitionCallbackId);
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
        ResetFinalizeCaches();
        m_acquisitionUnlocks.clear();
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
                ApplyProperties(entry, def.Body, def.Owner);
                if (m_createdEntries.contains(entry)) {
                    // Runtime journal/lore identity is the authored entry key.
                    // Reassert it after reflected fields so a default/accidental
                    // identity value cannot invalidate the ownership snapshot.
                    const auto identity = RC::to_string(def.Key);
                    for (const auto* fieldName : {TEXT("PersistenceID"), TEXT("InternalName")}) {
                        auto* field = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                            entry->GetClassPrivate(), fieldName));
                        if (!field) throw std::runtime_error(
                            "Owned journal/lore identity fields are unavailable");
                        PropertyHelper::CopyJsonValueToContainer(entry, field, identity);
                    }
                }
                RegisterEntry(entry,def.Owner);
                if(def.Body.contains("UnlockOnAcquire")) {
                    if(!def.Body.at("UnlockOnAcquire").is_boolean())
                        throw std::runtime_error("UnlockOnAcquire must be a boolean");
                    if(def.Body.at("UnlockOnAcquire").get<bool>()) {
                        if(!def.Body.contains("ItemData") || !def.Body.at("ItemData").is_string())
                            throw std::runtime_error("UnlockOnAcquire requires a string ItemData reference");
                        auto* item=ResolveSoftReference(ItemDataClassPath,
                            RC::to_generic_string(def.Body.at("ItemData").get<std::string>()),m_itemReferenceIndex);
                        if(!item)throw std::runtime_error("UnlockOnAcquire ItemData could not be resolved");
                        m_acquisitionUnlocks.push_back({item,def.Key});
                    }
                }
                if (def.Body.contains("AddTo")) {
                    if (Place(entry, def)) ++result.Placements;
                } else if (m_createdEntries.contains(entry))
                    throw std::runtime_error("New journal entries require AddTo");

                if(m_createdEntries.contains(entry) || def.Declared) {
                    auto* id=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("PersistenceID")));
                    auto* name=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("InternalName")));
                    if(!id || !name)throw std::runtime_error("Owned journal/lore identity fields are unavailable");
                    const auto& persistence=id->GetPropertyValue(id->ContainerPtrToValuePtr<void>(entry));
                    const auto actualId=RC::to_string(*persistence);
                    const auto actualName=RC::to_string(*name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(entry)));
                    TrackOwnedId(entry,persistence,def.Owner,def.Declared);
                }

                const bool acquire=def.Body.value("UnlockOnAcquire",false);
                if (ReadBool(def.Body, "Unlock", !def.Declared && !acquire))
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
            PS::LoaderSummary(m_loreOnly ? "lore" : "journal", result.EntriesReady,
                result.EntriesReady, 0, result.Placements, result.ErrorCount);
            if (result.EntriesReady)
                PS::Log<LogLevel::Normal>(STR(
                    "[REGISTRY][{}][ADDED] count={} placements={} errors={} verified={}.\n"),
                    m_loreOnly ? TEXT("LORE") : TEXT("JOURNAL"),
                    result.EntriesReady, result.Placements, result.ErrorCount,
                    result.ErrorCount == 0);
        }
        // Journal definitions still mount if the adapter is unavailable, but
        // player unlock delivery waits until save filtering is verified.
        if (!m_ownedIds.empty()) {
            try {
                InstallNativePersistence();
                m_nativePersistenceReady = true;
            } catch (const std::exception& error) {
                PS::Log<LogLevel::Warning>(STR("[FEATURE:journal-save-cleanup][UNAVAILABLE] {}. Journal/lore content remains active.\n"),
                    PS::ToWideSafe(error.what()));
            } catch (...) {
                PS::Log<LogLevel::Warning>(STR("[FEATURE:journal-save-cleanup][UNAVAILABLE] Native adapter initialization failed. Journal/lore content remains active.\n"));
            }
        }
        RegisterHooks();
        RegisterAcquisitionHook();
        m_initialJournalApplied = true;
        ResetFinalizeCaches();
        return result;
    }

    void DragonWildsJournalModLoader::ResetFinalizeCaches()
    {
        m_recipeReferenceIndex = {};
        m_itemReferenceIndex = {};
        m_tableReferenceIndex = {};
        m_subCategoryCache.clear();
        m_finalizeJournalSubsystem = nullptr;
        m_finalizeJournalSubsystemResolved = false;
    }

    UObject* DragonWildsJournalModLoader::ResolveSoftReference(const TCHAR* classPath,
        const RC::StringType& reference, ReferenceIndex& index)
    {
        auto* expected = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, classPath);
        if (!expected) return nullptr;
        if (reference.starts_with(TEXT("/")))
        {
            UECustom::TSoftObjectPtr<UObject> soft{ UECustom::FSoftObjectPath(reference) };
            auto* target = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            if (target && !target->IsA(expected))
                throw std::runtime_error("Journal reference has the wrong asset class: " + RC::to_string(reference));
            return target;
        }

        if (!index.Built)
        {
            TArray<UObject*> objects;
            UECustom::UObjectGlobals::GetObjectsOfClass(expected, objects, true);
            const auto addReference = [&](const RC::StringType& referenceKey, UObject* object) {
                if (referenceKey.empty()) return;
                const auto key = NormalizeReferenceKey(referenceKey);
                if (index.Ambiguous.contains(key)) return;
                const auto [found, inserted] = index.Unique.emplace(key, object);
                if (!inserted && found->second != object)
                {
                    index.Unique.erase(found);
                    index.Ambiguous.insert(key);
                }
            };
            for (auto* object : objects)
            {
                if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
                    continue;

                addReference(object->GetName(), object);
                for (const auto* propertyName : {TEXT("InternalName"), TEXT("PersistenceID")})
                {
                    auto* property = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                        object->GetClassPrivate(), propertyName));
                    if (!property || property->GetArrayDim() != 1
                        || property->GetElementSize() != sizeof(FString))
                        continue;
                    const auto& value = property->GetPropertyValue(
                        property->ContainerPtrToValuePtr<void>(object));
                    const auto& characters = value.GetCharArray();
                    if (characters.Num() > 1 && characters.GetData()
                        && characters.GetData()[characters.Num() - 1] == 0)
                        addReference(RC::StringType(characters.GetData(), characters.Num() - 1), object);
                }
            }
            index.Built = true;
        }

        const auto key = NormalizeReferenceKey(reference);
        if (index.Ambiguous.contains(key))
            throw std::runtime_error("Ambiguous journal reference; use the full asset path: " + RC::to_string(reference));
        const auto found = index.Unique.find(key);
        return found == index.Unique.end() ? nullptr : found->second;
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

    void DragonWildsJournalModLoader::ApplyProperties(UObject* entry,
        const nlohmann::json& body, const RC::StringType& owner)
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
        UObject* resolvedRecipe = nullptr;
        UObject* resolvedItem = nullptr;
        for (auto& [name, value] : body.items())
        {
            if (name == "Type" || name == "AddTo" || name == "Unlock" || name == "UnlockOnAcquire")
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
                auto* recipe = m_recipeService
                    ? m_recipeService->ResolveReference(owner, value.get<std::string>())
                    : nullptr;
                if (!recipe && reference.starts_with(TEXT("/")))
                    recipe = ResolveSoftReference(RecipeDataClassPath, reference,
                        m_recipeReferenceIndex);
                if (recipe)
                {
                    SetLiveSoftReference(entry, property, recipe);
                    resolvedRecipe = recipe;
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
                auto* item = ResolveSoftReference(ItemDataClassPath, reference, m_itemReferenceIndex);
                if (!item)
                {
                    throw std::runtime_error("ItemData could not be resolved; check the cooked item asset path.");
                }
                SetLiveSoftReference(entry, property, item);
                resolvedItem = item;
            }
            else if (name == "StationTableRowHandle" && value.is_object())
            {
                auto tableName = ReadString(value, "DataTable");
                auto rowName = ReadString(value, "RowName");
                if (tableName.empty() || rowName.empty())
                {
                    throw std::runtime_error("StationTableRowHandle requires DataTable and RowName.");
                }

                auto* table = ResolveSoftReference(DataTableClassPath, tableName, m_tableReferenceIndex);
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

        auto resolveField = [&](const TCHAR* name, bool required, UObject* alreadyResolved = nullptr) -> UObject* {
            auto* field = CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(entry->GetClassPrivate(), name));
            if (!field || field->GetSize() != sizeof(UECustom::FSoftObjectPtr))
                throw std::runtime_error("Journal reference property layout changed: " + RC::to_string(name));
            if (alreadyResolved) {
                SetLiveSoftReference(entry, field, alreadyResolved);
                return alreadyResolved;
            }
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
            auto* recipe = resolveField(TEXT("RecipeData"), true, resolvedRecipe);
            auto* item = resolveField(TEXT("ItemData"), false, resolvedItem);
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
        const auto categoryCacheKey = NormalizeReferenceKey(path);
        if (const auto cached = m_subCategoryCache.find(categoryCacheKey); cached != m_subCategoryCache.end()) {
            subCategory = cached->second;
        } else if(path.starts_with(TEXT("/"))) {
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
        m_subCategoryCache.emplace(categoryCacheKey, subCategory);

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
        if (m_finalizeJournalSubsystemResolved) return m_finalizeJournalSubsystem;
        m_finalizeJournalSubsystemResolved = true;
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_journalSubsystemClass, objects, true);
        UObject* selected=nullptr;
        for (auto* object : objects)
        {
            if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))continue;
            if(selected)throw std::runtime_error("Journal subsystem is ambiguous; registration refused");
            selected=object;
        }
        m_finalizeJournalSubsystem = selected;
        return m_finalizeJournalSubsystem;
    }

    void DragonWildsJournalModLoader::RegisterEntry(UObject* entry,const RC::StringType& owner)
    {
        (void)owner;
        auto* subsystem = FindJournalSubsystem();
        if (!subsystem)
        {
            throw std::runtime_error("Journal subsystem was unavailable.");
        }
        try {
            QuestRegistry::NativeRegistry::RegisterJournal(subsystem,subsystem->GetOuterPrivate(),entry);
        } catch(const std::exception& error) {
            throw std::runtime_error(std::string("Journal registry: ")+error.what());
        }
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
        // Temporary mode is runtime-visible on lanes where the native writer
        // adapter can exclude RuneSchema-owned IDs from the save payload.
        if (!m_nativePersistenceReady)
        {
            PS::RoutineLog("journal", STR("Journal/lore temporary unlocks are unavailable because the native save adapter is not ready; registered entries remain active.\n"));
            return;
        }
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
            if (!RegisteredJournalEntry(entry)) {++unavailable;continue;}
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

    void DragonWildsJournalModLoader::RegisterAcquisitionHook()
    {
        if(m_acquisitionCallbackId!=Hook::ERROR_ID || m_acquisitionUnlocks.empty())return;
        Hook::FCallbackOptions options{};
        options.OwnerModName=TEXT("RuneSchema");
        options.HookName=m_loreOnly?TEXT("RuneSchemaLoreAcquisition"):TEXT("RuneSchemaJournalAcquisition");
        m_acquisitionCallbackId=Hook::RegisterProcessEventPostCallback(
            [this](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void*) {
                try {ObserveAcquisition(source,function);} catch(...) {}
            },options);
        if(m_acquisitionCallbackId==Hook::ERROR_ID)
            PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL] Item-acquisition unlock observer was unavailable.\n"),
                RC::to_generic_string(GetModFolderType()));
    }

    void DragonWildsJournalModLoader::ObserveAcquisition(UObject* source,UFunction* function)
    {
        if(m_observingAcquisition || !source || !function || m_acquisitionUnlocks.empty()
            || !m_nativePersistenceReady)return;
        const auto path=function->GetPathName();
        UObject* controller=nullptr;bool credit=true;
        if(path==TEXT("/Game/Gameplay/Character/Player/BP_PlayerController.BP_PlayerController_C:OnInventoryChanged_BrokenItemFTUE"))
            controller=source;
        else if(path==TEXT("/Script/Dominion.LoadoutComponent:OnReceiveInventoryChanged"))
            controller=source->GetOuterPrivate();
        else if(path==TEXT("/Script/Dominion.InventoryComponent:OnRep_HasLoadedFromSave")) {
            controller=source->GetOuterPrivate();credit=false;
        } else return;
        if(!controller || !controller->GetWorld())return;
        auto* inventoryProperty=CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(controller->GetClassPrivate(),TEXT("InventoryComponent")));
        auto* inventory=inventoryProperty?inventoryProperty->GetObjectPropertyValue(
            inventoryProperty->ContainerPtrToValuePtr<void>(controller)):nullptr;
        if(!inventory || inventory->GetOuterPrivate()!=controller || inventory->GetWorld()!=controller->GetWorld())return;
        auto* slot=FUObjectArray::IndexToObject(controller->GetInternalIndex());
        if(!slot || slot->GetUObject()!=controller || !slot->IsValid(false) || slot->GetSerialNumber()<=0)return;
        const auto identity=std::to_string(controller->GetInternalIndex())+":"+std::to_string(slot->GetSerialNumber());
        auto& baseline=m_acquisitionBaselines[identity];
        auto* journal=FindJournalComponent(controller);
        m_observingAcquisition=true;
        struct Guard {bool& Value;~Guard(){Value=false;}} guard{m_observingAcquisition};
        std::unordered_map<RC::StringType,UObject*> items;
        for(const auto& binding:m_acquisitionUnlocks)if(binding.Item)
            items.emplace(binding.Item->GetPathName(),binding.Item);
        std::unordered_set<RC::StringType> added;
        for(const auto& [itemPath,item]:items) {
            ActorHelper::FunctionCall count(inventory,TEXT("/Script/Dominion.InventoryComponent:GetNumItemsByData"));
            count.Arg(TEXT("ItemData"),item).Invoke();
            const auto current=count.Result<int32_t>();
            if(current<0)continue;
            const auto found=baseline.find(itemPath);
            const auto previous=found==baseline.end()?(credit?0:current):found->second;
            baseline[itemPath]=current;
            if(credit && current>previous)added.insert(itemPath);
        }
        if(!journal)return;
        for(const auto& binding:m_acquisitionUnlocks) {
            if(!binding.Item || !added.contains(binding.Item->GetPathName()))continue;
            const auto entry=m_entries.find(binding.EntryKey);
            if(entry!=m_entries.end())if(auto* object=entry->second.Get();
                RegisteredJournalEntry(object))
                (void)JournalPlayerAccess::EnsureUnlocked(journal,object);
        }
    }

    UObject* DragonWildsJournalModLoader::FindJournalComponent(UObject* controller)
    {
        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_journalComponentClass, objects, true);
        UObject* sameWorld=nullptr;
        for (auto* object : objects)
        {
            if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))continue;
            if(controller) {
                if(object->GetWorld()!=controller->GetWorld())continue;
                bool owned=false;
                for(auto* outer=object->GetOuterPrivate();outer;outer=outer->GetOuterPrivate())
                    if(outer==controller){owned=true;break;}
                if(owned)return object;
                // Some builds attach JournalComponent beneath a player state
                // or pawn rather than directly beneath the controller.  A
                // unique same-world component is safe for single-player; an
                // ambiguous multiplayer world is deliberately rejected.
                if(sameWorld)return nullptr;
                sameWorld=object;
                continue;
            }
            return object;
        }
        return sameWorld;
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
        JournalPersistence::Install(this, JournalSave::Owners(m_ownedIds.begin(),m_ownedIds.end()), m_journalComponentClass);
    }
}
