#include <algorithm>
#include <array>
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
#include <unordered_set>
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
#include "Runtime/PluginCatalog.h"
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
#include "Loader/OwnedContentLedger.h"
#include "Misc/FileWatchWrapper.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
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
        DatatableSerialize_Hook = {};

        GameInstanceInit_Hook = {};

        GetPakFolders_Hook = {};

        DatatableSerializeCallbacks.clear();
        GameInstanceInitCallbacks.clear();
        m_registryBridge.Stop();
        m_dataRegistrar.Shutdown();

        if (AutoReloadCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(AutoReloadCallbackId);
        }
        if (m_coreStartupCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_coreStartupCallbackId);

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
        // Some WinGDK builds do not materialize the DataTable CDO/vtable during
        // PreInitialize. Retry once Unreal is ready, without duplicating a hook
        // that was already installed successfully.
        if (DatatableSerializeCallbacks.empty())
            HookDatatableSerialize();
        HookGameInstanceInit();
        // GameInstance::Init and the optional DataTable hook remain the earliest
        // paths.  A game-thread tick is the storefront-agnostic fallback.  It
        // avoids initializing loaders in on_unreal_init before their target
        // assets/tables are ready, while still completing before a player can
        // select and deserialize a character.
        Hook::FCallbackOptions startupOptions{};
        startupOptions.OwnerModName=TEXT("RuneSchema");
        startupOptions.HookName=TEXT("CoreStartupFallback");
        m_coreStartupCallbackId=Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&,UEngine*,float,bool) {
                if(m_coreStartupComplete.load(std::memory_order_acquire))return;
                if(InitCore())m_coreStartupComplete.store(true,std::memory_order_release);
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
        OwnedContent::BeginSnapshot(OwnedContent::LedgerPath(
            PS::HostServices::StateDirectory()));
        InitializeMods(EEngineLifecyclePhase::PostEngineInit);
        LoadMods(EEngineLifecyclePhase::PostEngineInit);
        // Active owners and their persistent IDs are now known.  Permanently
        // remove records owned by absent/disabled mods before the character
        // selection/load path can deserialize them.
        m_dataRegistrar.PrepareRetiredContent();
        PS::StartupTrace::Mark("PostEngineInit complete");
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

        if (m_buildingLoader)
        {
            try {m_buildingLoader->ActivateWorldRegistration();}
            catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[DEGRADED][LOADER:buildings] World registration disabled: {}.\n"),PS::ToWideSafe(error.what()));}
        }

        if (m_stringLoader)
        {
            try {m_stringLoader->ApplyPending();}
            catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[DEGRADED][LOADER:strings] Pending strings were not applied: {}.\n"),PS::ToWideSafe(error.what()));}
        }

        PS::StartupTrace::Mark("data registrar begin");
        try {m_dataRegistrar.Initialize();}
        catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[DEGRADED][SERVICE:data-registrar] Registration/save cleanup unavailable: {}.\n"),PS::ToWideSafe(error.what()));}
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

        RegisterLoader(std::make_unique<DragonWildsAssetModLoader>());

        RegisterLoader(std::make_unique<DragonWildsBlueprintModLoader>());

        auto recipes = std::make_unique<DragonWildsRecipeModLoader>();
        auto* recipeService = recipes.get();
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

        RegisterLoader(std::make_unique<DragonWildsJournalModLoader>());
        auto loreLoader=std::make_unique<DragonWildsJournalModLoader>(true);
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
                            const auto playersPath = modPath / "players";
                            if (fs::is_directory(playersPath))
                                m_spawnLoader->LoadPlayerRules(playersPath, pendingAutoReload.ModName, true);
                            m_spawnLoader->FinalizePlayerRules();
                            PS::Log<LogLevel::Normal>(
                                STR("Auto-reloaded /nameplates for mod {}\n"),
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

                if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
                {
                    WarnAboutUnknownFolders(modPath, modName);
                }

                for (auto& loader : m_loaders)
                {
                    if(loader->GetModFolderType()=="effects" || loader->GetModFolderType()=="niagara")continue;
                    const auto loaderKind = loader->GetModFolderType();
                    PS::StartupTrace::Mark("load loader begin: " + RC::to_string(modName)
                        + "/" + loaderKind);
                    if (!loader->Load(modPath, modName, engineLifecyclePhase)) modSuccessful=false;
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
                PS::Log<LogLevel::Warning>(STR("[MOD:{}][PARTIAL] Metadata or discovery failed: {}. Other mods continue.\n"), modName, PS::ToWideSafe(e.what()));
            }
        });

        for (auto& loader : m_loaders) {
            PS::StartupTrace::Mark("finalize loader: " + loader->GetModFolderType());
            try { loader->FinalizeLoad(engineLifecyclePhase); }
            catch (const std::exception& e) {
                PS::StartupTrace::Mark("ERROR finalize: " + loader->GetModFolderType());
                PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL] Finalization failed: {}. Other loaders continue.\n"), RC::to_generic_string(loader->GetModFolderType()), PS::ToWideSafe(e.what()));
            }
            catch (...) {
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
                    const auto nameplatesPath = modPath / "nameplates";
                    if (fs::is_directory(nameplatesPath))try {m_spawnLoader->LoadNameplateDefinitions(nameplatesPath, modName);}
                    catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:nameplates][PARTIAL][MOD:{}] Section skipped: {}. Other mods continue.\n"),modName,PS::ToWideSafe(error.what()));}
                });
                try {m_spawnLoader->FinalizeNameplateDefinitions();}
                catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:nameplates][PARTIAL] Finalization failed: {}. Other loaders continue.\n"),PS::ToWideSafe(error.what()));}
            }
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
                    PS::Log<LogLevel::Warning>(
                        STR("[LOADER:players][PARTIAL][MOD:{}] Section skipped: {}. Other sections continue.\n"),
                        modName, PS::ToWideSafe(e.what()));
                }
            });
            try {m_spawnLoader->FinalizePlayerRules();}
            catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("[LOADER:players][PARTIAL] Finalization failed: {}. Other loaders continue.\n"),PS::ToWideSafe(error.what()));}
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
            if (folderType == PS::ModFolderLayout::PakDirectory || folderType == "players" || folderType == "nameplates")
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

            PS::Log<LogLevel::Warning>(STR("{}: unknown folder '{}'. JSON folders: {}, players, nameplates. Put cooked packs in paks/<pack-name>/.\n"),
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
            PS::Log<LogLevel::Error>(STR("Plugin pak order rejected; using the plugin root fallback: {}\n"),PS::ToWideSafe(error.what()));
            pakRoots.push_back(runeSchemaRoot/"plugins");
        }
        const auto modsRoot=GetModsPath();std::vector<RC::StringType> discovered;
        if(fs::is_directory(modsRoot))for(const auto& entry:fs::directory_iterator(modsRoot))
            if(entry.is_directory()&&!entry.is_symlink())discovered.push_back(entry.path().filename().native());
        try {for(const auto& name:ModLoadOrder::Resolve(modsRoot,discovered))pakRoots.push_back(modsRoot/name);}
        catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR("Mod pak order rejected; using the mods root fallback: {}\n"),PS::ToWideSafe(error.what()));
            pakRoots.push_back(modsRoot);
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
