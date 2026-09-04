#pragma once

#include <mutex>
#include <functional>
#include <unordered_map>
#include <vector>
#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"

namespace DragonWilds {
    class DragonWildsAssetModLoader : public DragonWildsModLoaderBase {
        struct PendingAsset {
            RC::StringType Target;
            RC::StringType ObjectPath;
            RC::StringType ModName;
            nlohmann::json Properties;
        };

        struct LoadResult {
            int PropertiesWritten = 0;
            int ErrorCount = 0;
            bool Disabled = false;
        };
    public:
        DragonWildsAssetModLoader();

        ~DragonWildsAssetModLoader();

        void SetRuntimeAssetSink(std::function<void(
            const std::string&, RC::Unreal::UObject*)> sink);

        // Looks up a previously $Create/$Clone-produced runtime object by the
        // exact authoring key it was created under (the JSON target string -
        // commonly a /Game/Mods/.../Name.Name path or a simple name).
        // Registered as RuneSchema's PS::RuntimeObjectResolver fallback (see
        // Utility/RuntimeObjectResolver.h) so a soft-object reference written
        // elsewhere with that same literal string - a DataTable row appended
        // under /raw, or another asset's array field appended under /assets -
        // can still find the canonical /Game/RuneSchema/... in-memory object.
        // Thread-safe. Returns nullptr if nothing was created under that key.
        RC::Unreal::UObject* FindCreatedAssetByAuthoringPath(
            const RC::StringType& authoredPath);
    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
    private:
        std::mutex m_mutex;
        std::vector<PendingAsset> m_pendingAssets;
        std::vector<RC::Unreal::UObject*> m_createdAssets;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*>
            m_createdAssetsByTarget;
        RC::Unreal::UClass* m_dataAssetClass = nullptr;
        RC::Unreal::UClass* m_itemDataClass = nullptr;
        RC::Unreal::UClass* m_curveBaseClass = nullptr;
        std::function<void(const std::string&, RC::Unreal::UObject*)>
            m_runtimeAssetSink;

        void QueueData(const nlohmann::json& data, const RC::StringType& modName);
        void Apply(RC::Unreal::UObject* object, const PendingAsset& pendingAsset, LoadResult& outResult);
        void AppendProperties(RC::Unreal::UObject* object, RC::Unreal::UClass* objectClass, const nlohmann::json& appendData, LoadResult& outResult);
        void TryApplyPending();
        void ReportUnresolvedAssets();

        RC::Unreal::UObject* Resolve(const PendingAsset& pendingAsset);
        RC::Unreal::UObject* CreateFromClass(const PendingAsset& pendingAsset);
        RC::Unreal::UObject* CreateFromClone(const PendingAsset& pendingAsset);
        RC::Unreal::UObject* FindItemSubsystem() const;
        std::optional<std::string> ResolveItemObjectPath(
            const std::string& identifier) const;
        RC::StringType NormalizeObjectPath(const RC::StringType& target) const;

        bool IsSupportedTarget(RC::Unreal::UObject* object) const;
        bool IsReadyForPatch(RC::Unreal::UObject* object) const;
    };
}

