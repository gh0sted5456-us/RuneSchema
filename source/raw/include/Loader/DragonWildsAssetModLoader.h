#pragma once
#include "Loader/AssetAuthoringMetadata.h"

#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"
#include "Unreal/Hooks.hpp"

namespace RC::Unreal { class UWorld; }

namespace DragonWilds {
    class DragonWildsAssetModLoader : public DragonWildsModLoaderBase {
        struct PendingAsset {
            RC::StringType Target;
            RC::StringType ObjectPath;
            RC::StringType ModName;
            nlohmann::json Properties;
            bool IsPatch = false;
            PS::AssetMetadata::Declaration Metadata;
            bool InstalledDefinition = true;
        };

        struct LoadResult {
            int PropertiesWritten = 0;
            int ErrorCount = 0;
        };
        enum class PatchTargetMode { Object, ClassDefaultObject };
        struct PendingObjectPatch {
            RC::StringType ObjectPath;
            RC::StringType ExpectedClass;
            RC::StringType ModName;
            std::string Source;
            std::string PatchId;
            PatchTargetMode Mode = PatchTargetMode::Object;
            nlohmann::json Operations;
        };
    public:
        // Game-thread authoring facade; no UI thread touches Unreal objects.
        inline static DragonWildsAssetModLoader* AuthoringInstance=nullptr;
        nlohmann::json InspectToolClone(const std::string& sourcePath,bool requireCloneEligibility=true);
        nlohmann::json ExportToolRecipe(const nlohmann::json& request);
        nlohmann::json ExportToolOverrides(const nlohmann::json& request);
        nlohmann::json CreateToolClone(const nlohmann::json& request, RC::Unreal::UWorld* world);
        DragonWildsAssetModLoader();

        ~DragonWildsAssetModLoader();
    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;
    private:
        std::mutex m_mutex;
        std::set<std::filesystem::path> m_toolAssetFiles;
        std::set<std::string> m_temporaryToolClones;
        std::size_t m_toolCloneCount=0;
        std::vector<PendingAsset> m_pendingAssets;
        std::vector<PendingAsset> m_pendingPatches;
        std::vector<PendingObjectPatch> m_pendingObjectPatches;
        std::vector<PendingObjectPatch> m_retainedObjectPatches;
        RC::Unreal::Hook::GlobalCallbackId m_characterMenuPatchHook = RC::Unreal::Hook::ERROR_ID;
        bool m_replayingCharacterMenuPatches = false;
        std::vector<RC::Unreal::UObject*> m_createdAssets;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*> m_createdAssetsByTarget;
        RC::Unreal::UClass* m_dataAssetClass = nullptr;
        RC::Unreal::UClass* m_itemDataClass = nullptr;
        RC::Unreal::UClass* m_recipeDataClass = nullptr;
        RC::Unreal::UClass* m_curveBaseClass = nullptr;

        void QueueData(const nlohmann::json& data, const RC::StringType& modName,
            const std::string& source = "assets");
        bool QueueObjectPatch(const nlohmann::json& data, const RC::StringType& modName,
            const std::string& source);
        void ApplyObjectPatches(bool characterMenuReplay = false);
        void RegisterCharacterMenuPatchReplay();
        void RegisterDeclarations(const nlohmann::json& data, const RC::StringType& modName);
        void Apply(RC::Unreal::UObject* object, const PendingAsset& pendingAsset, LoadResult& outResult);
        void ApplyDominionSpheres(RC::Unreal::UObject* owner,
            const nlohmann::json& definitions, LoadResult& outResult);
        void AppendProperties(RC::Unreal::UObject* object, RC::Unreal::UClass* objectClass, const nlohmann::json& appendData, LoadResult& outResult);
        void TryApplyPending();
        void ApplyPendingPatches();
        void ReportUnresolvedAssets();

        RC::Unreal::UObject* Resolve(const PendingAsset& pendingAsset);
        RC::Unreal::UObject* CreateFromClone(const PendingAsset& pendingAsset, RC::Unreal::UObject* subsystem);
        RC::Unreal::UObject* FindItemSubsystem() const;
        bool RegisterCreatedItem(RC::Unreal::UObject* item,
            const PendingAsset& pendingAsset, RC::Unreal::UObject* subsystem);
        RC::StringType NormalizeObjectPath(const RC::StringType& target) const;

        bool IsSupportedTarget(RC::Unreal::UObject* object) const;
        bool IsReadyForPatch(RC::Unreal::UObject* object) const;
    };
}
