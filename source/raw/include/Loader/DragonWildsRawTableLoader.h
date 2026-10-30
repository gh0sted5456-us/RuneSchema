#pragma once

#include "Unreal/NameTypes.hpp"
#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/RegistryPatchPlan.h"
#include "nlohmann/json.hpp"
#include "Unreal/Hooks.hpp"
#include <set>
#include <mutex>
#include <vector>
#include "SDK/WeakObjectHandle.h"
#include "Core/RawTableTarget.h"
#include "Core/DeferredTraceExport.h"
#include <atomic>
#include <memory>
#include <map>

namespace RC::Unreal {
    class UDataTable;
}

namespace UECustom {
	class UCompositeDataTable;
}

namespace DragonWilds {
	class DragonWildsRawTableLoader : public DragonWildsModLoaderBase {
        struct LoadResult {
            int SuccessfulModifications = 0;
            int SuccessfulAdditions = 0;
            int SuccessfulDeletions = 0;
            int ErrorCount = 0;
            int Patched = 0;
        };
	public:
		DragonWildsRawTableLoader();

		~DragonWildsRawTableLoader() override;


        void Apply(const RC::StringType& datatableName, RC::Unreal::UDataTable* datatable);

        void Apply(UECustom::UCompositeDataTable* compositeDatatable);

        void Apply(const nlohmann::json& data, RC::Unreal::UDataTable* table, LoadResult& outResult);
    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;
        void OnFinalizeLoad(const EEngineLifecyclePhase& engineLifecyclePhase) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
        virtual void OnDatatableSerialized(RC::Unreal::UDataTable* datatable) override final;
    private:
        struct RawDefinition {
            nlohmann::json Data;
            std::string ModId, Source, OperationId;
        };
        std::unordered_map<RC::StringType, std::vector<RawDefinition>> m_tableDataMap;
        struct RawApplication { PS::WeakObjectHandle Table; PS::RawTableTarget::Cursor Cursor; };
        std::map<std::pair<RC::StringType,RC::StringType>,RawApplication> m_rawApplications;
        bool m_rawTargetsReady=false;
        std::vector<RC::Unreal::UDataTable*> RawDestinations(RC::Unreal::UDataTable* table);
        void ApplyDefinition(const RawDefinition* definition, const nlohmann::json& data,
            RC::Unreal::UDataTable* table, LoadResult& outResult);
        std::set<std::string> m_appliedExactTargets;
        struct PendingPatch {
            std::string Table, Row;
            nlohmann::json Changes;
            RC::StringType ModName;
            std::string Source, OperationId;
        };
        std::vector<PendingPatch> m_pendingPatches;
        std::vector<RegistryPatch::Document> m_registryDocuments;
        std::vector<RegistryPatch::Patch> m_registryPlan;
        std::unordered_map<std::string, std::pair<std::string,std::string>> m_ownedRows;
        std::set<std::string> m_appliedRegistryPatches;
        RC::Unreal::Hook::GlobalCallbackId m_traceJobCallbackId = RC::Unreal::Hook::ERROR_ID;
        struct TraceJob {
            std::string Id;
            std::vector<std::string> ClassContains;
            std::vector<std::string> FunctionContains;
            std::size_t MaxEvents = 256;
            bool ConsoleEvents = false;
            std::set<std::string> Seen;
            nlohmann::json Events = nlohmann::json::array();
            bool Dirty = false;
        };
        std::vector<TraceJob> m_traceJobs;
        std::mutex m_traceJobMutex;
        std::atomic_size_t m_traceJobsRemaining{0};
        std::unique_ptr<PS::DeferredTraceExport> m_traceExports;
        bool m_traceJobFailureReported = false;
        void LoadTraceJobs();
        void RegisterTraceJobs();
        void TraceJobEvent(RC::Unreal::UObject* source, RC::Unreal::UFunction* function);
        void FlushTraceJob(TraceJob& job, bool complete = false) noexcept;
        void LoadDocument(const nlohmann::json& data, const RC::StringType& modName,
            const std::string& source = "raw");
        void ReloadDocument(const nlohmann::json& data, const RC::StringType& modName,
            const std::string& source={});
        void LoadAndApplyRegistryTargets();
        void LoadAndApplyRawTargets();
        void ApplyCookedTableExtensions(RC::Unreal::UDataTable* only=nullptr);
        struct TableExtension {std::string Owner,Source,Target;};
        std::vector<TableExtension> m_cookedTables;
        struct TableImport {PS::WeakObjectHandle Source,Target;};
        std::map<std::pair<std::string,std::string>,TableImport> m_cookedTableImports;
        bool m_importingCookedTables=false;
        void ApplyRegistryPatches(RC::Unreal::UDataTable* datatable);
        void ApplyObjectRegistryPatches();
        nlohmann::json ResolveRegistryValue(const nlohmann::json& value, const RegistryPatch::Patch& patch) const;
        bool ProfileAllows(const RegistryPatch::Patch& patch) const;

        void HandleFilters(RC::Unreal::UDataTable* datatable, const nlohmann::json& data, LoadResult& outResult);

        void AddRow(RC::Unreal::UDataTable* datatable, const RC::Unreal::FName& rowName, const nlohmann::json& data, LoadResult& outResult);

        void EditRow(RC::Unreal::UDataTable* datatable, const RC::Unreal::FName& rowName, RC::Unreal::uint8* row, const nlohmann::json& data, LoadResult& outResult);

        void DeleteRow(RC::Unreal::UDataTable* datatable, const RC::Unreal::FName& rowName, LoadResult& outResult);

        bool ModifyRowProperties(RC::Unreal::UDataTable* datatable, const RC::Unreal::FName& rowName, void* rowPtr, const nlohmann::json& data, LoadResult& outResult);

        void AddToTableDataMap(const std::string& datatableName, const nlohmann::json& data,
            const std::string& modId={}, const std::string& source={}, const std::string& operationId={});
	};
}
