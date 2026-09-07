#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <vector>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/UnrealInitializer.hpp"
#include <unordered_map>
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Hooks.hpp"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/StartupTrace.h"
#include "Utility/InlineHook.h"
#include "Utility/ModFolderLayout.h"
#include "SDK/Helper/Memory.h"
#include "SDK/DragonWildsSignatures.h"
#include "SDK/StaticClassStorage.h"
#include "SDK/UnrealOffsets.h"
#include "Runtime/HostServices.h"
#include "Loader/DragonWildsRawTableLoader.h"
#include "Loader/DragonWildsAssetModLoader.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/DragonWildsEnumLoader.h"
#include "Loader/DragonWildsRecipeModLoader.h"
#include "Loader/DragonWildsJournalModLoader.h"
#include "Loader/DragonWildsBuildingModLoader.h"
#include "Loader/DragonWildsCourseLoader.h"
#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/DragonWildsStringModLoader.h"
#include "Loader/DragonWildsEquipmentLoader.h"
#include "Loader/DragonWildsMainLoader.h"
#include "Loader/ModLoadOrder.h"
#include "Misc/FileWatchWrapper.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

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
        PS::StartupTrace::Begin(GetModsPath().parent_path() / "diagnostics");
        PS::StartupTrace::Mark("construct loaders begin");
        CreateLoaders();
        PS::StartupTrace::Mark("construct loaders complete");
    }

    DragonWildsMainLoader::~DragonWildsMainLoader()
    {
        // Stop the watcher before destroying its consumers.
        PS::StartupTrace::Mark("shutdown begin");
        m_fileWatcher.reset();
        DatatableSerialize_Hook = {};

        GameInstanceInit_Hook = {};

        GetPakFolders_Hook = {};

        DatatableSerializeCallbacks.clear();
        GameInstanceInitCallbacks.clear();
        m_dataRegistrar.Shutdown();

        if (AutoReloadCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(AutoReloadCallbackId);
        }

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
        HookDatatableSerialize();
        SetupAlternativePakPathReader();
    }

    void DragonWildsMainLoader::Initialize()
	{
        m_readiness.MarkUnrealReady();
        HookGameInstanceInit();
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
        auto folderType = it->string();

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
                    if (entry.is_directory()) discovered.push_back(entry.path().filename().native());
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
        PS::StartupTrace::Mark("PostEngineInit complete");
    }

    void DragonWildsMainLoader::SetupGameInstanceInitLoaders()
    {
        PS::StartupTrace::Mark("GameInstanceInit loaders begin");
        InitializeMods(EEngineLifecyclePhase::GameInstanceInit);
        LoadMods(EEngineLifecyclePhase::GameInstanceInit);

        if (m_buildingLoader)
        {
            m_buildingLoader->ActivateWorldRegistration();
        }

        if (m_stringLoader)
        {
            m_stringLoader->ApplyPending();
        }

        PS::StartupTrace::Mark("data registrar begin");
        m_dataRegistrar.Initialize();
        PS::StartupTrace::Mark("GameInstanceInit loaders complete (deferred world work may remain)");
    }

    void DragonWildsMainLoader::HookDatatableSerialize()
    {
        auto DatatableSerializeFuncPtr = DragonWilds::SignatureManager::GetSignature("UDataTable::Serialize");
        if (!DatatableSerializeFuncPtr)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize RuneSchema core, signature for UDataTable::Serialize is outdated.\n"));
            return;
        }

        DatatableSerializeCallbacks.push_back([&](RC::Unreal::UDataTable* datatable) {
            if (m_readiness.Observe(datatable)) m_datatableRegistry.Add(datatable);
            else if (m_readiness.IsUnrealReady() && IsInGameThreadRaw()) InitCore();
        });

        if (!PS::InstallInlineHook(DatatableSerialize_Hook, reinterpret_cast<void*>(DatatableSerializeFuncPtr), reinterpret_cast<void*>(OnDataTableSerialized))) {
            DatatableSerializeCallbacks.clear();
            PS::StartupTrace::Mark("ERROR DataTable hook installation");
            PS::Log<LogLevel::Error>(STR("Unable to install DataTable serialization hook.\n"));
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
            if (InitCore()) SetupGameInstanceInitLoaders();
        });

        if (!PS::InstallInlineHook(GameInstanceInit_Hook, GameInstanceInitPtr, reinterpret_cast<void*>(OnGameInstanceInit))) {
            GameInstanceInitCallbacks.clear();
            PS::StartupTrace::Mark("ERROR GameInstance hook installation");
            PS::Log<LogLevel::Error>(STR("Unable to install GameInstance initialization hook.\n"));
        }
    }

    void DragonWildsMainLoader::CreateLoaders()
    {
        RegisterLoader(std::make_unique<DragonWildsEquipmentLoader>());
        RegisterLoader(std::make_unique<DragonWildsEnumLoader>());

        RegisterLoader(std::make_unique<DragonWildsRawTableLoader>());

        RegisterLoader(std::make_unique<DragonWildsAssetModLoader>());

        RegisterLoader(std::make_unique<DragonWildsBlueprintModLoader>());

        RegisterLoader(std::make_unique<DragonWildsRecipeModLoader>());

        RegisterLoader(std::make_unique<DragonWildsJournalModLoader>());

        auto buildingModLoader = std::make_unique<DragonWildsBuildingModLoader>();
        m_buildingLoader = buildingModLoader.get();
        RegisterLoader(std::move(buildingModLoader));

        auto spawnLoader = std::make_unique<DragonWildsSpawnLoader>();
        m_spawnLoader = spawnLoader.get();
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
                        for (auto& loader : m_loaders)
                        {
                            if (handled) break;
                            if (loader->GetModFolderType() == pendingAutoReload.FolderType)
                            {
                                loader->AutoReload(pendingAutoReload.ModName, pendingAutoReload.FilePath);
                                PS::Log<LogLevel::Normal>(STR("Auto-reloaded mod {}\n"), pendingAutoReload.ModName);
                                handled = true;
                                break;
                            }
                        }

                        if (!handled)
                        {
                            PS::Log<LogLevel::Warning>(STR("No loader found for folder '{}'.\n"),
                                RC::to_generic_string(pendingAutoReload.FolderType));
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
            PS::Log<LogLevel::Error>(STR("Early table queue exceeded 4096 entries; loader initialization stopped.\n"));
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
            return true;
        } catch (const std::exception& error) {
            m_readiness.Fail();
            PS::Log<LogLevel::Error>(STR("Readiness-gated initialization failed: {}\n"), PS::ToWideSafe(error.what()));
            return false;
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
                PS::Log<LogLevel::Error>(STR("Loader {} initialization failed: {}\n"), RC::to_generic_string(loader->GetModFolderType()), PS::ToWideSafe(e.what()));
            }
        }
    }

    void DragonWildsMainLoader::LoadMods(EEngineLifecyclePhase engineLifecyclePhase)
    {
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
                    PS::Log<LogLevel::Error>(
                        STR("Appearance source '{}' was rejected safely: {}\n"),
                        modName, PS::ToWideSafe(e.what()));
                }
            });
        }

        IterateModsFolder([&](const fs::path& modPath, const fs::path::string_type& modName)
        {
            try
            {
                PS::StartupTrace::Mark("load mod: " + RC::to_string(modName));
                PS::Log<LogLevel::Verbose>(STR("Loading mod: {}\n"), modName);

                if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
                {
                    WarnAboutUnknownFolders(modPath, modName);
                }

                for (auto& loader : m_loaders)
                {
                    try { loader->Load(modPath, modName, engineLifecyclePhase); }
                    catch (const std::exception& e) {
                        PS::StartupTrace::Mark("ERROR load " + RC::to_string(modName) + "/" + loader->GetModFolderType());
                        PS::Log<LogLevel::Error>(STR("Failed to load {}/{}: {}\n"), modName, RC::to_generic_string(loader->GetModFolderType()), PS::ToWideSafe(e.what()));
                    }
                }
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed to load mod {} - {}\n"), modName, PS::ToWideSafe(e.what()));
            }
        });

        for (auto& loader : m_loaders) {
            PS::StartupTrace::Mark("finalize loader: " + loader->GetModFolderType());
            try { loader->FinalizeLoad(engineLifecyclePhase); }
            catch (const std::exception& e) {
                PS::StartupTrace::Mark("ERROR finalize: " + loader->GetModFolderType());
                PS::Log<LogLevel::Error>(STR("Loader {} finalization failed: {}\n"), RC::to_generic_string(loader->GetModFolderType()), PS::ToWideSafe(e.what()));
            }
        }

        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit && m_spawnLoader
            && PS::PSConfig::Get()->IsLoaderEnabled("players"))
        {
            IterateModsFolder([&](const fs::path& modPath,
                const fs::path::string_type& modName)
            {
                const auto playersPath = modPath / "players";
                if (!fs::is_directory(playersPath)) return;
                try
                {
                    m_spawnLoader->LoadPlayerRules(playersPath, modName);
                }
                catch (const std::exception& e)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Failed to load /players for mod {} - {}\n"),
                        modName, PS::ToWideSafe(e.what()));
                }
            });
            m_spawnLoader->FinalizePlayerRules();
        }
    }

    void DragonWildsMainLoader::WarnAboutUnknownFolders(const fs::path& modPath, const RC::StringType& modName)
    {
        for (const auto& entry : fs::directory_iterator(modPath))
        {
            if (!entry.is_directory())
            {
                continue;
            }

            auto folderType = entry.path().filename().string();
            if (folderType == PS::ModFolderLayout::PakDirectory || folderType == "players")
            {
                continue;
            }

            auto known = std::any_of(m_loaders.begin(), m_loaders.end(),
                [&](const auto& loader) { return loader->GetModFolderType() == folderType; });
            if (known)
            {
                continue;
            }

            std::error_code error;
            if (PS::ModFolderLayout::ContainsLegacyPakContent(entry.path(), error))
            {
                continue;
            }
            if (error)
            {
                PS::Log<LogLevel::Warning>(STR("{}: could not inspect folder '{}': {}\n"),
                    modName, RC::to_generic_string(folderType), PS::ToWideSafe(error.message().c_str()));
                continue;
            }

            RC::StringType knownFolders;
            for (auto& loader : m_loaders)
            {
                if (!knownFolders.empty())
                {
                    knownFolders += STR(", ");
                }
                knownFolders += RC::to_generic_string(loader->GetModFolderType());
            }

            PS::Log<LogLevel::Warning>(STR("{}: unknown folder '{}'. JSON folders: {}, players. Put cooked packs in paks/<pack-name>/.\n"),
                modName, RC::to_generic_string(folderType), knownFolders);
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
            PS::Log<LogLevel::Error>(STR("RuneSchema won't be able to load paks from the RuneSchema/mods folder.\n"));
            return;
        }

        auto ModsFolderPath = GetModsPath();
        auto AbsolutePath = ModsFolderPath.native();
        auto AbsolutePathWithSuffix = std::format(STR("{}/"), RC::to_generic_string(AbsolutePath));

        OutPakFolders->Add(FString(AbsolutePathWithSuffix.c_str()));

        PS::Log<LogLevel::Verbose>(STR("Added extra .pak read directory at {}\n"), AbsolutePathWithSuffix);
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
