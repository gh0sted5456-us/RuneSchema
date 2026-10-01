#include "Utility/NativeFunctionHook.h"
#include <cstring>
#include <algorithm>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/World.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "Utility/Logging.h"
#include "Core/SaveCleanup.h"
#include "Core/SaveRegistrySnapshot.h"
#include "Misc/DragonWildsDataRegistrar.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    static constexpr const TCHAR* ItemDataClassPath = TEXT("/Script/Dominion.ItemData");
    static constexpr const TCHAR* RecipeDataClassPath = TEXT("/Script/Dominion.RecipeData");
    static constexpr const TCHAR* QuestDataClassPath = TEXT("/Script/Dominion.QuestData");
    static constexpr const TCHAR* JournalDataClassPath = TEXT("/Script/Dominion.JournalEntryWorldData");
    static constexpr const TCHAR* JournalSubsystemClassPath = TEXT("/Script/Dominion.JournalSubsystem");
    static constexpr const TCHAR* JournalLoadedPath =
        TEXT("/Script/Dominion.JournalComponent:Client_HandleJournalEntriesLoadedFromPersistence");

    static constexpr struct {
        const TCHAR* DataClassPath;
        const TCHAR* SubsystemClassPath;
    } RegistryBindings[] = {
        { TEXT("/Script/Dominion.ItemData"),   TEXT("/Script/Dominion.ItemSubsystem") },
        { TEXT("/Script/Dominion.RecipeData"), TEXT("/Script/Dominion.RecipeSubsystem") },
        { TEXT("/Script/Dominion.QuestData"),  TEXT("/Script/Dominion.QuestDataSubsystem") },
    };

    static constexpr const TCHAR* SaveLoadHookPaths[] = {
        TEXT("/Script/Dominion.DominionPlayerController:OnInventoryLoadedFromSave"),
        TEXT("/Script/Dominion.DominionPlayerController:OnPersonalInventoryLoadedFromSave"),
        TEXT("/Script/Dominion.QuestProgressComponent:OnQuestsUpdated"),
        JournalLoadedPath,
    };

    static std::string RegistryFingerprint(
        const PS::SaveCleanup::RegistrySnapshot& snapshot)
    {
        std::string result;
        const auto append = [&](char label,
            const std::unordered_set<std::string>& values) {
            std::vector<std::string_view> sorted;
            sorted.reserve(values.size());
            for (const auto& value : values) sorted.push_back(value);
            std::ranges::sort(sorted);
            result.push_back(label);
            result += std::to_string(sorted.size());
            result.push_back(':');
            for (const auto value : sorted)
            {
                result += std::to_string(value.size());
                result.push_back('=');
                result.append(value);
            }
            result.push_back(';');
        };
        append('I', snapshot.Items);
        append('R', snapshot.Recipes);
        append('Q', snapshot.Quests);
        append('J', snapshot.Journals);
        result += snapshot.QuestsComplete ? "Q1" : "Q0";
        result += snapshot.JournalsComplete ? "J1" : "J0";
        return result;
    }

    void DragonWildsDataRegistrar::Initialize()
    {
        if (!m_initialized)
        {
            m_pruner.PrepareForStartup();
            if (!ResolveBindings())
            {
                return;
            }

            InstallHooks();
            m_initialized = true;
        }

        RegisterAll();
    }

    bool DragonWildsDataRegistrar::ResolveBindings()
    {
        for (auto& binding : RegistryBindings)
        {
            auto* dataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, binding.DataClassPath);
            auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, binding.SubsystemClassPath);
            if (!dataClass || !subsystemClass)
            {
                PS::Log<LogLevel::Warning>(STR("Registry pair {} -> {} was not found and won't be handled.\n"),
                    binding.DataClassPath, binding.SubsystemClassPath);
                continue;
            }

            m_bindings.emplace_back(dataClass, subsystemClass);
        }

        if (m_bindings.empty())
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize the Data Registrar, no registry subsystems were found.\n"));
            return false;
        }

        return true;
    }

    void DragonWildsDataRegistrar::Shutdown()
    {
        if (m_gameStateStartingHook != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_gameStateStartingHook);
        m_gameStateStartingHook = Hook::ERROR_ID;
        if (m_gameStateReadyHook != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_gameStateReadyHook);
        m_gameStateReadyHook = Hook::ERROR_ID;
        for (const auto& [function, id] : m_functionHooks) if (function && id) function->UnregisterHook(id);
        m_functionHooks.clear();
        m_registryCandidateFingerprint.clear();
        m_registryCandidatePasses = 0;
        PS::SaveCleanup::PublishRegistry({});
    }

    void DragonWildsDataRegistrar::InstallHooks()
    {
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DataRegistrarBeforeGameState");

        // Character JSON is hydrated during startup and the first world
        // transition. Restore every loaded identity before Dominion reads the
        // character. Save cleanup is performed only on the JSON value the game
        // is about to hydrate; RuneSchema never rewrites the stored file.
        m_gameStateStartingHook = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase* mode) {
                RegisterAll();
            }, options);

        options.HookName = TEXT("DataRegistrarGameStateReady");
        m_gameStateReadyHook = Hook::RegisterInitGameStatePostCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                RegisterAll();
            }, options);

        // The mandatory character boundary is installed during PreInitialize.
        // These best-effort hooks only keep its live-registry snapshot current.
        for (auto* hookPath : SaveLoadHookPaths)
        {
            try
            {
                auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                    nullptr, nullptr, hookPath);
                if (!function)
                {
                    PS::Log<LogLevel::Warning>(STR(
                        "Save load hook '{}' was not found.\n"), hookPath);
                    continue;
                }

                const auto id = PS::RegisterNativePreHook(function,
                    [](UnrealScriptFunctionCallableContext&, void* customData) {
                        static_cast<DragonWildsDataRegistrar*>(
                            customData)->RegisterAll();
                    }, this);
                if (id != Hook::ERROR_ID)
                    m_functionHooks.emplace_back(function, id);
                else
                    PS::Log<LogLevel::Warning>(STR(
                        "Save load hook '{}' could not be registered.\n"),
                        hookPath);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Save load hook '{}' was isolated after registration failed: {}.\n"),
                    hookPath, PS::ToWideSafe(error.what()));
            }
            catch (...)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Save load hook '{}' was isolated after registration failed.\n"),
                    hookPath);
            }
        }
    }

    void DragonWildsDataRegistrar::RegisterAll()
    {
        PS::SaveCleanup::RegistrySnapshot snapshot;
        bool itemsReady = false;
        bool recipesReady = false;
        bool questsReady = false;
        bool registrationsComplete = true;

        for (auto& [dataClass, subsystemClass] : m_bindings)
        {
            TArray<UObject*> subsystems;
            UECustom::UObjectGlobals::GetObjectsOfClass(
                subsystemClass, subsystems, true);
            bool foundSubsystem = false;
            for (auto* subsystem : subsystems)
            {
                if (!subsystem || subsystem->HasAnyFlags(
                    static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject
                        | RF_BeginDestroyed | RF_FinishDestroyed)))
                    continue;
                foundSubsystem = true;

                // An outgoing world may still own a subsystem when the next
                // world starts. Populate every live instance so saved recipe
                // identities cannot be written against only the old one.
                registrationsComplete = RegisterMissing(dataClass, subsystem)
                    && registrationsComplete;

                auto* idMapProperty = CastField<FMapProperty>(
                    PropertyHelper::GetPropertyByName(
                        subsystem->GetClassPrivate(),
                        TEXT("PersistenceIDToDataMap")));
                if (!idMapProperty) continue;

                std::unordered_set<std::string>* target = nullptr;
                const auto classPath = dataClass->GetPathName();
                if (classPath == ItemDataClassPath)
                {
                    target = &snapshot.Items;
                    itemsReady = true;
                }
                else if (classPath == RecipeDataClassPath)
                {
                    target = &snapshot.Recipes;
                    recipesReady = true;
                }
                else if (classPath == QuestDataClassPath)
                {
                    target = &snapshot.Quests;
                    questsReady = true;
                }
                if (!target) continue;

                UECustom::FScriptMapHelper idMap(
                    idMapProperty,
                    idMapProperty->ContainerPtrToValuePtr<void>(subsystem));
                idMap.ForEachPair([&](void* keyPtr, void*) {
                    auto* key = static_cast<FString*>(keyPtr);
                    if (key && key->GetCharArray().Num() > 1)
                        target->insert(RC::to_string(RC::StringType(**key)));
                });
            }
            if (!foundSubsystem)
            {
                registrationsComplete = false;
                PS::Log<LogLevel::Warning>(STR(
                    "No {} instance exists yet; {} assets cannot be registered.\n"),
                    subsystemClass->GetName(), dataClass->GetName());
            }
        }

        snapshot.QuestsComplete = questsReady && !snapshot.Quests.empty();
        if (auto* journalClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, JournalSubsystemClassPath, false))
        {
            TArray<UObject*> journalSubsystems;
            UECustom::UObjectGlobals::GetObjectsOfClass(
                journalClass, journalSubsystems, true);
            for (auto* journalSubsystem : journalSubsystems)
            {
                if (!journalSubsystem || journalSubsystem->HasAnyFlags(
                    static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject
                        | RF_BeginDestroyed | RF_FinishDestroyed)))
                    continue;
                if (auto* journalMapProperty = CastField<FMapProperty>(
                        PropertyHelper::GetPropertyByName(
                            journalSubsystem->GetClassPrivate(),
                            TEXT("PersistenceIDToDataMap"))))
                {
                    UECustom::FScriptMapHelper journalMap(
                        journalMapProperty,
                        journalMapProperty->ContainerPtrToValuePtr<void>(
                            journalSubsystem));
                    journalMap.ForEachPair([&](void* keyPtr, void*) {
                        auto* key = static_cast<FString*>(keyPtr);
                        if (key && key->GetCharArray().Num() > 1)
                            snapshot.Journals.insert(RC::to_string(
                                RC::StringType(**key)));
                    });
                    snapshot.JournalsComplete = !snapshot.Journals.empty();
                }
            }
        }
        if (itemsReady && recipesReady && registrationsComplete)
        {
            // Never prune from the first apparently complete view. A second
            // identical capture must prove that late native and mod
            // registration has settled. Any change immediately withdraws the
            // prior snapshot, making character preflight a strict no-op.
            const auto fingerprint = RegistryFingerprint(snapshot);
            if (fingerprint != m_registryCandidateFingerprint)
            {
                m_registryCandidateFingerprint = fingerprint;
                m_registryCandidatePasses = 1;
                PS::SaveCleanup::PublishRegistry({});
                return;
            }
            if (m_registryCandidatePasses < 2)
                ++m_registryCandidatePasses;
            PS::SaveCleanup::PublishRegistry(snapshot);
            if (!m_registrySummaryReported)
            {
                m_registrySummaryReported = true;
                PS::Log<LogLevel::Verbose>(STR(
                    "Persistence registry ready from base game and loaded paks: items={}, recipes={}, quests={}, journal={}.\n"),
                    snapshot.Items.size(), snapshot.Recipes.size(),
                    snapshot.Quests.size(), snapshot.Journals.size());
            }
        }
        else
        {
            // Never leave a previous world's registry available to Safe Clean
            // when the current world could not prove complete item/recipe maps
            // and successful primary + network registration for loaded assets.
            m_registryCandidateFingerprint.clear();
            m_registryCandidatePasses = 0;
            PS::SaveCleanup::PublishRegistry({});
        }
    }

    bool DragonWildsDataRegistrar::RegisterMissing(UClass* dataClass, UObject* subsystem)
    {
        auto* idMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
        if (!idMapProperty)
        {
            PS::Log<LogLevel::Warning>(STR("PersistenceIDToDataMap was not found on {}.\n"), subsystem->GetClassPrivate()->GetName());
            return false;
        }

        bool complete = true;

        std::unordered_map<RC::StringType, UObject*> known;
        UECustom::FScriptMapHelper idMap(idMapProperty, idMapProperty->ContainerPtrToValuePtr<void>(subsystem));
        idMap.ForEachPair([&](void* keyPtr, void* valuePtr) {
            auto* key = static_cast<FString*>(keyPtr);
            UObject* value = nullptr;
            std::memcpy(&value, valuePtr, sizeof(value));
            if (key->GetCharArray().Num() > 1 && value)
            {
                const auto identity = RC::StringType(**key);
                auto* property = value->GetClassPrivate()
                    ? CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                        value->GetClassPrivate(), TEXT("PersistenceID")))
                    : nullptr;
                const auto roundTrip = property
                    ? property->GetPropertyValue(
                        property->ContainerPtrToValuePtr<void>(value))
                    : FString{};
                if (!property || roundTrip.GetCharArray().Num() <= 1
                    || RC::StringType(*roundTrip) != identity
                    || !known.emplace(identity, value).second)
                {
                    complete = false;
                    PS::Log<LogLevel::Error>(STR(
                        "Persistence registry entry '{}' does not round-trip to one live data asset; startup cleanup is disabled.\n"),
                        **key);
                }
            }
            else
            {
                complete = false;
            }
        });

        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(dataClass, candidates, true);

        for (auto* candidate : candidates)
        {
            try
            {
                if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                {
                    continue;
                }

                auto* candidateClass = candidate->GetClassPrivate();
                auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(candidateClass, TEXT("PersistenceID")));
                if (!idProperty)
                {
                    PS::Log<LogLevel::Warning>(STR("'{}' has no PersistenceID property and cannot be registered.\n"), candidate->GetName());
                    continue;
                }

                auto persistenceId = idProperty->GetPropertyValue(idProperty->ContainerPtrToValuePtr<void>(candidate));
                if (persistenceId.GetCharArray().Num() <= 1)
                {
                    PS::Log<LogLevel::Warning>(STR("'{}' has an empty PersistenceID and cannot be registered.\n"), candidate->GetName());
                    continue;
                }

                auto idString = RC::StringType(*persistenceId);
                candidate->SetRootSet();

                const auto existing = known.find(idString);
                if (existing == known.end())
                {
                    if (!InsertIntoMap(subsystem, TEXT("PersistenceIDToDataMap"), persistenceId, candidate))
                        throw std::runtime_error(
                            "primary persistence registry rejected the asset");
                    InsertIntoMap(subsystem, TEXT("InternalNameToDataMap"), persistenceId, candidate);

                    if (auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(candidateClass, TEXT("InternalName"))))
                    {
                        auto internalName = nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(candidate));
                        if (internalName.GetCharArray().Num() > 1 && RC::StringType(*internalName) != idString)
                        {
                            InsertIntoMap(subsystem, TEXT("InternalNameToDataMap"), internalName, candidate);
                        }
                    }

                    known.emplace(idString, candidate);
                }
                else if (existing->second != candidate)
                    throw std::runtime_error(
                        "duplicate PersistenceID resolves to multiple live assets");

                if (EnsureNetworkIdentity(candidate, subsystem) < 0)
                {
                    throw std::runtime_error("network registry rejected the asset");
                }
            }
            catch (const std::exception& e)
            {
                complete = false;
                PS::Log<LogLevel::Error>(STR("Failed registering '{}': {}\n"),
                    candidate ? candidate->GetName() : STR("<null>"), PS::ToWideSafe(e.what()));
            }
        }
        return complete;
    }

    int32_t DragonWildsDataRegistrar::EnsureNetworkIdentity(
        UObject* dataAsset, UObject* subsystem)
    {
        auto* subsystemClass = subsystem->GetClassPrivate();
        auto* reverseProperty = CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(subsystemClass, TEXT("DataToNetIdMap")));
        auto* arrayProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(subsystemClass, TEXT("NetIdToData")));
        if (!reverseProperty || !arrayProperty)
        {
            PS::Log<LogLevel::Warning>(STR("Network data registry was not found on {}.\n"),
                subsystemClass->GetName());
            return -1;
        }

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        int32_t existingId = -1;
        reverse.ForEachPair([&](void* keyPtr, void* valuePtr) {
            UObject* existing = nullptr;
            std::memcpy(&existing, keyPtr, sizeof(existing));
            if (existing == dataAsset)
            {
                uint16 netId = 0;
                std::memcpy(&netId, valuePtr, sizeof(netId));
                existingId = static_cast<int32_t>(netId);
            }
        });
        if (existingId >= 0)
        {
            return existingId;
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        if (array->Num() >= std::numeric_limits<uint16>::max())
        {
            return -1;
        }

        const auto netId = static_cast<uint16>(array->Num());
        UECustom::FManagedValue value;
        arrayHelper.InitializeValue(value);
        std::memcpy(value.GetData(), &dataAsset, sizeof(dataAsset));
        arrayHelper.Add(value);

        UECustom::FManagedValue reversePair;
        reverse.InitializePair(reversePair);
        std::memcpy(reverse.GetKeyPtr(reversePair.GetData()), &dataAsset, sizeof(dataAsset));
        std::memcpy(reverse.GetValuePtr(reversePair.GetData()), &netId, sizeof(netId));
        reverse.Add(reversePair);
        reverse.Rehash();
        return static_cast<int32_t>(netId);
    }

    UObject* DragonWildsDataRegistrar::FindSubsystemInstance(UClass* subsystemClass)
    {
        TArray<UObject*> subsystems;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, subsystems, true);

        for (auto* subsystem : subsystems)
        {
            if (subsystem && !subsystem->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                return subsystem;
            }
        }

        return nullptr;
    }

    bool DragonWildsDataRegistrar::InsertIntoMap(UObject* subsystem, const RC::StringType& mapName,
        const FString& key, UObject* value)
    {
        auto* mapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(), mapName));
        if (!mapProperty)
        {
            PS::Log<LogLevel::Warning>(STR("Map '{}' was not found on {}.\n"), mapName, subsystem->GetClassPrivate()->GetName());
            return false;
        }

        auto* mapPtr = mapProperty->ContainerPtrToValuePtr<void>(subsystem);
        UECustom::FScriptMapHelper helper(mapProperty, mapPtr);

        UECustom::FManagedValue pair;
        helper.InitializePair(pair);
        *static_cast<FString*>(helper.GetKeyPtr(pair.GetData())) = key;
        *static_cast<UObject**>(helper.GetValuePtr(pair.GetData())) = value;

        helper.Add(pair);
        helper.Rehash();
        return true;
    }
}
