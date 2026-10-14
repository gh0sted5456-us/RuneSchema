#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/UnrealInitializer.hpp"
#include "Unreal/AGameModeBase.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include <unordered_map>
#include <unordered_set>
#include <Windows.h>
#include "Unreal/Engine/UDataTable.hpp"
#include "Core/PersistenceDiagnosticLedger.h"
#include "Core/MountedModRegistryOwners.h"
#include "Unreal/Hooks.hpp"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StartupTrace.h"
#include "Utility/InlineHook.h"
#include "Utility/ModFolderLayout.h"
#include "SDK/Helper/Memory.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/DragonWildsSignatures.h"
#include "SDK/StaticClassStorage.h"
#include "SDK/UnrealOffsets.h"
#include "Runtime/HostServices.h"
#include "Runtime/WorldBuildingSavePreflight.h"
#include "Runtime/WorldBuildingRegistryValidation.h"
#include "Runtime/PluginCatalog.h"
#include "Runtime/Storefront.h"
#include "Loader/DragonWildsRawTableLoader.h"
#include "Loader/DragonWildsAssetModLoader.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/DragonWildsVendorLoader.h"
#include "Loader/DragonWildsDialogueLoader.h"
#include "Loader/DragonWildsNpcLoader.h"
#include "Loader/DragonWildsQuestLoader.h"
#include "Loader/DragonWildsEventLoader.h"
#include "Loader/DragonWildsEnumLoader.h"
#include "Loader/DragonWildsRecipeModLoader.h"
#include "Loader/DragonWildsJournalModLoader.h"
#include "Loader/DragonWildsBuildingModLoader.h"
#include "Loader/DragonWildsCourseLoader.h"
#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/DragonWildsStringModLoader.h"
#include "Loader/DragonWildsEquipmentLoader.h"
#include "Loader/DragonWildsGameplayEffectLoader.h"
#include "Loader/DragonWildsNiagaraLoader.h"
#include "Loader/DragonWildsRegistryLoader.h"
#include "Loader/DragonWildsMainLoader.h"
#include "Loader/ModLoadOrder.h"
#include "Loader/NativeAssetPrefetch.h"
#include "Misc/FileWatchWrapper.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    // Each world is handled independently. The complete live building registry
    // is required before an unresolved type can be removed from any save.
    std::unordered_set<std::string> ReadValidatedLiveBuildingIds(
        const std::vector<PS::WorldBuildingRegistryValidation::Expected>& expected)
    {
        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.BuildingPieceSubsystem"));
        if (!subsystemClass) throw std::runtime_error("building subsystem class is unavailable");
        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, candidates, true);
        UObject* subsystem = nullptr;
        for (auto* candidate : candidates) {
            if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(
                RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            if (subsystem) throw std::runtime_error("ambiguous live building subsystems");
            subsystem = candidate;
        }
        if (!subsystem) throw std::runtime_error("live building subsystem is unavailable");
        auto* cls = subsystem->GetClassPrivate();
        auto* mapProperty = cls ? CastField<FMapProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(
                cls, TEXT("PersistenceIDToDataMap"))) : nullptr;
        auto* arrayProperty = cls ? CastField<FArrayProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(
                cls, TEXT("NetIdToData"))) : nullptr;
        auto* reverseProperty = cls ? CastField<FMapProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(
                cls, TEXT("DataToNetIdMap"))) : nullptr;
        auto* internalProperty = cls ? CastField<FMapProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(
                cls, TEXT("InternalNameToDataMap"))) : nullptr;
        auto* mapKey = mapProperty ? CastField<FStrProperty>(mapProperty->GetKeyProp()) : nullptr;
        auto* mapValue = mapProperty ? CastField<FObjectPropertyBase>(mapProperty->GetValueProp()) : nullptr;
        auto* arrayInner = arrayProperty ? CastField<FObjectPropertyBase>(arrayProperty->GetInner()) : nullptr;
        auto* reverseKey = reverseProperty ? CastField<FObjectPropertyBase>(reverseProperty->GetKeyProp()) : nullptr;
        auto* reverseValue = reverseProperty ? reverseProperty->GetValueProp() : nullptr;
        auto* internalKey = internalProperty ? CastField<FStrProperty>(internalProperty->GetKeyProp()) : nullptr;
        auto* internalValue = internalProperty ? CastField<FObjectPropertyBase>(internalProperty->GetValueProp()) : nullptr;
        // A single generic failure hid which dedicated-server registry view
        // differed from the client. Keep the fail-closed contract, but report
        // the first missing field or incompatible reflected layout precisely.
        if (!mapProperty || !mapKey || !mapValue)
            throw std::runtime_error("building PersistenceIDToDataMap field or key/value type is unavailable");
        if (!arrayProperty || !arrayInner)
            throw std::runtime_error("building NetIdToData field or element type is unavailable");
        if (!reverseProperty)
            throw std::runtime_error("building DataToNetIdMap map field is unavailable");
        if (!reverseKey)
            throw std::runtime_error("building DataToNetIdMap key is not an object property");
        if (!reverseValue)
            throw std::runtime_error("building DataToNetIdMap value is unavailable");
        // The dedicated server reflects its two-byte NetId as a struct rather
        // than a numeric property. Its raw value is accepted only if every
        // reverse entry round-trips to the same object/index in NetIdToData.
        auto* reverseStruct = CastField<FStructProperty>(reverseValue);
        if (!CastField<FNumericProperty>(reverseValue)
            && (!reverseStruct || !reverseStruct->GetStruct().Get()
                || reverseStruct->GetStruct()->GetPropertiesSize() != sizeof(uint16)))
            throw std::runtime_error("building DataToNetIdMap value has unsupported reflected type '"
                + RC::to_string(reverseValue->GetClass().GetName()) + "'");
        if (!internalProperty || !internalKey || !internalValue)
            throw std::runtime_error("building InternalNameToDataMap field or key/value type is unavailable");
        const auto requireWidth = [](const char* field, std::size_t actual,
            std::size_t expectedWidth) {
            if (actual != expectedWidth)
                throw std::runtime_error(std::string("building ") + field
                    + " reflected width is " + std::to_string(actual)
                    + " bytes; expected " + std::to_string(expectedWidth));
        };
        requireWidth("PersistenceIDToDataMap value", mapValue->GetElementSize(), sizeof(UObject*));
        requireWidth("NetIdToData element", arrayInner->GetElementSize(), sizeof(UObject*));
        requireWidth("DataToNetIdMap key", reverseKey->GetElementSize(), sizeof(UObject*));
        requireWidth("DataToNetIdMap value", reverseValue->GetElementSize(), sizeof(uint16));
        requireWidth("InternalNameToDataMap value", internalValue->GetElementSize(), sizeof(UObject*));
        auto* netIds = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        if (!netIds || netIds->Num() <= 0)
            throw std::runtime_error("live building NetId registry is empty");
        std::unordered_set<std::string> live;
        std::vector<PS::WorldBuildingRegistryValidation::Entry> persistenceEntries;
        bool invalidEntry = false;
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        map.ForEachPair([&](void* key, void* value) {
            UObject* object = nullptr;
            std::memcpy(&object, value, sizeof(object));
            auto* string = static_cast<FString*>(key);
            if (!object || !string || string->GetCharArray().Num() <= 1) {
                invalidEntry = true;
                return;
            }
            std::string id;
            const auto& characters = string->GetCharArray();
            for (int32 i = 0; i + 1 < characters.Num(); ++i) {
                const auto ch = characters[i];
                if (ch < 32 || ch > 126) { invalidEntry = true; return; }
                id.push_back(static_cast<char>(ch));
            }
            auto* idProperty = CastField<FStrProperty>(
                DragonWilds::PropertyHelper::GetPropertyByName(object->GetClassPrivate(), TEXT("PersistenceID")));
            if (!idProperty || RC::to_string(*idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object))) != id) {
                invalidEntry = true;
                return;
            }
            persistenceEntries.push_back({id, reinterpret_cast<std::uintptr_t>(object)});
            if (!live.insert(std::move(id)).second) invalidEntry = true;
        });
        if (invalidEntry || live.size() < static_cast<std::size_t>(netIds->Num()))
            throw std::runtime_error("live building registry did not pass completeness checks");
        std::vector<std::uintptr_t> networkEntries;
        for (int32 index = 0; index < netIds->Num(); ++index) {
            UObject* object = nullptr;
            std::memcpy(&object, static_cast<uint8*>(netIds->GetData())
                + index * arrayInner->GetElementSize(), sizeof(object));
            networkEntries.push_back(reinterpret_cast<std::uintptr_t>(object));
        }
        std::vector<PS::WorldBuildingRegistryValidation::ReverseEntry> reverseEntries;
        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        reverse.ForEachPair([&](void* key, void* value) {
            UObject* object = nullptr;
            uint16 index = 0;
            std::memcpy(&object, key, sizeof(object));
            std::memcpy(&index, value, sizeof(index));
            reverseEntries.push_back({reinterpret_cast<std::uintptr_t>(object), index});
        });
        std::vector<PS::WorldBuildingRegistryValidation::InternalEntry> internalEntries;
        UECustom::FScriptMapHelper internal(
            internalProperty, internalProperty->ContainerPtrToValuePtr<void>(subsystem));
        internal.ForEachPair([&](void* key, void* value) {
            UObject* object = nullptr;
            std::memcpy(&object, value, sizeof(object));
            auto* string = static_cast<FString*>(key);
            internalEntries.push_back({string && string->GetCharArray().Num() > 1
                    ? RC::to_string(string->GetCharArray().GetData()) : std::string{},
                reinterpret_cast<std::uintptr_t>(object)});
        });
        live = PS::WorldBuildingRegistryValidation::Validate(
            persistenceEntries, networkEntries, expected, reverseEntries, internalEntries,
            // Menu preflight precedes building world registration on both
            // standalone clients and dedicated servers. Protect only enabled
            // definitions whose cooked identity and live object were already
            // certified by CleanupExpectations; conflicting registry entries
            // still abort, and disabled mods provide no protected identity.
            PS::WorldBuildingRegistryValidation::ExpectedMode::ProtectPendingDefinition);
        PS::Log<LogLevel::Verbose>(STR(
            "[PERSISTENCE-PRUNER][BUILDINGS][REGISTRY] persistence={} net={} reverse={} names={} enabled={} verified=true.\n"),
            persistenceEntries.size(), networkEntries.size(), reverseEntries.size(),
            internalEntries.size(), expected.size());
        return live;
    }

    void RunLocalWorldBuildingPreflight(const fs::path& target,
        const std::unordered_set<std::string>& live)
    {
        namespace Preflight = PS::WorldBuildingSavePreflight;
        if (!fs::is_regular_file(target) || fs::is_symlink(fs::symlink_status(target))
            || target.extension() != L".sav")
            throw std::runtime_error("world save candidate is not a regular .sav file");
        std::ifstream input(target, std::ios::binary);
        if (!input) throw std::runtime_error("world save could not be read");
        Preflight::Bytes original(std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{});
        input.close();
        auto repair = Preflight::RemoveUnresolved(original, live);
        if (repair.removedIds.empty()) {
            PS::Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][BUILDINGS][CHECKED] '{}' has no unresolved building IDs; save unchanged.\n"),
                target.filename().wstring());
            return;
        }

        // Fail if another writer touched the world after it was read. This
        // startup pass runs before a world is selected or loaded.
        std::ifstream recheck(target, std::ios::binary);
        const Preflight::Bytes current(std::istreambuf_iterator<char>{recheck},
            std::istreambuf_iterator<char>{});
        recheck.close();
        if (current != original) throw std::runtime_error("save changed during preflight");
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto backupEnabled = PS::PSConfig::Get()->GetSettings()
            .worldSaveCleanup.backupBeforePrune;
        const fs::path backup = backupEnabled
            ? fs::path(target.wstring() + L".runeschema-" + std::to_wstring(nonce) + L".bak")
            : fs::path{};
        const fs::path temporary = target.wstring() + L".runeschema-"
            + std::to_wstring(nonce) + L".tmp";
        if ((backupEnabled && fs::exists(backup)) || fs::exists(temporary))
            throw std::runtime_error("preflight output path already exists");
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(repair.bytes.data()),
                static_cast<std::streamsize>(repair.bytes.size()));
            output.flush();
            if (!output) throw std::runtime_error("temporary repaired save write failed");
        }
        {
            std::ifstream verification(temporary, std::ios::binary);
            const Preflight::Bytes written(std::istreambuf_iterator<char>{verification},
                std::istreambuf_iterator<char>{});
            if (written != repair.bytes)
                throw std::runtime_error("temporary repaired save did not verify");
            (void)Preflight::Parse(written);
        }
        if (backupEnabled) {
            if (!fs::copy_file(target, backup, fs::copy_options::none))
                throw std::runtime_error("pre-cleanup backup could not be created; original retained");
            std::ifstream verification(backup, std::ios::binary);
            const Preflight::Bytes saved(std::istreambuf_iterator<char>{verification},
                std::istreambuf_iterator<char>{});
            if (!verification || saved != original)
                throw std::runtime_error("pre-cleanup backup did not verify; original retained");
        }
        const auto repairedBytes = std::make_shared<const Preflight::Bytes>(std::move(repair.bytes));
        const auto logSuccess = [target, backup, removedIds = repair.removedIds,
            persistenceIds = repair.removedPersistenceIds, repairedBytes]() {
            std::ifstream verification(target, std::ios::binary);
            const Preflight::Bytes saved(std::istreambuf_iterator<char>{verification},
                std::istreambuf_iterator<char>{});
            if (!verification || saved != *repairedBytes)
                throw std::runtime_error("committed repaired save did not verify; pre-cleanup backup retained");
            (void)Preflight::Parse(saved);
            const auto backupLabel = backup.empty() ? L"disabled" : backup.filename().wstring();
            RC::Output::send<RC::LogLevel::Normal>(STR(
                "[RuneSchema] [summary] Save cleanup | world='{}' | missing building IDs removed={} | backup='{}'.\n"),
                target.filename().wstring(), removedIds.size(), backupLabel);
            for (std::size_t i = 0; i < removedIds.size(); ++i)
                PS::Log<LogLevel::Verbose>(STR(
                    "[PERSISTENCE-PRUNER][BUILDINGS][REMOVED] world='{}' instance={} PersistenceID='{}' backup='{}'.\n"),
                    target.filename().wstring(),
                    removedIds[i], PS::ToWideSafe(persistenceIds[i]),
                    backupLabel);
        };
        if (ReplaceFileW(target.c_str(), temporary.c_str(),
            nullptr,
            REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            logSuccess();
            return;
        }
        const auto firstError = GetLastError();
        if (firstError != ERROR_SHARING_VIOLATION)
            throw std::runtime_error(std::format(
                "atomic save replacement failed (Windows error {}); original retained",
                firstError));

        // Another save-provider handle can still be open at GameInstanceInit.
        // Retry the already-validated bytes for at most 30 seconds, no more
        // than once per half-second, and never after the target has changed.
        struct Pending {
            fs::path Target;
            fs::path Temporary;
            fs::path Backup;
            Preflight::Bytes Original;
            std::chrono::steady_clock::time_point Next;
            std::chrono::steady_clock::time_point Deadline;
        };
        const auto now = std::chrono::steady_clock::now();
        auto pending = std::make_shared<Pending>(Pending{
            target, temporary, backup, std::move(original),
            now + std::chrono::milliseconds(500), now + std::chrono::seconds(30) });
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("WorldBuildingPreflightMenuRetry");
        const auto callback = Hook::RegisterEngineTickPostCallback(
            [pending, logSuccess](Hook::TCallbackIterationData<void>& iteration,
                UEngine*, float, bool) {
                const auto now = std::chrono::steady_clock::now();
                if (now < pending->Next) return;
                pending->Next = now + std::chrono::milliseconds(500);
                if (now > pending->Deadline) {
                    PS::Log<LogLevel::Warning>(STR(
                        "[PERSISTENCE-PRUNER][BUILDINGS][SKIPPED] Save cleanup could not get a safe write window; the world save was not changed.\n"));
                    iteration.RemoveSelf();
                    return;
                }
                try {
                    std::ifstream verificationStream(pending->Target, std::ios::binary);
                    const Preflight::Bytes current(std::istreambuf_iterator<char>{verificationStream},
                        std::istreambuf_iterator<char>{});
                    verificationStream.close();
                    if (current != pending->Original)
                        throw std::runtime_error("world save changed before menu retry");
                    if (ReplaceFileW(pending->Target.c_str(), pending->Temporary.c_str(),
                        nullptr,
                        REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
                        logSuccess();
                        iteration.RemoveSelf();
                        return;
                    }
                    const auto winError = GetLastError();
                    if (winError != ERROR_SHARING_VIOLATION)
                        throw std::runtime_error(std::format(
                            "atomic menu retry failed (Windows error {})", winError));
                } catch (const std::exception& error) {
                    PS::Log<LogLevel::Warning>(STR(
                        "[PERSISTENCE-PRUNER][BUILDINGS][UNVERIFIED] World cleanup could not be certified: {}. Check the retained backup before loading this world.\n"),
                        PS::ToWideSafe(error.what()));
                    iteration.RemoveSelf();
                }
            }, options);
        if (callback == Hook::ERROR_ID)
            throw std::runtime_error("save is locked and bounded menu retry could not be installed");
        PS::Log<LogLevel::Verbose>(STR(
            "[PERSISTENCE-PRUNER][BUILDINGS][DEFERRED] Save locked at registry initialization; bounded menu retry armed.\n"));
    }

    void RunAllLocalWorldBuildingPreflights(DragonWilds::DragonWildsBuildingModLoader* buildingLoader)
    {
        if (!PS::Storefront::IsDedicatedServer()
            && PS::Storefront::Current() != PS::Storefront::Kind::SteamGog) {
            PS::Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][BUILDINGS][PROVIDER-DEFERRED] Direct world files are not available through this save provider.\n"));
            return;
        }
        fs::path saves;
        if (PS::Storefront::IsDedicatedServer()) {
            const auto& executable = PS::Storefront::CurrentDetection().Executable;
            const auto binaries = executable.parent_path();
            if (PS::Storefront::Lower(binaries.filename().wstring()) != L"win64"
                || PS::Storefront::Lower(binaries.parent_path().filename().wstring())
                    != L"binaries")
                throw std::runtime_error("dedicated server executable layout is unrecognized");
            saves = binaries.parent_path().parent_path()
                / L"Saved" / L"SaveGames";
        } else {
            wchar_t appData[32768]{};
            const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", appData, _countof(appData));
            if (!length || length >= _countof(appData))
                throw std::runtime_error("LOCALAPPDATA is unavailable");
            saves = fs::path(std::wstring(appData, length))
                / L"RSDragonwilds" / L"Saved" / L"SaveGames";
        }
        RC::Output::send<RC::LogLevel::Normal>(STR(
            "[RuneSchema] [PERSISTENCE-PRUNER][BUILDINGS][SOURCE] mode={} directory='{}'.\n"),
            PS::Storefront::IsDedicatedServer() ? TEXT("dedicated-server")
                : TEXT("local-client"), saves.wstring());
        if (!fs::exists(saves)) return;
        if (!fs::is_directory(saves) || fs::is_symlink(fs::symlink_status(saves)))
            throw std::runtime_error("world save directory is not a regular directory");
        if (!buildingLoader)
            throw std::runtime_error("building loader is unavailable; enabled building identities cannot be certified");
        const auto expected = buildingLoader->CleanupExpectations();

        std::vector<fs::path> worlds;
        std::uintmax_t totalBytes = 0;
        constexpr std::uintmax_t maximumTotalBytes = 2ull * 1024 * 1024 * 1024;
        for (const auto& entry : fs::directory_iterator(saves)) {
            if (entry.path().extension() != L".sav") continue;
            if (entry.path().filename() == L"EnhancedInputUserSettings.sav") continue;
            if (!entry.is_regular_file() || entry.is_symlink())
                throw std::runtime_error("world save candidate is not a regular file");
            const auto size = entry.file_size();
            if (size > 512ull * 1024 * 1024 || size > maximumTotalBytes - totalBytes
                || worlds.size() >= 256)
                throw std::runtime_error("world save directory exceeds safe cleanup limits");
            totalBytes += size;
            worlds.push_back(entry.path());
        }
        std::ranges::sort(worlds);
        if (worlds.empty()) return;
        // Registry readiness is shared by every local save. Validate it once
        // before visiting any file, so an unavailable menu registry is a
        // single fail-closed diagnostic rather than one warning per world.
        const auto live = ReadValidatedLiveBuildingIds(expected);
        std::vector<std::wstring> skipped;
        for (const auto& world : worlds) {
            try { RunLocalWorldBuildingPreflight(world, live); }
            catch (const std::exception& error) {
                skipped.push_back(world.filename().wstring());
                PS::Log<LogLevel::Warning>(STR(
                    "[PERSISTENCE-PRUNER][BUILDINGS][UNVERIFIED] '{}' cleanup could not be certified: {}.\n"),
                    world.filename().wstring(), PS::ToWideSafe(error.what()));
            }
        }
        if (!skipped.empty())
            PS::Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][BUILDINGS][PARTIAL] {} local world save(s) did not complete verified cleanup; inspect each reason.\n"),
                skipped.size());
        else
            PS::Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][BUILDINGS][CHECKED] {} local world save(s) checked.\n"),
                worlds.size());
    }

    // Game Pass save-provider hooks. The slot hook supplies the world identity;
    // the archive hook repairs a validated payload before deserialization.
    SafetyHookInline NativeSpudLoadGameProbe{};
    SafetyHookInline NativeSpudDeserializeProbe{};
    DragonWilds::DragonWildsBuildingModLoader* NativeWorldBuildingLoader = nullptr;
    thread_local std::wstring ActiveSpudLoadSlot;
    bool NativeSpudLoadSlotReady = false;

    bool ProbeMemoryReadable(const void* address, std::size_t bytes)
    {
        if (!address || !bytes) return false;
        MEMORY_BASIC_INFORMATION page{};
        if (!VirtualQuery(address, &page, sizeof(page)) || page.State != MEM_COMMIT
            || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto protection = page.Protect & 0xFF;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE
            && protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ
            && protection != PAGE_EXECUTE_READWRITE
            && protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        const auto regionStart = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        return start >= regionStart && bytes <= page.RegionSize - (start - regionStart);
    }

    bool ProbeMemoryWritable(const void* address, std::size_t bytes)
    {
        if (!address || !bytes) return false;
        MEMORY_BASIC_INFORMATION page{};
        if (!VirtualQuery(address, &page, sizeof(page)) || page.State != MEM_COMMIT
            || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto protection = page.Protect & 0xFF;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY
            && protection != PAGE_EXECUTE_READWRITE
            && protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        const auto regionStart = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        return start >= regionStart && bytes <= page.RegionSize - (start - regionStart);
    }

    std::wstring ReadProbeFString(const void* address)
    {
        struct Layout { const wchar_t* Data; std::int32_t Count; std::int32_t Capacity; };
        if (!ProbeMemoryReadable(address, sizeof(Layout))) return L"<unavailable>";
        Layout string{};
        std::memcpy(&string, address, sizeof(string));
        if (string.Count < 1 || string.Count > 512 || string.Capacity < string.Count
            || !ProbeMemoryReadable(string.Data,
                static_cast<std::size_t>(string.Count) * sizeof(wchar_t)))
            return L"<unavailable>";
        if (string.Data[string.Count - 1] != L'\0') return L"<unavailable>";
        return std::wstring(string.Data, static_cast<std::size_t>(string.Count - 1));
    }

    fs::path BackupWorldBuildingPayload(const std::wstring& slot,
        std::span<const std::uint8_t> original)
    {
        std::wstring safeSlot;
        for (const auto ch : slot) {
            if (safeSlot.size() == 48) break;
            safeSlot.push_back((ch >= L'a' && ch <= L'z')
                || (ch >= L'A' && ch <= L'Z')
                || (ch >= L'0' && ch <= L'9')
                || ch == L'-' || ch == L'_' ? ch : L'_');
        }
        if (safeSlot.empty()) throw std::runtime_error("world slot is unavailable");
        const auto directory = PS::HostServices::StateDirectory()
            / L"backups" / L"world-building";
        std::error_code error;
        fs::create_directories(directory, error);
        if (error) throw std::runtime_error("world backup directory is unavailable");
        static std::atomic<std::uint64_t> sequence{0};
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto name = safeSlot + L"-" + std::to_wstring(nonce) + L"-"
            + std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed)) + L".sav";
        const auto backup = directory / name;
        if (fs::exists(backup)) throw std::runtime_error("world backup path already exists");
        {
            std::ofstream output(backup, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("world backup could not be created");
            output.write(reinterpret_cast<const char*>(original.data()),
                static_cast<std::streamsize>(original.size()));
            output.flush();
            if (!output) throw std::runtime_error("world backup write failed");
        }
        std::ifstream verification(backup, std::ios::binary);
        const std::vector<std::uint8_t> written(std::istreambuf_iterator<char>{verification},
            std::istreambuf_iterator<char>{});
        if (!verification.is_open() || verification.bad()
            || written.size() != original.size()
            || !std::equal(written.begin(), written.end(), original.begin()))
            throw std::runtime_error("world backup verification failed");
        return backup;
    }

    void NativeSpudLoadGameReadOnly(void* subsystem, const void* slotName,
        std::uint8_t clientLoadingListen)
    {
        const auto slot = ReadProbeFString(slotName);
        const auto previousSlot = std::exchange(ActiveSpudLoadSlot, slot);
        NativeSpudLoadGameProbe.call<void>(subsystem, slotName, clientLoadingListen);
        ActiveSpudLoadSlot = previousSlot;
    }

    void InstallNativeSpudLoadGameProbe()
    {
        constexpr std::uintptr_t routineRva = 0x6545470;
        constexpr std::array<std::uint8_t, 21> expected{
            0x44,0x88,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,
            0x55,0x53,0x56,0x41,0x54,0x41,0x57,0x48,0x8D,0x6C,0x24};
        auto* image = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
        if (!image || !ProbeMemoryReadable(image, sizeof(IMAGE_DOS_HEADER))) return;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0
            || dos->e_lfanew > 4096) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
        if (!ProbeMemoryReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE
            || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
            || nt->OptionalHeader.SizeOfImage < routineRva + expected.size()) return;
        auto* routine = const_cast<std::uint8_t*>(image + routineRva);
        const auto verified = ProbeMemoryReadable(routine, expected.size())
            && std::memcmp(routine, expected.data(), expected.size()) == 0;
        const auto installed = verified && PS::InstallInlineHook(NativeSpudLoadGameProbe,
            routine, reinterpret_cast<void*>(&NativeSpudLoadGameReadOnly));
        NativeSpudLoadSlotReady = installed;
    }

    void NativeSpudDeserializeGuarded(void* state, const void* archive,
        std::uint8_t loadingListen, void* context, void* callback)
    {
        if (!NativeSpudLoadSlotReady) {
            NativeSpudDeserializeProbe.call<void>(state, archive, loadingListen,
                context, callback);
            return;
        }
        if (ProbeMemoryReadable(archive, 0x10)) {
            if (ProbeMemoryReadable(archive, 0x98)) {
                void* innerArchive{};
                std::memcpy(&innerArchive,
                    static_cast<const std::uint8_t*>(archive) + 0x90,
                    sizeof(innerArchive));
                if (ProbeMemoryReadable(innerArchive, sizeof(void*))) {
                    if (ProbeMemoryReadable(innerArchive, 0xA8)) {
                        const std::uint8_t* bytes{};
                        const void* array{};
                        std::int64_t position{};
                        std::int64_t limit{};
                        std::memcpy(&position,
                            static_cast<const std::uint8_t*>(innerArchive) + 0x90,
                            sizeof(position));
                        std::memcpy(&array,
                            static_cast<const std::uint8_t*>(innerArchive) + 0x98,
                            sizeof(array));
                        std::memcpy(&limit,
                            static_cast<const std::uint8_t*>(innerArchive) + 0xA0,
                            sizeof(limit));
                        if (ProbeMemoryReadable(array, 0x10)) {
                            std::int32_t count{};
                            std::int32_t capacity{};
                            std::memcpy(&bytes, array, sizeof(bytes));
                            std::memcpy(&count,
                                static_cast<const std::uint8_t*>(array) + 0x08,
                                sizeof(count));
                            std::memcpy(&capacity,
                                static_cast<const std::uint8_t*>(array) + 0x0C,
                                sizeof(capacity));
                            const bool bounded = count >= 8 && count <= 32 * 1024 * 1024
                                && capacity >= count
                                && ProbeMemoryReadable(bytes, static_cast<std::size_t>(count));
                            std::uint32_t marker{};
                            if (bounded) std::memcpy(&marker, bytes, sizeof(marker));
                            if (bounded && marker == 0x45564153u) {
                                try {
                                    const std::vector<std::uint8_t> original(bytes,
                                        bytes + static_cast<std::size_t>(count));
                                    const auto parsed = PS::WorldBuildingSavePreflight::Parse(
                                        original);
                                    try {
                                        if (!NativeWorldBuildingLoader)
                                            throw std::runtime_error("building loader is unavailable for Game Pass world cleanup");
                                        const auto live = ReadValidatedLiveBuildingIds(
                                            NativeWorldBuildingLoader->CleanupExpectations());
                                        const auto repair = PS::WorldBuildingSavePreflight::RemoveUnresolved(
                                            original, live);
                                        const bool loadEligible = !ActiveSpudLoadSlot.empty()
                                            && ActiveSpudLoadSlot != L"<unavailable>"
                                            && loadingListen == 0 && position == 0
                                            && limit == std::numeric_limits<std::int64_t>::max()
                                            && !repair.removedIds.empty()
                                            && repair.removedIds.size()
                                                == repair.removedPersistenceIds.size()
                                            && repair.bytes.size() >= 8
                                            && repair.bytes.size() < original.size();
                                        if (loadEligible) {
                                            const auto verified = PS::WorldBuildingSavePreflight::Parse(
                                                repair.bytes);
                                            const std::unordered_set<std::uint32_t> removed(
                                                repair.removedIds.begin(), repair.removedIds.end());
                                            bool retainedMatch = verified.pieces.size() + removed.size()
                                                == parsed.pieces.size();
                                            std::size_t retainedIndex = 0;
                                            for (const auto& piece : parsed.pieces) {
                                                if (removed.contains(piece.id)) continue;
                                                if (retainedIndex >= verified.pieces.size()) {
                                                    retainedMatch = false;
                                                    break;
                                                }
                                                const auto& retained = verified.pieces[retainedIndex++];
                                                std::vector<std::uint32_t> expectedLinks;
                                                for (const auto link : piece.links)
                                                    if (!removed.contains(link)) expectedLinks.push_back(link);
                                                if (piece.id != retained.id
                                                    || piece.persistenceId != retained.persistenceId
                                                    || expectedLinks != retained.links
                                                    || piece.linksOffset - piece.offset
                                                        != retained.linksOffset - retained.offset
                                                    || std::memcmp(original.data() + piece.offset,
                                                        repair.bytes.data() + retained.offset,
                                                        piece.linksOffset - piece.offset) != 0) {
                                                    retainedMatch = false;
                                                    break;
                                                }
                                            }
                                            retainedMatch = retainedMatch
                                                && retainedIndex == verified.pieces.size();
                                            const bool writable = retainedMatch
                                                && ProbeMemoryWritable(array, 0x10)
                                                && ProbeMemoryWritable(bytes,
                                                    static_cast<std::size_t>(count));
                                            const std::uint8_t* currentBytes{};
                                            std::int32_t currentCount{};
                                            std::int32_t currentCapacity{};
                                            if (writable) {
                                                std::memcpy(&currentBytes, array,
                                                    sizeof(currentBytes));
                                                std::memcpy(&currentCount,
                                                    static_cast<const std::uint8_t*>(array) + 0x08,
                                                    sizeof(currentCount));
                                                std::memcpy(&currentCapacity,
                                                    static_cast<const std::uint8_t*>(array) + 0x0C,
                                                    sizeof(currentCapacity));
                                            }
                                            if (writable && currentBytes == bytes
                                                && currentCount == count
                                                && currentCapacity == capacity
                                                && std::memcmp(bytes, original.data(),
                                                    original.size()) == 0) {
                                                fs::path backup;
                                                if (PS::PSConfig::Get()->GetSettings()
                                                        .worldSaveCleanup.backupBeforePrune)
                                                    backup = BackupWorldBuildingPayload(
                                                        ActiveSpudLoadSlot, original);
                                                std::memcpy(const_cast<std::uint8_t*>(bytes),
                                                    repair.bytes.data(), repair.bytes.size());
                                                const auto repairedCount = static_cast<std::int32_t>(
                                                    repair.bytes.size());
                                                std::memcpy(static_cast<std::uint8_t*>(
                                                    const_cast<void*>(array)) + 0x08,
                                                    &repairedCount, sizeof(repairedCount));
                                                RC::Output::send<RC::LogLevel::Normal>(STR(
                                                    "[RuneSchema] [summary] Save cleanup | world='{}' | missing building IDs removed={} | retained={} | backup='{}'.\n"),
                                                    ActiveSpudLoadSlot, removed.size(),
                                                    verified.pieces.size(),
                                                    backup.empty() ? L"disabled" : backup.filename().wstring());
                                            } else {
                                                RC::Output::send<RC::LogLevel::Warning>(STR(
                                                    "[RuneSchema] [SAVE-CLEANUP][BUILDINGS][SKIPPED] Native buffer changed during validation; world unchanged.\n"));
                                            }
                                        } else if (!repair.removedIds.empty()) {
                                            RC::Output::send<RC::LogLevel::Warning>(STR(
                                                "[RuneSchema] [SAVE-CLEANUP][BUILDINGS][SKIPPED] World load contract was not safe for repair; world unchanged.\n"));
                                        }
                                    } catch (const std::exception& error) {
                                        RC::Output::send<RC::LogLevel::Warning>(STR(
                                            "[RuneSchema] [SAVE-CLEANUP][BUILDINGS][SKIPPED] World unchanged: {}.\n"),
                                            PS::ToWideSafe(error.what()));
                                    }
                                } catch (const std::exception& error) {
                                    RC::Output::send<RC::LogLevel::Warning>(STR(
                                        "[RuneSchema] [SAVE-CLEANUP][BUILDINGS][SKIPPED] World data could not be validated: {}.\n"),
                                        PS::ToWideSafe(error.what()));
                                }
                            }
                        }
                    }
                }
            }
        }
        NativeSpudDeserializeProbe.call<void>(state, archive, loadingListen,
            context, callback);
    }

    void InstallNativeSpudDeserializeProbe()
    {
        constexpr std::uintptr_t routineRva = 0x6529820;
        constexpr std::array<std::uint8_t, 16> expected{
            0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,
            0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
        auto* image = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
        if (!image || !ProbeMemoryReadable(image, sizeof(IMAGE_DOS_HEADER))) return;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0
            || dos->e_lfanew > 4096) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
        if (!ProbeMemoryReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE
            || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
            || nt->OptionalHeader.SizeOfImage < routineRva + expected.size()) return;
        auto* routine = const_cast<std::uint8_t*>(image + routineRva);
        const auto verified = ProbeMemoryReadable(routine, expected.size())
            && std::memcmp(routine, expected.data(), expected.size()) == 0;
        const auto installed = verified && PS::InstallInlineHook(NativeSpudDeserializeProbe,
            routine, reinterpret_cast<void*>(&NativeSpudDeserializeGuarded));
        if (installed && NativeSpudLoadSlotReady)
            RC::Output::send<RC::LogLevel::Normal>(STR(
                "[RuneSchema] [SAVE-CLEANUP][BUILDINGS] Game Pass world cleanup is ready.\n"));
        else
            RC::Output::send<RC::LogLevel::Warning>(STR(
                "[RuneSchema] [SAVE-CLEANUP][BUILDINGS][GAMEPASS-UNAVAILABLE] Required world hooks could not be installed; saves will not be changed.\n"));
    }

    void ProbeGamePassWorldSaveFunctions(DragonWilds::DragonWildsBuildingModLoader* buildingLoader)
    {
        if (PS::Storefront::Current() != PS::Storefront::Kind::GamePass) return;
        NativeWorldBuildingLoader = buildingLoader;
        InstallNativeSpudLoadGameProbe();
        InstallNativeSpudDeserializeProbe();
    }

    struct ModIdentity { std::string Id; std::string Name; std::string Author="Unknown Author"; std::string Website; bool Present=false; };
    std::string TrimIdentity(std::string value) {
        const auto first=value.find_first_not_of(" \t\r\n");
        if(first==std::string::npos)return {};
        const auto last=value.find_last_not_of(" \t\r\n");
        return value.substr(first,last-first+1);
    }
    ModIdentity ReadModIdentity(const fs::path& root,const RC::StringType& folder) {
        ModIdentity result;result.Id=RC::to_string(folder);result.Name=result.Id;
        const auto file=root/"ID.txt";if(!fs::exists(file))return result;
        result.Present=true;std::ifstream input(file);if(!input)throw std::runtime_error("ID.txt could not be opened");
        std::string line;bool structured=false;
        while(std::getline(input,line)) {
            line=TrimIdentity(line);if(line.empty()||line.starts_with('#'))continue;
            const auto split=line.find_first_of(":=");
            if(split==std::string::npos) {if(!structured){result.Id=line;result.Name=line;}continue;}
            structured=true;auto key=TrimIdentity(line.substr(0,split));auto value=TrimIdentity(line.substr(split+1));
            if(key=="ModID"||key=="ModId")result.Id=value;
            else if(key=="Name"||key=="ModName")result.Name=value;
            else if(key=="Author"||key=="ModAuthor")result.Author=value;
            else if(key=="Website"||key=="ModWebsite"||key=="ModSite")result.Website=value;
        }
        if(result.Id.empty())throw std::runtime_error("ID.txt requires ModID when the file is present");
        if(result.Name.empty())result.Name=result.Id;
        if(result.Author.empty())throw std::runtime_error("ID.txt Author cannot be empty");
        if(result.Id.size()>128||result.Name.size()>256||result.Author.size()>256||result.Website.size()>2048)
            throw std::runtime_error("ID.txt metadata exceeds its safe length limit");
        if(!result.Website.empty()&&!result.Website.starts_with("https://")&&!result.Website.starts_with("http://"))
            throw std::runtime_error("ID.txt Website must be an http or https link");
        return result;
    }
}

