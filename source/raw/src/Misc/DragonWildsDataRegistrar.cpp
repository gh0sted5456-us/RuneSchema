#include "Utility/NativeFunctionHook.h"
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <map>
#include <unordered_set>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
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
#include "Core/ConfigFiles.h"
#include "Core/SaveCleanup.h"
#include "Core/SaveRegistrySnapshot.h"
#include "Loader/OwnedContentLedger.h"
#include "Loader/ModLoadOrder.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Misc/DragonWildsDataRegistrar.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    static constexpr const TCHAR* ItemDataClassPath = TEXT("/Script/Dominion.ItemData");
    static constexpr const TCHAR* RecipeDataClassPath = TEXT("/Script/Dominion.RecipeData");
    static constexpr const TCHAR* QuestDataClassPath = TEXT("/Script/Dominion.QuestData");

    static std::filesystem::path CharacterSaveDirectory()
    {
        const auto required=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
        if(!required)throw std::runtime_error("LOCALAPPDATA is unavailable");
        std::vector<wchar_t> value(required);
        if(GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),required)+1!=required)
            throw std::runtime_error("LOCALAPPDATA changed while it was read");
        return std::filesystem::path(value.data())/L"RSDragonwilds"/L"Saved"/L"SaveCharacters";
    }

    static std::filesystem::path PreserveCharacterSave(
        const std::filesystem::path& path)
    {
        // SafeSave backups are recovery state, not game saves. Keep them out of
        // SaveCharacters and rotate a small fixed history instead of creating
        // an unbounded series of numbered .bak files beside every character.
        constexpr unsigned BackupRetention = 3;
        const auto folder = PS::HostServices::StateDirectory()
            / "safesave" / "backups";
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error)
            throw std::system_error(error,
                "Cannot create the RuneSchema SafeSave backup directory");

        auto base = folder / path.filename();
        base += L".before-clean.bak";
        const auto slot = [&](unsigned index) {
            auto value = base;
            if (index) value += L"." + std::to_wstring(index);
            return value;
        };

        auto pending = base;
        pending += L".next";
        std::filesystem::remove(pending, error);
        if (error)
            throw std::system_error(error,
                "Cannot clear the pending RuneSchema SafeSave backup");
        if (!std::filesystem::copy_file(path, pending,
                std::filesystem::copy_options::overwrite_existing, error))
        {
            if (!error) error = std::make_error_code(std::errc::io_error);
            throw std::system_error(error,
                "Cannot preserve character save before cleanup");
        }

        try
        {
            for (unsigned index = BackupRetention - 1; index > 0; --index)
            {
                const auto from = slot(index - 1);
                const auto to = slot(index);
                const bool exists = std::filesystem::exists(from, error);
                if (error)
                    throw std::system_error(error,
                        "Cannot inspect RuneSchema SafeSave backup rotation");
                if (!exists) continue;

                std::filesystem::remove(to, error);
                if (error)
                    throw std::system_error(error,
                        "Cannot prune an old RuneSchema SafeSave backup");
                std::filesystem::rename(from, to, error);
                if (error)
                    throw std::system_error(error,
                        "Cannot rotate RuneSchema SafeSave backups");
            }

            std::filesystem::rename(pending, slot(0), error);
            if (error)
                throw std::system_error(error,
                    "Cannot activate the new RuneSchema SafeSave backup");
        }
        catch (...)
        {
            std::error_code ignored;
            std::filesystem::remove(pending, ignored);
            throw;
        }
        return slot(0);
    }

    static void PruneLegacyCharacterBackups(
        const std::filesystem::path& source)
    {
        const auto prefix=source.filename().wstring()
            +L".runeschema-before-clean";
        std::size_t removed=0;
        std::error_code iterationError;
        std::filesystem::directory_iterator it(source.parent_path(),iterationError);
        if(iterationError)
        {
            PS::Log<LogLevel::Warning>(
                STR("[SAVE-CLEANER] Could not inspect legacy backup files beside '{}': {}.\n"),
                source.filename().native(),PS::ToWideSafe(iterationError.message().c_str()));
            return;
        }
        for(const auto& entry:it)
        {
            std::error_code typeError;
            if(!entry.is_regular_file(typeError) || typeError)continue;
            const auto name=entry.path().filename().wstring();
            if(name.size()<prefix.size()+4
                || name.compare(0,prefix.size(),prefix)!=0
                || name.compare(name.size()-4,4,L".bak")!=0)
                continue;
            std::error_code removeError;
            if(std::filesystem::remove(entry.path(),removeError))++removed;
            else if(removeError)
                PS::Log<LogLevel::Warning>(
                    STR("[SAVE-CLEANER] Could not remove legacy backup '{}': {}.\n"),
                    entry.path().filename().native(),PS::ToWideSafe(removeError.message().c_str()));
        }
        if(removed)
            PS::Log<LogLevel::Normal>(
                STR("[SAVE-CLEANER] Removed {} legacy RuneSchema backup file(s) from SaveCharacters; recovery copies now live under Saved/RuneSchema/safesave/backups.\n"),
                removed);
    }

    static bool CleanRetiredCharacterSaves(
        const std::vector<OwnedContent::Record>& retired)
    {
        std::unordered_map<std::string,std::string> items,recipes;
        std::set<std::string> owners;
        for(const auto& record:retired)
        {
            owners.insert(record.Owner);
            if(record.Kind=="Item")items.emplace(record.PersistenceID,record.Owner);
            else if(record.Kind=="Recipe")recipes.emplace(record.PersistenceID,record.Owner);
        }
        if(owners.empty())return true;
        if (PS::Storefront::CurrentNativeLane() == PS::Storefront::NativeLane::GamePassNative)
        {
            // WinGDK persists this title through Xbox Game Save (WGS). Its
            // provider database is not a directory of independently writable
            // character JSON files. Keep the retired identities alive for the
            // reflected post-load scrub below; the game then writes the clean
            // state back through its active provider lock.
            PS::Log<LogLevel::Verbose>(
                STR("[SAVE-CLEANER][PROVIDER] Xbox WGS save detected at '{}'; using in-game owned-content cleanup instead of direct Steam JSON editing.\n"),
                PS::HostServices::XboxSaveRoot().native());
            return true;
        }
        const auto folder=CharacterSaveDirectory();
        std::error_code statusError;
        if(!std::filesystem::exists(folder,statusError))return true;
        if(statusError || !std::filesystem::is_directory(folder,statusError) || statusError)
            throw std::runtime_error("Character-save directory is unreadable");
        std::size_t entries=0,files=0,bytes=0,changed=0,removed=0;
        bool complete=true;
        for(const auto& entry:std::filesystem::directory_iterator(folder))
        {
            if(++entries>512)throw std::runtime_error("Character-save directory exceeds 512 entries");
            if(!entry.is_regular_file() || entry.path().extension()!=L".json")continue;
            if(++files>64)throw std::runtime_error("Character-save directory exceeds 64 JSON files");
            const auto size=entry.file_size();
            if(size>8*1024*1024 || (bytes+=size)>64*1024*1024)
                throw std::runtime_error("Character-save scan exceeds its bounded-read limit");
            std::filesystem::path backup;
            try
            {
                const auto source=nlohmann::json::parse(
                    PS::ConfigFiles::Read(entry.path(),8*1024*1024));
                const auto plan=PS::SaveCleanup::PlanOwned(
                    source,items,recipes,owners);
                if(plan.Removed.empty())continue;
                const auto serialized=plan.Save.dump();
                if(nlohmann::json::parse(serialized)!=plan.Save)
                    throw std::runtime_error("Cleaned character save failed JSON verification");
                backup=PreserveCharacterSave(entry.path());
                PS::ConfigFiles::Write(entry.path(),serialized);
                if(nlohmann::json::parse(PS::ConfigFiles::Read(
                    entry.path(),8*1024*1024))!=plan.Save)
                    throw std::runtime_error("Written character save failed verification");
                PruneLegacyCharacterBackups(entry.path());
                ++changed;removed+=plan.Removed.size();
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][OWNED-ONLY] Cleaned {} retired RuneSchema record(s) from '{}' before character deserialization. Backup: '{}'.\n"),
                    plan.Removed.size(),entry.path().filename().native(),backup.native());
            }
            catch(const std::exception& error)
            {
                complete=false;
                if(backup.empty())
                    PS::Log<LogLevel::Error>(
                        STR("[SAVE-CLEANER][DEGRADED] Character save '{}' was left unchanged: {}.\n"),
                        entry.path().filename().native(),PS::ToWideSafe(error.what()));
                else
                    PS::Log<LogLevel::Error>(
                        STR("[SAVE-CLEANER][DEGRADED] Cleanup of character save '{}' did not complete; its original is preserved at '{}': {}.\n"),
                        entry.path().filename().native(),backup.native(),PS::ToWideSafe(error.what()));
            }
        }
        if(changed)
            PS::Log<LogLevel::Normal>(
                STR("[SAVE-CLEANER][OWNED-ONLY] Pre-load cleanup completed for {} character save(s), removing {} ledger-confirmed record(s) from {} absent or disabled owner(s).\n"),
                changed,removed,owners.size());
        return complete;
    }

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
    };

    static bool IsCustomDataPath(const RC::StringType& path)
    {
        return path.starts_with(TEXT("/Game/Mods/"))
            || path.starts_with(TEXT("/Game/RuneSchema/"))
            || path.starts_with(TEXT("/Engine/Transient"))
            || OwnedContent::IsActiveDeclarationPath(RC::to_string(path));
    }

    void DragonWildsDataRegistrar::Initialize()
    {
        if (!m_initialized)
        {
            if (!ResolveBindings())
            {
                return;
            }

            PrepareRetiredContent();
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
        if (m_gameStateHook != Hook::ERROR_ID) Hook::UnregisterCallback(m_gameStateHook);
        m_gameStateHook = Hook::ERROR_ID;
        for (const auto& [function, id] : m_functionHooks) if (function && id) function->UnregisterHook(id);
        m_functionHooks.clear();
        for (auto& retired : m_retiredContent)
            if (retired.Data) retired.Data->ClearRootSet();
        m_retiredContent.clear();
    }

    void DragonWildsDataRegistrar::InstallHooks()
    {
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DataRegistrarInitGameState");

        m_gameStateHook = Hook::RegisterInitGameStatePostCallback(
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
            const auto postId = PS::RegisterNativePostHook(function, [](UnrealScriptFunctionCallableContext& context, void* customData) {
                static_cast<DragonWildsDataRegistrar*>(customData)->ScrubRetiredContent(context.Context);
            }, this);
            m_functionHooks.emplace_back(function, postId);
        }
    }

    void DragonWildsDataRegistrar::PrepareRetiredContent()
    {
        if(m_retiredContentPrepared)return;
        m_retiredContentPrepared=true;
        try
        {
            const auto path = OwnedContent::LedgerPath(
                PS::HostServices::StateDirectory());
            // The previous file is one compact snapshot, not an accumulating
            // history. Loaders contributed the identities that succeeded this
            // run; finalization returns only identities that disappeared and
            // atomically overwrites the snapshot with the current set.
            const auto retired=OwnedContent::CompareSnapshot(path);
            if(!CleanRetiredCharacterSaves(retired))
                throw std::runtime_error("one or more character saves could not be cleaned; the previous identity snapshot was retained for retry");
            const bool providerCleanup = PS::Storefront::CurrentNativeLane()
                == PS::Storefront::NativeLane::GamePassNative && !retired.empty();
            if (providerCleanup)
            {
                m_pendingProviderSnapshot = path;
                for (const auto& record : retired)
                    if (record.Kind != "Item" && record.Kind != "Recipe")
                        m_pendingProviderUnsupportedKinds.insert(record.Kind);
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][PROVIDER][PENDING] Retaining the previous Game Pass identity snapshot until provider-backed cleanup is read-back verified.\n"));
            }
            else
            {
                OwnedContent::CommitSnapshot(path);
            }
            for (const auto& record : retired)
            {
                if(record.Kind!="Item" && record.Kind!="Recipe" && record.Kind!="Quest")continue;
                const auto* classPath=record.Kind=="Item"?ItemDataClassPath:
                    record.Kind=="Recipe"?RecipeDataClassPath:QuestDataClassPath;
                auto* dataClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,classPath,false);
                if(!dataClass)throw std::runtime_error("retired content class is unavailable");
                auto* item = ActorHelper::ConstructTransientObject(dataClass,
                    RC::to_generic_string("RuneSchema_Retired"+record.Kind+"_"
                        + record.PersistenceID));
                auto* persistence = item ? CastField<FStrProperty>(
                    PropertyHelper::GetPropertyByName(
                        item->GetClassPrivate(), TEXT("PersistenceID"))) : nullptr;
                auto* internal = item ? CastField<FStrProperty>(
                    PropertyHelper::GetPropertyByName(
                        item->GetClassPrivate(), TEXT("InternalName"))) : nullptr;
                if (!item || !persistence || !internal)
                    throw std::runtime_error(
                        "retired item identity layout is unavailable");
                const FString id(RC::to_generic_string(record.PersistenceID).c_str());
                const FString name(RC::to_generic_string(record.InternalName).c_str());
                persistence->SetPropertyValue(
                    persistence->ContainerPtrToValuePtr<void>(item), id);
                internal->SetPropertyValue(
                    internal->ContainerPtrToValuePtr<void>(item), name);
                item->SetRootSet();
                m_retiredContent.push_back(
                    {item, record.Kind, record.Owner, record.PersistenceID});
            }
            if (!m_retiredContent.empty())
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][OWNED-ONLY] Prepared {} retired persistent-content tombstone(s) for disabled or removed RuneSchema mods.\n"),
                    m_retiredContent.size());
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("[SAVE-CLEANER][DEGRADED] Owned-content tombstones were not prepared; save records were left untouched: {}.\n"),
                PS::ToWideSafe(error.what()));
        }
    }

    void DragonWildsDataRegistrar::ScrubRetiredContent(UObject* controller)
    {
        if (!controller || (m_retiredContent.empty() && m_pendingProviderSnapshot.empty())) return;
        try
        {
            if (!m_pendingProviderSnapshot.empty()
                && !m_pendingProviderUnsupportedKinds.empty())
            {
                if (!m_providerBlockReported)
                {
                    m_providerBlockReported = true;
                    std::string kinds;
                    for (const auto& kind : m_pendingProviderUnsupportedKinds)
                    {
                        if (!kinds.empty()) kinds += ", ";
                        kinds += kind;
                    }
                    PS::Log<LogLevel::Error>(
                        STR("[SAVE-CLEANER][PROVIDER][PARTIAL] Game Pass cleanup has no verified live adapter for retired kind(s): {}. Supported item/recipe cleanup will continue, while the previous ledger is retained for the remaining kinds.\n"),
                        PS::ToWideSafe(kinds.c_str()));
                }
            }
            const bool hasItems=std::any_of(m_retiredContent.begin(),m_retiredContent.end(),[](const auto& value){return value.Kind=="Item";});
            auto* inventoryProperty = hasItems ? CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(controller->GetClassPrivate(), TEXT("InventoryComponent"))) : nullptr;
            auto* inventory = inventoryProperty ? inventoryProperty->GetObjectPropertyValue(
                inventoryProperty->ContainerPtrToValuePtr<void>(controller)) : nullptr;
            if (hasItems && (!inventory || inventory->GetOuterPrivate() != controller))
                throw std::runtime_error("player inventory ownership is unavailable");
            std::size_t removedKinds = 0;
            int64_t removedCount = 0;
            for (const auto& retired : m_retiredContent)
            {
                if(retired.Kind!="Item")continue;
                ActorHelper::FunctionCall count(inventory,
                    TEXT("/Script/Dominion.InventoryComponent:GetNumItemsByData"));
                count.Arg(TEXT("ItemData"), retired.Data).Invoke();
                const auto amount = count.Result<int32>();
                if (amount <= 0) continue;
                ActorHelper::FunctionCall remove(inventory,
                    TEXT("/Script/Dominion.InventoryComponent:RemoveItemByData"));
                remove.Arg(TEXT("ItemData"), retired.Data)
                    .Arg(TEXT("Count"), amount).Invoke();
                ActorHelper::FunctionCall verification(inventory,
                    TEXT("/Script/Dominion.InventoryComponent:GetNumItemsByData"));
                verification.Arg(TEXT("ItemData"), retired.Data).Invoke();
                if (!remove.Result<bool>() || verification.Result<int32>() != 0)
                    throw std::runtime_error(
                        "native inventory did not confirm owned-item removal");
                ++removedKinds;
                removedCount += amount;
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][OWNER:{}] Removed {} instance(s) of retired RuneSchema item '{}'.\n"),
                    RC::to_generic_string(retired.Owner), amount,
                    RC::to_generic_string(retired.PersistenceID));
            }
            std::size_t removedRecipes=0;
            auto* progress=ActorHelper::GetObjectRef(controller,TEXT("ProgressComponent"));
            const bool hasRecipes=std::any_of(m_retiredContent.begin(),m_retiredContent.end(),[](const auto& value){return value.Kind=="Recipe";});
            if(hasRecipes && !progress)throw std::runtime_error("player recipe progress ownership is unavailable");
            if(progress)for(const auto& retired:m_retiredContent)if(retired.Kind=="Recipe") {
                bool removed=false;
                for(const auto* name:{TEXT("RecipesUnlocked"),TEXT("RecipesUnlockedThatShouldNotPersist")}) {
                    auto* property=CastField<FSetProperty>(PropertyHelper::GetPropertyByName(progress->GetClassPrivate(),name));
                    auto* element=property?CastField<FObjectPropertyBase>(property->GetElementProp()):nullptr;
                    if(!property || !element || element->GetElementSize()!=sizeof(UObject*))
                        throw std::runtime_error("recipe unlock set layout is unavailable");
                    UECustom::FScriptSetHelper set(property,property->ContainerPtrToValuePtr<void>(progress));
                    UObject* value=retired.Data;
                    removed=set.Remove(&value)||removed;
                    if(set.Contains(&value))throw std::runtime_error("retired recipe removal verification failed");
                }
                if(removed)++removedRecipes;
            }
            if (removedKinds)
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][OWNED-ONLY] Removed {} item stack identity(s), {} total item(s); the next native save persists the clean inventory.\n"),
                    removedKinds, removedCount);
            if(removedRecipes)
                PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][OWNED-ONLY] Removed {} retired recipe unlock identity(s); the next native save persists the clean progress state.\n"),removedRecipes);
            if (!m_pendingProviderSnapshot.empty())
            {
                if (!m_pendingProviderUnsupportedKinds.empty())
                {
                    if (!m_providerPartialReported)
                    {
                        m_providerPartialReported = true;
                        PS::Log<LogLevel::Normal>(
                            STR("[SAVE-CLEANER][PROVIDER][VERIFIED-PARTIAL] Supported Game Pass item/recipe state was read-back verified. The previous ownership snapshot remains pending for unsupported save categories.\n"));
                    }
                    return;
                }
                // All owned item counts and recipe sets above were read back
                // as absent. Only now may WinGDK replace its previous identity
                // snapshot; a crash or missed provider event will retry on the
                // next launch instead of forgetting the cleanup obligation.
                OwnedContent::CommitSnapshot(m_pendingProviderSnapshot);
                m_pendingProviderSnapshot.clear();
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][PROVIDER][VERIFIED] Game Pass live cleanup was verified; the ownership snapshot is now committed.\n"));
            }
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("[SAVE-CLEANER][DEGRADED] Retired RuneSchema content was retained because cleanup could not be verified: {}.\n"),
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
            auto* subsystem = FindSubsystemInstance(subsystemClass);
            if (!subsystem)
            {
                PS::Log<LogLevel::Warning>(STR("No {} instance exists yet; {} assets cannot be registered.\n"),
                    subsystemClass->GetName(), dataClass->GetName());
                continue;
            }

            RegisterMissing(dataClass, subsystem);

            // Safe Clean compares character-save identities against the same
            // native maps the game actually uses. Capture only after custom
            // registrations have been applied so successfully registered
            // clones are considered valid too.
            auto* idMapProperty = CastField<FMapProperty>(
                PropertyHelper::GetPropertyByName(
                    subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
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

        snapshot.QuestsComplete = questsReady;
        if (itemsReady && recipesReady)
            PS::SaveCleanup::PublishRegistry(std::move(snapshot));
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

                if (!IsCustomDataPath(candidate->GetPathName()))
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
