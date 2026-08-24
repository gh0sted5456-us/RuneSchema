#pragma once

#include <vector>
#include <functional>
#include "Loader/DragonWildsModLoaderBase.h"
#include "Misc/DragonWildsDataRegistrar.h"
#include "SDK/Classes/Custom/UDataTableStore.h"
#include "safetyhook.hpp"

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
    class DragonWildsStringModLoader;
    class DragonWildsBuildingModLoader;

	class DragonWildsMainLoader {
	public:
        DragonWildsMainLoader();

        ~DragonWildsMainLoader();

		void PreInitialize();

		void Initialize();
	private:
        std::vector<std::unique_ptr<DragonWildsModLoaderBase>> m_loaders;

        DragonWildsStringModLoader* m_stringLoader = nullptr;
        DragonWildsBuildingModLoader* m_buildingLoader = nullptr;

        std::unique_ptr<PS::FileWatchWrapper> m_fileWatcher;

        UECustom::UDataTableRegistry m_datatableRegistry;

        DragonWildsDataRegistrar m_dataRegistrar;

        void AutoReload(const std::filesystem::path& filePath);

        void IterateModsFolder(const std::function<void(const std::filesystem::path&, const RC::StringType&)>& callback);

        void SetupPostEngineInitLoaders();

        void SetupGameInstanceInitLoaders();

        void HookDatatableSerialize();

        void HookGameInstanceInit();

        void CreateLoaders();

        void SetupAutoReload();

        void SetupAlternativePakPathReader();

        void InitCore();

        void RegisterLoader(std::unique_ptr<DragonWildsModLoaderBase> newLoader);

        void InitializeMods(EEngineLifecyclePhase engineLifecyclePhase);
        void LoadMods(EEngineLifecyclePhase engineLifecyclePhase);
        void WarnAboutUnknownFolders(const std::filesystem::path& modPath, const RC::StringType& modName);
    private:
        static std::filesystem::path GetModsPath();

        static void GetPakFolders(const RC::Unreal::TCHAR* CmdLine, RC::Unreal::TArray<RC::Unreal::FString>* OutPakFolders);

        static void OnDataTableSerialized(RC::Unreal::UDataTable* This, RC::Unreal::FArchive* Archive);

        static void OnGameInstanceInit(RC::Unreal::UObject* This);

        bool m_hasInit = false;
        bool m_compatibilityReportGenerated = false;

        static inline std::vector<std::function<void(RC::Unreal::UDataTable*)>> DatatableSerializeCallbacks;
        static inline std::vector<std::function<void(RC::Unreal::UObject*)>> GameInstanceInitCallbacks;
        static inline std::vector<std::function<void()>> GetPakFoldersCallback;

        static inline SafetyHookInline DatatableSerialize_Hook;
        static inline SafetyHookInline GameInstanceInit_Hook;
        static inline SafetyHookInline GetPakFolders_Hook;
	};
}
