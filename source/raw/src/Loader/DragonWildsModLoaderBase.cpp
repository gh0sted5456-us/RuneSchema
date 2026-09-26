#include "Loader/DragonWildsModLoaderBase.h"
#include <chrono>
#include "Unreal/Engine/UDataTable.hpp"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Utility/ModFolderLayout.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace DragonWilds {
	DragonWildsModLoaderBase::DragonWildsModLoaderBase(const std::string& modFolderType) : m_modFolderType(modFolderType) {}

	DragonWildsModLoaderBase::~DragonWildsModLoaderBase() {
        if (m_datatableRegistry)
        {
            m_datatableRegistry->UnregisterDatatableSerializeCallback(m_datatableSerializeCallbackId);
        }
    }

    void DragonWildsModLoaderBase::AssignDatatableRegistry(UECustom::UDataTableRegistry& datatableRegistry)
    {
        m_datatableRegistry = &datatableRegistry;

        m_datatableSerializeCallbackId = m_datatableRegistry->RegisterDatatableSerializeCallback([&](RC::Unreal::UDataTable* datatable) {
            if (HasInitialized()) OnDatatableSerialized(datatable);
        });
    }

    const RC::StringType& DragonWildsModLoaderBase::GetDisplayName() const
    {
        return m_displayName;
    }

    void DragonWildsModLoaderBase::Setup()
    {
        OnSetup();
    }

    void DragonWildsModLoaderBase::AutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath)
    {
        if (HasInitialized()) OnAutoReload(modName, modFilePath);
    }

    bool DragonWildsModLoaderBase::Load(const fs::path& modPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        const auto loaderPath = PS::ModFolderLayout::FindChildDirectory(
            modPath, m_modFolderType);
        if (loaderPath.empty()) return true;

        if (!HasInitialized())
        {
            return false;
        }

        const auto started=std::chrono::steady_clock::now();
        try {
            OnLoad(loaderPath, modName, engineLifecyclePhase);
            PS::RoutineLog(m_modFolderType, STR("[LOADER:{}][OK][MOD:{}] Section loaded.\n"),
                RC::to_generic_string(m_modFolderType), modName);
        }
        catch(const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL][MOD:{}] Section skipped: {}. Remaining sections continue.\n"),
                RC::to_generic_string(m_modFolderType),modName,PS::ToWideSafe(error.what()));
            return false;
        }
        catch(...) {
            PS::Log<LogLevel::Warning>(STR("[LOADER:{}][PARTIAL][MOD:{}] Section skipped after an unknown failure. Remaining sections continue.\n"),
                RC::to_generic_string(m_modFolderType),modName);
            return false;
        }
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-started).count();
        if(elapsed>=1000)
            PS::Log<LogLevel::Normal>(STR("[PERF][LOADER:{}][MOD:{}] Section load took {} ms.\n"),
                RC::to_generic_string(m_modFolderType),modName,elapsed);
        return true;
    }

    void DragonWildsModLoaderBase::FinalizeLoad(const EEngineLifecyclePhase& phase)
    {
        if (!HasInitialized()) return;
        const auto started=std::chrono::steady_clock::now();
        OnFinalizeLoad(phase);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-started).count();
        if(elapsed>=1000)
            PS::Log<LogLevel::Normal>(STR("[PERF][LOADER:{}] Finalization took {} ms.\n"),
                RC::to_generic_string(m_modFolderType),elapsed);
    }

	void DragonWildsModLoaderBase::Initialize(const EEngineLifecyclePhase& engineLifecyclePhase) {
        if (!PS::PSConfig::Get()->IsLoaderEnabled(m_modFolderType)) return;
        if (!CanInitialize(engineLifecyclePhase))
        {
            return;
        }

        Initialize_Internal();
    }

    bool DragonWildsModLoaderBase::HasInitialized() const
    {
        return m_hasInitialized.load(std::memory_order_acquire);
    }

    const std::string& DragonWildsModLoaderBase::GetModFolderType()
    {
        return m_modFolderType;
    }

    void DragonWildsModLoaderBase::SetDisplayName(const RC::StringType& displayName)
    {
        m_displayName = displayName;
    }

    RC::Unreal::UDataTable* DragonWildsModLoaderBase::TryGetDatatableByName(const std::string& name)
    {
        if (!m_datatableRegistry)
        {
            return nullptr;
        }

        return m_datatableRegistry->GetDatatableByName(name);
    }

    RC::Unreal::UDataTable* DragonWildsModLoaderBase::TryGetDatatableByPath(const std::string& path)
    {
        return m_datatableRegistry ? m_datatableRegistry->GetDatatableByPath(path) : nullptr;
    }

    std::vector<RC::Unreal::UDataTable*> DragonWildsModLoaderBase::GetDatatablesByName(const std::string& name)
    {
        return m_datatableRegistry ? m_datatableRegistry->GetDatatablesByName(name) : std::vector<RC::Unreal::UDataTable*>{};
    }

    RC::Unreal::UDataTable* DragonWildsModLoaderBase::GetDatatableByName(const std::string& name)
    {
        if (!m_datatableRegistry)
        {
            throw std::runtime_error(std::format("Unable to process 'GetDatatableByName', UDataTableRegistry has not been initialized properly."));
        }

        auto datatable = TryGetDatatableByName(name);
        if (!datatable)
        {
            throw std::runtime_error(std::format("Failed to find UDataTable '{}'", name));
        }

        return datatable;
    }

    void DragonWildsModLoaderBase::OnSetup() {}

    void DragonWildsModLoaderBase::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) {}

    void DragonWildsModLoaderBase::OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath) {}

    void DragonWildsModLoaderBase::OnFinalizeLoad(const EEngineLifecyclePhase&) {}


    void DragonWildsModLoaderBase::OnDatatableSerialized(RC::Unreal::UDataTable* datatable) {}

    void DragonWildsModLoaderBase::Initialize_Internal()
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        if (HasInitialized())
        {
            PS::Log<LogLevel::Warning>(STR("Loader '{}' attempted to initialize more than once. Skipping.\n"), RC::to_generic_string(m_modFolderType));
            return;
        }

        const auto started=std::chrono::steady_clock::now();
        if (!OnInitialize())
        {
            PS::Log<LogLevel::Warning>(STR("[LOADER:{}][DISABLED] Required capability is unavailable; other loaders continue.\n"), RC::to_generic_string(m_modFolderType));
            return;
        }

        m_hasInitialized = true;

        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-started).count();
        if(elapsed>=1000)
            PS::Log<LogLevel::Normal>(STR("[PERF][LOADER:{}] Initialization took {} ms.\n"),
                RC::to_generic_string(m_modFolderType),elapsed);

        PS::RoutineLog(m_modFolderType, STR("[LOADER:{}][OK] Initialized.\n"), RC::to_generic_string(m_modFolderType));
    }
}
