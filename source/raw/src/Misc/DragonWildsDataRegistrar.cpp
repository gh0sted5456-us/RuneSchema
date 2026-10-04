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
#include "Utility/Logging.h"
#include "Core/ConfigFiles.h"
#include "Core/CookedPakRegistryManifest.h"
#include "Core/RegistryProvenance.h"
#include "Core/SaveCleanup.h"
#include "Core/SaveRegistrySnapshot.h"
#include "Runtime/Storefront.h"
#include "Runtime/HostServices.h"
#include "Misc/DragonWildsDataRegistrar.h"
#include "Unreal/UAssetRegistryHelpers.hpp"
#include "Unreal/UAssetRegistry.hpp"
#include "Unreal/FAssetData.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        constexpr std::size_t CharacterSaveLimit = 8 * 1024 * 1024;

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

        std::filesystem::path BackupCharacterSave(
            const std::filesystem::path& source)
        {
            auto backup = source;
            backup += L".runeschema-startup-" + std::to_wstring(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count())
                + L".bak";
            if (!std::filesystem::copy_file(source, backup,
                    std::filesystem::copy_options::none))
                throw std::runtime_error("Character save backup was not created");
            return backup;
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
            const auto backup = BackupCharacterSave(path);
            try {
                PS::ConfigFiles::Write(path, encoded);
                if (ParseCharacterText(PS::ConfigFiles::Read(
                        path, CharacterSaveLimit)) != clean)
                    throw std::runtime_error(
                        "Character save failed post-write verification");
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
        const auto name = function->GetFName();
        return name == FName(TEXT("ProcessPlayerStateLoad"), FNAME_Add)
            || name == FName(TEXT("OnPersistentStoreLoadPlayerResult"), FNAME_Add)
            || name == FName(TEXT("LoadStateFromJson"), FNAME_Add);
    }

    static bool IsCombatComponentReadyFunction(UFunction* function)
    {
        if (!function) return false;
        const auto path = function->GetPathName();
        return path == TEXT("/Script/Engine.PlayerController:ClientRestart")
            || path == TEXT("/Script/Dominion.DominionPlayerController:OnInventoryLoadedFromSave")
            || path == TEXT("/Script/Dominion.DominionPlayerController:OnPersonalInventoryLoadedFromSave");
    }

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
        // Registry mutation is process-startup work, not world lifecycle work.
        // Re-running it while an outgoing world is being destroyed can touch
        // stale subsystem maps and destabilize menu -> world re-entry.
        if (m_initialized) return;

        m_pruner.PrepareForStartup();
        if (!ResolveBindings()) return;

        PreloadMountedPersistenceAssets();

        InstallHooks();
        m_initialized = true;

        RegisterAll();
        // Capture twice during this single startup boundary so only an
        // identical, settled registry can authorize pruning. Nothing refreshes
        // or mutates the registries on later menu/world transitions.
        RegisterAll();
        CleanLocalCharacterSavesOnce();
        PS::Log<LogLevel::Normal>(STR(
            "[REGISTRY][LIFECYCLE][SEALED] Startup registration completed once for this game execution; world transitions are read-only.\n"));
    }

    void DragonWildsDataRegistrar::CleanLocalCharacterSavesOnce()
    {
        m_startupSaveCleanupAttempted = true;
        if (PS::Storefront::CurrentNativeLane()
            == PS::Storefront::NativeLane::GamePassNative)
        {
            PS::Log<LogLevel::Normal>(STR(
                "[PERSISTENCE-PRUNER][STARTUP][PROVIDER-DEFERRED] Xbox WGS character files were not edited directly.\n"));
            return;
        }

        const auto registry = PS::SaveCleanup::ReadRegistry();
        if (!registry || !registry->Ready())
        {
            PS::Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][STARTUP][UNCHANGED] Complete stable item and recipe registries were unavailable; no character file was modified.\n"));
            return;
        }

        std::size_t scanned = 0;
        std::size_t changed = 0;
        std::size_t removed = 0;
        std::uintmax_t bytes = 0;
        const auto directory = LocalCharacterSaveDirectory();
        if (!std::filesystem::is_directory(directory)) return;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != L".json")
                continue;
            if (++scanned > 64 || (bytes += entry.file_size()) > 64 * 1024 * 1024)
                throw std::runtime_error(
                    "Character save directory exceeds safe startup cleanup limits");
            try
            {
                const auto original = PS::ConfigFiles::Read(
                    entry.path(), CharacterSaveLimit);
                const auto decoded = DecodeCharacterText(original);
                const auto source = nlohmann::json::parse(
                    decoded.Utf8, nullptr, true, true);
                if (PS::SaveCleanup::ClassifyCharacterDocument(source)
                    != PS::SaveCleanup::CharacterDocumentKind::Gameplay)
                    continue;
                // This pass runs during GameInstance startup, before the
                // InitGameState pre-callback can register transient
                // RuneSchema quest assets (including the hidden per-mod
                // dialogue state quest).  Item and recipe registries are
                // already complete here, but treating the early native quest
                // or journal view as complete can purge a valid RuneSchema
                // PersistenceID before character hydration.  Those two
                // categories are therefore validated only by the reflected
                // character-load preflight after all live registrations have
                // settled.
                auto startupRegistry = *registry;
                startupRegistry.QuestsComplete = false;
                startupRegistry.JournalsComplete = false;
                const auto plan = PS::SaveCleanup::Plan(source, {}, false,
                    &startupRegistry, false, true, true);
                if (plan.Removed.empty()) continue;
                ReplaceCharacterSave(entry.path(), original, plan.Save,
                    decoded.Utf16Le);
                ++changed;
                removed += plan.Removed.size();
                for (const auto& row : plan.Removed)
                    PS::Log<LogLevel::Warning>(STR(
                        "[PERSISTENCE-PRUNER][STARTUP][ORPHAN-REMOVED] {} '{}' from '{}'.\n"),
                        PS::ToWideSafe(row.value("Kind", std::string("Unknown")).c_str()),
                        PS::ToWideSafe(row.value("Id", std::string("<unknown>")).c_str()),
                        entry.path().filename().wstring());
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR(
                    "[PERSISTENCE-PRUNER][STARTUP][UNCHANGED] '{}' was not modified: {}.\n"),
                    entry.path().filename().wstring(),
                    PS::ToWideSafe(error.what()));
            }
        }
        if (changed)
            PS::Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][STARTUP][COMPLETE] Atomically removed {} unresolved persistence reference(s) from {} character save(s). Cleanup will not run again until game restart.\n"),
                removed, changed);
        else
            PS::Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][STARTUP][CHECKED] {} character save(s) checked; every applicable persistence ID resolved.\n"),
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
                PS::Log<LogLevel::Warning>(STR("Registry pair {} -> {} was not found and won't be handled.\n"),
                    binding.DataClassPath, binding.SubsystemClassPath);
                continue;
            }
            if (binding.ExcludedClassPath && !excludedClass)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Registry exclusion {} was not found; {} registration is disabled to prevent cross-registry IDs.\n"),
                    binding.ExcludedClassPath, binding.DataClassPath);
                continue;
            }

            m_bindings.push_back({dataClass, subsystemClass, excludedClass,
                binding.CleanupAuthority, binding.StatusTag});
        }

        if (m_bindings.empty())
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize the Data Registrar, no registry subsystems were found.\n"));
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
        const auto manifests = PS::CookedPakRegistryManifest::Snapshot();
        for (const auto& manifest : manifests)
        {
            if (!manifest.MeleeAttackClasses.empty())
            {
                const auto duplicate = std::ranges::find_if(
                    m_manifestMeleeCollections, [&](const auto& existing) {
                        return existing.Owner == manifest.Owner
                            && existing.Source == manifest.Source;
                    });
                if (duplicate == m_manifestMeleeCollections.end())
                    m_manifestMeleeCollections.push_back({manifest.Owner,
                        manifest.Source, manifest.MeleeAttackClasses});
            }

            for (const auto& lane : manifest.AssetLanes)
            {
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
                    PS::Log<LogLevel::Error>(STR(
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
                        || (binding->ExcludedClass && object->IsA(binding->ExcludedClass)))
                    {
                        valid = false;
                        failedPath = path;
                        break;
                    }
                    resolvedObjects.push_back(object);
                }
                if (!valid)
                {
                    m_rejectedManifestAssets.insert(
                        resolvedObjects.begin(), resolvedObjects.end());
                    PS::Log<LogLevel::Error>(STR(
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
            PS::Log<LogLevel::Warning>(STR(
                "[REGISTRY][PAK-DISCOVERY][SKIPPED] Live Asset Registry query failed: {}; already-loaded PAK assets will still be registered.\n"),
                PS::ToWideSafe(error.what()));
            return;
        }
        catch (...)
        {
            PS::Log<LogLevel::Warning>(STR(
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
                PS::Log<LogLevel::Error>(STR(
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
            PS::Log<LogLevel::Warning>(STR(
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
        for (auto* attack : m_ownedFlintlockAttackRoots)
            if (attack && attack->IsRootSet()) attack->ClearRootSet();
        m_ownedAdditionalWeaponAttackRoots.clear();
        m_additionalWeaponAttackClasses.clear();
        m_ownedFlintlockAttackRoots.clear();
        m_flintlockAttackClasses.clear();
        for (auto& collection : m_manifestMeleeCollections)
            for (auto* attack : collection.OwnedRoots)
                if (attack && attack->IsRootSet()) attack->ClearRootSet();
        m_manifestMeleeCollections.clear();
        m_rejectedManifestAssets.clear();
        m_additionalWeaponsReadyReported = false;
        m_additionalWeaponsIncompleteReported = false;
        m_flintlocksReadyReported = false;
        m_flintlocksIncompleteReported = false;
        m_registryStatusReported.clear();
        m_registryWaitingReported.clear();
        m_registrySummaryReported = false;
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

        // Runtime attack collections are component state, not persistence
        // registries. Apply the already-resolved ordered classes after a
        // player controller becomes world-ready, scoped to that exact world.
        // This lane may run for each new world but never refreshes ItemData,
        // recipes, quests, spells, or the pruning snapshot.
        Hook::FCallbackOptions combatOptions{};
        combatOptions.OwnerModName = TEXT("RuneSchema");
        combatOptions.HookName = TEXT("CombatComponentWorldReady");
        m_combatLifecycleHook = Hook::RegisterProcessEventPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UObject* source,
                UFunction* function, void*) {
                if (!source || !IsCombatComponentReadyFunction(function))
                    return;
                auto* world = source->GetWorld();
                if (!world || world->HasAnyFlags(static_cast<EObjectFlags>(
                        RF_BeginDestroyed | RF_FinishDestroyed))
                    || world->GetPathName().contains(TEXT("L_FrontEnd")))
                    return;
                BootstrapCombatRegistries(world);
            }, combatOptions);
        if (m_combatLifecycleHook != Hook::ERROR_ID)
            PS::Log<LogLevel::Normal>(STR(
                "[LIFECYCLE][COMBAT-COMPONENT][READY] World-scoped attack collection lane installed.\n"));
        else
            PS::Log<LogLevel::Warning>(STR(
                "[LIFECYCLE][COMBAT-COMPONENT][UNAVAILABLE] Custom attack collections cannot attach to new player components.\n"));
    }

    void DragonWildsDataRegistrar::ScrubCharacterJsonBeforeLoad(
        UObject* context, UFunction* function, void* parameters)
    {
        m_pruner.PruneBeforeCharacterLoad(context, function, parameters);
    }

    void DragonWildsDataRegistrar::RegisterAll()
    {
        BootstrapCombatRegistries();

        PS::SaveCleanup::RegistrySnapshot snapshot;
        bool itemsReady = false;
        bool recipesReady = false;
        bool questsReady = false;
        // The settled live registries remain authoritative. PAK discovery is
        // a one-time loading aid, not a second ownership or pruning ledger.
        bool registrationsComplete = true;

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
                if (binding.CleanupAuthority)
                    registrationsComplete = registered && registrationsComplete;

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
                idMap.ForEachPair([&](void* keyPtr, void*) {
                    auto* key = static_cast<FString*>(keyPtr);
                    if (key && key->GetCharArray().Num() > 1)
                    {
                        ++persistenceIds;
                        if (target)
                            target->insert(RC::to_string(
                                RC::StringType(**key)));
                    }
                });
            }
            if (!foundSubsystem && binding.CleanupAuthority)
            {
                registrationsComplete = false;
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
                PS::Log<LogLevel::Normal>(STR(
                    "[REGISTRY][PERSISTENCE-PRUNER][READY] base game and loaded paks: items={}, recipes={}, quests={}, journal={}.\n"),
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

    void DragonWildsDataRegistrar::BootstrapCombatRegistries(UWorld* world)
    {
        static constexpr const TCHAR* ComponentClassPath =
            TEXT("/Script/Dominion.PlayerMeleeAttackComponent");
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
        static constexpr std::array<const TCHAR*, 3> FlintlockAttackClassPaths{{
            TEXT("/Game/Mods/AdditionalWeapons/Flintlocks/Gameplay/Attacks/BP_Flintlock_Single_Shot.BP_Flintlock_Single_Shot_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Flintlocks/Gameplay/Attacks/BP_Flintlock_Dual_RightShot.BP_Flintlock_Dual_RightShot_C"),
            TEXT("/Game/Mods/AdditionalWeapons/Flintlocks/Gameplay/Attacks/BP_Flintlock_Dual_LeftShot.BP_Flintlock_Dual_LeftShot_C"),
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

        const bool additionalWeaponsReady = resolveAttackCollection(
            AdditionalWeaponAttackClassPaths, m_additionalWeaponAttackClasses,
            m_ownedAdditionalWeaponAttackRoots, m_additionalWeaponsIncompleteReported,
            TEXT("AdditionalWeapons"));
        const bool flintlocksReady = resolveAttackCollection(
            FlintlockAttackClassPaths, m_flintlockAttackClasses,
            m_ownedFlintlockAttackRoots, m_flintlocksIncompleteReported,
            TEXT("Flintlocks"));

        const auto resolveManifestCollection = [&](ManifestMeleeCollection& collection) {
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
                            "[PAK-REGISTRY][REJECTED] owner='{}' lane='MeleeAttackClasses' resolved={}/{} missing='{}'; no component was modified.\n"),
                            PS::ToWideSafe(collection.Owner.c_str()), resolved.size(),
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
        bool anyManifestReady = false;
        for (auto& collection : m_manifestMeleeCollections)
            anyManifestReady = resolveManifestCollection(collection) || anyManifestReady;
        if (!additionalWeaponsReady && !flintlocksReady && !anyManifestReady) return;

        if (!world)
        {
            const auto reportResolved = [](const TCHAR* owner,
                const std::vector<UClass*>& classes, bool& reported) {
                if (classes.empty() || reported) return;
                PS::Log<LogLevel::Normal>(STR(
                    "[COMBAT-REGISTRY][MELEE-ATTACK][RESOLVED] {}: {} ordered attack classes loaded; awaiting a gameplay player component.\n"),
                    owner, classes.size());
                reported = true;
            };
            if (additionalWeaponsReady)
            {
                reportResolved(TEXT("AdditionalWeapons"),
                    m_additionalWeaponAttackClasses, m_additionalWeaponsReadyReported);
            }
            if (flintlocksReady)
            {
                reportResolved(TEXT("Flintlocks"),
                    m_flintlockAttackClasses, m_flintlocksReadyReported);
            }
            for (auto& collection : m_manifestMeleeCollections)
            {
                if (collection.Classes.empty() || collection.ReadyReported) continue;
                PS::Log<LogLevel::Normal>(STR(
                    "[PAK-REGISTRY][MELEE-ATTACK][RESOLVED] owner='{}' ordered_classes={}; awaiting a gameplay player component.\n"),
                    PS::ToWideSafe(collection.Owner.c_str()), collection.Classes.size());
                collection.ReadyReported = true;
            }
            return;
        }

        auto* componentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, ComponentClassPath, false);
        if (!componentClass) return;

        TArray<UObject*> components;
        UECustom::UObjectGlobals::GetObjectsOfClass(componentClass, components, true);
        std::size_t additionalChangedComponents = 0;
        std::size_t additionalAddedClasses = 0;
        std::size_t flintlockChangedComponents = 0;
        std::size_t flintlockAddedClasses = 0;
        for (auto* component : components)
        {
            if (!component || component->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject
                    | RF_BeginDestroyed | RF_FinishDestroyed)))
                continue;
            if (component->GetWorld() != world) continue;
            auto* property = CastField<FArrayProperty>(
                PropertyHelper::GetPropertyByName(component->GetClassPrivate(),
                    TEXT("AttackDataCollection")));
            if (!property || !CastField<FClassProperty>(property->GetInner())
                || property->GetInner()->GetElementSize() != sizeof(UClass*))
                continue;
            auto* array = property->ContainerPtrToValuePtr<FScriptArray>(component);
            if (!array || array->Num() < 0) continue;
            UECustom::FScriptArrayHelper helper(array, property);
            struct AppendResult { std::size_t Added = 0; bool Conflict = false; };
            const auto appendCollection = [&](const std::vector<UClass*>& classes) {
                AppendResult result;
                std::vector<std::size_t> positions;
                std::size_t index = 0;
                helper.ForEachElement([&](void* value) {
                    UClass* existing = nullptr;
                    std::memcpy(&existing, value, sizeof(existing));
                    const auto declared = std::ranges::find(classes, existing);
                    if (declared != classes.end())
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
            const auto additionalResult = additionalWeaponsReady
                ? appendCollection(m_additionalWeaponAttackClasses) : AppendResult{};
            const auto flintlockResult = flintlocksReady
                ? appendCollection(m_flintlockAttackClasses) : AppendResult{};
            if (additionalResult.Added)
            {
                ++additionalChangedComponents;
                additionalAddedClasses += additionalResult.Added;
            }
            if (flintlockResult.Added)
            {
                ++flintlockChangedComponents;
                flintlockAddedClasses += flintlockResult.Added;
            }
            if (additionalResult.Conflict && !m_additionalWeaponsIncompleteReported)
            {
                PS::Log<LogLevel::Error>(STR(
                    "[COMBAT-REGISTRY][MELEE-ATTACK][CONFLICT] AdditionalWeapons is partially present or out of order; live collection was not modified.\n"));
                m_additionalWeaponsIncompleteReported = true;
            }
            if (flintlockResult.Conflict && !m_flintlocksIncompleteReported)
            {
                PS::Log<LogLevel::Error>(STR(
                    "[COMBAT-REGISTRY][MELEE-ATTACK][CONFLICT] Flintlocks is partially present or out of order; live collection was not modified.\n"));
                m_flintlocksIncompleteReported = true;
            }
            for (auto& collection : m_manifestMeleeCollections)
            {
                if (collection.Classes.empty()) continue;
                const auto result = appendCollection(collection.Classes);
                if (result.Conflict)
                {
                    if (!collection.ConflictReported)
                    {
                        PS::Log<LogLevel::Error>(STR(
                            "[PAK-REGISTRY][MELEE-ATTACK][CONFLICT] owner='{}': declared collection is partially present or out of order; live collection was not modified.\n"),
                            PS::ToWideSafe(collection.Owner.c_str()));
                        collection.ConflictReported = true;
                    }
                    continue;
                }
                if (result.Added || !collection.ReadyReported)
                {
                    PS::Log<LogLevel::Normal>(STR(
                        "[PAK-REGISTRY][MELEE-ATTACK][{}] owner='{}' ordered_classes={} appended={}; live collection is authoritative.\n"),
                        result.Added ? TEXT("ADDED") : TEXT("READY"),
                        PS::ToWideSafe(collection.Owner.c_str()),
                        collection.Classes.size(), result.Added);
                    collection.ReadyReported = true;
                }
            }
        }
        const auto reportAttached = [](const TCHAR* owner,
            const std::vector<UClass*>& classes, std::size_t changedComponents,
            std::size_t addedClasses, bool& reported) {
            if (classes.empty() || (!addedClasses && reported)) return;
            PS::Log<LogLevel::Normal>(STR(
                "[COMBAT-REGISTRY][MELEE-ATTACK][{}] {}: {} ordered attack classes; updated {} live melee component(s), appended {} class reference(s).\n"),
                addedClasses ? TEXT("ADDED") : TEXT("READY"),
                owner, classes.size(), changedComponents, addedClasses);
            reported = true;
        };
        if (additionalWeaponsReady)
        {
            reportAttached(TEXT("AdditionalWeapons"),
                m_additionalWeaponAttackClasses, additionalChangedComponents,
                additionalAddedClasses, m_additionalWeaponsReadyReported);
        }
        if (flintlocksReady)
        {
            reportAttached(TEXT("Flintlocks"), m_flintlockAttackClasses,
                flintlockChangedComponents, flintlockAddedClasses,
                m_flintlocksReadyReported);
        }
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

        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(dataClass, candidates, true);
        constexpr std::size_t IdentityDetailLimit = 12;
        std::size_t identityDetailLines = 0;
        std::size_t identityDetailsOmitted = 0;

        for (auto* candidate : candidates)
        {
            try
            {
                if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                {
                    continue;
                }
                if (m_rejectedManifestAssets.contains(candidate)) continue;
                if (excludedClass && candidate->IsA(excludedClass)) continue;

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
                const bool runeSchemaAuthored = stats
                    && PS::RegistryProvenance::IsRuneSchemaItem(
                        RC::to_string(idString));
                candidate->SetRootSet();

                const auto existing = known.find(idString);
                const bool inserted = existing == known.end();
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
                if (inserted)
                {
                    if (addedCount) ++*addedCount;
                    if (stats)
                    {
                        if (runeSchemaAuthored) ++stats->AddedRuneSchema;
                        else ++stats->AddedCookedOrPak;
                    }
                    if (identityDetailLines < IdentityDetailLimit)
                    {
                        ++identityDetailLines;
                        PS::Log<LogLevel::Verbose>(STR(
                            "[REGISTRY][{}][IDENTITY] source={} PersistenceID='{}' asset='{}' subsystem='{}' networkId={}.\n"),
                            statusTag,
                            runeSchemaAuthored ? TEXT("runeschema")
                                               : TEXT("cooked-or-pak"),
                            *persistenceId, candidate->GetPathName(),
                            subsystem->GetClassPrivate()->GetName(), networkId);
                    }
                    else ++identityDetailsOmitted;
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
        if (identityDetailsOmitted)
            PS::Log<LogLevel::Verbose>(STR(
                "[REGISTRY][{}][IDENTITY-SUMMARY] {} additional registered identities omitted; aggregate counts follow.\n"),
                statusTag, identityDetailsOmitted);
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