namespace
{
    struct PendingAutoReload
    {
        fs::path FilePath;
        std::string FolderType;
        RC::StringType ModName;
    };

    std::mutex AutoReloadMutex;
    std::vector<PendingAutoReload> PendingAutoReloads;
    RC::Unreal::Hook::GlobalCallbackId AutoReloadCallbackId = RC::Unreal::Hook::ERROR_ID;
    std::atomic<bool> AutoReloadWorkPending{false};

    bool IsReloadableJsonFile(const fs::path& filePath)
    {
        auto extension = filePath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        return extension == ".json" || extension == ".jsonc";
    }
}

namespace DragonWilds {
    DragonWildsMainLoader::DragonWildsMainLoader() {
        PS::StartupTrace::Begin(PS::HostServices::ExportsDirectory() / "startup");
        PS::StartupTrace::Mark("construct loaders begin");
        CreateLoaders();
        PS::StartupTrace::Mark("construct loaders complete");
    }

    DragonWildsMainLoader::~DragonWildsMainLoader()
    {
        // Stop the watcher before destroying its consumers.
        PS::StartupTrace::Mark("shutdown begin");
        m_fileWatcher.reset();
        m_logicPakBridge.Stop();
        DatatableSerialize_Hook = {};

        GameInstanceInit_Hook = {};

        GetPakFolders_Hook = {};

        DatatableSerializeCallbacks.clear();
        GameInstanceInitCallbacks.clear();
        m_registryBridge.Stop();
        m_dataRegistrar.Shutdown();
        m_characterEntryRecovery.Shutdown();

        if (AutoReloadCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(AutoReloadCallbackId);
        }
        if (m_coreStartupCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_coreStartupCallbackId);
        if (m_dedicatedServerReadyCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_dedicatedServerReadyCallbackId);

        AutoReloadWorkPending.store(false);
        AutoReloadCallbackId = Hook::ERROR_ID;

        {
            std::scoped_lock lock{AutoReloadMutex};
            PendingAutoReloads.clear();
        }
        // Loaders must die before their registry.
        m_loaders.clear();
        PS::StartupTrace::Mark("shutdown complete");

    }

