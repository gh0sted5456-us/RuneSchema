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
#include "Core/SaveRegistrySnapshot.h"
#include "Loader/OwnedContentLedger.h"
#include "Loader/NativeQuestCleanup.h"
#include "Loader/QuestNativeRegistry.h"
#include "Runtime/HostServices.h"
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
        PS::SaveCleanup::PublishRegistry({});
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
                auto* target=context.Context;
                auto* journalClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.JournalComponent"));
                if(target && journalClass && target->IsA(journalClass)) {
                    static_cast<DragonWildsDataRegistrar*>(customData)->ScrubRetiredJournal(target);
                    return;
                }
                auto* questProgressClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestProgressComponent"));
                auto* controller=target && questProgressClass && target->IsA(questProgressClass)
                    ? target->GetOuterPrivate():target;
                static_cast<DragonWildsDataRegistrar*>(customData)->ScrubRetiredContent(controller);
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
            if (!retired.empty())
            {
                m_pendingSnapshot = path;
                m_itemsVerified=std::ranges::none_of(retired,[](const auto& record){return record.Kind=="Item";});
                m_recipesVerified=std::ranges::none_of(retired,[](const auto& record){return record.Kind=="Recipe";});
                m_questsVerified=std::ranges::none_of(retired,[](const auto& record){return record.Kind=="Quest";});
                m_journalVerified=std::ranges::none_of(retired,[](const auto& record){return record.Kind=="Journal" || record.Kind=="Lore";});
                for (const auto& record : retired)
                    // Building declarations live in world state, not the
                    // character provider. Quest state has a native live
                    // adapter below. Journal/lore use the same hydrated-state
                    // cleanup path through JournalComponent.
                    if (record.Kind != "Item" && record.Kind != "Recipe"
                        && record.Kind != "Quest" && record.Kind != "Journal"
                        && record.Kind != "Lore" && record.Kind != "Building")
                        m_pendingUnsupportedKinds.insert(record.Kind);
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][PENDING] {} exact RuneSchema identity removal(s) will be applied once to live character state after load.\n"),retired.size());
            }
            else
            {
                OwnedContent::CommitSnapshot(path);
            }
            for (const auto& record : retired)
            {
                if(record.Kind!="Item" && record.Kind!="Recipe" && record.Kind!="Quest"
                    && record.Kind!="Journal" && record.Kind!="Lore")continue;
                const auto* classPath=record.Kind=="Item"?ItemDataClassPath:
                    record.Kind=="Recipe"?RecipeDataClassPath:
                    record.Kind=="Quest"?QuestDataClassPath:JournalDataClassPath;
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
                if(record.Kind=="Journal" || record.Kind=="Lore") {
                    auto* subsystemClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                        nullptr,nullptr,JournalSubsystemClassPath,false);
                    auto* subsystem=subsystemClass?FindSubsystemInstance(subsystemClass):nullptr;
                    if(!subsystem || !subsystem->GetOuterPrivate())
                        throw std::runtime_error("retired journal subsystem is unavailable");
                    (void)QuestRegistry::NativeRegistry::RegisterJournal(
                        subsystem,subsystem->GetOuterPrivate(),item);
                }
            }
            if (!m_retiredContent.empty())
                PS::Log<LogLevel::Verbose>(
                    STR("[SAVE-CLEANER] Prepared {} exact retired identity reference(s).\n"),
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
        if (!controller || (m_retiredContent.empty() && m_pendingSnapshot.empty())) return;
        try
        {
            if (!m_pendingSnapshot.empty()
                && !m_pendingUnsupportedKinds.empty())
            {
                if (!m_cleanupBlockReported)
                {
                    m_cleanupBlockReported = true;
                    std::string kinds;
                    for (const auto& kind : m_pendingUnsupportedKinds)
                    {
                        if (!kinds.empty()) kinds += ", ";
                        kinds += kind;
                    }
                    PS::Log<LogLevel::Error>(
                        STR("[SAVE-CLEANER][PARTIAL] No verified live adapter exists for retired kind(s): {}. Supported cleanup will continue and the previous ownership vector will be retained.\n"),
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
            if(hasItems)m_itemsVerified=true;
            if(hasRecipes)m_recipesVerified=true;
            if (removedKinds)
                PS::Log<LogLevel::Normal>(
                    STR("[SAVE-CLEANER][OWNED-ONLY] Removed {} item stack identity(s), {} total item(s); the next native save persists the clean inventory.\n"),
                    removedKinds, removedCount);
            if(removedRecipes)
                PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][OWNED-ONLY] Removed {} retired recipe unlock identity(s); the next native save persists the clean progress state.\n"),removedRecipes);
            std::size_t removedQuests=0;
            if(std::any_of(m_retiredContent.begin(),m_retiredContent.end(),
                [](const auto& value){return value.Kind=="Quest";}))
            {
                std::map<std::string,std::string> retiredQuests;
                for(const auto& retired:m_retiredContent)
                    if(retired.Kind=="Quest")retiredQuests.emplace(retired.PersistenceID,retired.Owner);
                const auto result=Quests::NativeQuestCleanup::Run(controller,retiredQuests);
                m_questsVerified=result.Ready;
                removedQuests=result.Removed;
                if(removedQuests)
                    PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][OWNED-ONLY] Removed {} retired quest identity(s); the next native save persists the clean quest state.\n"),removedQuests);
            }
            TryCommitSnapshot();
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("[SAVE-CLEANER][DEGRADED] Retired RuneSchema content was retained because cleanup could not be verified: {}.\n"),
                PS::ToWideSafe(error.what()));
        }
    }

    void DragonWildsDataRegistrar::ScrubRetiredJournal(UObject* component)
    {
        if(!component || m_journalVerified || m_pendingSnapshot.empty())return;
        try {
            auto* componentClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.JournalComponent"));
            auto* entryClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.JournalEntryData"));
            if(!componentClass || !entryClass || !component->IsA(componentClass) || !component->GetWorld())
                throw std::runtime_error("journal live state is not ready");
            std::set<UObject*> retired;
            for(const auto& value:m_retiredContent)
                if(value.Kind=="Journal" || value.Kind=="Lore")retired.insert(value.Data);
            if(retired.empty()){m_journalVerified=true;TryCommitSnapshot();return;}
            struct Field {FArrayProperty* Property{};FScriptArray* Array{};std::vector<UObject*> Original;};
            std::array<Field,2> fields{};
            size_t removed=0;
            for(size_t i=0;i<fields.size();++i) {
                const auto* name=i?TEXT("UnreadJournalEntries"):TEXT("UnlockedJournalEntries");
                auto* property=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),name));
                auto* inner=property?CastField<FObjectPropertyBase>(property->GetInner()):nullptr;
                if(!property || property->GetElementSize()!=sizeof(FScriptArray) || property->GetArrayDim()!=1
                    || !inner || inner->GetElementSize()!=sizeof(UObject*) || !inner->GetPropertyClass().Get()
                    || !entryClass->IsChildOf(inner->GetPropertyClass().Get()))
                    throw std::runtime_error("journal live array layout changed");
                fields[i].Property=property;
                fields[i].Array=property->ContainerPtrToValuePtr<FScriptArray>(component);
                if(!fields[i].Array || fields[i].Array->Num()<0 || fields[i].Array->Num()>65535)
                    throw std::runtime_error("journal live array bounds are invalid");
                UECustom::FScriptArrayHelper view(fields[i].Array,property);
                view.ForEachElement([&](void* element){fields[i].Original.push_back(inner->GetObjectPropertyValue(element));});
            }
            const auto restore=[&] {
                for(auto& field:fields) {
                    UECustom::FScriptArrayHelper view(field.Array,field.Property);view.Empty();
                    for(auto* object:field.Original)view.Add(&object);
                }
            };
            try {
                for(auto& field:fields) {
                    UECustom::FScriptArrayHelper view(field.Array,field.Property);view.Empty();
                    for(auto* object:field.Original) {
                        if(retired.contains(object)){++removed;continue;}
                        view.Add(&object);
                    }
                    bool found=false;view.ForEachElement([&](void* element){
                        auto* inner=CastField<FObjectPropertyBase>(field.Property->GetInner());
                        if(retired.contains(inner->GetObjectPropertyValue(element)))found=true;
                    });
                    if(found)throw std::runtime_error("retired journal removal verification failed");
                }
            } catch(...) {restore();throw;}
            m_journalVerified=true;
            if(removed)PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][OWNED-ONLY] Removed {} retired journal/lore reference(s); the next native save persists the clean journal state.\n"),removed);
            TryCommitSnapshot();
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR("[SAVE-CLEANER][DEGRADED] Retired journal/lore content was retained because cleanup could not be verified: {}.\n"),PS::ToWideSafe(error.what()));
        }
    }

    void DragonWildsDataRegistrar::TryCommitSnapshot()
    {
        if(m_pendingSnapshot.empty())return;
        if(!m_pendingUnsupportedKinds.empty() || !m_itemsVerified || !m_recipesVerified
            || !m_questsVerified || !m_journalVerified) {
            if(!m_cleanupPartialReported) {
                m_cleanupPartialReported=true;
                PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][VERIFIED-PARTIAL] Ready live state was verified. The previous ownership vector remains pending until every retired category is available.\n"));
            }
            return;
        }
        OwnedContent::CommitSnapshot(m_pendingSnapshot);
        m_pendingSnapshot.clear();
        PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][VERIFIED] Live cleanup was verified; the ownership vector is now current.\n"));
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
        {
            // The complete native view is diagnostic/manual-repair context.
            // Automatic pruning never treats registry-unknown content as
            // removable; it uses only the exact previous/current RuneSchema
            // identity snapshot after every mod section loaded successfully.
            PS::SaveCleanup::PublishRegistry(snapshot);
        }
        else
            // Never leave a previous world's registry available to Safe Clean
            // when the current world could not prove a complete item/recipe map.
            PS::SaveCleanup::PublishRegistry({});
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
