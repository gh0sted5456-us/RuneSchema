#pragma once
namespace RC::Unreal { class UFunction; }

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"
#include "Loader/NpcCatalog.h"

namespace RC::Unreal {
    class UClass;
    class UObject;
    class UScriptStruct;
    class UDataTable;
}

namespace DragonWilds {
    class DragonWildsRecipeModLoader : public DragonWildsModLoaderBase {
        struct Placement {
            std::string Table;
            RC::StringType Row;
            RC::StringType Category;
            RC::StringType Array;
            RC::StringType Replaces;
        };

        struct RecipeDef {
            RC::StringType Key;
            nlohmann::json Body;
            std::vector<Placement> Placements;
        };

        struct LoadResult {
            int Created = 0;
            int Edited = 0;
            int ErrorCount = 0;
        };
    public:
        DragonWildsRecipeModLoader();

        ~DragonWildsRecipeModLoader();
        // Allocate identities before assets link recipe-unlocker references.
        void PrepareReferences();
        const std::vector<NpcCatalog::StoreOffer>& StoreOffers() const { return m_storeOffers; }
        // Internal bridge for /vendors. Creates owned recipes only; never
        // edits a cooked recipe or an authored /recipes definition.
        RC::Unreal::UObject* EnsureVendorRecipe(const std::string& owner,
            const std::string& identity, const nlohmann::json& properties);
        nlohmann::json PrepareStoreForPlayer(const std::string& owner,
            const nlohmann::json& items, RC::Unreal::UObject* controller);
    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;

        virtual void OnDatatableSerialized(RC::Unreal::UDataTable* datatable) override final;
    private:
        std::vector<RecipeDef> m_recipeDefs;
        std::vector<NpcCatalog::StoreOffer> m_storeOffers;
        bool m_autoReloading = false;
        struct PendingPatch { RC::StringType ModName; std::string Reference; nlohmann::json Changes; };
        std::vector<PendingPatch> m_pendingPatches;
        std::unordered_map<RC::StringType, RC::Unreal::UObject*> m_recipes;
        std::unordered_set<RC::StringType> m_unlock;
        std::unordered_set<RC::StringType> m_propsApplied;
        RC::Unreal::UClass* m_recipeClass = nullptr;
        RC::Unreal::UClass* m_progressComponentClass = nullptr;
        std::vector<std::pair<RC::Unreal::UFunction*, int32_t>> m_functionHooks;
        bool m_hooksActive = false;
        std::unordered_map<RC::StringType, std::string> m_vendorRecipeOwners;
        struct RecipeLease { int32_t Index; RC::StringType Path; };
        std::unordered_map<RC::StringType, RecipeLease> m_vendorRecipeLeases;
        uint64_t m_recipeRevision=0;
        RC::Unreal::UObject* LiveRecipe(const RC::StringType& key) const;

        void QueueData(const nlohmann::json& data, const RC::StringType& modName);
        void ApplyPendingPatches();
        void ApplyAll();
        void PlaceForTable(RC::Unreal::UDataTable* datatable);

        RC::Unreal::UObject* ResolveOrCreate(const RecipeDef& def, bool& outCreated);

        void ApplyProperties(RC::Unreal::UObject* recipe, const nlohmann::json& body, LoadResult& result);
        std::vector<Placement> ParsePlacements(const nlohmann::json& body);
        bool Place(RC::Unreal::UObject* recipe, const Placement& placement, RC::Unreal::UDataTable* datatable);
        bool PlaceInCategory(RC::Unreal::UObject* recipe, RC::Unreal::UScriptStruct* rowStruct, RC::Unreal::uint8* row, const RC::StringType& categoryLabel);
        bool PlaceInArray(RC::Unreal::UObject* recipe, RC::Unreal::UScriptStruct* rowStruct, RC::Unreal::uint8* row, const RC::StringType& arrayName, const RC::StringType& replaces);

        void RegisterHooks();
        void ApplyUnlocks(RC::Unreal::UObject* progressComponent);
        RC::Unreal::UObject* FindProgressComponent();
    };
}