    void DragonWildsMainLoader::PreInitialize()
    {
        if (PS::PSConfig::Get()->GetSettings().persistence.allowCorruptCharacterEntry)
            PS::Log<LogLevel::Error>(STR(
                "[SAVE-ENTRY][QUARANTINED] Corrupt-character entry recovery was requested but was not installed. Crash dumps identified an unsafe ProcessPlayerStateLoad ABI; native character acceptance remains unchanged.\n"));
        else
            PS::Log<LogLevel::Normal>(STR(
                "[SAVE-ENTRY][DISABLED] Corrupt-character entry recovery is disabled by default. Native character acceptance remains unchanged.\n"));
        HookDatatableSerialize();
        SetupAlternativePakPathReader();
    }

    void DragonWildsMainLoader::Initialize()
	{
        m_readiness.MarkUnrealReady();
        // Some WinGDK builds do not materialize the DataTable CDO/vtable during
        // PreInitialize. Retry once Unreal is ready, without duplicating a hook
        // that was already installed successfully.
        if (DatatableSerializeCallbacks.empty())
            HookDatatableSerialize();
        if (!PS::Storefront::IsDedicatedServer())
            HookGameInstanceInit();
        else
        {
            Hook::FCallbackOptions serverOptions{};
            serverOptions.OwnerModName=TEXT("RuneSchema");
            serverOptions.HookName=TEXT("DedicatedServerRegistryReady");
            m_dedicatedServerReadyCallbackId=Hook::RegisterInitGameStatePostCallback(
                [this](Hook::TCallbackIterationData<void>& iteration,AGameModeBase* gameMode) {
                    if (!gameMode || !gameMode->GetWorld()) return;
                    m_dedicatedServerWorldReady.store(true,std::memory_order_release);
                    if (InitCore()) {
                        m_coreStartupComplete.store(true,std::memory_order_release);
                        SetupGameInstanceInitLoadersOnce();
                        m_dedicatedServerReadyCallbackId=Hook::ERROR_ID;
                        iteration.RemoveSelf();
                    } else if (m_coreStartupFailed.load(std::memory_order_acquire)) {
                        m_dedicatedServerReadyCallbackId=Hook::ERROR_ID;
                        iteration.RemoveSelf();
                    }
                },serverOptions);
            if (m_dedicatedServerReadyCallbackId==Hook::ERROR_ID)
                PS::Log<LogLevel::Error>(STR(
                    "[SERVER][REGISTRY-DEFERRED][UNAVAILABLE] InitGameState boundary could not be installed; authoritative registration will remain inactive rather than mutate an unsettled server subsystem.\n"));
            else
                PS::Log<LogLevel::Normal>(STR(
                    "[SERVER][REGISTRY-DEFERRED] Runtime cloning and registration will begin after the dedicated server reaches InitGameState.\n"));
        }
        // GameInstance::Init and the optional DataTable hook remain the earliest
        // paths.  A game-thread tick is the storefront-agnostic fallback.  It
        // avoids initializing loaders in on_unreal_init before their target
        // assets/tables are ready, while still completing before a player can
        // select and deserialize a character.
        Hook::FCallbackOptions startupOptions{};
        startupOptions.OwnerModName=TEXT("RuneSchema");
        startupOptions.HookName=TEXT("CoreStartupFallback");
        m_coreStartupCallbackId=Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>& iteration,UEngine*,float,bool) {
                if(m_coreStartupComplete.load(std::memory_order_acquire)
                    || InitCore()) {
                    m_coreStartupComplete.store(true,std::memory_order_release);
                    // GameInstance::Init may already have completed before the
                    // game-thread readiness fallback finished InitCore.  In
                    // that race the inline hook cannot fire again, so enter
                    // the GameInstance loader phase here as well.  The atomic
                    // once gate keeps the normal hook and this fallback from
                    // ever running registration twice.
                    if (!PS::Storefront::IsDedicatedServer()
                        || m_dedicatedServerWorldReady.load(std::memory_order_acquire))
                        SetupGameInstanceInitLoadersOnce();
                    m_coreStartupCallbackId=Hook::ERROR_ID;
                    iteration.RemoveSelf();
                } else if(m_coreStartupFailed.load(std::memory_order_acquire)) {
                    m_coreStartupCallbackId=Hook::ERROR_ID;
                    iteration.RemoveSelf();
                }
            },startupOptions);
        if(m_coreStartupCallbackId==Hook::ERROR_ID)
            PS::Log<LogLevel::Error>(STR("[DEGRADED][CORE] Game-thread startup fallback could not be installed.\n"));
        PS::StartupTrace::Mark("UE4SS readiness published; awaiting engine lifecycle event");
        SetupAutoReload();
	}

    void DragonWildsMainLoader::AutoReload(const std::filesystem::path& filePath)
    {
        if (!IsReloadableJsonFile(filePath))
        {
            return;
        }

        auto it = std::find_if(filePath.begin(), filePath.end(),
            [](const auto& p) { return p == "RuneSchema"; });

        if (it == filePath.end() || std::distance(it, filePath.end()) < 4)
        {
            return;
        }

        std::advance(it, 2);
        auto modName = it->native();

        std::advance(it, 1);
        auto folderType = PS::ModFolderLayout::AsciiLower(it->string());
        const bool supportedFolder = folderType == "players" || folderType == "nameplates"
            || std::any_of(m_loaders.begin(), m_loaders.end(), [&](const auto& loader) {
                return PS::ModFolderLayout::EqualsInsensitive(loader->GetModFolderType(), folderType);
            });
        if (!supportedFolder) return;

        std::ifstream f(filePath);
        if (f.peek() == std::ifstream::traits_type::eof()) {
            return;
        }
        f.close();

        {
            std::scoped_lock lock{AutoReloadMutex};

            auto existing = std::find_if(PendingAutoReloads.begin(), PendingAutoReloads.end(),
                [&](const PendingAutoReload& pendingAutoReload) {
                    return pendingAutoReload.FilePath == filePath;
                });

            if (existing != PendingAutoReloads.end())
            {
                existing->FolderType = folderType;
                existing->ModName = modName;
            }
            else
            {
                PendingAutoReloads.push_back({ filePath, folderType, modName });
            }

            AutoReloadWorkPending.store(true, std::memory_order_release);
        }

    }

    void DragonWildsMainLoader::IterateModsFolder(const std::function<void(const std::filesystem::path&, const RC::StringType&)>& callback)
    {
        const auto modsPath = GetModsPath();
        if (!m_orderResolved) {
            std::vector<RC::StringType> discovered;
            if (fs::exists(modsPath))
                for (const auto& entry : fs::directory_iterator(modsPath))
                    if (PS::ModFolderLayout::LooksLikeRuneSchemaMod(entry.path()))
                        discovered.push_back(entry.path().filename().native());
            m_orderedMods = ModLoadOrder::Resolve(modsPath, discovered);
            m_orderResolved = true;
            for (const auto& name : m_orderedMods)
                PS::StartupTrace::Mark("mod order: " + RC::to_string(name));
        }
        for (const auto& folderName : m_orderedMods) callback(modsPath / folderName, folderName);
    }

    void DragonWildsMainLoader::SetupPostEngineInitLoaders()
    {
        PS::StartupTrace::Mark("PostEngineInit begin");
        InitializeMods(EEngineLifecyclePhase::PostEngineInit);
        LoadMods(EEngineLifecyclePhase::PostEngineInit);
        SetupLogicPakBridge();
        PS::StartupTrace::Mark("PostEngineInit complete");
    }

    void DragonWildsMainLoader::SetupLogicPakBridge()
    {
        if (PS::Storefront::IsDedicatedServer()) return;
        std::vector<std::pair<fs::path,std::string>> mods;
        IterateModsFolder([&](const fs::path& path,const RC::StringType& owner) {
            mods.emplace_back(path,RC::to_string(owner));
        });
        m_logicPakBridge.Start(mods);
    }

    void DragonWildsMainLoader::SetupGameInstanceInitLoadersOnce()
    {
        bool expected=false;
        if (!m_gameInstanceLoadersStarted.compare_exchange_strong(expected,true)) return;
        SetupGameInstanceInitLoaders();
    }

    void DragonWildsMainLoader::SetupGameInstanceInitLoaders()
    {
        PS::StartupTrace::Mark("GameInstanceInit loaders begin");
        InitializeMods(EEngineLifecyclePhase::GameInstanceInit);
        LoadMods(EEngineLifecyclePhase::GameInstanceInit);

        bool buildingRegistrationReady = m_buildingLoader != nullptr;
        if (m_buildingLoader)
        {
            try {m_buildingLoader->ActivateWorldRegistration();}
            catch(const std::exception& error){buildingRegistrationReady=false;PS::Log<LogLevel::Error>(STR("[DEGRADED][LOADER:buildings] World registration disabled: {}.\n"),PS::ToWideSafe(error.what()));}
        }

        if (m_stringLoader)
        {
            try {m_stringLoader->ApplyPending();}
            catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[DEGRADED][LOADER:strings] Pending strings were not applied: {}.\n"),PS::ToWideSafe(error.what()));}
        }

        PS::StartupTrace::Mark("data registrar begin");
        bool dataRegistrarReady = false;
        try {
            m_dataRegistrar.Initialize();
            if(!m_dataRegistrar.IsInitialized())
                throw std::runtime_error("required live persistence registry bindings are unavailable");
            dataRegistrarReady = true;
            if (PS::Storefront::IsDedicatedServer())
                PS::Log<LogLevel::Normal>(STR(
                    "[SERVER][REGISTRIES-READY] Live persistence registries, cooked PAK discovery and configured combat registration completed.\n"));
        }
        catch(const std::exception& error){
            PS::Log<LogLevel::Error>(STR("[DEGRADED][SERVICE:data-registrar] General registration and character-save cleanup unavailable: {}. Dedicated-server building cleanup will still check its live registries independently.\n"),PS::ToWideSafe(error.what()));
        }
        // Dedicated servers attempt the world pass independently of unrelated
        // loader health. Each candidate still requires the live building
        // registries to pass ReadValidatedLiveBuildingIds before any write.
        if (PS::Storefront::IsDedicatedServer()
            || (buildingRegistrationReady && dataRegistrarReady)) {
            try { RunAllLocalWorldBuildingPreflights(m_buildingLoader); }
            catch(const std::exception& error) {
                PS::Log<LogLevel::Warning>(STR(
                    "[PERSISTENCE-PRUNER][BUILDINGS][SKIPPED] World cleanup could not be certified: {}.\n"),
                    PS::ToWideSafe(error.what()));
            }
        }
        try { ProbeGamePassWorldSaveFunctions(m_buildingLoader); }
        catch (const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR("[SAVE-CLEANUP][BUILDINGS] Game Pass world-save hooks could not start: {}.\n"),
                PS::ToWideSafe(error.what()));
        }
        try {m_registryBridge.Start();}
        catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[DEGRADED][SERVICE:registry-bridge] Networking bridge unavailable: {}.\n"),PS::ToWideSafe(error.what()));}
        PS::StartupTrace::Mark("GameInstanceInit loaders complete (deferred world work may remain)");
    }

    void DragonWildsMainLoader::HookDatatableSerialize()
    {
        auto DatatableSerializeFuncPtr = DragonWilds::SignatureManager::GetSignature("UDataTable::Serialize");
        if (!DatatableSerializeFuncPtr)
            DatatableSerializeFuncPtr = DragonWilds::SignatureManager::ResolveUObjectVirtual(
                "UDataTable::Serialize", TEXT("/Script/Engine.DataTable"),
                {TEXT("Serialize__Ref_FArchive"), TEXT("Serialize")});
        if (!DatatableSerializeFuncPtr)
        {
            PS::Log<LogLevel::Warning>(STR("[BINDING:UDataTable::Serialize][UNAVAILABLE] Early table observation is disabled; normal GameInstance loading continues.\n"));
            return;
        }

        PS::Log<LogLevel::Normal>(STR("Native binding UDataTable::Serialize provider: {}.\n"),
            PS::ToWideSafe(DragonWilds::SignatureManager::GetSource("UDataTable::Serialize").c_str()));

        DatatableSerializeCallbacks.push_back([&](RC::Unreal::UDataTable* datatable) {
            if (m_readiness.Observe(datatable)) m_datatableRegistry.Add(datatable);
            else if (m_readiness.IsUnrealReady() && IsInGameThreadRaw()) InitCore();
        });

        if (!PS::InstallInlineHook(DatatableSerialize_Hook, reinterpret_cast<void*>(DatatableSerializeFuncPtr), reinterpret_cast<void*>(OnDataTableSerialized))) {
            DatatableSerializeCallbacks.clear();
            PS::StartupTrace::Mark("ERROR DataTable hook installation");
            PS::Log<LogLevel::Warning>(STR("[BINDING:UDataTable::Serialize][UNAVAILABLE] Early table hook could not be installed; normal GameInstance loading continues.\n"));
            return;
        }
        PS::Log<LogLevel::Verbose>(STR("Core pre-initialized.\n"));
    }

    void DragonWildsMainLoader::HookGameInstanceInit()
    {
        auto VTable = DragonWilds::GetVTablePtrByClassPath(TEXT("/Script/Engine.GameInstance"));
        if (!VTable)
        {
            PS::Log<LogLevel::Error>(STR("Something went wrong with getting VTable pointer for GameInstance."));
            return;
        }

        void* GameInstanceInitPtr = DragonWilds::GetVirtualFunctionFromVTable(VTable, 90);
        PS::Log<LogLevel::Verbose>(STR("Found GameInstance::Init: {}\n"), GameInstanceInitPtr);

        GameInstanceInitCallbacks.push_back([&](UObject* Instance) {
            if (InitCore()) SetupGameInstanceInitLoadersOnce();
        });

        if (!PS::InstallInlineHook(GameInstanceInit_Hook, GameInstanceInitPtr, reinterpret_cast<void*>(OnGameInstanceInit))) {
            GameInstanceInitCallbacks.clear();
            PS::StartupTrace::Mark("ERROR GameInstance hook installation");
            PS::Log<LogLevel::Error>(STR("Unable to install GameInstance initialization hook.\n"));
        }
    }

    void DragonWildsMainLoader::CreateLoaders()
    {
        RegisterLoader(std::make_unique<DragonWildsRegistryLoader>(m_registryBridge));
        RegisterLoader(std::make_unique<DragonWildsEquipmentLoader>());
        RegisterLoader(std::make_unique<DragonWildsGameplayEffectLoader>());
        RegisterLoader(std::make_unique<DragonWildsNiagaraLoader>());
        RegisterLoader(std::make_unique<DragonWildsEnumLoader>());

        RegisterLoader(std::make_unique<DragonWildsRawTableLoader>());

        auto assets=std::make_unique<DragonWildsAssetModLoader>();
        auto* assetService=assets.get();
        RegisterLoader(std::move(assets));

        RegisterLoader(std::make_unique<DragonWildsBlueprintModLoader>());

        auto recipes = std::make_unique<DragonWildsRecipeModLoader>();
        auto* recipeService = recipes.get();
        assetService->SetRecipeService(recipeService);
        RegisterLoader(std::move(recipes));
        auto npcs=std::make_unique<DragonWildsNpcLoader>(recipeService);
        auto* npcService=npcs.get();
        m_registryBridge.QuestControl=[npcService](RC::Unreal::UObject* player,const std::string& quest,
            const std::string& action,const std::string& payload){
            return npcService->HandleNetworkQuestControl(player,quest,action,payload);
        };
        m_registryBridge.ClientNotification=[npcService](RC::Unreal::UObject* player,const std::string& channel,
            const std::string& entity,const std::string& payload){
            npcService->HandleNetworkNotification(player,channel,entity,payload);
        };
        npcService->PublishWorldState=[this](const std::string& instance,const std::string& record){
            (void)m_registryBridge.UpsertWorldInstance(instance,record);
        };
        RegisterLoader(std::move(npcs));
        RegisterLoader(std::make_unique<DragonWildsVendorLoader>(npcService));
        RegisterLoader(std::make_unique<DragonWildsDialogueLoader>(npcService));
        RegisterLoader(std::make_unique<DragonWildsQuestLoader>(npcService));
        RegisterLoader(std::make_unique<DragonWildsEventLoader>(npcService));

        auto journalLoader=std::make_unique<DragonWildsJournalModLoader>();
        journalLoader->SetRecipeService(recipeService);
        RegisterLoader(std::move(journalLoader));
        auto loreLoader=std::make_unique<DragonWildsJournalModLoader>(true);
        loreLoader->SetRecipeService(recipeService);
        npcService->OpenLore=[service=loreLoader.get()](RC::Unreal::UObject* controller,const std::string& entry){return service->OpenLoreForPlayer(controller,entry);};
        RegisterLoader(std::move(loreLoader));

        auto buildingModLoader = std::make_unique<DragonWildsBuildingModLoader>();
        m_buildingLoader = buildingModLoader.get();
        RegisterLoader(std::move(buildingModLoader));

        auto spawnLoader = std::make_unique<DragonWildsSpawnLoader>();
        m_spawnLoader = spawnLoader.get();
        m_buildingLoader->ImportPlacements=[service=m_spawnLoader](const nlohmann::json& placements,const RC::StringType& mod){
            service->LoadBuildingPlacements(placements,mod);
        };
        m_spawnLoader->PublishWorldState=[this](const std::string& instance,const std::string& record){
            (void)m_registryBridge.UpsertWorldInstance(instance,record);
        };
        m_registryBridge.PersistentState=[npcService,service=m_spawnLoader](RC::Unreal::UObject*,const std::string& channel,
            const std::string&,const std::string& payload){
            if(channel!="world.state")return;
            npcService->HandleNetworkWorldState(payload);
            service->HandleNetworkWorldState(payload);
        };
        m_spawnLoader->ForwardHelpyAuthority=[this](const std::string& action,const std::string& payload){
            return m_registryBridge.RequestAuthority(action,payload);
        };
        m_spawnLoader->IsQuestCompleted=[npcService](RC::Unreal::UWorld* world,const std::string& quest){
            return npcService->IsQuestCompleted(world,quest);
        };
        m_spawnLoader->IsQuestActive=[npcService](RC::Unreal::UWorld* world,const std::string& quest){
            return npcService->IsQuestActive(world,quest);
        };
        m_spawnLoader->IsEventActive=[npcService](RC::Unreal::UWorld* world,const std::string& event){
            return npcService->EventService().Running(event,world);
        };
        m_registryBridge.AuthorityChannel=[service=m_spawnLoader](RC::Unreal::UObject* player,const std::string& channel,
            const std::string&,const std::string& action,const std::string& payload){
            if(channel!="helpy.authority")throw std::runtime_error("Unsupported RuneSchema authority channel");
            return service->HandleNetworkHelpyAuthority(player,action,payload);
        };
        m_spawnLoader->PublishEventIdentity=[npcService](RC::Unreal::AActor* actor,const std::string& payload){npcService->PublishEventIdentity(actor,payload);};
        npcService->PresentEventIdentity=[service=m_spawnLoader](RC::Unreal::AActor* actor,const std::string& payload){return service->PresentEventIdentity(actor,payload);};
        m_spawnLoader->PresentEventState=[npcService](RC::Unreal::AActor* actor,const std::string& event){npcService->ObserveClientEvent(actor,event);};
        npcService->EventService().SpawnManifest=[service=m_spawnLoader](const std::string& key){return service->EventSpawnManifest(key);};
        npcService->EventService().Validate=[service=m_spawnLoader](const std::string& key){
            if(!service->HasInitialized())throw std::runtime_error("Spawn loader is disabled or not ready");
            service->ValidateEventSpawn(key);
        };
        npcService->EventService().Spawn=[service=m_spawnLoader](const std::string& key,const std::string& event,RC::Unreal::UWorld* world,const RC::Unreal::FVector& position){return service->SpawnEventAI(key,world,position,false,0,event);};
        RegisterLoader(std::move(spawnLoader));

        RegisterLoader(std::make_unique<DragonWildsCourseLoader>());

        auto stringModLoader = std::make_unique<DragonWildsStringModLoader>();
        m_stringLoader = stringModLoader.get();
        RegisterLoader(std::move(stringModLoader));

    }

    void DragonWildsMainLoader::SetupAutoReload()
    {
        auto config = PS::PSConfig::Get();
        if (!config->IsAutoReloadEnabled()) return;

        PS::Log<LogLevel::Normal>(STR("Auto-reload is enabled.\n"));

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DragonWildsAutoReload");

        AutoReloadCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
                if (!AutoReloadWorkPending.exchange(false, std::memory_order_acq_rel)) return;
                std::vector<PendingAutoReload> pendingAutoReloads;
                {
                    std::scoped_lock lock{AutoReloadMutex};
                    pendingAutoReloads.swap(PendingAutoReloads);
                }

                for (const auto& pendingAutoReload : pendingAutoReloads)
                {
                    try
                    {
                        if (std::find(m_orderedMods.begin(), m_orderedMods.end(), pendingAutoReload.ModName) == m_orderedMods.end()) continue;
                        if (!PS::PSConfig::Get()->IsLoaderEnabled(pendingAutoReload.FolderType)) continue;
                        bool handled = false;
                        if (pendingAutoReload.FolderType == "players" && m_spawnLoader)
                        {
                            m_spawnLoader->LoadPlayerRules(
                                pendingAutoReload.FilePath.parent_path(),
                                pendingAutoReload.ModName, true);
                            m_spawnLoader->FinalizePlayerRules();
                            PS::Log<LogLevel::Normal>(
                                STR("Auto-reloaded /players for mod {}\n"),
                                pendingAutoReload.ModName);
                            handled = true;
                        }
                        if (pendingAutoReload.FolderType == "nameplates" && m_spawnLoader)
                        {
                            const auto modPath = pendingAutoReload.FilePath.parent_path().parent_path();
                            m_spawnLoader->LoadNameplateDefinitions(
                                pendingAutoReload.FilePath.parent_path(), pendingAutoReload.ModName, true);
                            m_spawnLoader->FinalizeNameplateDefinitions();
                            const auto playersPath = PS::ModFolderLayout::ResolveLoaderDirectory(modPath,"players");
                            if (playersPath)
                                m_spawnLoader->LoadPlayerRules(*playersPath, pendingAutoReload.ModName, true);
                            m_spawnLoader->FinalizePlayerRules();
                            PS::Log<LogLevel::Normal>(
                                STR("Auto-reloaded /nameplates for mod {}\n"),
                                pendingAutoReload.ModName);
                            handled = true;
                        }
                        for (auto& loader : m_loaders)
                        {
                            if (handled) break;
                            if (PS::ModFolderLayout::EqualsInsensitive(
                                loader->GetModFolderType(),pendingAutoReload.FolderType))
                            {
                                loader->AutoReload(pendingAutoReload.ModName, pendingAutoReload.FilePath);
                                PS::Log<LogLevel::Normal>(STR("Auto-reloaded mod {}\n"), pendingAutoReload.ModName);
                                handled = true;
                                break;
                            }
                        }

                    }
                    catch (const std::exception& e)
                    {
                        PS::Log<LogLevel::Error>(STR("Failed to auto-reload mod {} - {}\n"),
                            pendingAutoReload.ModName, PS::ToWideSafe(e.what()));
                    }
                }

            },
            options);

        if (AutoReloadCallbackId == Hook::ERROR_ID)
        {

            PS::Log<LogLevel::Error>(STR("Failed to register auto-reload engine tick callback.\n"));
            return;
        }

        auto modsPath = GetModsPath();

        m_fileWatcher = std::make_unique<PS::FileWatchWrapper>(modsPath, [this](efsw::WatchID watchId, const std::string& dir,
            const std::string& filename, efsw::Action action,
            std::string oldFilename) {
                if (action == efsw::Actions::Add || action == efsw::Actions::Modified)
                {
                    auto path = fs::path(dir) / filename;
                    AutoReload(path);
                }
            }
        );
        m_fileWatcher->Watch();
    }

    void DragonWildsMainLoader::SetupAlternativePakPathReader()
    {
        auto GetPakFolders_Address = DragonWilds::SignatureManager::GetSignature("FPakPlatformFile::GetPakFolders");
        if (GetPakFolders_Address)
        {
            if (!PS::InstallInlineHook(GetPakFolders_Hook, reinterpret_cast<void*>(GetPakFolders_Address), reinterpret_cast<void*>(GetPakFolders))) {
                PS::StartupTrace::Mark("ERROR pak hook installation");
                PS::Log<LogLevel::Error>(STR("Unable to install additional pak-folder hook.\n"));
            }
        }
        else
        {
            PS::Log<LogLevel::Error>(STR("Unable to setup additional .pak read directory, signature for FPakPlatformFile::GetPakFolders is outdated.\n"));
        }
    }

    bool DragonWildsMainLoader::InitCore()
    {
        using Gate = PS::UnrealReadinessGate<UDataTable*>;
        const auto begin = m_readiness.Begin();
        if (begin == Gate::BeginResult::Wait) return m_readiness.IsActive();
        if (begin == Gate::BeginResult::Overflow) {
            constexpr auto reason = "Early table queue exceeded 4096 entries";
            FailStartup(reason);
            return false;
        }
        try {
            PS::StartupTrace::Mark("InitCore after UE4SS readiness begin");
            DragonWilds::StaticClassStorage::Initialize();
            SetupPostEngineInitLoaders();
            auto pending = m_readiness.Complete();
            if (!m_readiness.IsActive()) throw std::runtime_error("Early table queue overflowed during initialization");
            // Serial zero is valid for live objects, but UE4SS weak pointers reject it.
            struct ReplayIdentity { int32_t Index = -1; int32_t Serial = 0; };
            std::unordered_map<const void*, ReplayIdentity> live;
            for (auto* table : pending) live.emplace(table, ReplayIdentity{});
            if (!live.empty()) {
                size_t remaining = live.size();
                UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
                    const auto found = live.find(object);
                    if (found != live.end() && found->second.Index < 0 && object->IsA(UDataTable::StaticClass())) {
                        const auto index = object->GetInternalIndex();
                        auto* item = FUObjectArray::IndexToObject(index);
                        if (item && item->GetUObject() == object && item->IsValid(false)) {
                            found->second = {index, item->GetSerialNumber()};
                            if (--remaining == 0) return LoopAction::Break;
                        }
                    }
                    return LoopAction::Continue;
                });
            }
            size_t replayed = 0, zeroSerial = 0;
            for (auto* address : pending) {
                const auto identity = live.at(address);
                if (identity.Index < 0) continue;
                auto* item = FUObjectArray::IndexToObject(identity.Index);
                if (!item || item->GetUObject() != address ||
                    item->GetSerialNumber() != identity.Serial || !item->IsValid(false)) continue;
                m_datatableRegistry.Add(static_cast<UDataTable*>(item->GetUObject()));
                ++replayed;
                if (identity.Serial == 0) ++zeroSerial;
            }
            PS::Log<LogLevel::Normal>(STR("Early DataTable replay: {}/{} restored ({} with zero serial).\n"),
                replayed, pending.size(), zeroSerial);
            if (replayed != pending.size())
                PS::Log<LogLevel::Warning>(STR("Early DataTable replay skipped {} expired or changed object identities.\n"),
                    pending.size() - replayed);
            PS::StartupTrace::Mark("early table replay: " + std::to_string(replayed) + "/" + std::to_string(pending.size()));
            PS::StartupTrace::Mark("InitCore complete");
            PS::Log<LogLevel::Normal>(STR("[CORE][READY] RuneSchema loaders initialized. Advanced logging and auto-reload are optional.\n"));
            return true;
        } catch (const std::exception& error) {
            m_readiness.Fail();
            FailStartup(error.what());
            return false;
        }
    }

    void DragonWildsMainLoader::FailStartup(const std::string& reason)
    {
        AbortStartup("core", reason);
    }

    void DragonWildsMainLoader::AbortStartup(const std::string& stage, const std::string& reason)
    {
        if (m_coreStartupFailed.exchange(true,std::memory_order_acq_rel)) return;
        m_fileWatcher.reset();
        DatatableSerialize_Hook = {};
        GameInstanceInit_Hook = {};
        GetPakFolders_Hook = {};
        DatatableSerializeCallbacks.clear();
        GameInstanceInitCallbacks.clear();
        m_registryBridge.Stop();
        m_dataRegistrar.Shutdown();
        m_characterEntryRecovery.Shutdown();
        m_loaders.clear();
        m_stringLoader = nullptr;
        m_buildingLoader = nullptr;
        m_spawnLoader = nullptr;
        PS::StartupTrace::Fatal(stage,reason);
        if (stage == "core") {
            PS::Log<LogLevel::Error>(STR("[RuneSchema][DID-NOT-START][CORE] {}. All RuneSchema runtime hooks and services were stopped.\n"),
                PS::ToWideSafe(reason.c_str()));
        } else {
            PS::Log<LogLevel::Error>(STR("[RuneSchema][DID-NOT-START] stage={} reason={}. All RuneSchema runtime hooks and services were stopped.\n"),
                PS::ToWideSafe(stage.c_str()),PS::ToWideSafe(reason.c_str()));
        }
        if (m_fatalStartupHandler) {
            try { m_fatalStartupHandler(reason); } catch (...) {}
        }
    }

    void DragonWildsMainLoader::RegisterLoader(std::unique_ptr<DragonWildsModLoaderBase> newLoader)
    {
        newLoader->AssignDatatableRegistry(m_datatableRegistry);
        newLoader->Setup();

        m_loaders.push_back(std::move(newLoader));
    }

    void DragonWildsMainLoader::InitializeMods(EEngineLifecyclePhase engineLifecyclePhase)
    {
        for (auto& loader : m_loaders)
        {
            PS::StartupTrace::Mark("initialize loader: " + loader->GetModFolderType());
            try { loader->Initialize(engineLifecyclePhase); }
            catch (const std::exception& e) {
                PS::StartupTrace::Mark("ERROR initialize: " + loader->GetModFolderType());
                PS::Log<LogLevel::Warning>(STR("[LOADER:{}][DISABLED] Initialization failed: {}. Other loaders continue.\n"), RC::to_generic_string(loader->GetModFolderType()), PS::ToWideSafe(e.what()));
            }
            catch (...) {
                PS::StartupTrace::Mark("ERROR initialize: " + loader->GetModFolderType());
                PS::Log<LogLevel::Warning>(STR("[LOADER:{}][DISABLED] Initialization failed after an unknown error. Other loaders continue.\n"), RC::to_generic_string(loader->GetModFolderType()));
            }
        }
    }

    void DragonWildsMainLoader::LoadMods(EEngineLifecyclePhase engineLifecyclePhase)
    {
        if(engineLifecyclePhase==EEngineLifecyclePhase::GameInstanceInit){
            PS::AssetPreload::Plan<RC::StringType> plan;
            for(auto& loader:m_loaders){
                const auto& kind=loader->GetModFolderType();
                if(!PS::PSConfig::Get()->IsLoaderEnabled(kind))continue;
                if(kind=="recipes")for(const auto& path:static_cast<DragonWildsRecipeModLoader*>(loader.get())->CookedReferencePaths())plan.Add(path);
                if(kind=="assets")for(const auto& path:static_cast<DragonWildsAssetModLoader*>(loader.get())->CookedReferencePaths())plan.Add(path);
            }
            try {PrefetchCookedAssets(plan);}catch(const std::exception& error){PS::Log<LogLevel::Normal>(STR("[PERF][ASSETS] Native prefetch unavailable: {}. Required-asset loading continues normally.\n"),PS::ToWideSafe(error.what()));}
        }
        if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit
            && PS::PSConfig::Get()->IsLoaderEnabled("recipes")) {
            for (auto& loader : m_loaders) if (loader->GetModFolderType() == "recipes")
                try {static_cast<DragonWildsRecipeModLoader*>(loader.get())->PrepareReferences();}
                catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:recipes][PARTIAL] Reference preparation failed: {}. Other loaders continue.\n"),PS::ToWideSafe(error.what()));}
        }
        // Definitions must exist before any mod's property or appearance consumers.
        if(engineLifecyclePhase==EEngineLifecyclePhase::PostEngineInit) {
            for(auto& loader:m_loaders) {
                const auto& kind=loader->GetModFolderType();
                if(kind!="effects" && kind!="niagara")continue;
                IterateModsFolder([&](const fs::path& path,const fs::path::string_type& owner) {
                    loader->Load(path,owner,engineLifecyclePhase);
                });
                try {loader->FinalizeLoad(engineLifecyclePhase);}
                catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL] Definition finalization failed: {}. Other loaders continue.\n"),RC::to_generic_string(kind),PS::ToWideSafe(error.what()));}
            }
        }
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit && m_spawnLoader
            && PS::PSConfig::Get()->IsLoaderEnabled("players"))
        {
            m_spawnLoader->ClearAppearanceSources();
            IterateModsFolder([&](const fs::path& modPath,
                const fs::path::string_type& modName)
            {
                try
                {
                    m_spawnLoader->RegisterAppearanceSource(modPath, modName);
                }
                catch (const std::exception& e)
                {
                    PS::Log<LogLevel::Warning>(
                        STR("[LOADER:players][PARTIAL][MOD:{}] Appearance source skipped: {}. Other sections continue.\n"),
                        modName, PS::ToWideSafe(e.what()));
                }
            });
        }

        IterateModsFolder([&](const fs::path& modPath, const fs::path::string_type& modName)
        {
            try
            {
                const auto identity=ReadModIdentity(modPath,modName);
                bool modSuccessful=true;
                PS::StartupTrace::Mark("load mod: " + RC::to_string(modName));

                for (auto& loader : m_loaders)
                {
                    if(loader->GetModFolderType()=="effects" || loader->GetModFolderType()=="niagara")continue;
                    const auto loaderKind = loader->GetModFolderType();
                    PS::StartupTrace::Mark("load loader begin: " + RC::to_string(modName)
                        + "/" + loaderKind);
                    if (!loader->Load(modPath, modName, engineLifecyclePhase)) {
                        modSuccessful=false;
                        if (loaderKind == "buildings" && m_buildingLoader)
                            m_buildingLoader->RejectCleanup("building section failed for enabled mod '"
                                + RC::to_string(modName) + "'");
                    }
                    PS::StartupTrace::Mark("load loader end: " + RC::to_string(modName)
                        + "/" + loaderKind);
                }
                if(engineLifecyclePhase==EEngineLifecyclePhase::PostEngineInit && modSuccessful) {
                    PS::Log<LogLevel::Normal>(STR("[MOD:{}][OK] '{}' by '{}' loaded.\n"), modName,
                        PS::ToWideSafe(identity.Name.c_str()),PS::ToWideSafe(identity.Author.c_str()));
                } else if(engineLifecyclePhase==EEngineLifecyclePhase::PostEngineInit) {
                    PS::Log<LogLevel::Warning>(STR("[MOD:{}][PARTIAL] One or more sections were skipped; successfully loaded sections remain active.\n"),modName);
                }
            }
            catch (const std::exception& e)
            {
                if(engineLifecyclePhase==EEngineLifecyclePhase::PostEngineInit)
                PS::Log<LogLevel::Warning>(STR("[MOD:{}][PARTIAL] Metadata or discovery failed: {}. Other mods continue.\n"), modName, PS::ToWideSafe(e.what()));
            }
        });

        for (auto& loader : m_loaders) {
            PS::StartupTrace::Mark("finalize loader: " + loader->GetModFolderType());
            try { loader->FinalizeLoad(engineLifecyclePhase); }
            catch (const std::exception& e) {
                if(engineLifecyclePhase==EEngineLifecyclePhase::PostEngineInit)
                PS::StartupTrace::Mark("ERROR finalize: " + loader->GetModFolderType());
                PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL] Finalization failed: {}. Other loaders continue.\n"), RC::to_generic_string(loader->GetModFolderType()), PS::ToWideSafe(e.what()));
            }
            catch (...) {
                if(engineLifecyclePhase==EEngineLifecyclePhase::PostEngineInit)
                PS::StartupTrace::Mark("ERROR finalize: " + loader->GetModFolderType());
                PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL] Finalization failed after an unknown error. Other loaders continue.\n"), RC::to_generic_string(loader->GetModFolderType()));
            }
        }

        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit && m_spawnLoader
            && PS::PSConfig::Get()->IsLoaderEnabled("players"))
        {
            if (PS::PSConfig::Get()->IsLoaderEnabled("nameplates"))
            {
                IterateModsFolder([&](const fs::path& modPath, const fs::path::string_type& modName) {
                    const auto nameplatesPath = PS::ModFolderLayout::ResolveLoaderDirectory(modPath,"nameplates");
                    if (nameplatesPath)try {m_spawnLoader->LoadNameplateDefinitions(*nameplatesPath, modName);}
                    catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:nameplates][PARTIAL][MOD:{}] Section skipped: {}. Other mods continue.\n"),modName,PS::ToWideSafe(error.what()));}
                });
                try {m_spawnLoader->FinalizeNameplateDefinitions();}
                catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:nameplates][PARTIAL] Finalization failed: {}. Other loaders continue.\n"),PS::ToWideSafe(error.what()));}
            }
            IterateModsFolder([&](const fs::path& modPath,
                const fs::path::string_type& modName)
            {
                const auto playersPath = PS::ModFolderLayout::ResolveLoaderDirectory(modPath,"players");
                if (!playersPath) return;
                try
                {
                    m_spawnLoader->LoadPlayerRules(*playersPath, modName);
                }
                catch (const std::exception& e)
                {
                    PS::Log<LogLevel::Warning>(
                        STR("[LOADER:players][PARTIAL][MOD:{}] Section skipped: {}. Other sections continue.\n"),
                        modName, PS::ToWideSafe(e.what()));
                }
            });
            try {m_spawnLoader->FinalizePlayerRules();}
            catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:players][PARTIAL] Finalization failed: {}. Other loaders continue.\n"),PS::ToWideSafe(error.what()));}
        }
        if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit
            && PS::PSConfig::Get()->GetSettings().advancedRuntime
            && PS::PSConfig::Get()->GetSettings().diagnostics.persistenceLedger)
        {
            try
            {
                const auto output = PS::HostServices::ReferencesDirectory()
                    / "PersistenceLedger.json";
                PS::PersistenceDiagnostics::Write(
                    GetModsPath(), m_orderedMods, output);
                PS::Log<LogLevel::Normal>(
                    STR("Persistence diagnostic ledger written to '{}'. It is not a cleanup authority.\n"),
                    output.native());
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(
                    STR("Persistence diagnostic ledger was not written: {}\n"),
                    PS::ToWideSafe(error.what()));
            }
        }
    }

    std::filesystem::path DragonWildsMainLoader::GetModsPath()
    {
        static auto modsPath = fs::path(PS::HostServices::WorkingDirectory()) / "Mods" / "RuneSchema" / "mods";
        return modsPath;
    }

    void DragonWildsMainLoader::GetPakFolders(const TCHAR* CmdLine, TArray<FString>* OutPakFolders)
    {
        GetPakFolders_Hook.call(CmdLine, OutPakFolders);

        try
        {
            UnrealOffsets::InitializeGMalloc();
        }
        catch (const std::exception& e)
        {
            PS::Log<LogLevel::Error>(STR("Failed to initialize GMalloc early: {}\n"), PS::ToWideSafe(e.what()));
            PS::Log<LogLevel::Error>(STR("RuneSchema won't be able to load paks from RuneSchema/plugins or RuneSchema/mods.\n"));
            return;
        }

        const auto runeSchemaRoot=fs::path(PS::HostServices::WorkingDirectory())/"Mods"/"RuneSchema";
        std::vector<fs::path> pakRoots;
        try {
            for(const auto& plugin:PS::PluginCatalog::Discover(runeSchemaRoot/"plugins")) {
                if(!plugin.Enabled)continue;
                for(const auto& package:PS::PluginCatalog::PakDirectories(plugin))pakRoots.push_back(package);
            }
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR(
                "Plugin pak order rejected; no plugin PAK directories were added (fail-closed): {}\n"),
                PS::ToWideSafe(error.what()));
        }
        const auto modsRoot=GetModsPath();std::vector<RC::StringType> discovered;
        PS::MountedModRegistryOwners::Reset();
        if(fs::is_directory(modsRoot))for(const auto& entry:fs::directory_iterator(modsRoot)) {
            if(!entry.is_directory()||entry.is_symlink())continue;
            std::error_code scanError;
            const auto paks=PS::ModFolderLayout::ResolveLoaderDirectory(entry.path(),PS::ModFolderLayout::PakDirectory);
            const auto logicMods=PS::ModFolderLayout::ResolveLoaderDirectory(entry.path(),PS::ModFolderLayout::LogicModDirectory);
            const bool legacy=PS::ModFolderLayout::ContainsLegacyPakContent(entry.path(),scanError);
            if((paks.has_value()||logicMods.has_value()||legacy)&&!scanError)discovered.push_back(entry.path().filename().native());
        }
        try {for(const auto& name:ModLoadOrder::Resolve(modsRoot,discovered)) {
            const auto modRoot=modsRoot/name;
            auto readDirectories=PS::ModFolderLayout::PakReadDirectories(modRoot);
            if(readDirectories.empty()) {
                std::error_code legacyError;
                if(PS::ModFolderLayout::ContainsLegacyPakContent(modRoot,legacyError)&&!legacyError)
                    readDirectories.push_back(modRoot);
            }
            for(const auto& directory:readDirectories)pakRoots.push_back(directory);
            const auto owner=RC::to_string(name);
            if(!readDirectories.empty())
                PS::Log<LogLevel::Normal>(STR("[PAK-MOUNT][REGISTERED][MOD:{}] {} concrete container director{} registered for Unreal startup.\n"),
                    name,readDirectories.size(),readDirectories.size()==1?STR("y"):STR("ies"));
            if(!PS::MountedModRegistryOwners::Remember(owner))
                PS::Log<LogLevel::Verbose>(STR("Mod folder '{}' cannot use conventional cooked-registry fallback; use Asset Registry metadata or an underscore-only folder name.\n"),name);
        }}
        catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR(
                "Mod pak order rejected; no mod PAK directories were added (fail-closed): {}\n"),
                PS::ToWideSafe(error.what()));
        }
        std::unordered_set<std::wstring> registered;
        size_t addedPakDirectories = 0;
        for (const auto& pakRoot : pakRoots) {
            if(!fs::is_directory(pakRoot))continue;
            const auto canonical=fs::weakly_canonical(pakRoot).wstring();if(!registered.emplace(canonical).second)continue;
            const auto absolute = pakRoot.native();
            const auto withSuffix = std::format(STR("{}/"), RC::to_generic_string(absolute));
            OutPakFolders->Add(FString(withSuffix.c_str()));
            ++addedPakDirectories;
        }
        if (addedPakDirectories)
            PS::Log<LogLevel::Verbose>(STR("Added {} ordered RuneSchema .pak read director{}.\n"),
                addedPakDirectories, addedPakDirectories == 1 ? STR("y") : STR("ies"));
    }

    void DragonWildsMainLoader::OnDataTableSerialized(RC::Unreal::UDataTable* This, RC::Unreal::FArchive* Archive)
    {
        DatatableSerialize_Hook.call(This, Archive);

        for (auto& Callback : DatatableSerializeCallbacks)
        {
            Callback(This);
        }
    }

    void DragonWildsMainLoader::OnGameInstanceInit(RC::Unreal::UObject* This)
    {
        GameInstanceInit_Hook.call(This);

        for (auto& Callback : GameInstanceInitCallbacks)
        {
            Callback(This);
        }
    }
}
