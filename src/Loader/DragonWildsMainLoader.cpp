#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <vector>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Hooks.hpp"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "SDK/Helper/Memory.h"
#include "SDK/DragonWildsSignatures.h"
#include "SDK/StaticClassStorage.h"
#include "SDK/UnrealOffsets.h"
#include "UE4SSProgram.hpp"
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
#include "Loader/ModLoadOrder.h"
#include "Loader/CompatibilityReporter.h"
#include "Loader/DragonWildsMainLoader.h"
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
    bool AutoReloadCallbackActive = false;

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
        CreateLoaders();
    }

    DragonWildsMainLoader::~DragonWildsMainLoader()
    {
        auto expected1 = DatatableSerialize_Hook.disable();
        DatatableSerialize_Hook = {};

        auto expected2 = GameInstanceInit_Hook.disable();
        GameInstanceInit_Hook = {};

        auto expected4 = GetPakFolders_Hook.disable();
        GetPakFolders_Hook = {};

        DatatableSerializeCallbacks.clear();
        GameInstanceInitCallbacks.clear();
        GetPakFoldersCallback.clear();

        if (AutoReloadCallbackActive && AutoReloadCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(AutoReloadCallbackId);
        }

        AutoReloadCallbackActive = false;
        AutoReloadCallbackId = Hook::ERROR_ID;

        {
            std::scoped_lock lock{AutoReloadMutex};
            PendingAutoReloads.clear();
        }

    }

    void DragonWildsMainLoader::PreInitialize()
    {
        HookDatatableSerialize();
        SetupAlternativePakPathReader();
    }

    void DragonWildsMainLoader::Initialize()
	{
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
        if (folderType == "config")
        {
            return;
        }

        std::ifstream f(filePath);
        if (f.peek() == std::ifstream::traits_type::eof()) {
            return;
        }
        f.close();

        bool shouldRegisterCallback = false;
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

            shouldRegisterCallback = !AutoReloadCallbackActive;
            AutoReloadCallbackActive = true;
        }

        if (!shouldRegisterCallback) return;

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("DragonWildsAutoReload");

        AutoReloadCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>& callbackData, UEngine*, float, bool) {
                std::vector<PendingAutoReload> pendingAutoReloads;
                {
                    std::scoped_lock lock{AutoReloadMutex};
                    pendingAutoReloads.swap(PendingAutoReloads);
                }

                for (const auto& pendingAutoReload : pendingAutoReloads)
                {
                    try
                    {
                        bool handled = false;
                        for (auto& loader : m_loaders)
                        {
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

                {
                    std::scoped_lock lock{AutoReloadMutex};
                    if (!PendingAutoReloads.empty())
                    {
                        return;
                    }
                }

                AutoReloadCallbackActive = false;
                AutoReloadCallbackId = Hook::ERROR_ID;
                callbackData.RemoveSelf();
            },
            options);

        if (AutoReloadCallbackId == Hook::ERROR_ID)
        {
            AutoReloadCallbackActive = false;
            PS::Log<LogLevel::Error>(STR("Failed to register auto-reload engine tick callback.\n"));
            return;
        }
    }

    void DragonWildsMainLoader::IterateModsFolder(const std::function<void(const std::filesystem::path&, const RC::StringType&)>& callback)
    {
        static auto modsPath = fs::path(UE4SSProgram::get_program().get_working_directory()) / "Mods" / "RuneSchema" / "mods";
        if (!fs::exists(modsPath))
        {
            return;
        }

        // fs::directory_iterator's order is filesystem-defined, not a deliberate
        // load order, so it's only used here to discover which mod folders exist.
        // ModLoadOrder::Resolve() is what actually decides load order, driven by a
        // `mods.txt` file inside the mods folder (mirroring UE4SS's own mods.txt):
        // mods are loaded top-to-bottom, new folders are appended automatically,
        // and a mod can be disabled by setting its entry to `Name : 0` without
        // removing its folder.
        std::vector<RC::StringType> discoveredModNames;
        for (const auto& entry : fs::directory_iterator(modsPath))
        {
            if (entry.is_directory())
            {
                // Folder names may legitimately contain dots (for example a
                // version suffix). stem() treats the final portion as a file
                // extension and truncates it, so preserve the exact name.
                discoveredModNames.push_back(entry.path().filename().native());
            }
        }
        auto orderedModNames = ModLoadOrder::Resolve(modsPath, discoveredModNames);

        if (!m_compatibilityReportGenerated)
        {
            CompatibilityReporter::Generate(modsPath, orderedModNames);
            m_compatibilityReportGenerated = true;
        }

        for (const auto& modName : orderedModNames)
        {
            callback(modsPath / modName, modName);
        }
    }

    void DragonWildsMainLoader::SetupPostEngineInitLoaders()
    {
        InitializeMods(EEngineLifecyclePhase::PostEngineInit);
        LoadMods(EEngineLifecyclePhase::PostEngineInit);
    }

    void DragonWildsMainLoader::SetupGameInstanceInitLoaders()
    {
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

        m_dataRegistrar.Initialize();
    }

    void DragonWildsMainLoader::HookDatatableSerialize()
    {
        auto DatatableSerializeFuncPtr = DragonWilds::SignatureManager::GetSignature("UDataTable::Serialize");
        if (!DatatableSerializeFuncPtr)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize RuneSchema core, signature for UDataTable::Serialize is outdated.\n"));
            return;
        }

        DatatableSerialize_Hook = safetyhook::create_inline(reinterpret_cast<void*>(DatatableSerializeFuncPtr),
            OnDataTableSerialized);

        DatatableSerializeCallbacks.push_back([&](RC::Unreal::UDataTable* datatable) {
            InitCore();
            m_datatableRegistry.Add(datatable);
        });

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
            SetupGameInstanceInitLoaders();
        });

        GameInstanceInit_Hook = safetyhook::create_inline(GameInstanceInitPtr,
            reinterpret_cast<void*>(OnGameInstanceInit));
    }

    void DragonWildsMainLoader::CreateLoaders()
    {
        auto enumLoader = std::make_unique<DragonWildsEnumLoader>();
        RegisterLoader(std::move(enumLoader));

        auto rawTableModLoader = std::make_unique<DragonWildsRawTableLoader>();
        RegisterLoader(std::move(rawTableModLoader));

        auto assetModLoader = std::make_unique<DragonWildsAssetModLoader>();
        RegisterLoader(std::move(assetModLoader));

        auto blueprintModLoader = std::make_unique<DragonWildsBlueprintModLoader>();
        RegisterLoader(std::move(blueprintModLoader));

        auto recipeModLoader = std::make_unique<DragonWildsRecipeModLoader>();
        RegisterLoader(std::move(recipeModLoader));

        auto journalModLoader = std::make_unique<DragonWildsJournalModLoader>();
        RegisterLoader(std::move(journalModLoader));

        auto buildingModLoader = std::make_unique<DragonWildsBuildingModLoader>();
        m_buildingLoader = buildingModLoader.get();
        RegisterLoader(std::move(buildingModLoader));

        auto spawnLoader = std::make_unique<DragonWildsSpawnLoader>();
        RegisterLoader(std::move(spawnLoader));

        auto courseLoader = std::make_unique<DragonWildsCourseLoader>();
        RegisterLoader(std::move(courseLoader));

        auto stringModLoader = std::make_unique<DragonWildsStringModLoader>();
        m_stringLoader = stringModLoader.get();
        RegisterLoader(std::move(stringModLoader));

    }

    void DragonWildsMainLoader::SetupAutoReload()
    {
        auto config = PS::PSConfig::Get();
        if (!config->IsAutoReloadEnabled()) return;

        PS::Log<LogLevel::Normal>(STR("Auto-reload is enabled.\n"));

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
            GetPakFolders_Hook = safetyhook::create_inline(reinterpret_cast<void*>(GetPakFolders_Address),
                GetPakFolders);
        }
        else
        {
            PS::Log<LogLevel::Error>(STR("Unable to setup additional .pak read directory, signature for FPakPlatformFile::GetPakFolders is outdated.\n"));
        }
    }

    void DragonWildsMainLoader::InitCore()
    {
        if (m_hasInit) return;
        m_hasInit = true;

        PS::Log<LogLevel::Verbose>(STR("Initializing Static Class Storage...\n"));
        DragonWilds::StaticClassStorage::Initialize();

        SetupPostEngineInitLoaders();

        HookGameInstanceInit();

        PS::Log<LogLevel::Verbose>(STR("Initialized Core\n"));
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
            loader->Initialize(engineLifecyclePhase);
        }
    }

    void DragonWildsMainLoader::LoadMods(EEngineLifecyclePhase engineLifecyclePhase)
    {
        IterateModsFolder([&](const fs::path& modPath, const fs::path::string_type& modName)
        {
            try
            {
                PS::Log<RC::LogLevel::Normal>(STR("Loading mod: {}\n"), modName);

                if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
                {
                    WarnAboutUnknownFolders(modPath, modName);
                }

                for (auto& loader : m_loaders)
                {
                    loader->Load(modPath, modName, engineLifecyclePhase);
                }
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed to load mod {} - {}\n"), modName, PS::ToWideSafe(e.what()));
            }
        });
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
            if (folderType == "paks" || folderType == "config")
            {
                continue;
            }

            auto known = std::any_of(m_loaders.begin(), m_loaders.end(),
                [&](const auto& loader) { return loader->GetModFolderType() == folderType; });
            if (known)
            {
                continue;
            }

            auto hasPakContent = false;
            for (const auto& file : fs::recursive_directory_iterator(entry.path()))
            {
                auto extension = file.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (extension == ".pak" || extension == ".utoc" || extension == ".ucas" || extension == ".sig")
                {
                    hasPakContent = true;
                    break;
                }
            }
            if (hasPakContent)
            {
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

            PS::Log<LogLevel::Warning>(STR("{}: folder '{}' does not match any loader ({} or paks); its files will be ignored.\n"),
                modName, RC::to_generic_string(folderType), knownFolders);
        }
    }

    std::filesystem::path DragonWildsMainLoader::GetModsPath()
    {
        static auto modsPath = fs::path(UE4SSProgram::get_program().get_working_directory()) / "Mods" / "RuneSchema" / "mods";
        return modsPath;
    }

    void DragonWildsMainLoader::GetPakFolders(const TCHAR* CmdLine, TArray<FString>* OutPakFolders)
    {
        PS::Log<LogLevel::Verbose>(STR("Calling original FPakPlatformFile::GetPakFolders...\n"));
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

        PS::Log<LogLevel::Verbose>(STR("Preparing to add extra .pak read directory...\n"));
        auto ModsFolderPath = GetModsPath();
        auto AbsolutePath = ModsFolderPath.native();
        auto AbsolutePathWithSuffix = std::format(STR("{}/"), RC::to_generic_string(AbsolutePath));

        PS::Log<LogLevel::Verbose>(STR("Setting extra .pak read directory to {}\n"), AbsolutePathWithSuffix);

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
