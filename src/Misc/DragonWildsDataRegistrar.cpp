#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_set>
#include <Windows.h>
#include "nlohmann/json.hpp"
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
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/Logging.h"
#include "Misc/DragonWildsDataRegistrar.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    static constexpr struct {
        const TCHAR* DataClassPath;
        const TCHAR* SubsystemClassPath;
    } RegistryBindings[] = {
        { TEXT("/Script/Dominion.ItemData"),   TEXT("/Script/Dominion.ItemSubsystem") },
        { TEXT("/Script/Dominion.RecipeData"), TEXT("/Script/Dominion.RecipeSubsystem") },
    };

    static constexpr const TCHAR* SaveLoadHookPaths[] = {
        TEXT("/Script/Dominion.DominionPlayerController:OnInventoryLoadedFromSave"),
        TEXT("/Script/Dominion.DominionPlayerController:OnPersonalInventoryLoadedFromSave"),
    };

    static bool IsCustomDataPath(const RC::StringType& path)
    {
        return path.starts_with(TEXT("/Game/Mods/")) || path.starts_with(TEXT("/Engine/Transient"));
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

        if (!m_savesCleaned)
        {
            m_savesCleaned = true;
            CleanSaves();
        }
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

    void DragonWildsDataRegistrar::InstallHooks()
    {
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DataRegistrarInitGameState");

        Hook::RegisterInitGameStatePostCallback(
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

            function->RegisterPreHook([](UnrealScriptFunctionCallableContext& context, void* customData) {
                static_cast<DragonWildsDataRegistrar*>(customData)->RegisterAll();
            }, this);
        }
    }

    void DragonWildsDataRegistrar::RegisterAll()
    {
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

    void DragonWildsDataRegistrar::CleanSaves()
    {
        std::unordered_set<std::string> knownItems;
        std::unordered_set<std::string> knownRecipes;

        for (auto& [dataClass, subsystemClass] : m_bindings)
        {
            auto className = RC::to_string(dataClass->GetName());
            if (className == "ItemData" && !ReadKnownIds(subsystemClass, knownItems))
            {
                return;
            }
            if (className == "RecipeData" && !ReadKnownIds(subsystemClass, knownRecipes))
            {
                return;
            }
        }

        if (knownItems.size() < 500 || knownRecipes.size() < 300)
        {
            PS::Log<LogLevel::Warning>(STR("Registries look incomplete ({} items, {} recipes); leaving character saves untouched.\n"),
                knownItems.size(), knownRecipes.size());
            return;
        }

        auto* localAppData = std::getenv("LOCALAPPDATA");
        if (!localAppData)
        {
            return;
        }

        auto saveDir = std::filesystem::path(localAppData) / "RSDragonwilds" / "Saved" / "SaveCharacters";
        if (!std::filesystem::is_directory(saveDir))
        {
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
                CleanCharacterSave(entry.path(), knownItems, knownRecipes);
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed cleaning save '{}': {}\n"),
                    entry.path().filename().wstring(), PS::ToWideSafe(e.what()));
            }
        }
    }

    bool DragonWildsDataRegistrar::ReadKnownIds(UClass* subsystemClass, std::unordered_set<std::string>& outIds)
    {
        auto* subsystem = FindSubsystemInstance(subsystemClass);
        if (!subsystem)
        {
            return false;
        }

        auto* idMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
        if (!idMapProperty)
        {
            return false;
        }

        UECustom::FScriptMapHelper idMap(idMapProperty, idMapProperty->ContainerPtrToValuePtr<void>(subsystem));
        idMap.ForEachPair([&](void* keyPtr, void*) {
            auto* key = static_cast<FString*>(keyPtr);
            if (key->GetCharArray().Num() > 1)
            {
                outIds.insert(RC::to_string(RC::StringType(**key)));
            }
        });

        return true;
    }

    bool DragonWildsDataRegistrar::CleanCharacterSave(const std::filesystem::path& savePath,
        const std::unordered_set<std::string>& knownItems, const std::unordered_set<std::string>& knownRecipes)
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

        if (!save.contains("GameProgress"))
        {
            return false;
        }
        auto& progress = save["GameProgress"];

        int removed = 0;
        std::unordered_set<std::string> removedInventoryIndexes;

        for (auto* sectionName : { "Inventory", "PersonalInventory" })
        {
            if (!progress.contains(sectionName) || !progress[sectionName].is_object())
            {
                continue;
            }

            auto& section = progress[sectionName];
            for (auto it = section.begin(); it != section.end();)
            {
                if (it.value().is_object() && it.value().contains("ItemData") && it.value()["ItemData"].is_string()
                    && !knownItems.contains(it.value()["ItemData"].get<std::string>()))
                {
                    PS::Log<LogLevel::Warning>(STR("Removing unknown item '{}' from {} slot {} in '{}'.\n"),
                        RC::to_generic_string(it.value()["ItemData"].get<std::string>()),
                        RC::to_generic_string(sectionName), RC::to_generic_string(it.key()),
                        savePath.filename().wstring());
                    if (std::string(sectionName) == "Inventory")
                    {
                        removedInventoryIndexes.insert(it.key());
                    }
                    it = section.erase(it);
                    removed++;
                }
                else
                {
                    ++it;
                }
            }
        }

        if (progress.contains("Loadout") && progress["Loadout"].is_object())
        {
            auto& loadout = progress["Loadout"];
            for (auto it = loadout.begin(); it != loadout.end();)
            {
                bool erase = false;
                if (it.value().is_object())
                {
                    if (it.value().contains("ItemData") && it.value()["ItemData"].is_string()
                        && !knownItems.contains(it.value()["ItemData"].get<std::string>()))
                    {
                        erase = true;
                    }
                    else if (it.value().contains("PlayerInventoryItemIndex")
                        && it.value()["PlayerInventoryItemIndex"].is_number_integer()
                        && removedInventoryIndexes.contains(std::to_string(it.value()["PlayerInventoryItemIndex"].get<int>())))
                    {
                        erase = true;
                    }
                }

                if (erase)
                {
                    PS::Log<LogLevel::Warning>(STR("Removing unknown equipped item from Loadout slot {} in '{}'.\n"),
                        RC::to_generic_string(it.key()), savePath.filename().wstring());
                    it = loadout.erase(it);
                    removed++;
                }
                else
                {
                    ++it;
                }
            }
        }

        if (progress.contains("Progress") && progress["Progress"].is_object())
        {
            auto& lists = progress["Progress"];
            auto filterList = [&](const char* listName, const std::unordered_set<std::string>& knownIds) {
                if (!lists.contains(listName) || !lists[listName].is_array())
                {
                    return;
                }
                auto& list = lists[listName];
                for (auto it = list.begin(); it != list.end();)
                {
                    if (it->is_string() && !knownIds.contains(it->get<std::string>()))
                    {
                        PS::Log<LogLevel::Warning>(STR("Removing unknown id '{}' from {} in '{}'.\n"),
                            RC::to_generic_string(it->get<std::string>()), RC::to_generic_string(listName),
                            savePath.filename().wstring());
                        it = list.erase(it);
                        removed++;
                    }
                    else
                    {
                        ++it;
                    }
                }
            };

            filterList("ItemsPickedUp", knownItems);
            filterList("MilestoneMaterialsPickedUp", knownItems);
            filterList("RecipesUnlocked", knownRecipes);
            filterList("RecipesNew", knownRecipes);
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

        auto backupPath = savePath;
        backupPath += ".runeschema_backup";
        std::filesystem::copy_file(savePath, backupPath, std::filesystem::copy_options::overwrite_existing);

        std::ofstream out(savePath, std::ios::binary | std::ios::trunc);
        out << serialized;
        out.close();

        PS::Log<LogLevel::Normal>(STR("Removed {} orphaned mod entr{} from '{}' (backup written).\n"),
            removed, removed == 1 ? STR("y") : STR("ies"), savePath.filename().wstring());
        return true;
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
