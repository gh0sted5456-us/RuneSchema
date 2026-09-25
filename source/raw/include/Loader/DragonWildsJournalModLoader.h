#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class UClass;
    class UObject;
    class FString;
}

namespace DragonWilds {
    class DragonWildsJournalModLoader : public DragonWildsModLoaderBase {
        struct JournalDef {
            RC::StringType Key;
            nlohmann::json Body;
            RC::StringType Owner;
            bool Declared=false;
            std::string DeclaredPersistenceID;
            std::string DeclaredInternalName;
            bool DeclaredInternalNameAsserted=false;
        };

        struct LoadResult {
            int EntriesReady = 0;
            int Placements = 0;
            int ErrorCount = 0;
        };

    public:
        explicit DragonWildsJournalModLoader(bool loreOnly = false);
        bool OpenLoreForPlayer(RC::Unreal::UObject* controller,const std::string& reference);

    protected:
        void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName,
            const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;
        void OnFinalizeLoad(const EEngineLifecyclePhase& phase) override final;
        bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        bool OnInitialize() override final;

    private:
        bool m_loreOnly = false;
        bool m_initialJournalApplied = false;
        std::vector<JournalDef> m_defs;
        struct PendingPatch { std::string Reference; nlohmann::json Changes; RC::StringType Owner; };
        std::vector<PendingPatch> m_pendingPatches;
        struct EntryHandle {
            RC::Unreal::UObject* Object=nullptr;
            int32_t Index=-1;
            mutable int32_t Serial=0;
            RC::StringType Path;
            explicit EntryHandle(RC::Unreal::UObject* object);
            RC::Unreal::UObject* Get() const;
        };
        std::unordered_map<RC::StringType, EntryHandle> m_entries;
        std::unordered_set<RC::StringType> m_rejectedEntries;
        std::unordered_set<RC::StringType> m_unlock;
        std::unordered_set<RC::Unreal::UObject*> m_createdEntries;
        std::unordered_map<std::string,std::string> m_ownedIds;
        RC::Unreal::UClass* m_baseEntryClass = nullptr;
        RC::Unreal::UClass* m_noBiomeSubCategoryClass = nullptr;
        RC::Unreal::UClass* m_journalComponentClass = nullptr;
        RC::Unreal::UClass* m_journalSubsystemClass = nullptr;
        bool m_hooksActive = false;

        struct ReferenceIndex {
            bool Built = false;
            std::unordered_map<RC::StringType, RC::Unreal::UObject*> Unique;
            std::unordered_set<RC::StringType> Ambiguous;
        };
        ReferenceIndex m_recipeReferenceIndex;
        ReferenceIndex m_itemReferenceIndex;
        ReferenceIndex m_tableReferenceIndex;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*> m_subCategoryCache;
        RC::Unreal::UObject* m_finalizeJournalSubsystem = nullptr;
        bool m_finalizeJournalSubsystemResolved = false;

        void QueueData(const nlohmann::json& data, const RC::StringType& modName);
        void ApplyPendingPatches();
        LoadResult ApplyAll();
        RC::Unreal::UObject* ResolveOrCreate(const JournalDef& def);
        RC::Unreal::UClass* ResolveEntryClass(const nlohmann::json& body) const;
        void ApplyProperties(RC::Unreal::UObject* entry, const nlohmann::json& body);
        bool Place(RC::Unreal::UObject* entry, const JournalDef& def);
        void RegisterEntry(RC::Unreal::UObject* entry,const RC::StringType& owner);
        void RegisterHooks();
        void UnlockEntries(RC::Unreal::UObject* journalComponent);
        RC::Unreal::UObject* FindJournalComponent();
        RC::Unreal::UObject* FindJournalSubsystem();

        void TrackOwnedId(RC::Unreal::UObject* entry, const RC::Unreal::FString& persistenceId,const RC::StringType& owner,bool declared=false);
        void InstallNativePersistence();
        RC::Unreal::UObject* ResolveSoftReference(const TCHAR* classPath,
            const RC::StringType& reference, ReferenceIndex& index);
        void ResetFinalizeCaches();
    };
}
