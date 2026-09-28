#include "Utility/NativeFunctionHook.h"
#include <cstring>
#include <algorithm>
#include <limits>
#include <map>
#include <ranges>
#include <unordered_set>
#include <vector>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
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

    static constexpr const TCHAR* CharacterJsonLoadHookPaths[] = {
        TEXT("/Script/Dominion.DominionPlayerControllerBase:LoadStateFromJson"),
        TEXT("/Script/Dominion.DominionPlayerController:LoadStateFromJson"),
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
        if (m_characterJsonHook != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_characterJsonHook);
        m_characterJsonHook = Hook::ERROR_ID;
        for (const auto& [function, id] : m_functionHooks) if (function && id) function->UnregisterHook(id);
        m_functionHooks.clear();
        m_registryCandidateFingerprint.clear();
        m_registryCandidatePasses = 0;
        m_checkedCharacters.clear();
        PS::SaveCleanup::PublishRegistry({});
    }

    void DragonWildsDataRegistrar::InstallHooks()
    {
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DataRegistrarBeforeGameState");

        // Character JSON is hydrated during world startup. Restore every
        // loaded identity to the new world's subsystem before Dominion reads
        // the character. Save cleanup is performed only on the JSON value the
        // game is about to hydrate; RuneSchema never rewrites the stored file.
        m_gameStateStartingHook = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                RegisterAll();
            }, options);

        options.HookName = TEXT("DataRegistrarGameStateReady");
        m_gameStateReadyHook = Hook::RegisterInitGameStatePostCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                RegisterAll();
            }, options);

        for (auto* hookPath : SaveLoadHookPaths)
        {
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, hookPath);
            if (!function)
            {
                PS::Log<LogLevel::Warning>(STR("Save load hook '{}' was not found.\n"), hookPath);
                continue;
            }

            const auto id = PS::RegisterNativePreHook(function, [](UnrealScriptFunctionCallableContext& context, void* customData) {
                static_cast<DragonWildsDataRegistrar*>(customData)->RegisterAll();
            }, this);
            m_functionHooks.emplace_back(function, id);
        }

        // Both storefronts eventually hydrate the same native character JSON.
        // Repair it at that shared boundary, after every RuneSchema registry has
        // been registered but before Dominion rejects an unresolved identity.
        UFunction* characterJsonLoad = nullptr;
        for (auto* hookPath : CharacterJsonLoadHookPaths)
        {
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr, hookPath, false);
            if (function && (function->GetFunctionFlags() & FUNC_Native))
            {
                characterJsonLoad = function;
                break;
            }
        }
        if (!characterJsonLoad)
        {
            std::vector<UFunction*> matches;
            UObjectGlobals::ForEachUObject(
                [&](UObject* object, int32_t, int32_t) -> LoopAction {
                    if (!object || !object->IsA(UFunction::StaticClass()))
                        return LoopAction::Continue;
                    auto* function = static_cast<UFunction*>(object);
                    if (function->GetFName()
                            != FName(TEXT("LoadStateFromJson"), FNAME_Add)
                        || !(function->GetFunctionFlags() & FUNC_Native)
                        || !function->GetPathName().starts_with(
                            TEXT("/Script/Dominion.")))
                        return LoopAction::Continue;
                    bool hasStringInput = false;
                    for (auto* field : TFieldRange<FProperty>(
                        function, EFieldIterationFlags::Default))
                        if (CastField<FStrProperty>(field)
                            && field->HasAnyPropertyFlags(CPF_Parm)
                            && !field->HasAnyPropertyFlags(
                                CPF_ReturnParm | CPF_OutParm))
                            hasStringInput = true;
                    if (hasStringInput) matches.push_back(function);
                    return LoopAction::Continue;
                });
            if (matches.size() == 1) characterJsonLoad = matches.front();
            else if (!matches.empty())
                PS::Log<LogLevel::Warning>(STR(
                    "Character save preflight found {} ambiguous native LoadStateFromJson functions; no hook was installed.\n"),
                    matches.size());
        }
        if (characterJsonLoad)
        {
            const auto id = PS::RegisterNativePreHook(characterJsonLoad,
                [this, characterJsonLoad](
                    UnrealScriptFunctionCallableContext& context, void*) {
                    RegisterAll();
                    ScrubCharacterJsonBeforeLoad(
                        characterJsonLoad, context.TheStack.Locals());
                });
            if (id != Hook::ERROR_ID)
            {
                m_functionHooks.emplace_back(characterJsonLoad, id);
                PS::Log<LogLevel::Normal>(
                    STR("Character save preflight enabled through '{}'.\n"),
                    characterJsonLoad->GetPathName());
            }
        }
        else
        {
            Hook::FCallbackOptions preflightOptions{};
            preflightOptions.OwnerModName = TEXT("RuneSchema");
            preflightOptions.HookName = TEXT("CharacterJsonSavePreflight");
            m_characterJsonHook = Hook::RegisterProcessEventPreCallback(
                [this](Hook::TCallbackIterationData<void>&, UObject*,
                    UFunction* function, void* parameters) {
                    if (!function || !parameters || m_preflightingCharacterJson
                        || function->GetFName()
                            != FName(TEXT("LoadStateFromJson"), FNAME_Add)
                        || !function->GetPathName().starts_with(
                            TEXT("/Script/Dominion.")))
                        return;
                    m_preflightingCharacterJson = true;
                    try
                    {
                        RegisterAll();
                        ScrubCharacterJsonBeforeLoad(function, parameters);
                    }
                    catch (const std::exception& error)
                    {
                        PS::Log<LogLevel::Error>(STR(
                            "[SAVE-CLEANER][PREFLIGHT][UNCHANGED] Character JSON was not modified: {}.\n"),
                            PS::ToWideSafe(error.what()));
                    }
                    catch (...) {}
                    m_preflightingCharacterJson = false;
                }, preflightOptions);
            if (m_characterJsonHook != Hook::ERROR_ID)
                PS::Log<LogLevel::Normal>(STR(
                    "Character save preflight enabled through reflected game events.\n"));
            else
                PS::Log<LogLevel::Warning>(STR(
                    "Character save preflight is unavailable; the reflected event hook could not be installed.\n"));
        }
    }

    void DragonWildsDataRegistrar::ScrubCharacterJsonBeforeLoad(
        UFunction* function, void* parameters)
    {
        if (!function || !parameters) return;
        const auto registry = PS::SaveCleanup::ReadRegistry();
        if (!registry || !registry->Ready()) return;

        try
        {
            FStrProperty* jsonProperty = nullptr;
            void* jsonAddress = nullptr;
            nlohmann::json source;
            for (auto* field : TFieldRange<FProperty>(
                function, EFieldIterationFlags::Default))
            {
                if (!field->HasAnyPropertyFlags(CPF_Parm)
                    || field->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm)
                    || field->GetArrayDim() != 1
                    || field->GetOffset_Internal() < 0
                    || field->GetOffset_Internal() + field->GetElementSize()
                        > function->GetParmsSize())
                    continue;
                auto* stringField = CastField<FStrProperty>(field);
                if (!stringField) continue;
                auto* address = stringField->ContainerPtrToValuePtr<void>(parameters);
                const auto value = stringField->GetPropertyValue(address);
                if (value.GetCharArray().Num() <= 1) continue;
                const auto utf8 = RC::to_string(RC::StringType(*value));
                if (utf8.find("\"GameProgress\"") == std::string::npos) continue;
                auto parsed = nlohmann::json::parse(utf8, nullptr, true, true);
                if (PS::SaveCleanup::ClassifyCharacterDocument(parsed)
                    != PS::SaveCleanup::CharacterDocumentKind::Gameplay)
                    continue;
                if (jsonProperty)
                    throw std::runtime_error(
                        "character load exposed more than one gameplay JSON parameter");
                jsonProperty = stringField;
                jsonAddress = address;
                source = std::move(parsed);
            }
            if (!jsonProperty) return;

            const auto characterId = source.contains("meta_data")
                && source.at("meta_data").is_object()
                ? source.at("meta_data").value("char_guid", std::string{})
                : std::string{};
            if (characterId.empty()
                || m_checkedCharacters.contains(characterId))
                return;

            const auto cleaned = PS::SaveCleanup::Plan(
                source, {}, false, registry.get(), false, true, true);
            if (cleaned.Removed.empty()) {
                m_checkedCharacters.insert(characterId);
                return;
            }

            const auto serialized = cleaned.Save.dump();
            const FString replacement(RC::to_generic_string(serialized).c_str());
            jsonProperty->SetPropertyValue(jsonAddress, replacement);
            const auto verified = RC::to_string(RC::StringType(
                *jsonProperty->GetPropertyValue(jsonAddress)));
            if (verified != serialized)
                throw std::runtime_error(
                    "clean character JSON did not survive reflected writeback");
            m_checkedCharacters.insert(characterId);

            std::map<std::string, std::size_t> counts;
            for (const auto& row : cleaned.Removed)
                ++counts[row.value("Kind", std::string("Unknown"))];
            std::string summary;
            for (const auto& [kind, count] : counts)
            {
                if (!summary.empty()) summary += ", ";
                summary += kind + "=" + std::to_string(count);
            }
            PS::Log<LogLevel::Normal>(
                STR("[SAVE-CLEANER][PREFLIGHT] Removed {} unresolved character reference(s) before load ({}).\n"),
                cleaned.Removed.size(), PS::ToWideSafe(summary.c_str()));
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("[SAVE-CLEANER][PREFLIGHT][UNCHANGED] Character JSON was not modified: {}.\n"),
                PS::ToWideSafe(error.what()));
        }
    }


    void DragonWildsDataRegistrar::RegisterAll()
    {
        PS::SaveCleanup::RegistrySnapshot snapshot;
        bool itemsReady = false;
        bool recipesReady = false;
        bool questsReady = false;

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
                RegisterMissing(dataClass, subsystem);

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
                PS::Log<LogLevel::Warning>(STR(
                    "No {} instance exists yet; {} assets cannot be registered.\n"),
                    subsystemClass->GetName(), dataClass->GetName());
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
        if (itemsReady && recipesReady)
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
                    "Persistence registry ready: items={}, recipes={}, quests={}, journal={}.\n"),
                    snapshot.Items.size(), snapshot.Recipes.size(),
                    snapshot.Quests.size(), snapshot.Journals.size());
            }
        }
        else
        {
            // Never leave a previous world's registry available to Safe Clean
            // when the current world could not prove a complete item/recipe map.
            m_registryCandidateFingerprint.clear();
            m_registryCandidatePasses = 0;
            PS::SaveCleanup::PublishRegistry({});
        }
    }

    void DragonWildsDataRegistrar::RegisterMissing(UClass* dataClass, UObject* subsystem)
    {
        auto* idMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
        if (!idMapProperty)
        {
            PS::Log<LogLevel::Warning>(STR("PersistenceIDToDataMap was not found on {}.\n"), subsystem->GetClassPrivate()->GetName());
            return;
        }

        std::unordered_set<RC::StringType> known;
        UECustom::FScriptMapHelper idMap(idMapProperty, idMapProperty->ContainerPtrToValuePtr<void>(subsystem));
        idMap.ForEachPair([&](void* keyPtr, void*) {
            auto* key = static_cast<FString*>(keyPtr);
            if (key->GetCharArray().Num() > 1)
            {
                known.insert(RC::StringType(**key));
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

                if (!known.contains(idString))
                {
                    InsertIntoMap(subsystem, TEXT("PersistenceIDToDataMap"), persistenceId, candidate);
                    InsertIntoMap(subsystem, TEXT("InternalNameToDataMap"), persistenceId, candidate);

                    if (auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(candidateClass, TEXT("InternalName"))))
                    {
                        auto internalName = nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(candidate));
                        if (internalName.GetCharArray().Num() > 1 && RC::StringType(*internalName) != idString)
                        {
                            InsertIntoMap(subsystem, TEXT("InternalNameToDataMap"), internalName, candidate);
                        }
                    }

                    known.insert(idString);
                }

                if (EnsureNetworkIdentity(candidate, subsystem) < 0)
                {
                    throw std::runtime_error("network registry rejected the asset");
                }
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed registering '{}': {}\n"),
                    candidate ? candidate->GetName() : STR("<null>"), PS::ToWideSafe(e.what()));
            }
        }
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
