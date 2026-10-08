#pragma once

#include <vector>
#include <atomic>
#include <functional>
#include <string>
#include <utility>
#include "Loader/DragonWildsModLoaderBase.h"
#include "Core/CharacterEntryRecovery.h"
#include "Misc/DragonWildsDataRegistrar.h"
#include "SDK/Classes/Custom/UDataTableStore.h"
#include "safetyhook.hpp"
#include "Utility/UnrealReadinessGate.h"
#include "Runtime/RegistryBridge.h"
#include "Runtime/LogicPakBridge.h"

namespace RC::Unreal {
    class AGameModeBase;
    class UDataTable;
}

namespace UECustom {
    class UCompositeDataTable;
}

namespace PS {
    class FileWatchWrapper;
}

namespace DragonWilds {
    class DragonWildsSpawnLoader;
    class DragonWildsStringModLoader;
    class DragonWildsBuildingModLoader;

	class DragonWildsMainLoader {
	public:
        DragonWildsMainLoader();

        ~DragonWildsMainLoader();

		void PreInitialize();

		void Initialize();
        void AbortStartup(const std::string& stage, const std::string& reason);
        void SetFatalStartupHandler(std::function<void(std::string)> handler)
        { m_fatalStartupHandler = std::move(handler); }
	private:
        std::vector<std::unique_ptr<DragonWildsModLoaderBase>> m_loaders;

        DragonWildsStringModLoader* m_stringLoader = nullptr;
        DragonWildsBuildingModLoader* m_buildingLoader = nullptr;
        DragonWildsSpawnLoader* m_spawnLoader = nullptr;

        std::unique_ptr<PS::FileWatchWrapper> m_fileWatcher;

        UECustom::UDataTableRegistry m_datatableRegistry;

        DragonWildsDataRegistrar m_dataRegistrar;
        CharacterEntryRecovery m_characterEntryRecovery;
        PS::Network::RegistryBridge m_registryBridge;
        PS::LogicPakBridge m_logicPakBridge;

        void AutoReload(const std::filesystem::path& filePath);

        void IterateModsFolder(const std::function<void(const std::filesystem::path&, const RC::StringType&)>& callback);

        void SetupPostEngineInitLoaders();

        void SetupGameInstanceInitLoaders();

        void SetupGameInstanceInitLoadersOnce();

        void HookDatatableSerialize();

        void HookGameInstanceInit();

        void CreateLoaders();

        void SetupAutoReload();

        void SetupAlternativePakPathReader();

        void SetupLogicPakBridge();

        bool InitCore();
        void FailStartup(const std::string& reason);

        void RegisterLoader(std::unique_ptr<DragonWildsModLoaderBase> newLoader);

        void InitializeMods(EEngineLifecyclePhase engineLifecyclePhase);
        void LoadMods(EEngineLifecyclePhase engineLifecyclePhase);
    private:
        static std::filesystem::path GetModsPath();

        static void GetPakFolders(const RC::Unreal::TCHAR* CmdLine, RC::Unreal::TArray<RC::Unreal::FString>* OutPakFolders);

        static void OnDataTableSerialized(RC::Unreal::UDataTable* This, RC::Unreal::FArchive* Archive);

        static void OnGameInstanceInit(RC::Unreal::UObject* This);

        PS::UnrealReadinessGate<RC::Unreal::UDataTable*> m_readiness;
        bool m_orderResolved = false;
        std::atomic<bool> m_gameInstanceLoadersStarted{false};
        std::atomic<bool> m_dedicatedServerWorldReady{false};
        std::atomic<bool> m_coreStartupComplete{false};
        std::atomic<bool> m_coreStartupFailed{false};
        std::function<void(std::string)> m_fatalStartupHandler;
        RC::Unreal::Hook::GlobalCallbackId m_coreStartupCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_dedicatedServerReadyCallbackId = RC::Unreal::Hook::ERROR_ID;
        std::vector<RC::StringType> m_orderedMods;

        static inline std::vector<std::function<void(RC::Unreal::UDataTable*)>> DatatableSerializeCallbacks;
        static inline std::vector<std::function<void(RC::Unreal::UObject*)>> GameInstanceInitCallbacks;

        static inline SafetyHookInline DatatableSerialize_Hook;
        static inline SafetyHookInline GameInstanceInit_Hook;
        static inline SafetyHookInline GetPakFolders_Hook;
	};
}
