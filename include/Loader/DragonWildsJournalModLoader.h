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
        };

        struct LoadResult {
            int EntriesReady = 0;
            int Placements = 0;
            int ErrorCount = 0;
        };

    public:
        DragonWildsJournalModLoader();

    protected:
        void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName,
            const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;
        bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        bool OnInitialize() override final;

    private:
        std::vector<JournalDef> m_defs;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*> m_entries;
        std::unordered_map<RC::Unreal::UObject*, RC::StringType> m_pendingRecipeReferences;
        std::unordered_set<RC::StringType> m_unlock;
        std::unordered_set<RC::Unreal::UObject*> m_createdEntries;
        std::unordered_set<std::string> m_ownedIds;
        std::unordered_set<std::string> m_knownIds;
        bool m_knownIdsValid = false;
        bool m_saveGuardActive = false;
        RC::Unreal::UClass* m_baseEntryClass = nullptr;
        RC::Unreal::UClass* m_noBiomeSubCategoryClass = nullptr;
        RC::Unreal::UClass* m_byBiomeSubCategoryClass = nullptr;
        RC::Unreal::UClass* m_journalComponentClass = nullptr;
        RC::Unreal::UClass* m_journalSubsystemClass = nullptr;
        bool m_hooksActive = false;

        void QueueData(const nlohmann::json& data);
        LoadResult ApplyAll();
        RC::Unreal::UObject* ResolveOrCreate(const JournalDef& def);
        RC::Unreal::UClass* ResolveEntryClass(const nlohmann::json& body) const;
        void ApplyProperties(RC::Unreal::UObject* entry, const nlohmann::json& body);
        bool Place(RC::Unreal::UObject* entry, const JournalDef& def);
        void RegisterEntry(RC::Unreal::UObject* entry);
        void RegisterHooks();
        void ResolvePendingRecipeReferences();
        void UnlockEntries(RC::Unreal::UObject* journalComponent);
        RC::Unreal::UObject* FindJournalComponent();
        RC::Unreal::UObject* FindJournalSubsystem();

        void TrackOwnedId(RC::Unreal::UObject* entry, const RC::Unreal::FString& persistenceId);
        void SnapshotRegistryIds();
        void InstallSaveGuard();
        void StripUnusableIdsFromCharacterSaves();
        bool StripUnusableIdsFromCharacterSave(const std::filesystem::path& savePath);
        static std::filesystem::path GetCharacterSaveDirectory();
    };
}
