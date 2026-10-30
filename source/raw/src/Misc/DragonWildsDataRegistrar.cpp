#include "Utility/NativeFunctionHook.h"
#include <Windows.h>
#include <chrono>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <array>
#include <fstream>
#include <filesystem>
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
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftClassPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Utility/Logging.h"
#include "Utility/StartupTrace.h"
#include "Utility/StructuredDiagnosticB3C.h"
#include "Utility/Config.h"
#include "Core/ConfigFiles.h"
#include "Core/CharacterSaveEditor.h"
#include "Core/CookedPakRegistryManifest.h"
#include "Core/CookedEnumContract.h"
#include "Core/HelpyCacheSources.h"
#include "Core/RegistryEnrollment.h"
#include "Core/NetworkIdentityPlan.h"
#include "Core/CacheBundleIO.h"
#include "Core/RegistrationDiagnostics.h"
#include "Core/NetworkIdentityState.h"
#include "Loader/AssetProvenance.h"
#include "Loader/AssetAuthoringMetadata.h"
#include "Loader/QuestNativeRegistry.h"
#include "Core/RegistryProvenance.h"
#include "Core/SaveCleanup.h"
#include "Core/WorldCleanupPolicy.h"
#include "Core/LoadProtectionNotice.h"
#include "Core/SaveRegistrySnapshot.h"
#include "Core/HelpySourceCache.h"
#include "Core/ItemSource.h"
#include "Runtime/Storefront.h"
#include "Runtime/HostServices.h"
#include "Runtime/NativePersistenceRecovery.h"
#include "Core/CharacterLoadAudit.h"
#include "Misc/DragonWildsDataRegistrar.h"
#include "Loader/EquipmentWearableBridge.h"
#include "Unreal/UAssetRegistryHelpers.hpp"
#include "Unreal/UAssetRegistry.hpp"
#include "Unreal/FAssetData.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        constexpr std::size_t CharacterSaveLimit = 8 * 1024 * 1024;
        constexpr std::size_t CharacterBackupRetention = 5;

        struct NetworkRegistryAudit {
            bool Valid = false;
            std::size_t Count = 0;
            std::unordered_set<UObject*> Objects;
            std::vector<UObject*> OrderedObjects;
            std::vector<std::string> OrderedPaths;
            std::string Fingerprint;
            std::string Detail;
        };

        NetworkRegistryAudit AuditNetworkRegistry(UObject* subsystem)
        {
            NetworkRegistryAudit result;
            if (!subsystem || !subsystem->GetClassPrivate())
            {
                result.Detail = "subsystem is unavailable";
                return result;
            }
            auto* reverseProperty = CastField<FMapProperty>(
                PropertyHelper::GetPropertyByName(
                    subsystem->GetClassPrivate(), TEXT("DataToNetIdMap")));
            auto* arrayProperty = CastField<FArrayProperty>(
                PropertyHelper::GetPropertyByName(
                    subsystem->GetClassPrivate(), TEXT("NetIdToData")));
            if (!reverseProperty || !arrayProperty)
            {
                result.Detail = "forward or reverse network registry is unavailable";
                return result;
            }
            auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
            if (!array || array->Num() < 0
                || static_cast<std::size_t>(array->Num()) > PS::NetworkIdentityPlan::Capacity)
            {
                result.Detail = "network registry array has an invalid size";
                return result;
            }

            UECustom::FScriptMapHelper reverse(reverseProperty,
                reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
            FScriptArrayHelper entries(arrayProperty, array);
            std::vector<std::string> orderedPaths;
            orderedPaths.reserve(static_cast<std::size_t>(entries.Num()));
            std::unordered_set<UObject*> seen;
            for (int32 index = 0; index < entries.Num(); ++index)
            {
                UObject* object = nullptr;
                std::memcpy(&object, entries.GetRawPtr(index), sizeof(object));
                if (!object || !seen.emplace(object).second)
                {
                    result.Detail = "network registry contains a null or duplicate object";
                    return result;
                }
                auto* stored = reverse.FindValue(&object);
                std::uint16_t reverseId = 0;
                if (!stored)
                {
                    result.Detail = "network registry reverse entry is missing";
                    return result;
                }
                std::memcpy(&reverseId, stored, sizeof(reverseId));
                if (reverseId != static_cast<std::uint16_t>(index))
                {
                    result.Detail = "network registry forward/reverse IDs disagree";
                    return result;
                }
                orderedPaths.push_back(RC::to_string(object->GetPathName()));
                result.OrderedObjects.push_back(object);
            }

            std::size_t reverseCount = 0;
            bool reverseValid = true;
            reverse.ForEachPair([&](void* keyPtr, void* valuePtr) {
                ++reverseCount;
                UObject* object = nullptr;
                std::uint16_t id = 0;
                std::memcpy(&object, keyPtr, sizeof(object));
                std::memcpy(&id, valuePtr, sizeof(id));
                if (!object || id >= entries.Num())
                {
                    reverseValid = false;
                    return;
                }
                UObject* roundTrip = nullptr;
                std::memcpy(&roundTrip, entries.GetRawPtr(id), sizeof(roundTrip));
                reverseValid = reverseValid && roundTrip == object;
            });
            if (!reverseValid || reverseCount != orderedPaths.size())
            {
                result.Detail = "network registry reverse map has extra or invalid entries";
                return result;
            }
            result.Valid = true;
            result.Count = orderedPaths.size();
            result.Objects = std::move(seen);
            result.Fingerprint = PS::NetworkIdentityPlan::Fingerprint(orderedPaths);
            result.OrderedPaths = std::move(orderedPaths);
            return result;
        }

        struct CharacterText {
            std::string Utf8;
            bool Utf16Le = false;
        };

        CharacterText DecodeCharacterText(const std::string& bytes)
        {
            if (bytes.size() < 2
                || static_cast<unsigned char>(bytes[0]) != 0xff
                || static_cast<unsigned char>(bytes[1]) != 0xfe)
                return {bytes, false};
            if ((bytes.size() - 2) % sizeof(wchar_t))
                throw std::runtime_error("UTF-16 character save has an incomplete code unit");
            std::wstring wide((bytes.size() - 2) / sizeof(wchar_t), L'\0');
            std::memcpy(wide.data(), bytes.data() + 2,
                wide.size() * sizeof(wchar_t));
            const auto length = WideCharToMultiByte(CP_UTF8,
                WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()),
                nullptr, 0, nullptr, nullptr);
            if (length <= 0) throw std::runtime_error(
                "UTF-16 character save could not be decoded");
            std::string utf8(length, '\0');
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                    wide.data(), static_cast<int>(wide.size()), utf8.data(),
                    length, nullptr, nullptr) != length)
                throw std::runtime_error("UTF-16 character save decoding changed");
            return {std::move(utf8), true};
        }

        std::string EncodeCharacterText(const nlohmann::json& document,
            bool utf16Le)
        {
            auto text = document.dump(1, '\t', false,
                nlohmann::json::error_handler_t::strict);
            if (!utf16Le) return text;
            const auto length = MultiByteToWideChar(CP_UTF8,
                MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                nullptr, 0);
            if (length <= 0) throw std::runtime_error(
                "Clean character save could not be encoded as UTF-16");
            std::wstring wide(length, L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                    text.data(), static_cast<int>(text.size()), wide.data(),
                    length) != length)
                throw std::runtime_error("Clean character save encoding changed");
            std::string encoded("\xff\xfe", 2);
            encoded.append(reinterpret_cast<const char*>(wide.data()),
                wide.size() * sizeof(wchar_t));
            return encoded;
        }

        nlohmann::json ParseCharacterText(const std::string& bytes)
        {
            const auto decoded = DecodeCharacterText(bytes);
            return nlohmann::json::parse(decoded.Utf8, nullptr, true, true);
        }

        std::string LowerAscii(std::string value)
        {
            std::ranges::transform(value, value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        std::unordered_set<std::string> MountedModRootNames()
        {
            std::unordered_set<std::string> roots;
            const auto mods = PS::HostServices::WorkingDirectory()
                / L"Mods" / L"RuneSchema" / L"mods";
            std::error_code error;
            if (!std::filesystem::is_directory(mods, error)) return roots;
            for (std::filesystem::recursive_directory_iterator iterator(
                    mods, std::filesystem::directory_options::skip_permission_denied,
                    error), end;
                iterator != end && !error; iterator.increment(error))
            {
                const auto& entry = *iterator;
                if (entry.is_directory(error))
                {
                    roots.insert(LowerAscii(
                        entry.path().filename().string()));
                    continue;
                }
                if (!entry.is_regular_file(error)
                    || LowerAscii(entry.path().extension().string()) != ".pak")
                    continue;
                auto stem = LowerAscii(entry.path().stem().string());
                if (stem.ends_with("_p")) stem.resize(stem.size() - 2);
                if (!stem.empty()) roots.insert(std::move(stem));
            }
            return roots;
        }

        bool IsMountedModPackage(const std::string& package,
            const std::unordered_set<std::string>& roots)
        {
            const auto lower = LowerAscii(package);
            if (lower.starts_with("/game/mods/")
                || lower.starts_with("/game/runeschema/"))
                return true;
            if (lower.size() < 3 || lower.front() != '/') return false;
            const auto slash = lower.find('/', 1);
            if (slash == std::string::npos) return false;
            return roots.contains(lower.substr(1, slash - 1));
        }

        bool IsPersistenceAssetClass(const std::string& assetClass)
        {
            static const std::unordered_set<std::string> classes{
                "ItemData", "RecipeData", "QuestData",
                "JournalEntryWorldData", "DominionSpellData",
                "UtilitySpellData", "HeldEquipmentEffectData"
            };
            return classes.contains(assetClass);
        }

        std::string AssetClassName(FAssetData& asset)
        {
            auto value = RC::to_string(
                asset.AssetClassPath().GetAssetName().ToString());
            // Modern UE builds retain the legacy AssetClass FName but can
            // expose an empty AssetClassPath through compatibility wrappers.
            if (value.empty() || value == "None")
                value = RC::to_string(asset.AssetClass().ToString());
            const auto separator = value.find_last_of("./:");
            if (separator != std::string::npos)
                value.erase(0, separator + 1);
            return value;
        }

        std::filesystem::path LocalCharacterSaveDirectory()
        {
            const auto required = GetEnvironmentVariableW(
                L"LOCALAPPDATA", nullptr, 0);
            if (!required) throw std::runtime_error("LOCALAPPDATA is unavailable");
            std::vector<wchar_t> value(required);
            if (GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), required)
                    + 1 != required)
                throw std::runtime_error("LOCALAPPDATA changed while it was read");
            return std::filesystem::path(value.data()) / L"RSDragonwilds"
                / L"Saved" / L"SaveCharacters";
        }

        struct CharacterBackup {
            std::filesystem::path Path;
            std::filesystem::file_time_type Modified{};
        };

        std::vector<CharacterBackup> OwnedCharacterBackups(
            const std::filesystem::path& source)
        {
            std::vector<CharacterBackup> backups;
            const auto parent = source.parent_path();
            const auto prefix = source.filename().wstring()
                + L".runeschema-startup-";
            std::error_code error;
            for (std::filesystem::directory_iterator iterator(
                    parent, std::filesystem::directory_options::skip_permission_denied,
                    error), end;
                iterator != end && !error; iterator.increment(error))
            {
                const auto& entry = *iterator;
                std::error_code entryError;
                if (!entry.is_regular_file(entryError)) continue;
                const auto name = entry.path().filename().wstring();
                if (!name.starts_with(prefix) || !name.ends_with(L".bak"))
                    continue;
                backups.push_back({entry.path(), entry.last_write_time(entryError)});
            }
            std::ranges::sort(backups, [](const auto& left, const auto& right) {
                return left.Modified > right.Modified;
            });
            return backups;
        }

        std::filesystem::path BackupCharacterSave(
            const std::filesystem::path& source, const std::string& original)
        {
            for (const auto& existing : OwnedCharacterBackups(source))
            {
                try
                {
                    if (PS::ConfigFiles::Read(existing.Path, CharacterSaveLimit)
                            == original)
                    {
                        PS::Log<LogLevel::Verbose>(STR(
                            "[PERSISTENCE-BACKUP][REUSED] '{}'.\n"),
                            existing.Path.filename().wstring());
                        return existing.Path;
                    }
                }
                catch (...) {}
            }

            auto backup = source;
            backup += L".runeschema-startup-" + std::to_wstring(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count())
                + L".bak";
            if (!std::filesystem::copy_file(source, backup,
                    std::filesystem::copy_options::none))
                throw std::runtime_error("Character save backup was not created");
            if(PS::ConfigFiles::Read(backup,CharacterSaveLimit)!=original)
                throw std::runtime_error("Character save backup did not verify; original retained");
            return backup;
        }

        void PruneCharacterBackups(const std::filesystem::path& source)
        {
            auto backups = OwnedCharacterBackups(source);
            std::size_t removed = 0;
            for (std::size_t index = CharacterBackupRetention;
                index < backups.size(); ++index)
            {
                std::error_code error;
                if (std::filesystem::remove(backups[index].Path, error)) ++removed;
                else if (error)
                    PS::Log<LogLevel::Warning>(STR(
                        "[PERSISTENCE-BACKUP][RETAINED] Old RuneSchema backup '{}' could not be removed: {}.\n"),
                        backups[index].Path.filename().wstring(),
                        PS::ToWideSafe(error.message()));
            }
            if (removed)
                PS::Log<LogLevel::Normal>(STR(
                    "[PERSISTENCE-BACKUP][PRUNED] removed={} retained={} character='{}'.\n"),
                    removed, std::min(backups.size(), CharacterBackupRetention),
                    source.filename().wstring());
        }

        void ReplaceCharacterSave(const std::filesystem::path& path,
            const std::string& original, const nlohmann::json& clean,
            bool utf16Le)
        {
            const auto encoded = EncodeCharacterText(clean, utf16Le);
            if (ParseCharacterText(encoded) != clean)
                throw std::runtime_error("Clean character save failed pre-write verification");
            if (PS::ConfigFiles::Read(path, CharacterSaveLimit) != original)
                throw std::runtime_error("Character save changed during cleanup");
            const auto backup = BackupCharacterSave(path, original);
            try {
                PS::ConfigFiles::Write(path, encoded);
                if (ParseCharacterText(PS::ConfigFiles::Read(
                        path, CharacterSaveLimit)) != clean)
                    throw std::runtime_error(
                        "Character save failed post-write verification");
                PruneCharacterBackups(path);
            } catch (...) {
                try {
                    if (PS::ConfigFiles::Read(path, CharacterSaveLimit)
                            != original)
                        PS::ConfigFiles::Write(path, original);
                } catch (...) {
                    PS::Log<LogLevel::Error>(STR(
                        "[PERSISTENCE-PRUNER][FATAL-RESTORE] '{}' could not be restored automatically; use backup '{}'.\n"),
                        path.filename().wstring(), backup.filename().wstring());
                }
                throw;
            }
        }
    }

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
        const TCHAR* ExcludedClassPath;
        bool CleanupAuthority;
        const TCHAR* StatusTag;
    } RegistryBindings[] = {
        { TEXT("/Script/Dominion.ItemData"), TEXT("/Script/Dominion.ItemSubsystem"), nullptr, true, TEXT("ITEM") },
        { TEXT("/Script/Dominion.RecipeData"), TEXT("/Script/Dominion.RecipeSubsystem"), nullptr, true, TEXT("RECIPE") },
        { TEXT("/Script/Dominion.QuestData"), TEXT("/Script/Dominion.QuestDataSubsystem"), nullptr, true, TEXT("QUEST") },
        // Combat spells derive directly from DominionSpellData. Utility spells
        // form a derived branch and must never receive combat net IDs as well.
        { TEXT("/Script/Dominion.DominionSpellData"),
            TEXT("/Script/Dominion.CombatSpellDataSubsystem"),
            TEXT("/Script/Dominion.UtilitySpellData"), false,
            TEXT("COMBAT-SPELL") },
        { TEXT("/Script/Dominion.UtilitySpellData"),
            TEXT("/Script/Dominion.UtilitySpellDataSubsystem"), nullptr, false,
            TEXT("UTILITY-SPELL") },
        { TEXT("/Script/Dominion.HeldEquipmentEffectData"),
            TEXT("/Script/Dominion.HeldEquipmentEffectDataSubsystem"), nullptr, false,
            TEXT("EQUIPMENT-EFFECT") },
    };

    static bool IsCharacterJsonLoadFunction(UFunction* function)
    {
        if (!function) return false;
        static const FName playerState(TEXT("ProcessPlayerStateLoad"),FNAME_Add),persistentResult(TEXT("OnPersistentStoreLoadPlayerResult"),FNAME_Add),jsonState(TEXT("LoadStateFromJson"),FNAME_Add),diskResult(TEXT("Server_ReceiveLoadFromDisk"),FNAME_Add);
        const auto name = function->GetFName();
        if(name==diskResult) {
            static auto* native=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerControllerBase:Server_ReceiveLoadFromDisk"));
            return native&&function==native;
        }
        return name==playerState||name==persistentResult||name==jsonState;
    }

    static bool IsGameplayCharacterLoadContext(UObject* context)
    {
        auto* world=context?context->GetWorld():nullptr;
        if(!world||world->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return false;
        auto* field=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(world->GetClassPrivate(),TEXT("GameState")));
        if(!DragonWilds::ReflectedContract::Fits(field,world->GetClassPrivate()->GetPropertiesSize())
            ||field->GetElementSize()!=sizeof(UObject*))return false;
        auto* state=field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(world));
        static auto* gameplayClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr,nullptr,TEXT("/Script/Engine.GameStateBase"));
        const bool gameplay=gameplayClass&&state&&state->IsA(gameplayClass)&&state->GetWorld()==world
            &&!state->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));
        return PS::WorldCleanupPolicy::CharacterEntryEligible(gameplay,world->GetName());
    }

    static bool IsCombatComponentReadyFunction(UFunction* function)
    {
        if (!function) return false;
        static const FName restart(TEXT("ClientRestart"),FNAME_Add),inventory(TEXT("OnInventoryLoadedFromSave"),FNAME_Add),personal(TEXT("OnPersonalInventoryLoadedFromSave"),FNAME_Add);
        const auto name=function->GetFName();
        if(name!=restart&&name!=inventory&&name!=personal)return false;
        const auto path = function->GetPathName();
        return path == TEXT("/Script/Engine.PlayerController:ClientRestart")
            || path == TEXT("/Script/Dominion.DominionPlayerController:OnInventoryLoadedFromSave")
            || path == TEXT("/Script/Dominion.DominionPlayerController:OnPersonalInventoryLoadedFromSave");
    }

    static std::string RegistryFingerprint(
        const PS::SaveCleanup::RegistrySnapshot& snapshot)
    {
        std::string result=std::to_string(snapshot.ItemsComplete)+std::to_string(snapshot.RecipesComplete)+std::to_string(snapshot.QuestsComplete)+std::to_string(snapshot.JournalsComplete)+":"+std::to_string(snapshot.QuestCompletedState.value_or(-1));
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
        for(const auto& [key,binding]:snapshot.Bindings)result+=key+"="+binding.Asset+"|"+binding.Class+"|"+binding.Owner+";";
        result += snapshot.QuestsComplete ? "Q1" : "Q0";
        result += snapshot.JournalsComplete ? "J1" : "J0";
        return result;
    }

    void DragonWildsDataRegistrar::Initialize()
    {
        // Registry mutation is process-startup work, not world lifecycle work.
        // Re-running it while an outgoing world is being destroyed can touch
        // stale subsystem maps and destabilize menu -> world re-entry.
        if (m_initialized) return;

        const auto began=std::chrono::steady_clock::now();
        m_pruner.PrepareForStartup();
        m_registrationThread=GetCurrentThreadId();
        if (!ResolveBindings()) return;

        const auto& combat = PS::PSConfig::Get()->GetSettings().combatFallback;
        PS::Log<LogLevel::Normal>(STR(
            "[COMBAT-REGISTRY][FALLBACK-SETTINGS] enabled={} additional_weapons={} manifest_melee={} ranged_equipment={} initial_world_mutation={}; persistence registries are unaffected.\n"),
            combat.enabled, combat.additionalWeapons, combat.manifestMelee,
            combat.rangedEquipment, combat.initialWorldMutation);
        PreloadMountedPersistenceAssets();
        const auto discovered=std::chrono::steady_clock::now();
        PS::StartupTrace::Mark("data registrar discovery complete");

        InstallHooks();
        m_initialized = true;

        RegisterAll();
        const auto registered=std::chrono::steady_clock::now();
        PS::StartupTrace::Mark("data registrar registration complete");
        // Capture twice during this single startup boundary so only an
        // identical, settled registry can authorize pruning. Nothing refreshes
        // or mutates the registries on later menu/world transitions.
        RegisterAll();
        const auto confirmed=std::chrono::steady_clock::now();
        PS::StartupTrace::Mark("data registrar confirmation complete");
        EquipmentWearableBridge::ReportDiagnostics();
        PS::NetworkIdentityState::FinishStartupBatch();
        PS::Log<LogLevel::Normal>(STR(
            "[SAVE-CLEANUP][ENTRY-SCOPED] Character cleanup waits for world entry; preview/selection does not authorize cleanup.\n"));
        const auto cleaned=std::chrono::steady_clock::now();
        PS::StartupTrace::Mark("data registrar cleanup deferred to world entry");
        const auto milliseconds=[](auto start,auto end){return std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();};
        PS::Log<LogLevel::Normal>(STR(
            "[PERF][REGISTRY] discovery_ms={} registration_ms={} confirmation_ms={} cleanup_ms={} total_ms={}; required startup work only, not total game loading.\n"),
            milliseconds(began,discovered),milliseconds(discovered,registered),
            milliseconds(registered,confirmed),milliseconds(confirmed,cleaned),milliseconds(began,cleaned));
        PS::Log<LogLevel::Normal>(STR(
            "[REGISTRY][LIFECYCLE][SEALED] Startup registration completed once for this game execution; world transitions are read-only.\n"));
    }

    void DragonWildsDataRegistrar::CleanLocalCharacterSaves(std::wstring_view selectedName,std::string_view expectedGuid)
    {
        if(PS::Storefront::IsDedicatedServer()) {
            PS::Log<LogLevel::Normal>(STR(
                "[SAVE-CLEANUP][PROVIDER-DEFERRED] Server character cleanup uses incoming player payloads, not local client character files.\n"));
            return;
        }
        if (PS::Storefront::CurrentNativeLane()
            == PS::Storefront::NativeLane::GamePassNative)
        {
            PS::Log<LogLevel::Normal>(STR(
                "[PERSISTENCE-PRUNER][ENTRY][PROVIDER-DEFERRED] Xbox WGS character files were not edited directly.\n"));
            return;
        }

        const auto registry = PS::SaveCleanup::ReadRegistry();
        if (!registry || !registry->AnyCleanable())
        {
            PS::Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][ENTRY][UNCHANGED] Cleanup skipped: complete stable item and recipe registries were unavailable; no character file was modified. Non-blocking: native loading continues.\n"));
            return;
        }

        std::size_t scanned = 0;
        std::size_t changed = 0;
        std::size_t removed = 0;
        std::uintmax_t bytes = 0;
        const auto directory = LocalCharacterSaveDirectory();
        if (!std::filesystem::is_directory(directory)) return;
        std::vector<std::filesystem::path> candidates;
        if(!selectedName.empty()||!expectedGuid.empty()) {
            if(!PS::WorldCleanupPolicy::LocalSlotEligible(selectedName,false)
                ||PS::CharacterSaveEditor::Guid(std::string(expectedGuid)).empty())
                throw std::runtime_error("Selected character name/GUID was not verified");
            const auto target=directory/(std::wstring(selectedName)+L".json");
            if(!std::filesystem::exists(target))return;
            if(std::filesystem::is_symlink(target)||!std::filesystem::is_regular_file(target))
                throw std::runtime_error("Selected character is not a regular save file");
            candidates.push_back(target);
        }else for(const auto& entry:std::filesystem::directory_iterator(directory)) {
            if(entry.is_regular_file()&&!entry.is_symlink()&&entry.path().extension()==L".json")
                candidates.push_back(entry.path());
            if(candidates.size()>64)throw std::runtime_error("Character save directory exceeds cleanup limits");
        }
        for (const auto& path : candidates)
        {
            if (++scanned > 64 || (bytes += std::filesystem::file_size(path)) > 64 * 1024 * 1024)
                throw std::runtime_error(
                    "Character save directory exceeds safe entry cleanup limits");
            try
            {
                const auto original = PS::ConfigFiles::Read(
                    path, CharacterSaveLimit);
                const auto decoded = DecodeCharacterText(original);
                const auto source = nlohmann::json::parse(
                    decoded.Utf8, nullptr, true, true);
                if (PS::SaveCleanup::ClassifyCharacterDocument(source)
                    != PS::SaveCleanup::CharacterDocumentKind::Gameplay)
                    continue;
                if(!expectedGuid.empty()&&PS::CharacterSaveEditor::Identity(source)
                    !=PS::CharacterSaveEditor::Guid(std::string(expectedGuid)))
                    throw std::runtime_error("Selected character GUID differs from the save file");
                // Entry still precedes world-dependent quest registration;
                // those adapters validate quests/journals after world readiness.
                auto startupRegistry = *registry;
                startupRegistry.QuestsComplete = false;
                startupRegistry.JournalsComplete = false;
                auto plan = PS::SaveCleanup::PlanAvailable(source,startupRegistry);
                for(const auto& skipped:plan.Skipped)PS::Log<LogLevel::Verbose>(STR("[PERSISTENCE-PRUNER][ENTRY][SECTION-SKIPPED] {}: {}. Other sections continue.\n"),
                    PS::ToWideSafe(skipped.at("Section").get<std::string>()),PS::ToWideSafe(skipped.at("Reason").get<std::string>()));
                if (plan.Removed.empty()) continue;
                ReplaceCharacterSave(path, original, plan.Save,
                    decoded.Utf16Le);
                if(source.contains("meta_data")&&source["meta_data"].is_object())PS::LoadProtectionNotice::Record(source["meta_data"].value("char_guid",std::string{}),plan.Removed.size(),0,false);
                ++changed;
                removed += plan.Removed.size();
                for (const auto& row : plan.Removed)
                    PS::Log<LogLevel::Verbose>(STR(
                        "[PERSISTENCE-PRUNER][ENTRY][ORPHAN-REMOVED] {} '{}' from '{}'.\n"),
                        PS::ToWideSafe(row.value("Kind", std::string("Unknown")).c_str()),
                        PS::ToWideSafe(row.value("Id", std::string("<unknown>")).c_str()),
                        path.filename().wstring());
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR(
                    "[PERSISTENCE-PRUNER][ENTRY][UNCHANGED] '{}' was not modified: {}.\n"),
                    path.filename().wstring(),
                    PS::ToWideSafe(error.what()));
            }
        }
        if (changed)
            PS::ConsolidatedLog<RC::LogLevel::Normal>(STR(
                "[RuneSchema] [summary] Save cleanup | missing character IDs removed={} | saves updated={}. Valid IDs kept; originals backed up.\n"),
                removed, changed);
        else
            PS::Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][ENTRY][CHECKED] {} character save(s) checked; every applicable persistence ID resolved.\n"),
                scanned);
    }

    bool DragonWildsDataRegistrar::ResolveBindings()
    {
        for (const auto& binding : RegistryBindings)
        {
            auto* dataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, binding.DataClassPath);
            auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, binding.SubsystemClassPath);
            auto* excludedClass = binding.ExcludedClassPath
                ? UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, binding.ExcludedClassPath)
                : nullptr;
            if (!dataClass || !subsystemClass)
            {
                PS::LogStructuredFallback<LogLevel::Warning>(
                    PS::StructuredDiagnosticB3C::Report("RS-RGST-008","Warning","Skipped","RegistryLoader",
                    "Native registry binding unavailable","A configured DataClass / Subsystem pair could not be resolved.",{}, {},RC::to_string(binding.DataClassPath),"Resolution"),
                    STR("Registry pair {} -> {} was not found and won't be handled.\n"),
                    binding.DataClassPath, binding.SubsystemClassPath);
                continue;
            }
            if (binding.ExcludedClassPath && !excludedClass)
            {
                PS::LogStructuredFallback<LogLevel::Warning>(
                    PS::StructuredDiagnosticB3C::Report("RS-RGST-009","Warning","Blocked","RegistryLoader",
                    "Native registry exclusion unavailable","Registration is disabled to avoid cross-registry identities.",{}, {},RC::to_string(binding.DataClassPath),"Validation"),
                    STR(
                    "Registry exclusion {} was not found; {} registration is disabled to prevent cross-registry IDs.\n"),
                    binding.ExcludedClassPath, binding.DataClassPath);
                continue;
            }

            m_bindings.push_back({dataClass, subsystemClass, excludedClass,
                binding.CleanupAuthority, binding.StatusTag});
        }

        if (m_bindings.empty())
        {
            PS::LogStructuredFallback<LogLevel::Error>(
                PS::StructuredDiagnosticB3C::Report("RS-RGST-010","Error","Blocked","RegistryLoader",
                "Native registrar unavailable","No supported native registry subsystems could be initialized.",{}, {},"DataRegistrar","Resolution"),
                STR("Unable to initialize the Data Registrar, no registry subsystems were found.\n"));
            return false;
        }

        return true;
    }

    void DragonWildsDataRegistrar::PreloadMountedPersistenceAssets()
    {
        // PAK mounting only exposes package metadata. Unreal does not create a
        // UObject until something references the asset, so GetObjectsOfClass
        // alone misses otherwise-unreferenced mod ItemData/RecipeData/etc.
        // Restrict preloading to persistence classes in RuneSchema-owned mount
        // namespaces; loading every cooked asset caused unacceptable stalls.
        const auto& combat = PS::PSConfig::Get()->GetSettings().combatFallback;
        const auto manifests = PS::CookedPakRegistryManifest::Snapshot();
        for (const auto& manifest : manifests)
        {
            const auto collectAttackLane = [&](const std::string& lane,
                const std::vector<std::string>& paths,
                std::vector<ManifestAttackCollection>& collections) {
                if (paths.empty() || !combat.enabled
                    || (lane == "MeleeAttackClasses" && !combat.manifestMelee)
                    || (lane == "RangedAttackClasses" && !combat.rangedEquipment))
                    return;
                const auto duplicate = std::ranges::find_if(
                    collections, [&](const auto& existing) {
                        return existing.Owner == manifest.Owner
                            && existing.Source == manifest.Source;
                    });
                if (duplicate == collections.end())
                    collections.push_back({manifest.Owner, manifest.Source,
                        lane, paths});
            };
            collectAttackLane("MeleeAttackClasses", manifest.MeleeAttackClasses,
                m_manifestMeleeCollections);
            collectAttackLane("RangedAttackClasses", manifest.RangedAttackClasses,
                m_manifestRangedCollections);

            // Wearable mesh data is a dependency of an item, not a persistence
            // registry entry. Resolve only explicitly declared cooked assets;
            // the game's equipment component remains the hide-map authority.
            if (!manifest.WearableMeshData.empty())
            {
                auto* meshDataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, TEXT("/Script/Dominion.WearableEquipmentMeshData"));
                std::size_t alreadyLoaded = 0;
                std::size_t newlyLoaded = 0;
                std::string failedPath;
                if (!meshDataClass) failedPath = "<native WearableEquipmentMeshData class unavailable>";
                for (const auto& path : manifest.WearableMeshData)
                {
                    if (!failedPath.empty()) break;
                    const auto widePath = PS::ToWideSafe(path.c_str());
                    auto* object = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                        nullptr, nullptr, widePath.c_str(), false);
                    const bool wasLoaded = object != nullptr;
                    if (!object) object = ActorHelper::ResolveObject(widePath);
                    if (!object || !object->IsA(meshDataClass))
                    {
                        failedPath = path;
                        break;
                    }
                    if (wasLoaded) ++alreadyLoaded;
                    else ++newlyLoaded;
                }
                if (!failedPath.empty())
                    PS::Log<LogLevel::Warning>(STR(
                        "[PAK-PRESENTATION][REJECTED] owner='{}' lane='WearableMeshData' unresolved_or_wrong_class='{}'; no equipment hide map was modified.\n"),
                        PS::ToWideSafe(manifest.Owner.c_str()),
                        PS::ToWideSafe(failedPath.c_str()));
                else
                    PS::Log<LogLevel::Normal>(STR(
                        "[PAK-PRESENTATION][RESOLVED] owner='{}' wearable_mesh_data={} newly_loaded={} already_loaded={}; live equipment hide map remains game-owned.\n"),
                        PS::ToWideSafe(manifest.Owner.c_str()),
                        manifest.WearableMeshData.size(), newlyLoaded, alreadyLoaded);
            }

            for (const auto& lane : manifest.AssetLanes)
            {
                // BuildingPieceData uses its catalogue/world registrar, never ItemSubsystem.
                if(lane.Name=="Buildings")continue;
                // Dedicated cooked-only enrollment runs at the existing startup boundary.
                if(PS::NativeDataFamilies::Find(lane.Name))continue;
                // Reflected base maps alone do not verify a subsystem's startup caches or consumers.
                if(lane.Name=="Skills"||lane.Name=="SkillPerks"){
                    const std::string reason=lane.Name=="Skills"?"SkillSubsystem enrollment is blocked until native ESkill consumers, UI order and save/network lifecycle are verified"
                        :"SkillPerkSubsystem enrollment is blocked until native startup/cache and unlock lifecycle are verified";
                    PS::RegistryEnrollment::RecordDeclaration(manifest.Owner,manifest.Source,reason);
                    PS::Log<LogLevel::Warning>(STR("[NATIVE-REGISTRY][UNVERIFIED] owner='{}' lane='{}' declared={}: {}; no skill/perk registry or progress was modified.\n"),
                        PS::ToWideSafe(manifest.Owner),PS::ToWideSafe(lane.Name),lane.Paths.size(),PS::ToWideSafe(reason));
                    continue;
                }
                if(lane.Name=="Enums"){
                    for(const auto& path:lane.Paths)try{
                        auto* object=ActorHelper::ResolveObject(PS::ToWideSafe(path));
                        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Engine.UserDefinedEnum"));
                        if(!object||!type||!object->IsA(type)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization))
                            ||PS::CookedPakRegistryManifest::Detail::Fold(RC::to_string(object->GetPathName()))!=PS::CookedPakRegistryManifest::Detail::Fold(path))
                            throw std::runtime_error("Declared enum must resolve to its exact mod-owned, fully loaded UserDefinedEnum asset");
                        auto* enumeration=static_cast<UEnum*>(object);const auto count=enumeration->NumEnums();
                        if(count<1||count>256)throw std::runtime_error("Cooked enum symbol count exceeds byte bounds");
                        PS::CookedEnumContract::Symbols symbols;for(const auto& pair:enumeration->ForEachName())symbols.emplace_back(RC::to_string(pair.Key.ToString()),pair.Value);
                        if(symbols.size()!=static_cast<size_t>(count))throw std::runtime_error("Cooked enum symbol table changed while reading");
                        const auto fingerprint=PS::CookedEnumContract::Fingerprint(manifest.Owner,path,symbols);
                        const auto key="cooked-enum:"+PS::CookedPakRegistryManifest::Detail::Fold(path);
                        PS::NetworkIdentityState::Publish(key,fingerprint);PS::NetworkIdentityState::PublishOwner(key,manifest.Owner,fingerprint);
                        PS::Log<LogLevel::Normal>(STR("[COOKED-ENUM][VERIFIED] owner='{}' enum='{}' symbols={} fingerprint={}; cooked values retained without append/reindex or native property writes.\n"),
                            PS::ToWideSafe(manifest.Owner),PS::ToWideSafe(path),symbols.size(),PS::ToWideSafe(fingerprint));
                    }catch(const std::exception& error){
                        PS::RegistryEnrollment::RecordDeclaration(manifest.Owner,manifest.Source,error.what());
                        PS::Log<LogLevel::Warning>(STR("[COOKED-ENUM][REJECTED] owner='{}' enum='{}': {}; no enum or registry was modified.\n"),PS::ToWideSafe(manifest.Owner),PS::ToWideSafe(path),PS::ToWideSafe(error.what()));
                    }
                    continue;
                }
                // These lanes use the established journal/lore service for
                // native registration, placement and unlock triggers.
                if(lane.Name=="Journals"||lane.Name=="Lore"){
                    for(const auto& path:lane.Paths)if(manifest.SchemaPaths.contains(PS::CookedPakRegistryManifest::Detail::Fold(path))){
                        bool verified=false;
                        try{
                            auto* entry=ActorHelper::ResolveObject(PS::ToWideSafe(path.c_str()));
                            auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.JournalSubsystem"));
                            auto* subsystem=type?FindSubsystemInstance(type):nullptr;
                            verified=entry&&subsystem&&QuestRegistry::NativeRegistry::ResolvesPersistenceIdentity(subsystem,entry,true);
                        }catch(...){}
                        if(!verified){PS::RegistryEnrollment::Reject(manifest.Source,"schema journal/lore identity is not registered: "+path);
                            PS::LogStructuredFallback<LogLevel::Error>(
                                PS::StructuredDiagnosticB3C::Report("RS-RGST-011","Error","Blocked","RegistryLoader",
                                "Journal native identity unverified","The cooked journal entry has no verified native persistence identity; cleanup remains deferred.",
                                manifest.Owner,manifest.Source,path,"Validation"),
                                STR("[NATIVE-REGISTRY][REJECTED] owner='{}' lane='{}' asset='{}': schema entry has no verified native journal identity; cleanup deferred.\n"),
                                PS::ToWideSafe(manifest.Owner),PS::ToWideSafe(lane.Name),PS::ToWideSafe(path));}
                    }
                    continue;
                }
                const auto binding = std::ranges::find_if(m_bindings,
                    [&](const RegistryBinding& value) {
                        const auto tag = RC::to_string(value.StatusTag);
                        if (lane.Name == "Items") return tag == "ITEM";
                        if (lane.Name == "Recipes") return tag == "RECIPE";
                        if (lane.Name == "Quests") return tag == "QUEST";
                        if (lane.Name == "CombatSpells") return tag == "COMBAT-SPELL";
                        if (lane.Name == "UtilitySpells") return tag == "UTILITY-SPELL";
                        if (lane.Name == "EquipmentEffects") return tag == "EQUIPMENT-EFFECT";
                        return false;
                    });
                if (binding == m_bindings.end())
                {
                    if(lane.Name=="Items"||lane.Name=="Recipes"||lane.Name=="Quests")
                        if(const auto kind=PS::RegistryEnrollment::KindForLane(lane.Name))PS::RegistryEnrollment::Reject(manifest.Source,"required native registry binding is unavailable: "+lane.Name,kind);
                    PS::LogStructuredFallback<LogLevel::Error>(
                        PS::StructuredDiagnosticB3C::Report("RS-RGST-012","Error","Blocked","RegistryLoader",
                        "Cooked registry lane unavailable","Authoritative live registry binding is unavailable; nothing in this lane was registered.",
                        manifest.Owner,manifest.Source,lane.Name,"Validation"),
                        STR(
                        "[PAK-REGISTRY][REJECTED] owner='{}' lane='{}': authoritative live registry binding is unavailable; no registry was modified.\n"),
                        PS::ToWideSafe(manifest.Owner.c_str()),
                        PS::ToWideSafe(lane.Name.c_str()));
                    continue;
                }

                std::vector<UObject*> resolvedObjects;
                bool valid = true;
                std::string failedPath;
                for (const auto& path : lane.Paths)
                {
                    const auto widePath = PS::ToWideSafe(path.c_str());
                    auto* object = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                        nullptr, nullptr, widePath.c_str(), false);
                    if (!object) object = ActorHelper::ResolveObject(widePath);
                    if (!object || !object->IsA(binding->DataClass)
                        || (binding->ExcludedClass && object->IsA(binding->ExcludedClass))
                        || PS::AssetMetadata::IsIncomplete(object))
                    {
                        valid = false;
                        failedPath = path;
                        break;
                    }
                    const auto origin=PS::AssetProvenance::Lookup(object);
                    if(origin.is_object()&&(!origin.value("Registered",false)||origin.value("LastCloneDefinitionErrors",0)!=0)){
                        valid=false;failedPath=path;break;
                    }
                    resolvedObjects.push_back(object);
                }
                if (!valid)
                {
                    if(lane.Name=="Items"||lane.Name=="Recipes"||lane.Name=="Quests")
                        if(const auto kind=PS::RegistryEnrollment::KindForLane(lane.Name))PS::RegistryEnrollment::Reject(manifest.Source,"unresolved or wrong-class "+lane.Name+": "+failedPath,kind);
                    m_rejectedManifestAssets.insert(
                        resolvedObjects.begin(), resolvedObjects.end());
                    PS::LogStructuredFallback<LogLevel::Error>(
                        PS::StructuredDiagnosticB3C::Report("RS-RGST-013","Error","Blocked","RegistryLoader",
                        "Cooked registry lane rejected","One or more declared objects are unresolved or of the wrong class; the lane is atomic.",
                        manifest.Owner,manifest.Source,failedPath,"Validation"),
                        STR(
                        "[PAK-REGISTRY][REJECTED] owner='{}' lane='{}' unresolved_or_wrong_class='{}': the lane is atomic and no declared object from it will be registered.\n"),
                        PS::ToWideSafe(manifest.Owner.c_str()),
                        PS::ToWideSafe(lane.Name.c_str()),
                        PS::ToWideSafe(failedPath.c_str()));
                    continue;
                }
                PS::Log<LogLevel::Normal>(STR(
                    "[PAK-REGISTRY][VALIDATED] owner='{}' lane='{}' declared={}; live registry remains authoritative.\n"),
                    PS::ToWideSafe(manifest.Owner.c_str()),
                    PS::ToWideSafe(lane.Name.c_str()), lane.Paths.size());
            }
        }

        TArray<FAssetData> assets;
        try
        {
            // UE4SS's metadata-readiness flag is an inline implementation
            // detail. Reading it from a separately linked mod DLL observes a
            // different module-local copy, so use the live registry call as
            // the readiness probe instead.
            auto interface = UAssetRegistryHelpers::GetAssetRegistry();
            auto* registry = static_cast<UAssetRegistry*>(interface.ObjectPointer);
            if (!registry || !registry->GetAllAssets(assets, true)
                || assets.Num() < 0 || assets.Num() > 262144)
                throw std::runtime_error(
                    "mounted registry unavailable or outside the 262144-record safety bound");
        }
        catch (const std::exception& error)
        {
            PS::LogStructuredFallback<LogLevel::Warning>(
                PS::StructuredDiagnosticB3C::Report("RS-RGST-014","Warning","Partial","RegistryLoader",
                "Cooked asset discovery unavailable",error.what(),{}, {},"MountedAssetRegistry","Discovery"),
                STR(
                "[REGISTRY][PAK-DISCOVERY][SKIPPED] Live Asset Registry query failed: {}; already-loaded PAK assets will still be registered.\n"),
                PS::ToWideSafe(error.what()));
            return;
        }
        catch (...)
        {
            PS::LogStructuredFallback<LogLevel::Warning>(
                PS::StructuredDiagnosticB3C::Report("RS-RGST-014","Warning","Partial","RegistryLoader",
                "Cooked asset discovery unavailable","An unknown exception interrupted enumeration; already loaded assets remain available.",
                {},{},"MountedAssetRegistry","Discovery"),
                STR(
                "[REGISTRY][PAK-DISCOVERY][SKIPPED] Live Asset Registry query failed with an unknown error; already-loaded PAK assets will still be registered.\n"));
            return;
        }

        const auto roots = MountedModRootNames();
        // A single large content pack can legitimately contain ten thousand
        // ItemData assets. Stay below the uint16 network-ID ceiling while
        // leaving enough headroom for the native registry and other mods.
        constexpr std::size_t MaxPersistencePreloads = 32768;
        constexpr std::size_t PersistenceLoadTranche = 512;
        std::vector<FAssetData*> candidates;
        candidates.reserve(1024);
        std::unordered_set<std::string> candidatePaths;
        std::unordered_map<std::string, std::size_t> mountedClassCounts;
        std::unordered_map<std::string, std::size_t> registryRootCounts;
        std::size_t mountedRecords = 0;
        std::size_t unreadableRecords = 0;
        for (auto& asset : assets)
        {
            try
            {
                const auto package = RC::to_string(asset.PackageName().ToString());
                if (package.size() > 1 && package.size() <= 2048 && package.front() == '/')
                {
                    const auto slash = package.find('/', 1);
                    const auto root = LowerAscii(package.substr(1,
                        slash == std::string::npos ? std::string::npos : slash - 1));
                    if (!root.empty() && (registryRootCounts.contains(root)
                            || registryRootCounts.size() < 256))
                        ++registryRootCounts[root];
                }
                if (!IsMountedModPackage(package, roots)) continue;
                ++mountedRecords;
                auto assetClass = AssetClassName(asset);
                if (assetClass.empty()) assetClass = "<missing>";
                ++mountedClassCounts[assetClass];
                if (!IsPersistenceAssetClass(assetClass)) continue;
                const auto name = RC::to_string(asset.AssetName().ToString());
                if (name.empty() || !candidatePaths.emplace(package + "." + name).second)
                    continue;
                candidates.push_back(&asset);
            }
            catch (...)
            {
                ++unreadableRecords;
                continue;
            }
            if (candidates.size() > MaxPersistencePreloads)
            {
                PS::LogStructuredFallback<LogLevel::Error>(
                    PS::StructuredDiagnosticB3C::Report("RS-RGST-015","Error","Blocked","RegistryLoader",
                    "Cooked discovery safety cap exceeded","Candidate persistence assets exceeded the 32768-entry safety limit; no partial catalog was loaded.",
                    {},{},"MountedAssetRegistry","Validation"),
                    STR(
                    "[REGISTRY][PAK-DISCOVERY][ABORTED] {} mod persistence assets exceed the safe ceiling of {}; no partial catalog was loaded.\n"),
                    candidates.size(), MaxPersistencePreloads);
                return;
            }
        }

        const auto discovered = candidates.size();
        if (!discovered)
        {
            std::vector<std::pair<std::string, std::size_t>> classes(
                mountedClassCounts.begin(), mountedClassCounts.end());
            std::ranges::sort(classes, [](const auto& left, const auto& right) {
                return left.second != right.second
                    ? left.second > right.second : left.first < right.first;
            });
            std::string distribution;
            for (std::size_t index = 0; index < std::min<std::size_t>(classes.size(), 12); ++index)
            {
                if (!distribution.empty()) distribution += ", ";
                distribution += classes[index].first + "=" + std::to_string(classes[index].second);
            }
            if (distribution.empty()) distribution = "<none>";
            std::vector<std::pair<std::string, std::size_t>> registryRoots(
                registryRootCounts.begin(), registryRootCounts.end());
            std::ranges::sort(registryRoots, [](const auto& left, const auto& right) {
                return left.second != right.second
                    ? left.second > right.second : left.first < right.first;
            });
            std::string rootDistribution;
            for (std::size_t index = 0; index < std::min<std::size_t>(registryRoots.size(), 16); ++index)
            {
                if (!rootDistribution.empty()) rootDistribution += ", ";
                rootDistribution += registryRoots[index].first + "="
                    + std::to_string(registryRoots[index].second);
            }
            if (rootDistribution.empty()) rootDistribution = "<none>";
            PS::Log<LogLevel::Verbose>(STR(
                "[REGISTRY][PAK-DISCOVERY][NO-CANDIDATES] registry_records={} mounted_records={} configured_roots={} unreadable={} registry_roots='{}' mounted_classes='{}'.\n"),
                assets.Num(), mountedRecords, roots.size(), unreadableRecords,
                PS::ToWideSafe(rootDistribution.c_str()),
                PS::ToWideSafe(distribution.c_str()));
        }
        std::size_t loaded = 0;
        std::size_t alreadyLoaded = 0;
        std::size_t failed = 0;
        std::size_t failureDetails = 0;
        constexpr std::size_t FailureDetailLimit = 12;
        for (std::size_t trancheStart = 0; trancheStart < candidates.size();
            trancheStart += PersistenceLoadTranche)
        {
            const auto trancheEnd = std::min(candidates.size(),
                trancheStart + PersistenceLoadTranche);
            for (auto index = trancheStart; index < trancheEnd; ++index)
            {
                auto& asset = *candidates[index];
                const auto assetClass = AssetClassName(asset);
                const auto package = RC::to_string(asset.PackageName().ToString());
                const auto name = RC::to_string(asset.AssetName().ToString());
                const auto path = package + "." + name;
                const auto widePath = PS::ToWideSafe(path.c_str());
                auto* object = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                    nullptr, nullptr, widePath.c_str(), false);
                if (object)
                {
                    ++alreadyLoaded;
                    continue;
                }
                object = UAssetRegistryHelpers::GetAsset(asset);
                if (object) ++loaded;
                else
                {
                    ++failed;
                    if (failureDetails++ < FailureDetailLimit)
                        PS::Log<LogLevel::Warning>(STR(
                            "[REGISTRY][PAK-DISCOVERY][UNRESOLVED] class='{}' asset='{}'.\n"),
                            PS::ToWideSafe(assetClass.c_str()),
                            PS::ToWideSafe(path.c_str()));
                }
            }
        }

        if (failureDetails > FailureDetailLimit)
            PS::Log<LogLevel::Warning>(STR(
                "[REGISTRY][PAK-DISCOVERY][UNRESOLVED-SUMMARY] {} additional unresolved assets omitted from the log.\n"),
                failureDetails - FailureDetailLimit);

        PS::Log<LogLevel::Normal>(STR(
            "[REGISTRY][PAK-DISCOVERY][SUMMARY] registry_records={} mounted_records={} roots={} unreadable={} persistence_assets={} tranches={} tranche_size={} newly_loaded={} already_loaded={} unresolved={} verified={}.\n"),
            assets.Num(), mountedRecords, roots.size(), unreadableRecords, discovered,
            (discovered + PersistenceLoadTranche - 1) / PersistenceLoadTranche,
            PersistenceLoadTranche, loaded, alreadyLoaded, failed,
            failed == 0 ? TEXT("true") : TEXT("false"));
    }

    void DragonWildsDataRegistrar::Shutdown()
    {
        m_networkSyncLanes.clear();
        (void)PS::CharacterLoadAudit::End(m_nativePolicyAttempt);
        m_nativePolicyContext=nullptr;m_nativePolicyFunction=nullptr;m_nativePolicyParameters=nullptr;
        if(m_characterJsonPostHook!=Hook::ERROR_ID)Hook::UnregisterCallback(m_characterJsonPostHook);
        m_characterJsonPostHook=Hook::ERROR_ID;
        m_prepareCharacterRegistries={};
        if (m_characterJsonHook != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_characterJsonHook);
        m_characterJsonHook = Hook::ERROR_ID;
        if (m_combatLifecycleHook != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_combatLifecycleHook);
        m_combatLifecycleHook = Hook::ERROR_ID;
        m_registryCandidateFingerprint.clear();
        m_registryCandidatePasses = 0;
        for (auto* attack : m_ownedAdditionalWeaponAttackRoots)
            if (attack && attack->IsRootSet()) attack->ClearRootSet();
        m_ownedAdditionalWeaponAttackRoots.clear();
        m_additionalWeaponAttackClasses.clear();
        for (auto& collection : m_manifestMeleeCollections)
            for (auto* attack : collection.OwnedRoots)
                if (attack && attack->IsRootSet()) attack->ClearRootSet();
        m_manifestMeleeCollections.clear();
        m_manifestRangedCollections.clear();
        m_registeredRangedCollections.clear();
        m_rejectedManifestAssets.clear();
        m_additionalWeaponsReadyReported = false;
        m_additionalWeaponsIncompleteReported = false;
        m_rangedEquipmentReported = false;
        m_rangedEquipmentConflictReported = false;
        m_initialCombatFallbackAttempted = false;
        m_registryStatusReported.clear();
        m_registryWaitingReported.clear();
        m_registrySummaryReported = false;
        PS::NetworkIdentityState::Reset();
        PS::SaveCleanup::PublishRegistry({});
    }

    void DragonWildsDataRegistrar::InstallHooks()
    {
        // Register the filtered UE4SS ProcessEvent callback directly. Do not
        // refresh registries here: this hook consumes the immutable startup
        // snapshot and may run on every character/world load.
        Hook::FCallbackOptions preflightOptions{};
        preflightOptions.OwnerModName = TEXT("RuneSchema");
        preflightOptions.HookName = TEXT("CharacterJsonSavePreflight");
        auto postOptions=preflightOptions;postOptions.HookName=TEXT("CharacterNativePolicyRestore");
        m_characterJsonPostHook=Hook::RegisterProcessEventPostCallback(
            [this](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void* parameters){
                if(!m_nativePolicyContext||source!=m_nativePolicyContext||function!=m_nativePolicyFunction
                    ||parameters!=m_nativePolicyParameters||GetCurrentThreadId()!=m_registrationThread)return;
                m_nativePolicyContext=nullptr;m_nativePolicyFunction=nullptr;m_nativePolicyParameters=nullptr;
                PS::NativePersistenceRecovery::EnsureSessionPolicy(source);
                if(const auto receipt=PS::CharacterLoadAudit::End(m_nativePolicyAttempt)){
                    const auto report=[&]<auto Level>(){PS::Log<Level>(STR("[CHARACTER-LOAD][HANDLER-RETURNED] load={} registry_pruned={} validation_bypasses={} player_state_bypasses={} attribution_ambiguous={}; world entry and persisted save are not yet verified.\n"),receipt->Token,receipt->RegistryPruned,receipt->ValidationBypasses,receipt->PlayerStateBypasses,receipt->Ambiguous);};
                    if(receipt->RegistryPruned||receipt->ValidationBypasses||receipt->PlayerStateBypasses||receipt->Ambiguous)report.template operator()<LogLevel::Normal>();
                    else report.template operator()<LogLevel::Verbose>();
                }
            },postOptions);
        m_characterJsonHook = Hook::RegisterProcessEventPreCallback(
            [this](Hook::TCallbackIterationData<void>&, UObject* source,
                UFunction* function, void* parameters) {
                if (!parameters || m_preflightingCharacterJson
                    || !IsCharacterJsonLoadFunction(function)
                    || !function->GetPathName().starts_with(
                        TEXT("/Script/Dominion.")))
                    return;
                m_preflightingCharacterJson = true;
                try
                {
                    ScrubCharacterJsonBeforeLoad(source, function, parameters);
                }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Error>(STR(
                        "[PERSISTENCE-PRUNER][PREFLIGHT][UNCHANGED] Character JSON was not modified: {}.\n"),
                        PS::ToWideSafe(error.what()));
                }
                catch (...) {}
                m_preflightingCharacterJson = false;
            }, preflightOptions);
        if (m_characterJsonHook != Hook::ERROR_ID)
            PS::Log<LogLevel::Normal>(STR(
                "[PERSISTENCE-PRUNER][REFLECTED-BOUNDARY-READY] Character save preflight enabled through filtered game events.\n"));
        else
            PS::Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][REFLECTED-BOUNDARY-UNAVAILABLE] Character save preflight could not be installed.\n"));

        // The verified melee attack collection is component state, not a
        // persistence registry. Apply its already-resolved ordered classes
        // after a player controller becomes world-ready, scoped to that world.
        // Ranged combat is item-owned. Only the complete quick/full attack
        // data pair discovered from each registered equipment collection is
        // admitted here; shot/action implementation classes are never used.
        // This optional lane gets one attempt in the first gameplay world and
        // never refreshes ItemData, recipes, quests, spells, or the pruning
        // snapshot. Startup CDO registration does not require this hook.
        const auto& combat = PS::PSConfig::Get()->GetSettings().combatFallback;
        if (combat.enabled && combat.initialWorldMutation
            && (combat.additionalWeapons
                || combat.manifestMelee || combat.rangedEquipment))
        {
            Hook::FCallbackOptions combatOptions{};
            combatOptions.OwnerModName = TEXT("RuneSchema");
            combatOptions.HookName = TEXT("CombatComponentWorldReady");
            m_combatLifecycleHook = Hook::RegisterProcessEventPostCallback(
                [this](Hook::TCallbackIterationData<void>&, UObject* source,
                    UFunction* function, void*) {
                    if (!source || m_initialCombatFallbackAttempted
                        || !IsCombatComponentReadyFunction(function))
                        return;
                    auto* world = source->GetWorld();
                    if (!world || world->HasAnyFlags(static_cast<EObjectFlags>(
                            RF_BeginDestroyed | RF_FinishDestroyed))
                        || world->GetPathName().contains(TEXT("L_FrontEnd")))
                        return;
                    // Close the lane before touching live component state so a
                    // nested or later lifecycle callback cannot re-enter it.
                    m_initialCombatFallbackAttempted = true;
                    BootstrapCombatRegistries(world);
                }, combatOptions);
            if (m_combatLifecycleHook != Hook::ERROR_ID)
                PS::Log<LogLevel::Normal>(STR(
                    "[LIFECYCLE][COMBAT-COMPONENT][READY] One-shot first-world melee and ranged fallback installed.\n"));
            else
                PS::Log<LogLevel::Warning>(STR(
                    "[LIFECYCLE][COMBAT-COMPONENT][UNAVAILABLE] Custom melee attack collections cannot attach to new player components.\n"));
        }
        else
            PS::Log<LogLevel::Normal>(STR(
                "[LIFECYCLE][COMBAT-COMPONENT][DISABLED] Live combat component mutation is disabled; startup registration and save preflight remain active.\n"));
    }

    void DragonWildsDataRegistrar::ScrubCharacterJsonBeforeLoad(
        UObject* context, UFunction* function, void* parameters)
    {
        if(GetCurrentThreadId()==m_registrationThread)PS::NativePersistenceRecovery::EnsureSessionPolicy(context);
        // Preview hydration can carry full gameplay JSON; payload shape alone is not an entry permission.
        if(GetCurrentThreadId()!=m_registrationThread||!IsGameplayCharacterLoadContext(context)) {
            if(GetCurrentThreadId()==m_registrationThread&&PS::PSConfig::Get()->GetSettings().advancedLogging) {
                static unsigned reports=0;
                if(reports<16) {
                    ++reports;
                    auto* world=context?context->GetWorld():nullptr;
                    PS::Log<LogLevel::Verbose>(STR("[PERSISTENCE-PRUNER][CONTEXT-DEFERRED] function='{}' context='{}' world='{}'; preview/unverified context did not authorize character cleanup.\n"),
                        function->GetPathName(),context?context->GetClassPrivate()->GetPathName():TEXT("<unavailable>"),world?world->GetName():TEXT("<unavailable>"));
                }
            }
            return;
        }
        // This is targeted, idempotent preparation of already-declared quests,
        // not another RegisterAll()/asset scan. Run it before payload inspection
        // even if the load event precedes InitGameState's ready notification.
        if(m_prepareCharacterRegistries)try{m_prepareCharacterRegistries(context);}
        catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR(
            "[PERSISTENCE-PRUNER][PENDING-IDENTITIES] Quest preparation is incomplete: {}. Only declared pending identities are retained; general cleanup continues.\n"),PS::ToWideSafe(error.what()));}
        if(m_nativePolicyContext)PS::CharacterLoadAudit::Ambiguous();
        const auto auditToken=m_characterJsonPostHook!=Hook::ERROR_ID&&!m_nativePolicyContext&&GetCurrentThreadId()==m_registrationThread?PS::CharacterLoadAudit::Begin():0;
        bool gameplayPayload=false;
        try{gameplayPayload=m_pruner.PruneBeforeCharacterLoad(context,function,parameters);}
        catch(...){if(auditToken)(void)PS::CharacterLoadAudit::End(auditToken);throw;}
        if(!gameplayPayload){if(auditToken)(void)PS::CharacterLoadAudit::End(auditToken);return;}
        if(!auditToken)return;
        m_nativePolicyAttempt=auditToken;
        m_nativePolicyContext=context;m_nativePolicyFunction=function;m_nativePolicyParameters=parameters;
    }

    void DragonWildsDataRegistrar::RegisterDeclaredNativeFamilies()
    {
        for(const auto& manifest:PS::CookedPakRegistryManifest::Snapshot())for(const auto& lane:manifest.AssetLanes){
            const auto* family=PS::NativeDataFamilies::Find(lane.Name);if(!family)continue;
            const auto reportKey="cooked-family:"+manifest.Source+":"+lane.Name;
            try{
                auto* dataType=ActorHelper::ResolveClass(PS::ToWideSafe(family->DataClass));
                auto* subsystemType=ActorHelper::ResolveClass(PS::ToWideSafe(family->Subsystem));
                if(!dataType||!subsystemType)throw std::runtime_error("Native data family classes are unavailable");
                const std::set<RC::StringType> fields{TEXT("DataPaths"),TEXT("AssetNameToDataMap"),TEXT("PersistenceIDToDataMap"),TEXT("InternalNameToDataMap"),TEXT("NetIdToData"),TEXT("DataToNetIdMap")};
                for(auto* field:TFieldRange<FProperty>(subsystemType,EFieldIterationFlags::Default))
                    if(!fields.contains(field->GetName()))throw std::runtime_error("Native family has additional reflected state; a dedicated lifecycle adapter is required");
                std::vector<UObject*> objects;std::vector<PS::NativeDataFamilies::Identity> identities;
                for(const auto& path:lane.Paths){
                    auto* object=ActorHelper::ResolveObject(PS::ToWideSafe(path));
                    if(!object||!object->IsA(dataType)||!object->HasAnyFlags(RF_WasLoaded)||PS::AssetMetadata::IsIncomplete(object)
                        ||PS::CookedPakRegistryManifest::Detail::Fold(RC::to_string(object->GetPathName()))!=PS::CookedPakRegistryManifest::Detail::Fold(path))throw std::runtime_error("Unresolved, incomplete or wrong-class cooked asset: "+path);
                    auto* deleted=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("bSoftDeleted")));
                    auto* migration=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("MigrationDataAsset")));
                    const auto objectSize=object->GetClassPrivate()->GetPropertiesSize();
                    if(!ReflectedContract::Fits(deleted,objectSize)||!ReflectedContract::Fits(migration,objectSize)
                        ||deleted->GetPropertyValueInContainer(object)||migration->GetObjectPropertyValue(migration->ContainerPtrToValuePtr<void>(object)))throw std::runtime_error("Retired/migrating native assets remain under the game's migration policy");
                    const auto text=[&](const TCHAR* field){
                        auto* property=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),field));
                        if(!property||property->GetArrayDim()!=1)throw std::runtime_error("Native family identity field changed");
                        const auto value=property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(object));const auto& chars=value.GetCharArray();
                        if(chars.Num()<2||chars.Num()>1025||!chars.GetData()||chars.GetData()[chars.Num()-1]!=0)throw std::runtime_error("Native family identity is empty or malformed");
                        return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
                    };
                    identities.push_back({text(TEXT("PersistenceID")),text(TEXT("InternalName")),RC::to_string(object->GetName()),path,objects.size()});objects.push_back(object);
                }
                const auto ordered=PS::NativeDataFamilies::Order(std::move(identities));
                TArray<UObject*> subsystems;UECustom::UObjectGlobals::GetObjectsOfClass(subsystemType,subsystems,true);std::size_t enrolled=0;
                for(auto* subsystem:subsystems){
                    const auto invalid=static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization);
                    if(!subsystem||subsystem->HasAnyFlags(invalid))continue;
                    if(subsystem->GetClassPrivate()!=subsystemType)throw std::runtime_error("Derived native family subsystem needs its own verified lifecycle adapter");
                    auto* instance=subsystem->GetOuterPrivate();
                    // Validate all aliases against this instance before the first asset commits.
                    for(const auto& record:ordered)QuestRegistry::NativeRegistry::RegisterFamily(subsystem,instance,objects[record.SourceIndex],*family,false);
                    const auto before=AuditNetworkRegistry(subsystem);if(!before.Valid)throw std::runtime_error("Existing native network identities do not round-trip: "+before.Detail);
                    const auto key="network:"+lane.Name+":"+family->Subsystem;
                    auto& sync=m_networkSyncLanes[key];
                    if(sync.Subsystem.Get()!=subsystem){
                        sync={};sync.Subsystem.Assign(subsystem);sync.PrefixCount=before.Count;sync.PrefixFingerprint=before.Fingerprint;
                    }
                    for(const auto& record:ordered)QuestRegistry::NativeRegistry::RegisterFamily(subsystem,instance,objects[record.SourceIndex],*family);
                    const auto after=AuditNetworkRegistry(subsystem);if(!after.Valid)throw std::runtime_error("Native family network verification failed: "+after.Detail);
                    PS::NetworkIdentityState::Publish(key,after.Fingerprint);PS::NetworkIdentityState::PublishOwner(key,manifest.Owner,after.Fingerprint);
                    enrolled+=ordered.size();
                }
                if(!enrolled)throw std::runtime_error("No ready owning GameInstance subsystem at startup; no late-world mutation attempted");
                if(m_registryStatusReported.insert(reportKey+":ready").second)PS::Log<LogLevel::Normal>(STR("[NATIVE-REGISTRY][ENROLLED] owner='{}' lane='{}' identities={}; cooked behavior retained; consumer lifecycle still requires mod testing.\n"),PS::ToWideSafe(manifest.Owner),PS::ToWideSafe(lane.Name),enrolled);
            }catch(const std::exception& error){
                PS::RegistryEnrollment::RecordDeclaration(manifest.Owner,manifest.Source,lane.Name+": "+error.what());
                if(m_registryStatusReported.insert(reportKey+":rejected").second)PS::Log<LogLevel::Warning>(STR("[NATIVE-REGISTRY][REJECTED] owner='{}' lane='{}': {}; other families and save cleanup continue independently.\n"),PS::ToWideSafe(manifest.Owner),PS::ToWideSafe(lane.Name),PS::ToWideSafe(error.what()));
            }
        }
    }

    void DragonWildsDataRegistrar::RegisterAll()
    {
        const auto& combat = PS::PSConfig::Get()->GetSettings().combatFallback;
        BootstrapCombatRegistries();

        PS::SaveCleanup::RegistrySnapshot snapshot;
        bool itemsReady = false;
        bool recipesReady = false;
        bool questsReady = false;
        std::size_t rangedEquipmentCollections = 0;
        std::size_t rangedQuickAttacks = 0;
        std::size_t rangedFullAttacks = 0;
        std::size_t rangedIncompleteCollections = 0;
        bool rangedEquipmentLayoutVerified = false;
        std::unordered_set<UObject*> rangedCollectionsSeen;
        std::vector<ManifestAttackCollection> rangedDiscoveredCollections;
        // The settled live registries remain authoritative. PAK discovery is
        // a one-time loading aid, not a second ownership or pruning ledger.

        for (const auto& binding : m_bindings)
        {
            auto* dataClass = binding.DataClass;
            auto* subsystemClass = binding.SubsystemClass;
            const bool itemBinding = dataClass->GetPathName()
                == ItemDataClassPath;
            RegistrationStats registrationStats{};
            TArray<UObject*> subsystems;
            UECustom::UObjectGlobals::GetObjectsOfClass(
                subsystemClass, subsystems, true);
            bool foundSubsystem = false;
            bool bindingVerified = true;
            bool foundIdentityMap = false;
            std::size_t liveSubsystems = 0;
            std::size_t bindingAdded = 0;
            std::size_t persistenceIds = 0;
            for (auto* subsystem : subsystems)
            {
                if (!subsystem || subsystem->HasAnyFlags(
                    static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject
                        | RF_BeginDestroyed | RF_FinishDestroyed)))
                    continue;
                foundSubsystem = true;
                ++liveSubsystems;

                // Startup is the only mutation boundary. Verify every live
                // GameInstance registry now; later world transitions consume
                // the sealed snapshot without revisiting subsystem objects.
                std::size_t added = 0;
                const auto registered = RegisterMissing(dataClass, subsystem,
                    binding.ExcludedClass, binding.StatusTag, &added,
                    itemBinding ? &registrationStats : nullptr);
                bindingAdded += added;
                bindingVerified = registered && bindingVerified;
                if (added)
                    PS::Log<LogLevel::Normal>(STR(
                        "[REGISTRY][{}][ADDED] count={} subsystem='{}' verified={}.\n"),
                        binding.StatusTag, added,
                        subsystem->GetClassPrivate()->GetName(), registered);

                auto* idMapProperty = CastField<FMapProperty>(
                    PropertyHelper::GetPropertyByName(
                        subsystem->GetClassPrivate(),
                        TEXT("PersistenceIDToDataMap")));
                if (!idMapProperty)
                {
                    bindingVerified = false;
                    continue;
                }
                foundIdentityMap = true;

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
                UECustom::FScriptMapHelper idMap(
                    idMapProperty,
                    idMapProperty->ContainerPtrToValuePtr<void>(subsystem));
                idMap.ForEachPair([&](void* keyPtr, void* valuePtr) {
                    auto* key = static_cast<FString*>(keyPtr);
                    if (key && key->GetCharArray().Num() > 1)
                    {
                        ++persistenceIds;
                        if (target)
                            target->insert(RC::to_string(
                                RC::StringType(**key)));
                    }
                    if (!itemBinding || !valuePtr || !combat.enabled
                        || !combat.rangedEquipment) return;
                    UObject* item = nullptr;
                    std::memcpy(&item, valuePtr, sizeof(item));
                    if (!item || !item->GetClassPrivate()) return;
                    auto* rangedField = CastField<FObjectPropertyBase>(
                        PropertyHelper::GetPropertyByName(item->GetClassPrivate(),
                            TEXT("RangedAttackCollection")));
                    if (!rangedField) return;
                    rangedEquipmentLayoutVerified = true;
                    auto* collection = rangedField->GetObjectPropertyValue(
                        rangedField->ContainerPtrToValuePtr<void>(item));
                    if (!collection || !collection->GetClassPrivate()) return;
                    if (!rangedCollectionsSeen.insert(collection).second) return;
                    ++rangedEquipmentCollections;
                    auto* collectionData = collection;
                    if (collection->IsA<UClass>())
                        collectionData = static_cast<UClass*>(collection)
                            ->GetClassDefaultObject().Get();
                    if (!collectionData || !collectionData->GetClassPrivate())
                    {
                        ++rangedIncompleteCollections;
                        return;
                    }
                    const auto readAttack = [&](const TCHAR* propertyName) {
                        auto* field = CastField<FObjectPropertyBase>(
                            PropertyHelper::GetPropertyByName(
                                collectionData->GetClassPrivate(), propertyName));
                        return field ? field->GetObjectPropertyValue(
                            field->ContainerPtrToValuePtr<void>(collectionData)) : nullptr;
                    };
                    auto* quickObject = readAttack(TEXT("QuickAttackData"));
                    auto* fullObject = readAttack(TEXT("FullAttackData"));
                    auto* quickClass = quickObject && quickObject->IsA<UClass>()
                        ? static_cast<UClass*>(quickObject) : nullptr;
                    auto* fullClass = fullObject && fullObject->IsA<UClass>()
                        ? static_cast<UClass*>(fullObject) : nullptr;
                    const bool hasQuick = quickClass != nullptr;
                    const bool hasFull = fullClass != nullptr;
                    rangedQuickAttacks += hasQuick ? 1 : 0;
                    rangedFullAttacks += hasFull ? 1 : 0;
                    rangedIncompleteCollections += hasQuick && hasFull ? 0 : 1;
                    if (!hasQuick || !hasFull) return;
                    ManifestAttackCollection discovered;
                    discovered.Owner = RC::to_string(collection->GetPathName());
                    discovered.Source = RC::to_string(item->GetPathName());
                    discovered.Lane = "RangedEquipmentCollection";
                    discovered.Classes.push_back(quickClass);
                    if (fullClass != quickClass)
                        discovered.Classes.push_back(fullClass);
                    rangedDiscoveredCollections.push_back(std::move(discovered));
                });
            }
            const bool cleanBinding=foundSubsystem&&foundIdentityMap&&bindingVerified;
            if(dataClass->GetPathName()==ItemDataClassPath)itemsReady=itemsReady&&cleanBinding;
            if(dataClass->GetPathName()==RecipeDataClassPath)recipesReady=recipesReady&&cleanBinding;
            if(dataClass->GetPathName()==QuestDataClassPath)questsReady=questsReady&&cleanBinding;
            if (!foundSubsystem && binding.CleanupAuthority)
            {
                const auto statusKey = RC::to_string(
                    RC::StringType(binding.StatusTag));
                if (!m_registryWaitingReported.contains(statusKey))
                {
                    m_registryWaitingReported.insert(statusKey);
                    PS::Log<LogLevel::Warning>(STR(
                        "[REGISTRY][{}][WAITING] subsystem='{}' is not live; registration and pruning authority remain disabled.\n"),
                        binding.StatusTag, subsystemClass->GetName());
                }
            }
            else if (!foundSubsystem)
            {
                const auto statusKey = RC::to_string(
                    RC::StringType(binding.StatusTag));
                if (!m_registryWaitingReported.contains(statusKey))
                {
                    m_registryWaitingReported.insert(statusKey);
                    PS::Log<LogLevel::Normal>(STR(
                        "[REGISTRY][{}][WAITING] subsystem='{}' is not live yet; registration will retry.\n"),
                        binding.StatusTag, subsystemClass->GetName());
                }
            }
            else
            {
                const auto statusKey = RC::to_string(
                    RC::StringType(binding.StatusTag));
                if (!m_registryStatusReported.contains(statusKey))
                {
                    m_registryStatusReported.insert(statusKey);
                    PS::Log<LogLevel::Normal>(STR(
                        "[REGISTRY][{}][SUMMARY] live_subsystems={} persistence_ids={} added={} verified={}.\n"),
                        binding.StatusTag, liveSubsystems, persistenceIds,
                        bindingAdded, bindingVerified && foundIdentityMap);
                    if (itemBinding)
                        PS::Log<LogLevel::Normal>(STR(
                            "[REGISTRY][ITEM][PROVENANCE] cooked_or_pak_existing={} runeschema_existing={} cooked_or_pak_added={} runeschema_added={} rejected={} unresolved_runeschema={} verified={}.\n"),
                            registrationStats.ExistingCookedOrPak,
                            registrationStats.ExistingRuneSchema,
                            registrationStats.AddedCookedOrPak,
                            registrationStats.AddedRuneSchema,
                            registrationStats.Rejected,
                            registrationStats.UnresolvedRuneSchema,
                            registrationStats.Rejected == 0
                                && registrationStats.UnresolvedRuneSchema == 0);
                }
            }
        }

        // Enroll dependent cooked families only after the established item/recipe/spell passes.
        RegisterDeclaredNativeFamilies();
        snapshot.ItemsComplete=itemsReady;snapshot.RecipesComplete=recipesReady;
        snapshot.QuestsComplete = questsReady && !snapshot.Quests.empty();
        snapshot.QuestCompletedState=QuestRegistry::NativeRegistry::CompletedState();
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
                    journalMap.ForEachPair([&](void* keyPtr, void* valuePtr) {
                        auto* key = static_cast<FString*>(keyPtr);
                        if (key && key->GetCharArray().Num() > 1)
                            snapshot.Journals.insert(RC::to_string(
                                RC::StringType(**key)));
                    });
                    snapshot.JournalsComplete = !snapshot.Journals.empty();
                }
            }
        }
        if (snapshot.AnyCleanable())
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
            m_registeredRangedCollections = std::move(rangedDiscoveredCollections);
            // Registration is process-scoped.  Install complete ranged pairs on
            // the component default before a gameplay pawn is constructed;
            // subsequent world callbacks only validate inherited state.
            BootstrapCombatRegistries();
            if (!m_rangedEquipmentReported && rangedEquipmentLayoutVerified)
            {
                m_rangedEquipmentReported = true;
                PS::Log<LogLevel::Normal>(STR(
                    "[COMBAT-REGISTRY][RANGED-EQUIPMENT][READY] unique_collections={} quick_attacks={} full_attacks={} incomplete_collections={}; authority=ItemData->HeldEquipmentData.RangedAttackCollection; only complete quick/full PlayerAttackData pairs may attach to the process-scoped component default.\n"),
                    rangedEquipmentCollections, rangedQuickAttacks,
                    rangedFullAttacks, rangedIncompleteCollections);
            }
            if (!m_registrySummaryReported)
            {
                m_registrySummaryReported = true;
                const auto published=PS::SaveCleanup::ReadRegistry();
                if(published&&published->AnyCleanable())PS::Log<LogLevel::Normal>(STR(
                    "[REGISTRY][PERSISTENCE-PRUNER][READY] base game and loaded paks: items={}, recipes={}, quests={}, journal={}. Each section uses its own certification; unavailable sections do not block other cleanup.\n"),
                    snapshot.Items.size(), snapshot.Recipes.size(),
                    snapshot.Quests.size(), snapshot.Journals.size());
                else PS::Log<LogLevel::Warning>(STR("[REGISTRY][PERSISTENCE-PRUNER][DEFERRED] Registry enrollment is not certified; automatic character pruning remains disabled. Non-blocking: native loading continues; individually verified content remains eligible for its own runtime checks.\n"));
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

    void DragonWildsDataRegistrar::BootstrapCombatRegistries(UWorld* world)
    {
        const auto& combat = PS::PSConfig::Get()->GetSettings().combatFallback;
        if (!combat.enabled)
            return;
        static constexpr const TCHAR* MeleeComponentClassPath =
            TEXT("/Script/Dominion.PlayerMeleeAttackComponent");
        static constexpr const TCHAR* RangedComponentClassPath =
            TEXT("/Script/Dominion.PlayerRangedAttackComponent");

        const bool allowInitialWorldFallback = world
            && combat.initialWorldMutation && m_initialCombatFallbackAttempted;
        if (allowInitialWorldFallback)
            PS::Log<LogLevel::Normal>(STR(
                "[COMBAT-REGISTRY][INITIAL-WORLD-FALLBACK][ATTEMPT] Applying the settled startup registration to pre-existing combat components once.\n"));
        static constexpr std::array<const TCHAR*, 16> AdditionalWeaponAttackClassPaths{{
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_Attack1.BP_Player_Spear_Attack1_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_Attack2.BP_Player_Spear_Attack2_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_Attack2_ShortComboEnd.BP_Player_Spear_Attack2_ShortComboEnd_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_Attack3.BP_Player_Spear_Attack3_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_Attack4.BP_Player_Spear_Attack4_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_SpecialAction.BP_Player_Spear_SpecialAction_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_SprintAttack.BP_Player_Spear_SprintAttack_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Spear/BP_Player_Spear_VisceralAttack.BP_Player_Spear_VisceralAttack_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_Attack1.BP_Player_Hoplite_Attack1_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_Attack2.BP_Player_Hoplite_Attack2_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_Attack2_ShortComboEnd.BP_Player_Hoplite_Attack2_ShortComboEnd_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_Attack3.BP_Player_Hoplite_Attack3_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_Attack4.BP_Player_Hoplite_Attack4_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_SpecialAction.BP_Player_Hoplite_SpecialAction_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_SprintAttack.BP_Player_Hoplite_SprintAttack_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Gameplay/Attacks/Hoplite/BP_Player_Hoplite_VisceralAttack.BP_Player_Hoplite_VisceralAttack_C"),
        }};
        const auto resolveAttackCollection = [&](const auto& paths,
            std::vector<UClass*>& classes, std::vector<UClass*>& retainedRoots,
            bool& incompleteReported, const TCHAR* owner) {
            if (!classes.empty()) return true;
            std::vector<UClass*> resolved;
            std::vector<UClass*> ownedRoots;
            resolved.reserve(paths.size());
            ownedRoots.reserve(paths.size());
            for (const auto* path : paths)
            {
                auto* attack = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, path, false);
                if (!attack)
                    attack = UECustom::UKismetSystemLibrary::LoadClassAsset_Blocking(
                        UECustom::TSoftClassPtr<UObject>(
                            UECustom::FSoftObjectPath(RC::StringType(path))));
                if (!attack)
                {
                    for (auto* owned : ownedRoots)
                        if (owned && owned->IsRootSet()) owned->ClearRootSet();
                    // The first class missing means the optional PAK is not
                    // mounted. A partially present package is unsafe because
                    // attack replication uses collection indices.
                    if (!resolved.empty() && !incompleteReported)
                    {
                        PS::Log<LogLevel::Warning>(STR(
                            "[{}] Bootstrap deferred: only {}/{} ordered attack classes resolved; no component was modified.\n"),
                            owner, resolved.size(), paths.size());
                        incompleteReported = true;
                    }
                    return false;
                }
                // These class assets are retained across menu -> world
                // transitions. A native vector is invisible to Unreal GC;
                // without an owned root the cached pointer can become stale
                // after the first world's component releases its reference.
                if (!attack->IsRootSet())
                {
                    attack->SetRootSet();
                    ownedRoots.push_back(attack);
                }
                resolved.push_back(attack);
            }
            classes = std::move(resolved);
            retainedRoots = std::move(ownedRoots);
            return true;
        };

        const bool additionalWeaponsReady = combat.additionalWeapons
            && resolveAttackCollection(
                AdditionalWeaponAttackClassPaths, m_additionalWeaponAttackClasses,
                m_ownedAdditionalWeaponAttackRoots,
                m_additionalWeaponsIncompleteReported,
                TEXT("AdditionalWeapons"));
        const auto resolveManifestCollection = [&](ManifestAttackCollection& collection) {
            if (!collection.Classes.empty()) return true;
            std::vector<UClass*> resolved;
            std::vector<UClass*> roots;
            for (const auto& path : collection.Paths)
            {
                const auto widePath = PS::ToWideSafe(path.c_str());
                auto* attack = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, widePath.c_str(), false);
                if (!attack)
                    attack = UECustom::UKismetSystemLibrary::LoadClassAsset_Blocking(
                        UECustom::TSoftClassPtr<UObject>(
                            UECustom::FSoftObjectPath(widePath)));
                if (!attack)
                {
                    for (auto* owned : roots)
                        if (owned && owned->IsRootSet()) owned->ClearRootSet();
                    if (!collection.IncompleteReported)
                    {
                        PS::Log<LogLevel::Error>(STR(
                            "[PAK-REGISTRY][REJECTED] owner='{}' lane='{}' resolved={}/{} missing='{}'; no component was modified.\n"),
                            PS::ToWideSafe(collection.Owner.c_str()),
                            PS::ToWideSafe(collection.Lane.c_str()), resolved.size(),
                            collection.Paths.size(), widePath);
                        collection.IncompleteReported = true;
                    }
                    return false;
                }
                if (!attack->IsRootSet())
                {
                    attack->SetRootSet();
                    roots.push_back(attack);
                }
                resolved.push_back(attack);
            }
            collection.Classes = std::move(resolved);
            collection.OwnedRoots = std::move(roots);
            return true;
        };
        bool anyMeleeManifestReady = false;
        if (combat.manifestMelee)
            for (auto& collection : m_manifestMeleeCollections)
                anyMeleeManifestReady = resolveManifestCollection(collection)
                    || anyMeleeManifestReady;
        if (combat.rangedEquipment)
            for (auto& collection : m_manifestRangedCollections)
        {
            if (collection.IncompleteReported) continue;
            PS::Log<LogLevel::Warning>(STR(
                "[PAK-REGISTRY][RANGED-EQUIPMENT][DEFERRED] owner='{}' lane='{}' entries={}; player ranged combat is item-owned, so no live component was modified. Register the HeldEquipmentData item and keep its RangedAttackCollection reference intact.\n"),
                PS::ToWideSafe(collection.Owner.c_str()),
                PS::ToWideSafe(collection.Lane.c_str()),
                collection.Paths.size());
            collection.IncompleteReported = true;
        }
        if (!additionalWeaponsReady && !anyMeleeManifestReady
            && m_registeredRangedCollections.empty())
            return;

        const auto attachLane = [&](const TCHAR* componentClassPath,
            const TCHAR* laneTag, const TCHAR* componentLabel,
            const TCHAR* builtInOwner, bool builtInReady,
            const std::vector<UClass*>& builtInClasses,
            bool& builtInReadyReported, bool& builtInConflictReported,
            std::vector<ManifestAttackCollection>& collections) {
            auto* componentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, componentClassPath, false);
            if (!componentClass) return;

            const bool defaultTemplate = world == nullptr;
            const bool allowMutation = defaultTemplate
                || allowInitialWorldFallback;
            TArray<UObject*> components;
            if (defaultTemplate)
            {
                if (auto* componentDefault = componentClass->GetClassDefaultObject().Get())
                    components.Add(componentDefault);
            }
            else
            {
                UECustom::UObjectGlobals::GetObjectsOfClass(
                    componentClass, components, true);
            }
            std::size_t builtInChangedComponents = 0;
            std::size_t builtInAddedClasses = 0;
            for (auto* component : components)
            {
                if (!component || component->HasAnyFlags(static_cast<EObjectFlags>(
                        RF_BeginDestroyed | RF_FinishDestroyed)))
                    continue;
                if (!defaultTemplate
                    && (component->HasAnyFlags(static_cast<EObjectFlags>(
                            RF_ClassDefaultObject | RF_ArchetypeObject))
                        || component->GetWorld() != world))
                    continue;
                auto* property = CastField<FArrayProperty>(
                    PropertyHelper::GetPropertyByName(component->GetClassPrivate(),
                        TEXT("AttackDataCollection")));
                auto* classProperty = property
                    ? CastField<FClassProperty>(property->GetInner()) : nullptr;
                if (!property || !classProperty
                    || property->GetInner()->GetElementSize() != sizeof(UClass*))
                    continue;
                auto* expectedAttackClass = classProperty->GetMetaClass().Get();
                if (!expectedAttackClass) continue;
                auto* array = property->ContainerPtrToValuePtr<FScriptArray>(component);
                if (!array || array->Num() < 0) continue;
                UECustom::FScriptArrayHelper helper(array, property);
                struct AppendResult {
                    std::size_t Added = 0;
                    bool Conflict = false;
                    bool Missing = false;
                };
                const auto appendCollection = [&](const std::vector<UClass*>& classes) {
                    AppendResult result;
                    for (auto* declared : classes)
                    {
                        if (!declared
                            || !declared->IsChildOf(expectedAttackClass))
                        {
                            result.Conflict = true;
                            return result;
                        }
                    }
                    std::vector<std::size_t> positions;
                    std::size_t index = 0;
                    helper.ForEachElement([&](void* value) {
                        UClass* existing = nullptr;
                        std::memcpy(&existing, value, sizeof(existing));
                        if (std::ranges::find(classes, existing) != classes.end())
                            positions.push_back(index);
                        ++index;
                    });
                    if (!positions.empty())
                    {
                        if (positions.size() != classes.size()) result.Conflict = true;
                        else
                        {
                            std::size_t prior = 0;
                            bool first = true;
                            for (auto* declared : classes)
                            {
                                std::size_t found = 0;
                                bool present = false;
                                std::size_t scanIndex = 0;
                                helper.ForEachElement([&](void* value) {
                                    UClass* existing = nullptr;
                                    std::memcpy(&existing, value, sizeof(existing));
                                    if (!present && existing == declared)
                                    {
                                        found = scanIndex;
                                        present = true;
                                    }
                                    ++scanIndex;
                                });
                                if (!present || (!first && found <= prior))
                                {
                                    result.Conflict = true;
                                    break;
                                }
                                prior = found;
                                first = false;
                            }
                        }
                        return result;
                    }
                    if (!allowMutation)
                    {
                        result.Missing = true;
                        return result;
                    }
                    for (auto* attack : classes)
                    {
                        UECustom::FManagedValue value;
                        helper.InitializeValue(value);
                        std::memcpy(value.GetData(), &attack, sizeof(attack));
                        helper.Add(value);
                        ++result.Added;
                    }
                    return result;
                };

                const auto builtInResult = builtInReady
                    ? appendCollection(builtInClasses) : AppendResult{};
                if (builtInResult.Added)
                {
                    ++builtInChangedComponents;
                    builtInAddedClasses += builtInResult.Added;
                }
                if (builtInResult.Conflict && !builtInConflictReported)
                {
                    PS::Log<LogLevel::Error>(STR(
                        "[COMBAT-REGISTRY][{}][CONFLICT] {} is partially present or out of order; live collection was not modified.\n"),
                        laneTag, builtInOwner);
                    builtInConflictReported = true;
                }
                if (builtInResult.Missing && !builtInConflictReported)
                {
                    PS::Log<LogLevel::Warning>(STR(
                        "[COMBAT-REGISTRY][{}][MISSING] {} was not inherited by this world's {} component; runtime mutation is intentionally disabled.\n"),
                        laneTag, builtInOwner, componentLabel);
                    builtInConflictReported = true;
                }
                for (auto& collection : collections)
                {
                    if (collection.Classes.empty()) continue;
                    const auto* collectionTag = collection.Lane
                        == "RangedEquipmentCollection"
                        ? TEXT("COMBAT-REGISTRY") : TEXT("PAK-REGISTRY");
                    const auto result = appendCollection(collection.Classes);
                    if (result.Conflict)
                    {
                        if (!collection.ConflictReported)
                        {
                            PS::Log<LogLevel::Error>(STR(
                                "[{}][{}][CONFLICT] owner='{}': declared collection is partially present or out of order; live collection was not modified.\n"),
                                collectionTag, laneTag,
                                PS::ToWideSafe(collection.Owner.c_str()));
                            collection.ConflictReported = true;
                        }
                        continue;
                    }
                    if (result.Missing)
                    {
                        if (!collection.ConflictReported)
                        {
                            PS::Log<LogLevel::Warning>(STR(
                                "[{}][{}][MISSING] owner='{}': collection was not inherited by this world's {} component; runtime mutation is intentionally disabled.\n"),
                                collectionTag, laneTag,
                                PS::ToWideSafe(collection.Owner.c_str()),
                                componentLabel);
                            collection.ConflictReported = true;
                        }
                        continue;
                    }
                    if (result.Added || !collection.ReadyReported)
                    {
                        PS::Log<LogLevel::Normal>(STR(
                            "[{}][{}][{}] owner='{}' ordered_classes={} appended={}; component default is authoritative.\n"),
                            collectionTag, laneTag,
                            result.Added ? TEXT("ADDED") : TEXT("READY"),
                            PS::ToWideSafe(collection.Owner.c_str()),
                            collection.Classes.size(), result.Added);
                        collection.ReadyReported = true;
                    }
                }
            }

            if (builtInReady
                && (builtInAddedClasses || !builtInReadyReported))
            {
                PS::Log<LogLevel::Normal>(STR(
                    "[COMBAT-REGISTRY][{}][{}] {}: {} ordered attack classes; updated {} {} {} target(s), appended {} class reference(s).\n"),
                    laneTag, builtInAddedClasses ? TEXT("ADDED") : TEXT("READY"),
                    builtInOwner, builtInClasses.size(), builtInChangedComponents,
                    defaultTemplate ? TEXT("default") : TEXT("initial-world live"),
                    componentLabel, builtInAddedClasses);
                builtInReadyReported = true;
            }
        };

        if (combat.additionalWeapons || combat.manifestMelee)
            attachLane(MeleeComponentClassPath, TEXT("MELEE-ATTACK"), TEXT("melee"),
                TEXT("AdditionalWeapons"), additionalWeaponsReady,
                m_additionalWeaponAttackClasses, m_additionalWeaponsReadyReported,
                m_additionalWeaponsIncompleteReported, m_manifestMeleeCollections);
        static const std::vector<UClass*> NoBuiltInRangedClasses;
        if (combat.rangedEquipment)
            attachLane(RangedComponentClassPath, TEXT("RANGED-EQUIPMENT"),
                TEXT("ranged"), TEXT("Item-owned ranged equipment"), false,
                NoBuiltInRangedClasses, m_rangedEquipmentReported,
                m_rangedEquipmentConflictReported, m_registeredRangedCollections);
    }

    bool DragonWildsDataRegistrar::RegisterMissing(UClass* dataClass,
        UObject* subsystem, UClass* excludedClass, const TCHAR* statusTag,
        std::size_t* addedCount, RegistrationStats* stats)
    {
        if (addedCount) *addedCount = 0;
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

        if (stats)
        {
            for (const auto& [identity, unused] : known)
            {
                (void)unused;
                if (PS::RegistryProvenance::IsRuneSchemaItem(
                        RC::to_string(identity)))
                    ++stats->ExistingRuneSchema;
                else
                    ++stats->ExistingCookedOrPak;
            }
        }

        const auto baselineAudit = AuditNetworkRegistry(subsystem);
        if (!baselineAudit.Valid)
        {
            PS::Log<LogLevel::Error>(STR(
                "[NETWORK-REGISTRY][{}][REJECTED] Existing network identities are inconsistent: {}; no registry mutation was attempted.\n"),
                statusTag ? statusTag : TEXT("UNKNOWN"),
                PS::ToWideSafe(baselineAudit.Detail.c_str()));
            return false;
        }

        const auto syncKey = std::string("network:") + RC::to_string(statusTag ? statusTag : TEXT("UNKNOWN"))
            + ":" + RC::to_string(subsystem->GetClassPrivate()->GetPathName());
        auto& syncLane = m_networkSyncLanes[syncKey];
        if (syncLane.Subsystem.Get() != subsystem) {
            syncLane = {}; syncLane.Subsystem.Assign(subsystem);
            syncLane.PrefixCount = baselineAudit.Count;
            syncLane.PrefixFingerprint = baselineAudit.Fingerprint;
        }
        TArray<UObject*> discovered;
        UECustom::UObjectGlobals::GetObjectsOfClass(dataClass, discovered, true);
        struct CandidateRecord {
            UObject* Object{};
            FString PersistenceId;
            std::string Identity;
            std::string AssetPath;
        };
        std::vector<CandidateRecord> records;
        records.reserve(static_cast<std::size_t>(std::max(0, discovered.Num())));
        for (auto* candidate : discovered)
        {
            if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))
                || m_rejectedManifestAssets.contains(candidate)
                || (excludedClass && candidate->IsA(excludedClass)))
                continue;
            auto* candidateClass = candidate->GetClassPrivate();
            auto* idProperty = candidateClass
                ? CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                    candidateClass, TEXT("PersistenceID")))
                : nullptr;
            if (!idProperty)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "'{}' has no PersistenceID property and cannot be registered.\n"),
                    candidate->GetName());
                complete = false;
                continue;
            }
            auto persistenceId = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(candidate));
            if (persistenceId.GetCharArray().Num() <= 1)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "'{}' has an empty PersistenceID and cannot be registered.\n"),
                    candidate->GetName());
                complete = false;
                continue;
            }
            records.push_back({candidate, persistenceId,
                RC::to_string(RC::StringType(*persistenceId)),
                RC::to_string(candidate->GetPathName())});
        }
        std::vector<PS::NetworkIdentityPlan::Candidate> planInput;
        planInput.reserve(records.size());
        for (std::size_t index = 0; index < records.size(); ++index)
            planInput.push_back({records[index].Identity,
                records[index].AssetPath, index,
                baselineAudit.Objects.contains(records[index].Object)});
        std::vector<PS::NetworkIdentityPlan::Candidate> plan;
        try
        {
            plan = PS::NetworkIdentityPlan::Canonicalize(
                planInput, baselineAudit.Count);
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(STR(
                "[NETWORK-REGISTRY][{}][REJECTED] Canonical identity plan failed: {}; no registry mutation was attempted.\n"),
                statusTag ? statusTag : TEXT("UNKNOWN"),
                PS::ToWideSafe(error.what()));
            return false;
        }
        PS::RegistrationDiagnostics::Sources sourceCounts;
        std::map<std::string,std::string> cacheSourcePaths;

        for (const auto& planned : plan)
        {
            auto& record = records[planned.SourceIndex];
            auto* candidate = record.Object;
            try
            {
                auto* candidateClass = candidate->GetClassPrivate();
                auto& persistenceId = record.PersistenceId;
                auto idString = RC::StringType(*persistenceId);
                const bool runeSchemaAuthored = stats
                    && PS::RegistryProvenance::IsRuneSchemaItem(
                        RC::to_string(idString));
                const auto existing = known.find(idString);
                const bool inserted = existing == known.end();
                candidate->SetRootSet();
                if (existing == known.end())
                {
                    if (!InsertIntoMap(subsystem, TEXT("PersistenceIDToDataMap"), persistenceId, candidate))
                        throw std::runtime_error(
                            "primary persistence registry rejected the asset");
                    if (!InsertIntoMap(subsystem, TEXT("InternalNameToDataMap"),
                            persistenceId, candidate))
                        throw std::runtime_error(
                            "internal-name registry rejected the persistence identity");

                    if (auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(candidateClass, TEXT("InternalName"))))
                    {
                        auto internalName = nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(candidate));
                        if (internalName.GetCharArray().Num() > 1 && RC::StringType(*internalName) != idString)
                        {
                            if (!InsertIntoMap(subsystem,
                                    TEXT("InternalNameToDataMap"), internalName,
                                    candidate))
                                throw std::runtime_error(
                                    "internal-name registry rejected the authored name");
                        }
                    }

                    known.emplace(idString, candidate);
                }
                else if (existing->second != candidate)
                    throw std::runtime_error(
                        "duplicate PersistenceID resolves to multiple live assets");

                const auto networkId = EnsureNetworkIdentity(candidate, subsystem);
                if (networkId < 0)
                {
                    throw std::runtime_error("network registry rejected the asset");
                }
                if (!baselineAudit.Objects.contains(candidate)) syncLane.Managed.insert(candidate);
                if (stats) EquipmentWearableBridge::TryRegister(subsystem, candidate);
                try{cacheSourcePaths.try_emplace(PS::HelpyCacheSources::Source(record.AssetPath),record.AssetPath);}catch(...){}
                if(PS::PSConfig::Get()->IsDebugLoggingEnabled())sourceCounts.Add(record.AssetPath,inserted);
                if (inserted)
                {
                    if (addedCount) ++*addedCount;
                    if (stats)
                    {
                        if (runeSchemaAuthored) ++stats->AddedRuneSchema;
                        else ++stats->AddedCookedOrPak;
                    }
                }
            }
            catch (const std::exception& e)
            {
                complete = false;
                if (stats) ++stats->Rejected;
                PS::Log<LogLevel::Error>(STR("Failed registering '{}': {}\n"),
                    candidate ? candidate->GetName() : STR("<null>"), PS::ToWideSafe(e.what()));
            }
        }
        const auto finalAudit = AuditNetworkRegistry(subsystem);
        if (!finalAudit.Valid)
        {
            complete = false;
            PS::Log<LogLevel::Error>(STR(
                "[NETWORK-REGISTRY][{}][REJECTED] Final network identity audit failed: {}.\n"),
                statusTag ? statusTag : TEXT("UNKNOWN"),
                PS::ToWideSafe(finalAudit.Detail.c_str()));
        }
        else
        {
            for(const auto& [source,path]:cacheSourcePaths)PS::HelpyCacheSources::Registered(path);
            const auto auditKey = std::string("network:")
                + RC::to_string(statusTag ? statusTag : TEXT("UNKNOWN"));
            const auto laneKey = auditKey + ":"
                + RC::to_string(subsystem->GetClassPrivate()->GetPathName());
            if(m_registryStatusReported.emplace(laneKey+":sources:"+finalAudit.Fingerprint).second
                &&PS::PSConfig::Get()->IsDebugLoggingEnabled())
                for(const auto& [source,count]:sourceCounts.Rows())
                    PS::Log<LogLevel::Verbose>(STR("[REGISTRY][{}][SOURCE-SUMMARY] source='{}' added={} existing={}; individual asset messages suppressed.\n"),
                        statusTag,PS::ToWideSafe(source),count.Added,count.Existing);
            PS::NetworkIdentityState::Publish(laneKey, finalAudit.Fingerprint);
            std::map<std::string,std::vector<std::string>> ownerRows;
            for(const auto& planned:plan) {
                auto path=planned.AssetPath;
                std::string owner="<unknown>";
                std::string_view ownPrefix;
                const auto foldedPath=PS::HelpyCacheSources::Lower(path);
                for(const std::string_view prefix:{"/game/mods/","/game/runeschema/","/game/gameplay/mods/"})if(foldedPath.starts_with(prefix)){ownPrefix=prefix;break;}
                if(!ownPrefix.empty()) {
                    const auto begin=ownPrefix.size();
                    const auto slash=path.find('/',begin);
                    owner=path.substr(begin,slash==std::string::npos?slash:slash-begin);
                } else if(path.starts_with('/')) {
                    const auto slash=path.find('/',1);
                    owner=path.substr(1,slash==std::string::npos?slash:slash-1);
                }
                if(owner.empty() || owner.size()>96)owner="<unknown>";
                ownerRows[owner].push_back(planned.Identity+"="+planned.AssetPath);
            }
            for(const auto& [owner,rows]:ownerRows)
                PS::NetworkIdentityState::PublishOwner(laneKey,owner,
                    PS::NetworkIdentityPlan::Fingerprint(rows));
            if (m_registryStatusReported.emplace(auditKey).second)
                PS::Log<LogLevel::Normal>(STR(
                    "[NETWORK-REGISTRY][{}][SEALED] preserved_prefix={} final_count={} added={} ordered_fingerprint={}; the authority defines shared IDs and client-only content remains local.\n"),
                    statusTag ? statusTag : TEXT("UNKNOWN"),
                    baselineAudit.Count, finalAudit.Count,
                    finalAudit.Count - baselineAudit.Count,
                    PS::ToWideSafe(finalAudit.Fingerprint.c_str()));
        }
        if (stats)
        {
            for (const auto& identity :
                PS::RegistryProvenance::RuneSchemaItems())
            {
                if (known.contains(RC::to_generic_string(identity))) continue;
                ++stats->UnresolvedRuneSchema;
                complete = false;
                PS::Log<LogLevel::Error>(STR(
                    "[REGISTRY][ITEM][UNRESOLVED-RUNESCHEMA] PersistenceID='{}' was announced by RuneSchema but did not resolve in the live ItemData registry.\n"),
                    RC::to_generic_string(identity));
            }
        }
        return complete;
    }

    nlohmann::json DragonWildsDataRegistrar::NetworkIdentityManifest() const {
        using namespace PS::NetworkIdentitySync;
        Require(m_initialized && GetCurrentThreadId() == m_registrationThread, "Registry ID snapshot requires the initialized game thread");
        nlohmann::json result = {{"kind", "RuneSchemaNetworkIdentityPlan"}, {"version", 1},
            {"registryOwners", nlohmann::json::object()}, {"lanes", nlohmann::json::object()},
            {"fixedLanes", PS::NetworkIdentityState::Snapshot()}};
        for (const auto& [key, lane] : m_networkSyncLanes) {
            auto* subsystem = lane.Subsystem.Get(); const auto audit = AuditNetworkRegistry(subsystem);
            Require(audit.Valid && audit.Count >= lane.PrefixCount, "Registry changed before connection synchronization");
            const std::vector<std::string> prefix(audit.OrderedPaths.begin(), audit.OrderedPaths.begin() + lane.PrefixCount);
            Require(PS::NetworkIdentityPlan::Fingerprint(prefix) == lane.PrefixFingerprint, "Protected native IDs changed after startup");
            auto rows = nlohmann::json::array(), protectedRows = nlohmann::json::array();
            for (std::size_t i = 0; i < audit.Count; ++i) {
                auto* object = audit.OrderedObjects[i];
                auto* property = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(), TEXT("PersistenceID")));
                Require(property != nullptr, "Registry identity field is unavailable");
                const auto identity = property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(object));
                const auto identityText=RC::to_string(RC::StringType(*identity));
                if (i >= lane.PrefixCount && !lane.Managed.contains(object)
                    && !PS::NetworkIdentityState::HasNativeEnrollment(reinterpret_cast<uintptr_t>(subsystem),reinterpret_cast<uintptr_t>(object),identityText))
                    throw std::runtime_error("Registry entry outside RuneSchema enrollment: lane=" + key
                        + " index=" + std::to_string(i) + " protected=" + std::to_string(lane.PrefixCount)
                        + " asset=" + audit.OrderedPaths[i]);
                (i < lane.PrefixCount ? protectedRows : rows).push_back({{"path", audit.OrderedPaths[i]}, {"identity", identityText},
                    {"type", RC::to_string(object->GetClassPrivate()->GetPathName())}});
            }
            result["lanes"][key] = {{"prefixCount", lane.PrefixCount}, {"prefixFingerprint", PS::CacheBundleIO::Hash(protectedRows.dump())}, {"entries", std::move(rows)}};
            result["fixedLanes"].erase(key);
        }
        Validate(result); return result;
    }

    void DragonWildsDataRegistrar::ApplyNetworkIdentityOrder(const PS::NetworkIdentitySync::Permutations& orders) {
        using namespace PS::NetworkIdentitySync;
        Require(m_initialized && GetCurrentThreadId() == m_registrationThread, "Registry ID alignment requires the initialized game thread");
        Require(orders.size() == m_networkSyncLanes.size(), "Registry ID transaction is incomplete");
        struct Patch {
            UObject* Subsystem; FArrayProperty* ArrayProperty; std::size_t Prefix;
            std::vector<UObject*> Before, After; std::vector<void*> ReverseSlots;
            std::vector<std::uint16_t> AfterReverse;
            std::string Key, Fingerprint;
        };
        std::vector<Patch> patches;
        for (const auto& [key, order] : orders) {
            const auto found = m_networkSyncLanes.find(key); Require(found != m_networkSyncLanes.end(), "Unknown registry lane");
            const auto& lane = found->second; auto* subsystem = lane.Subsystem.Get(); const auto audit = AuditNetworkRegistry(subsystem);
            Require(audit.Valid && audit.Count >= lane.PrefixCount && order.size() == audit.Count - lane.PrefixCount, "Registry changed during ID planning");
            const std::vector<std::string> prefix(audit.OrderedPaths.begin(), audit.OrderedPaths.begin() + lane.PrefixCount);
            Require(PS::NetworkIdentityPlan::Fingerprint(prefix) == lane.PrefixFingerprint, "Protected native IDs changed during ID planning");
            auto* type = subsystem->GetClassPrivate();
            auto* array = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(type, TEXT("NetIdToData")));
            auto* map = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(type, TEXT("DataToNetIdMap")));
            Require(array && map && array->GetInner()->GetSize() == sizeof(UObject*)
                && map->GetKeyProp()->GetSize() == sizeof(UObject*) && map->GetValueProp()->GetSize() == sizeof(std::uint16_t), "Native registry layout changed");
            Patch patch{subsystem, array, lane.PrefixCount, audit.OrderedObjects, audit.OrderedObjects, {}, {}, key, {}};
            patch.AfterReverse.resize(audit.Count);
            UECustom::FScriptMapHelper reverse(map, map->ContainerPtrToValuePtr<void>(subsystem));
            std::set<std::size_t> seen;
            for (std::size_t i = 0; i < order.size(); ++i) {
                Require(order[i] < order.size() && seen.insert(order[i]).second, "Registry order is not a permutation");
                auto* object = audit.OrderedObjects[lane.PrefixCount + order[i]];
                if(!lane.Managed.contains(object)) {
                    auto* property=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("PersistenceID")));
                    Require(property!=nullptr,"Registry identity field is unavailable during alignment");
                    const auto identity=property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(object));
                    Require(PS::NetworkIdentityState::HasNativeEnrollment(reinterpret_cast<uintptr_t>(subsystem),reinterpret_cast<uintptr_t>(object),RC::to_string(RC::StringType(*identity))),
                        "Only RuneSchema-appended IDs can be aligned");
                }
                patch.After[lane.PrefixCount + i] = object;
                patch.AfterReverse[lane.PrefixCount + order[i]] = static_cast<std::uint16_t>(lane.PrefixCount + i);
            }
            std::vector<std::string> paths; paths.reserve(patch.After.size());
            for (auto* object : patch.After) paths.push_back(RC::to_string(object->GetPathName()));
            patch.Fingerprint = PS::NetworkIdentityPlan::Fingerprint(paths);
            for (auto* object : patch.Before) { auto* slot = reverse.FindValue(&object); Require(slot != nullptr, "Reverse registry slot disappeared"); patch.ReverseSlots.push_back(slot); }
            patches.push_back(std::move(patch));
        }
        const auto write = [&](bool restore) {
            for (auto& patch : patches) {
                FScriptArrayHelper array(patch.ArrayProperty, patch.ArrayProperty->ContainerPtrToValuePtr<FScriptArray>(patch.Subsystem));
                const auto& objects = restore ? patch.Before : patch.After;
                for (std::size_t i = patch.Prefix; i < objects.size(); ++i) std::memcpy(array.GetRawPtr(static_cast<int32>(i)), &objects[i], sizeof(UObject*));
                // Slots are captured before mutation; neither container is resized.
                for (std::size_t old = patch.Prefix; old < patch.Before.size(); ++old) {
                    const auto id = restore ? static_cast<std::uint16_t>(old) : patch.AfterReverse[old];
                    std::memcpy(patch.ReverseSlots[old], &id, sizeof(id));
                }
            }
        };
        Commit([&]{write(false);}, [&]{
            for (const auto& patch : patches) { const auto audit = AuditNetworkRegistry(patch.Subsystem); if (!audit.Valid || audit.Fingerprint != patch.Fingerprint) return false; }
            return true;
        }, [&]{write(true);});
        for (const auto& patch : patches) PS::NetworkIdentityState::Publish(patch.Key, patch.Fingerprint);
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
        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        if (!array || array->Num() < 0) return -1;
        int32_t existingId = -1;
        if (auto* value = reverse.FindValue(&dataAsset))
        {
            uint16 netId = 0;
            std::memcpy(&netId, value, sizeof(netId));
            existingId = static_cast<int32_t>(netId);
        }
        if (existingId >= 0)
        {
            if (existingId >= array->Num()) return -1;
            FScriptArrayHelper inspect(arrayProperty, array);
            UObject* roundTrip = nullptr;
            std::memcpy(&roundTrip, inspect.GetRawPtr(existingId),
                sizeof(roundTrip));
            return roundTrip == dataAsset ? existingId : -1;
        }

        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        if (static_cast<std::size_t>(array->Num()) >= PS::NetworkIdentityPlan::Capacity)
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

        bool reverseVerified = false;
        if (auto* value = reverse.FindValue(&dataAsset))
        {
            uint16 existingId = 0;
            std::memcpy(&existingId, value, sizeof(existingId));
            reverseVerified = existingId == netId;
        }
        FScriptArrayHelper inspect(arrayProperty, array);
        if (!reverseVerified || netId >= inspect.Num()) return -1;
        UObject* roundTrip = nullptr;
        std::memcpy(&roundTrip, inspect.GetRawPtr(netId), sizeof(roundTrip));
        return roundTrip == dataAsset ? static_cast<int32_t>(netId) : -1;
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
        auto* stored = helper.FindValue(&key);
        UObject* existingValue = nullptr;
        if (stored) std::memcpy(&existingValue, stored, sizeof(existingValue));
        return existingValue == value;
    }
}
