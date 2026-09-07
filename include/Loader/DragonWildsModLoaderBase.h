#pragma once

#include "Unreal/NameTypes.hpp"
#include "SDK/Classes/Custom/UDataTableStore.h"
#include "nlohmann/json.hpp"
#include <string>
#include <atomic>

namespace RC::Unreal {
	class UDataTable;
}

namespace DragonWilds {
    enum class EEngineLifecyclePhase {
        PreEngineInit,
        PostEngineInit,
        UE4SSInit,
        GameInstanceInit,
    };

	class DragonWildsModLoaderBase {
	public:
		virtual ~DragonWildsModLoaderBase();
        
        void AssignDatatableRegistry(UECustom::UDataTableRegistry& datatableRegistry);

        const RC::StringType& GetDisplayName() const;

        void Setup();
        void AutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath);
        void Load(const std::filesystem::path& modPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase);
        void FinalizeLoad(const EEngineLifecyclePhase& engineLifecyclePhase);

		void Initialize(const EEngineLifecyclePhase& engineLifecyclePhase);

        bool HasInitialized() const;

        const std::string& GetModFolderType();
    protected:
        DragonWildsModLoaderBase(const std::string& modFolderName);

        void SetDisplayName(const RC::StringType& displayName);


        RC::Unreal::UDataTable* TryGetDatatableByName(const std::string& name);

        RC::Unreal::UDataTable* GetDatatableByName(const std::string& name);
    protected:
        virtual void OnSetup();
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase);
        virtual void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath);
        virtual void OnFinalizeLoad(const EEngineLifecyclePhase& engineLifecyclePhase);

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) = 0;

        virtual bool OnInitialize() = 0;


        virtual void OnDatatableSerialized(RC::Unreal::UDataTable* datatable);
    private:
        void Initialize_Internal();

        std::string m_modFolderType = "";
        RC::StringType m_displayName = TEXT("Unknown Loader");
        UECustom::UDataTableRegistry* m_datatableRegistry = nullptr;
        std::atomic<bool> m_hasInitialized{false};
        std::mutex m_mutex;

        UECustom::DatatableSerializeCallbackId m_datatableSerializeCallbackId{};
	};
}
