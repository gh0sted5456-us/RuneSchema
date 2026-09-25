#include "Utility/NativeFunctionHook.h"
#include "SDK/WeakObjectHandle.h"
#include <algorithm>
#include <vector>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/UObjectArray.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsRecipeModLoader.h"
#include "Loader/VendorCategoryText.h"
#include "Loader/VendorCategoryLabel.h"
#include "Loader/RecipeUnlockPolicy.h"
#include "Core/JsonPatchDirective.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    static constexpr const TCHAR* RecipeDataClassPath = TEXT("/Script/Dominion.RecipeData");
    static constexpr const TCHAR* ProgressComponentClassPath = TEXT("/Script/Dominion.ProgressComponent");
    static constexpr const TCHAR* PlayerControllerClassPath = TEXT("/Script/Dominion.DominionPlayerController");
    static constexpr const TCHAR* ServerCraftRecipePath = TEXT("/Script/Dominion.InventoryController:Server_CraftRecipe");

    static constexpr const TCHAR* ClientUnlockHookPaths[] = {
        TEXT("/Script/Dominion.ProgressComponent:Client_HandleNewRecipesLoadedFromPersistence"),
        TEXT("/Script/Dominion.ProgressComponent:Client_OnRecipesUnlocked"),
    };

    static bool WantsUnlock(const nlohmann::json& body)
    {
        return PS::RecipeUnlockPolicy::Automatic(body);
    }

    static void AddRecipeUnlocks(UObject* progressComponent, const std::vector<UObject*>& recipes)
    {
        if (!progressComponent || recipes.empty())
        {
            return;
        }

        std::vector<const TCHAR*> targetSets{TEXT("RecipesUnlockedThatShouldNotPersist")};
        if (PS::PSConfig::Get()->GetSettings().persistence.recipes)
        {
            targetSets.insert(targetSets.begin(), TEXT("RecipesUnlocked"));
        }
        for (auto* propertyName : targetSets)
        {
            auto* setProperty = CastField<FSetProperty>(PropertyHelper::GetPropertyByName(progressComponent->GetClassPrivate(), propertyName));
            if (!setProperty)
            {
                continue;
            }

            UECustom::FScriptSetHelper helper(setProperty, setProperty->ContainerPtrToValuePtr<void>(progressComponent));
            for (auto* recipe : recipes)
            {
                helper.Add(&recipe);
            }
        }
    }

    static UObject* GetProgressComponentForInventoryController(UObject* inventoryController)
    {
        if (!inventoryController)
        {
            return nullptr;
        }

        static auto* playerControllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, PlayerControllerClassPath);
        if (!playerControllerClass)
        {
            return nullptr;
        }

        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(playerControllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }

            auto* inventoryProp = CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(controller->GetClassPrivate(), TEXT("InventoryController")));
            auto* progressProp = CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(controller->GetClassPrivate(), TEXT("ProgressComponent")));
            if (!inventoryProp || !progressProp)
            {
                continue;
            }

            UObject* controllerInventory = nullptr;
            FMemory::Memcpy(&controllerInventory, inventoryProp->ContainerPtrToValuePtr<void>(controller), sizeof(controllerInventory));
            if (controllerInventory != inventoryController)
            {
                continue;
            }

            UObject* progress = nullptr;
            FMemory::Memcpy(&progress, progressProp->ContainerPtrToValuePtr<void>(controller), sizeof(progress));
            return progress;
        }

        return nullptr;
    }

    DragonWildsRecipeModLoader::DragonWildsRecipeModLoader() : DragonWildsModLoaderBase("recipes")
    {
        SetDisplayName(TEXT("Recipe Loader"));
    }

    DragonWildsRecipeModLoader::~DragonWildsRecipeModLoader()
    {
        for (const auto& [function, id] : m_functionHooks) if (function && id) function->UnregisterHook(id);
        for (const auto& [key, owner] : m_vendorRecipeOwners) {
            if (auto* recipe=LiveRecipe(key))recipe->ClearRootSet();
        }
    }

    UObject* DragonWildsRecipeModLoader::LiveRecipe(const RC::StringType& key) const
    {
        auto found=m_recipes.find(key);
        if(found==m_recipes.end())return nullptr;
        if(!m_vendorRecipeOwners.contains(key))return found->second;
        auto lease=m_vendorRecipeLeases.find(key);
        if(lease==m_vendorRecipeLeases.end() || lease->second.Index<0)return nullptr;
        auto* slot=FUObjectArray::IndexToObject(lease->second.Index);
        if(!slot || slot->GetUObject()!=found->second || !slot->IsRootSet() || !slot->IsValid(false))return nullptr;
        auto* recipe=slot->GetUObject();
        return recipe && recipe->GetPathName()==lease->second.Path && recipe->IsA(m_recipeClass)?recipe:nullptr;
    }

    UObject* DragonWildsRecipeModLoader::EnsureVendorRecipe(const std::string& owner,
        const std::string& identity, const nlohmann::json& properties)
    {
        if (!m_recipeClass || !m_progressComponentClass)
            throw std::runtime_error("Vendor offers require the initialized /recipes loader");
        const auto key=RC::to_generic_string("RSVendor_"+identity);
        if(auto owned=m_vendorRecipeOwners.find(key);owned!=m_vendorRecipeOwners.end() && owned->second!=owner)
            throw std::runtime_error("Vendor recipe ownership collision; existing recipe preserved");
        UObject* recipe=nullptr;
        if (auto cached=m_recipes.find(key);cached!=m_recipes.end()) {
            auto tracked=m_vendorRecipeOwners.find(key);
            if (tracked==m_vendorRecipeOwners.end() || tracked->second!=owner)
                throw std::runtime_error("Vendor recipe identity collision; existing recipe preserved");
            recipe=LiveRecipe(key);
            if(!recipe) {
                m_recipes.erase(cached);
                m_vendorRecipeLeases.erase(key);
                m_propsApplied.erase(key);
            }
        }
        if(!recipe) {
            if(std::any_of(m_recipeDefs.begin(),m_recipeDefs.end(),[&](const auto& def){return def.Key==key;}))
                throw std::runtime_error("Vendor recipe conflicts with an authored recipe definition");
            auto* package=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Engine/Transient"));
            if (!package)throw std::runtime_error("Transient package unavailable for vendor recipe");
            const auto objectName=RC::to_generic_string(VendorOffers::RecipeObjectName(identity));
            const auto path=RC::StringType(TEXT("/Engine/Transient."))+objectName;
            if (UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path.c_str()))
                throw std::runtime_error("Vendor runtime recipe name collision; existing object preserved");
            FStaticConstructObjectParameters params(m_recipeClass,package);
            params.Name=FName(objectName,FNAME_Add);
            params.SetFlags=static_cast<EObjectFlags>(RF_Public|RF_Transient);
            recipe=UObjectGlobals::StaticConstructObject<UObject*>(params);
            if(!recipe)throw std::runtime_error("Failed to construct vendor RecipeData");
            ++m_recipeRevision;
            recipe->SetRootSet();
            // Retain ownership even if a field write fails. A bounded vendor
            // retry repairs this same object instead of leaking new recipes.
            m_recipes.emplace(key,recipe);
            m_vendorRecipeOwners.emplace(key,owner);
            m_vendorRecipeLeases.insert_or_assign(key,RecipeLease{recipe->GetInternalIndex(),recipe->GetPathName()});
        }
        {
            m_propsApplied.erase(key);
            m_unlock.erase(key);
            auto values=properties;
            values["InternalName"]=RC::to_string(key);
            values["PersistenceID"]=identity;
            for(const auto& [name,value]:values.items()) {
                auto* property=PropertyHelper::GetPropertyByName(m_recipeClass,RC::to_generic_string(name));
                if(!property)throw std::runtime_error("Vendor RecipeData field unavailable: "+name);
                PropertyHelper::CopyJsonValueToContainer(recipe,property,value);
            }
        }
        m_propsApplied.insert(key);
        m_unlock.insert(key);
        RegisterHooks();
        if(auto* progress=FindProgressComponent())ApplyUnlocks(progress);
        return recipe;
    }

    nlohmann::json DragonWildsRecipeModLoader::PrepareStoreForPlayer(
        const std::string& owner, const nlohmann::json& items, UObject* controller)
    {
        if(!controller || !items.is_array() || items.size()>128)
            throw std::runtime_error("Invalid store availability request");
        auto* progressField=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(controller->GetClassPrivate(),TEXT("ProgressComponent")));
        auto* progress=progressField?progressField->GetObjectPropertyValue(progressField->ContainerPtrToValuePtr<void>(controller)):nullptr;
        if(!progress || !m_progressComponentClass || !progress->IsA(m_progressComponentClass)
            || progress->GetWorld()!=controller->GetWorld())
            throw std::runtime_error("Current shop player has no valid world-owned ProgressComponent");
        std::vector<UObject*> recipes;
        nlohmann::json skipped=nlohmann::json::array();
        size_t index=0;
        for(const auto& item:items) {
            const auto slot=item.value("_RecipeSlot",std::to_string(index++));
            const auto key=RC::to_generic_string("RSVendor_"+VendorOffers::Identity(owner,slot));
            const auto found=m_recipes.find(key);
            const auto owned=m_vendorRecipeOwners.find(key);
            if(found==m_recipes.end() || owned==m_vendorRecipeOwners.end() || owned->second!=owner+":"+slot
                || !m_propsApplied.contains(key) || !LiveRecipe(key)) {
                skipped.push_back(RC::to_string(key));
                continue;
            }
            recipes.push_back(found->second);
        }
        nlohmann::json report={{"ExpectedOffers",recipes.size()},{"ProgressComponent",RC::to_string(progress->GetPathName())},{"Sets",nlohmann::json::object()}};
        report["RequestedOffers"]=items.size();report["SkippedOffers"]=std::move(skipped);
        report["Recipes"]=nlohmann::json::array();
        for(auto* recipe:recipes)report["Recipes"].push_back({{"Path",RC::to_string(recipe->GetPathName())},{"ObjectIndex",recipe->GetInternalIndex()}});
        report["RuntimeRecipeCreations"]=m_recipeRevision;
        std::vector<std::pair<const TCHAR*,FSetProperty*>> sets;
        std::vector<const TCHAR*> targetSets{TEXT("RecipesUnlockedThatShouldNotPersist")};
        if (PS::PSConfig::Get()->GetSettings().persistence.recipes)
            targetSets.insert(targetSets.begin(),TEXT("RecipesUnlocked"));
        for(const auto* name:targetSets) {
            auto* property=CastField<FSetProperty>(PropertyHelper::GetPropertyByName(progress->GetClassPrivate(),name));
            auto* element=property?CastField<FObjectPropertyBase>(property->GetElementProp()):nullptr;
            if(!property || property->GetArrayDim()!=1 || !element || element->GetElementSize()!=sizeof(UObject*)
                || !element->GetPropertyClass().Get() || !m_recipeClass->IsChildOf(element->GetPropertyClass().Get()))
                throw std::runtime_error("Unsupported recipe availability set layout");
            sets.emplace_back(name,property);
        }
        for(const auto& [name,property]:sets) {
            UECustom::FScriptSetHelper helper(property,property->ContainerPtrToValuePtr<void>(progress));
            size_t before=0,after=0;
            for(auto* recipe:recipes)if(helper.Contains(&recipe))++before;
            for(auto* recipe:recipes)helper.Add(&recipe);
            for(auto* recipe:recipes)if(helper.Contains(&recipe))++after;
            report["Sets"][RC::to_string(name)]={{"Before",before},{"After",after}};
            if(after!=recipes.size())throw std::runtime_error("Store recipe availability verification failed");
        }
        return report;
    }

    void DragonWildsRecipeModLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                QueueData(data, modName);
            });
        }
        else if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            ApplyPendingPatches();
            ApplyAll();
        }
    }

    void DragonWildsRecipeModLoader::OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath)
    {
        struct ReloadGuard { bool& Flag; ~ReloadGuard() { Flag=false; } } guard{m_autoReloading};
        m_autoReloading=true;
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            QueueData(data, modName);
        });

        ApplyAll();

        if (auto* progressComponent = FindProgressComponent())
        {
            ApplyUnlocks(progressComponent);
        }
    }

    bool DragonWildsRecipeModLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit;
    }

    bool DragonWildsRecipeModLoader::OnInitialize()
    {
        try
        {
            m_recipeClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, RecipeDataClassPath);
            if (!m_recipeClass)
            {
                throw std::runtime_error("Class RecipeData was not found");
            }

            m_progressComponentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, ProgressComponentClassPath);
            if (!m_progressComponentClass)
            {
                throw std::runtime_error("Class ProgressComponent was not found");
            }
        }
        catch (const std::exception& e)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, {}\n"), GetDisplayName(), PS::ToWideSafe(e.what()));
            return false;
        }

        return true;
    }

    void DragonWildsRecipeModLoader::OnDatatableSerialized(RC::Unreal::UDataTable* datatable)
    {
        if (datatable)
        {
            PlaceForTable(datatable);
        }
    }

    void DragonWildsRecipeModLoader::QueueData(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (!data.is_object())
        {
            PS::Log<LogLevel::Error>(STR("Recipe file for {} must be a JSON object.\n"), modName);
            return;
        }

        for (auto& [recipeKey, body] : data.items())
        {
            if (recipeKey.starts_with("$"))
            {
                continue;
            }
            if (recipeKey.starts_with("RSVendor_") || recipeKey.starts_with("RSMerchant_"))
                throw std::runtime_error("RSVendor_ recipe identities are reserved for generated /vendors offers");

            auto keyWide = RC::to_generic_string(recipeKey);
            if (keyWide.empty() || !body.is_object())
            {
                PS::Log<LogLevel::Error>(STR("Recipe '{}' must be a non-empty key with an object body. Skipping.\n"), keyWide);
                continue;
            }

            try
            {
                if (body.contains("VendorID") || body.contains("RuneSchemaVendors") || body.contains("VanillaVendors")) {
                    if(m_autoReloading)throw std::runtime_error("Store recipes require a restart; live reload was not applied");
                    auto offer=NpcCatalog::ParseStoreOffer(RC::to_string(modName),recipeKey,body);
                    for(const auto& existing:m_storeOffers)
                        if(existing.Mod==offer.Mod && existing.Id==offer.Id)
                            throw std::runtime_error("Duplicate store recipe: "+recipeKey);
                    auto placements=NpcCatalog::VanillaTargets(body);
                    if(!placements.empty()) {
                        for(auto& placement:placements)placement["Category"]=VendorOffers::Category(body);
                        nlohmann::json native={{"Properties",VendorOffers::Properties(body)},{"Unlock",true},{"AddTo",placements}};
                        const auto identity=RC::to_generic_string("RSMerchant_"+VendorOffers::Identity(offer.Mod,offer.Id));
                        m_recipeDefs.push_back({identity,native,ParsePlacements(native)});
                    }
                    m_storeOffers.push_back(std::move(offer));
                    continue;
                }
                static constexpr std::array<std::string_view, 1> protectedIdentity{"InternalName"};
                if (const auto patch = JsonPatchDirective::Parse(body, protectedIdentity, "recipe"))
                {
                    m_pendingPatches.push_back({modName, patch->Reference, patch->Changes});
                    continue;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Recipe patch '{}': {}. Skipping.\n"),
                    keyWide, PS::ToWideSafe(error.what()));
                continue;
            }

            try { WantsUnlock(body); }
            catch (const std::exception& error) {
                PS::Log<LogLevel::Error>(STR("Recipe '{}': {}. Skipping.\n"), keyWide, PS::ToWideSafe(error.what()));
                continue;
            }
            RecipeDef def{ keyWide, body, ParsePlacements(body) };

            auto existing = std::find_if(m_recipeDefs.begin(), m_recipeDefs.end(),
                [&](const RecipeDef& d) { return d.Key == keyWide; });
            if (existing != m_recipeDefs.end())
            {
                *existing = std::move(def);
            }
            else
            {
                m_recipeDefs.push_back(std::move(def));
            }

            m_propsApplied.erase(keyWide);
        }
    }

    void DragonWildsRecipeModLoader::ApplyPendingPatches()
    {
        size_t updated = 0, errors = 0;
        for (const auto& patch : m_pendingPatches)
        {
            auto key = patch.Reference;
            if (const auto colon = key.find(':'); colon != std::string::npos) key = key.substr(colon + 1);
            const auto wideKey = RC::to_generic_string(key);
            auto found = std::find_if(m_recipeDefs.begin(), m_recipeDefs.end(),
                [&](const RecipeDef& def) { return def.Key == wideKey; });
            if (found == m_recipeDefs.end())
            {
                PS::Log<LogLevel::Error>(STR("{}: recipe $Patch target '{}' was not loaded; no recipe was created.\n"),
                    patch.ModName, RC::to_generic_string(patch.Reference));
                ++errors;
                continue;
            }
            JsonPatchDirective::Directive directive{patch.Reference, patch.Changes};
            auto patchedBody = found->Body;
            const auto stats = JsonPatchDirective::Apply(patchedBody, directive, true);
            try { WantsUnlock(patchedBody); }
            catch (const std::exception& error) {
                PS::Log<LogLevel::Error>(STR("Recipe patch rejected: {}\n"), PS::ToWideSafe(error.what()));
                ++errors; continue;
            }
            found->Body = std::move(patchedBody);
            WarnPatchConflicts(m_patchConflicts, "recipes:" + key, patch.Changes, RC::to_string(patch.ModName));
            found->Placements = ParsePlacements(found->Body);
            m_propsApplied.erase(found->Key);
            ++updated;
            PS::Log<LogLevel::Verbose>( STR("{} patched recipe '{}' ({} fields, {} merged array rows, {} appended array rows).\n"),
                patch.ModName, found->Key, stats.FieldsOverwritten,
                stats.ArrayEntriesMerged, stats.ArrayEntriesAppended);
        }
        if (updated || errors) PS::RoutineLog("patches", STR("Recipes $Patch: {} updated, {} errors.\n"), updated, errors);
        m_pendingPatches.clear();
    }

    void DragonWildsRecipeModLoader::PrepareReferences()
    {
        if (!m_recipeClass || !m_progressComponentClass) return;
        ApplyPendingPatches();
        for (const auto& def : m_recipeDefs) {
            try { bool created = false; ResolveOrCreate(def, created); }
            catch (const std::exception& error) {
                PS::Log<LogLevel::Error>(STR("Recipe reference '{}' unavailable: {}\n"), def.Key, PS::ToWideSafe(error.what()));
            }
        }
    }

    void DragonWildsRecipeModLoader::ApplyAll()
    {
        if (m_recipeDefs.empty())
        {
            return;
        }

        LoadResult result{};
        constexpr size_t detailLimit = 12;
        size_t detailLines = 0;
        size_t omittedDetails = 0;

        for (auto& def : m_recipeDefs)
        {
            // Controls future automatic grants only; never revoke learned/save progress.
            if (WantsUnlock(def.Body)) m_unlock.insert(def.Key);
            else m_unlock.erase(def.Key);
            if (m_propsApplied.find(def.Key) != m_propsApplied.end())
            {
                continue;
            }

            bool created = false;
            auto* recipe = ResolveOrCreate(def, created);
            if (!recipe)
            {
                m_propsApplied.insert(def.Key);
                result.ErrorCount++;
                continue;
            }

            ApplyProperties(recipe, def.Body, result);
            m_propsApplied.insert(def.Key);

            if (created)
            {
                result.Created++;
                if (detailLines < detailLimit) {
                    PS::Log<LogLevel::Verbose>(STR("Created Recipe '{}'.\n"), def.Key);
                    ++detailLines;
                } else ++omittedDetails;
            }
            else
            {
                result.Edited++;
                if (detailLines < detailLimit) {
                    PS::Log<LogLevel::Verbose>(STR("Modified Recipe '{}'.\n"), def.Key);
                    ++detailLines;
                } else ++omittedDetails;
            }
        }

        RegisterHooks();

        int placed = 0;
        for (auto& def : m_recipeDefs)
        {
            auto it = m_recipes.find(def.Key);
            if (it == m_recipes.end() || !it->second)
            {
                continue;
            }

            for (auto& placement : def.Placements)
            {
                auto* datatable = ResolvePlacementTable(placement);
                if (datatable && Place(it->second, placement, datatable))
                {
                    ++placed;
                    if (detailLines < detailLimit) {
                        PS::Log<LogLevel::Verbose>(STR("Placed Recipe '{}' into {}.{}.\n"),
                            it->second->GetName(), RC::to_generic_string(PlacementTableLabel(placement)), placement.Row);
                        ++detailLines;
                    } else ++omittedDetails;
                }
            }
        }

        if (result.Created || result.Edited || placed || result.ErrorCount)
        {
            PS::RoutineLog("recipes", STR("Recipes: {} created, {} edited, {} placed, {} error{}.\n"),
                result.Created, result.Edited, placed, result.ErrorCount,
                result.ErrorCount == 1 ? STR("") : STR("s"));
        }
        if (omittedDetails)
            PS::Log<LogLevel::Verbose>(STR("Recipes: {} additional successful operation detail(s) omitted.\n"), omittedDetails);
    }

    std::string DragonWildsRecipeModLoader::PlacementTableLabel(const Placement& placement)
    {
        return placement.DataTable.empty() ? placement.Table : placement.DataTable;
    }

    UDataTable* DragonWildsRecipeModLoader::ResolvePlacementTable(const Placement& placement)
    {
        if (placement.DataTable.empty())
        {
            return TryGetDatatableByName(placement.Table);
        }

        const auto path = RC::to_generic_string(placement.DataTable);
        auto* object = UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path.c_str(), false);
        if (!object)
        {
            auto soft = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(path));
            object = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
        }

        static auto* dataTableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.DataTable"), false);
        if (!object || !dataTableClass || !object->IsA(dataTableClass))
        {
            PS::Log<LogLevel::Error>(STR("Recipe placement DataTable '{}' did not resolve to a DataTable. Skipping target.\n"), path);
            return nullptr;
        }
        return static_cast<UDataTable*>(object);
    }

    void DragonWildsRecipeModLoader::PlaceForTable(RC::Unreal::UDataTable* datatable)
    {
        auto tableName = RC::to_string(datatable->GetName());
        auto tablePath = RC::to_string(datatable->GetPathName());

        int placed = 0;
        for (auto& def : m_recipeDefs)
        {
            auto it = m_recipes.find(def.Key);
            if (it == m_recipes.end() || !it->second)
            {
                continue;
            }

            for (auto& placement : def.Placements)
            {
                const bool matches = placement.DataTable.empty()
                    ? placement.Table == tableName
                    : placement.DataTable == tablePath;
                if (matches && Place(it->second, placement, datatable))
                {
                    placed++;
                }
            }
        }

        if (placed > 0)
        {
            PS::Log<LogLevel::Normal>(STR("{}: {} recipe(s) placed.\n"), datatable->GetName(), placed);
        }
    }

    UObject* DragonWildsRecipeModLoader::ResolveOrCreate(const RecipeDef& def, bool& outCreated)
    {
        outCreated = false;

        auto cached = m_recipes.find(def.Key);
        if (cached != m_recipes.end())
        {
            return cached->second;
        }

        if (def.Key.starts_with(TEXT("/")))
        {
            auto* recipe = UECustom::UObjectGlobals::StaticFindObject(nullptr, nullptr, def.Key.c_str(), false);
            if (!recipe)
            {
                auto soft = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(def.Key));
                recipe = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            if (!recipe)
            {
                PS::Log<LogLevel::Error>(STR("Recipe '{}' was not found.\n"), def.Key);
                return nullptr;
            }

            recipe->SetRootSet();
            m_recipes.emplace(def.Key, recipe);

            if (WantsUnlock(def.Body))
            {
                m_unlock.insert(def.Key);
            }
            return recipe;
        }

        TArray<UObject*> recipes;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_recipeClass, recipes, true);
        const FName recipeName(def.Key,FNAME_Add);
        for (auto* candidate : recipes)
        {
            if (candidate && candidate->GetFName() == recipeName
                && !candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject)))
            {
                candidate->SetRootSet();
                m_recipes.emplace(def.Key, candidate);

                if (WantsUnlock(def.Body))
                {
                    m_unlock.insert(def.Key);
                }
                return candidate;
            }
        }

        static auto* transientPackage = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, TEXT("/Engine/Transient"), false);

        FStaticConstructObjectParameters params(m_recipeClass, transientPackage);
        params.Name = FName(def.Key, FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(RF_Public | RF_Standalone | RF_Transactional);

        auto* recipe = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!recipe)
        {
            PS::Log<LogLevel::Error>(STR("Failed to construct Recipe '{}'.\n"), def.Key);
            return nullptr;
        }

        recipe->SetRootSet();

        for (auto* propertyName : { TEXT("PersistenceID"), TEXT("InternalName") })
        {
            if (auto* property = PropertyHelper::GetPropertyByName(m_recipeClass, propertyName))
            {
                nlohmann::json value = RC::to_string(def.Key);
                PropertyHelper::CopyJsonValueToContainer(reinterpret_cast<uint8*>(recipe), property, value);
            }
        }

        if (WantsUnlock(def.Body))
        {
            m_unlock.insert(def.Key);
        }

        m_recipes.emplace(def.Key, recipe);
        outCreated = true;
        return recipe;
    }

    void DragonWildsRecipeModLoader::ApplyProperties(UObject* recipe, const nlohmann::json& body, LoadResult& result)
    {
        const nlohmann::json* properties = &body;
        if (body.contains("Properties") && body.at("Properties").is_object())
        {
            properties = &body.at("Properties");
        }

        auto* recipeClass = recipe->GetClassPrivate();
        for (auto& [propertyName, propertyValue] : properties->items())
        {
            if (propertyName == "AddTo" || propertyName == "Properties" || propertyName == "Unlock")
            {
                continue;
            }

            auto propertyNameWide = RC::to_generic_string(propertyName);
            auto* property = PropertyHelper::GetPropertyByName(recipeClass, propertyNameWide);
            if (!property)
            {
                PS::Log<LogLevel::Warning>(STR("Property '{}' not found on Recipe '{}'.\n"),
                    propertyNameWide, recipe->GetName());
                result.ErrorCount++;
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(reinterpret_cast<uint8*>(recipe), property, propertyValue);
            }
            catch (const std::exception& e)
            {
                PS::Log<LogLevel::Error>(STR("Failed writing '{}' on Recipe '{}': {}\n"),
                    propertyNameWide, recipe->GetName(), PS::ToWideSafe(e.what()));
                result.ErrorCount++;
            }
        }
    }

    std::vector<DragonWildsRecipeModLoader::Placement> DragonWildsRecipeModLoader::ParsePlacements(const nlohmann::json& body)
    {
        std::vector<Placement> placements;

        if (!body.contains("AddTo") || !body.at("AddTo").is_array())
        {
            return placements;
        }

        auto readString = [](const nlohmann::json& object, const char* key) -> RC::StringType {
            if (object.contains(key) && object.at(key).is_string())
            {
                return RC::to_generic_string(object.at(key).get<std::string>());
            }
            return {};
        };

        for (auto& target : body.at("AddTo"))
        {
            if (!target.is_object())
            {
                continue;
            }

            Placement placement{};
            placement.Table = target.contains("Table") && target.at("Table").is_string()
                ? target.at("Table").get<std::string>() : std::string{};
            placement.DataTable = target.contains("DataTable") && target.at("DataTable").is_string()
                ? target.at("DataTable").get<std::string>() : std::string{};
            placement.Row = readString(target, "Row");
            placement.Category = readString(target, "Category");
            placement.Array = readString(target, "Array");
            placement.Replaces = readString(target, "Replaces");

            const bool hasOneTableTarget = placement.Table.empty() != placement.DataTable.empty();
            const bool validDataTablePath = placement.DataTable.empty()
                || (placement.DataTable.starts_with('/') && placement.DataTable.find('.') != std::string::npos);
            if (hasOneTableTarget && validDataTablePath && !placement.Row.empty()
                && (!placement.Category.empty() || !placement.Array.empty()))
            {
                placements.push_back(std::move(placement));
            }
        }

        return placements;
    }

    bool DragonWildsRecipeModLoader::Place(UObject* recipe, const Placement& placement, RC::Unreal::UDataTable* datatable)
    {
        auto rowStruct = datatable->GetRowStruct();
        auto* row = datatable->FindRowUnchecked(FName(placement.Row, FNAME_Add));
        if (!rowStruct || !row)
        {
            return false;
        }

        try
        {
            if (!placement.Category.empty())
            {
                return PlaceInCategory(recipe, rowStruct.Get(), row, placement.Category);
            }
            return PlaceInArray(recipe, rowStruct.Get(), row, placement.Array, placement.Replaces);
        }
        catch (const std::exception& e)
        {
            PS::Log<LogLevel::Error>(STR("Failed placing Recipe '{}' into {}.{}: {}\n"),
                recipe->GetName(), RC::to_generic_string(PlacementTableLabel(placement)), placement.Row, PS::ToWideSafe(e.what()));
            return false;
        }
    }

    bool DragonWildsRecipeModLoader::PlaceInCategory(UObject* recipe, UScriptStruct* rowStruct, uint8* row, const RC::StringType& categoryLabel)
    {
        auto* labeledProp = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(rowStruct, TEXT("LabeledRecipes")));
        if (!labeledProp)
        {
            return false;
        }

        auto* categoryProp = CastField<FStructProperty>(labeledProp->GetInner());
        if (!categoryProp || !categoryProp->GetStruct())
        {
            return false;
        }

        auto* labelProp = CastField<FTextProperty>(
            PropertyHelper::GetPropertyByName(categoryProp->GetStruct().Get(), TEXT("Label")));
        auto* collectionProp = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(categoryProp->GetStruct().Get(), TEXT("Collection")));
        if (!labelProp || !collectionProp || !CastField<FSoftObjectProperty>(collectionProp->GetInner()))
        {
            return false;
        }

        if (collectionProp->GetInner()->GetElementSize() != static_cast<int32>(sizeof(UECustom::FSoftObjectPtr)))
        {
            PS::Log<LogLevel::Error>(STR("LabeledRecipes element size ({}) does not match FSoftObjectPtr ({}) - skipping placement.\n"),
                collectionProp->GetInner()->GetElementSize(), sizeof(UECustom::FSoftObjectPtr));
            return false;
        }

        auto* labeledArray = labeledProp->ContainerPtrToValuePtr<FScriptArray>(row);
        auto categorySize = categoryProp->GetElementSize();
        if (labeledProp->GetArrayDim() != 1 || categoryProp->GetArrayDim() != 1
            || categorySize <= 0 || labeledArray->Num() < 0
            || (labeledArray->Num() && !labeledArray->GetData()))
            throw std::runtime_error("Vendor category: invalid LabeledRecipes array layout");
        for (auto* field : {static_cast<FProperty*>(labelProp), static_cast<FProperty*>(collectionProp)})
            if (field->GetArrayDim() != 1 || field->GetOffset_Internal() < 0
                || field->GetElementSize() <= 0 || field->GetOffset_Internal() > categorySize
                || field->GetElementSize() > categorySize - field->GetOffset_Internal())
                throw std::runtime_error("Vendor category: label/collection is outside its reflected struct");

        int32 categoryIndex = -1;
        if (labeledArray->GetData())
        {
            auto* data = static_cast<uint8*>(labeledArray->GetData());
            for (int32 i = 0; i < labeledArray->Num(); ++i)
            {
                const auto label = VendorCategoryText::Read(data + i * categorySize, labelProp);
                if (label == RC::to_string(categoryLabel))
                {
                    categoryIndex = i;
                    break;
                }
            }
        }

        if (categoryIndex < 0)
        {
            UECustom::FScriptArrayHelper helper(labeledArray, labeledProp);
            UECustom::FManagedValue value;
            helper.InitializeValue(value);
            VendorCategoryText::Write(value.GetData(), labelProp, RC::to_string(categoryLabel));
            categoryIndex = labeledArray->Num();
            helper.Add(value);
        }

        auto* categoryData = static_cast<uint8*>(labeledArray->GetData()) + categoryIndex * categorySize;
        VendorCategoryLabel::RequireExact(RC::to_string(categoryLabel),
            VendorCategoryText::Read(categoryData, labelProp));
        auto* collectionArray = collectionProp->ContainerPtrToValuePtr<FScriptArray>(categoryData);
        auto collectionElementSize = collectionProp->GetInner()->GetElementSize();

        auto recipeID = UECustom::FSoftObjectPath(recipe->GetPathName());
        if (collectionArray->GetData())
        {
            auto* data = static_cast<uint8*>(collectionArray->GetData());
            for (int32 i = 0; i < collectionArray->Num(); ++i)
            {
                auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(data + i * collectionElementSize);
                // A soft-object reference is identified by its asset path.  Do not touch
                // the cached weak pointer here: during GameInstance initialization UE may
                // still be constructing the cloned recipe, and resolving/caching that weak
                // handle can enter UE4SS with an object slot that is not live yet.
                if (soft->ObjectID.AssetPath.GetPackageName() == recipeID.AssetPath.GetPackageName()
                    && soft->ObjectID.AssetPath.GetAssetName() == recipeID.AssetPath.GetAssetName())
                {
                    return false;
                }
            }
        }

        UECustom::FScriptArrayHelper helper(collectionArray, collectionProp);
        UECustom::FManagedValue value;
        helper.InitializeValue(value);

        auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(value.GetData());
        soft->ObjectID = recipeID;
        helper.Add(value);
        return true;
    }

    bool DragonWildsRecipeModLoader::PlaceInArray(UObject* recipe, UScriptStruct* rowStruct, uint8* row, const RC::StringType& arrayName, const RC::StringType& replaces)
    {
        auto* arrayProp = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(rowStruct, arrayName));
        if (!arrayProp || !CastField<FObjectProperty>(arrayProp->GetInner()))
        {
            return false;
        }

        auto* array = arrayProp->ContainerPtrToValuePtr<FScriptArray>(row);
        auto elementSize = arrayProp->GetInner()->GetElementSize();

        if (array->GetData())
        {
            auto* data = static_cast<uint8*>(array->GetData());
            for (int32 i = 0; i < array->Num(); ++i)
            {
                UObject* existing = nullptr;
                FMemory::Memcpy(&existing, data + i * elementSize, sizeof(existing));

                if (existing == recipe)
                {
                    return false;
                }

                if (!replaces.empty() && existing && existing->GetFName() == FName(replaces,FNAME_Add))
                {
                    UObject* recipePtr = recipe;
                    FMemory::Memcpy(data + i * elementSize, &recipePtr, sizeof(recipePtr));
                    return true;
                }
            }
        }

        if (!replaces.empty())
        {
            return false;
        }

        UECustom::FScriptArrayHelper helper(array, arrayProp);
        UECustom::FManagedValue value;
        helper.InitializeValue(value);
        UObject* recipePtr = recipe;
        FMemory::Memcpy(value.GetData(), &recipePtr, sizeof(recipePtr));
        helper.Add(value);
        return true;
    }

    void DragonWildsRecipeModLoader::RegisterHooks()
    {
        if (m_hooksActive || m_unlock.empty())
        {
            return;
        }

        for (auto* hookPath : ClientUnlockHookPaths)
        {
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, hookPath);
            if (!function)
            {
                PS::Log<LogLevel::Warning>(STR("Client unlock hook '{}' was not found.\n"), hookPath);
                continue;
            }

            const auto id = PS::RegisterNativePostHook(function, [this](UnrealScriptFunctionCallableContext& context, void*) {
                ApplyUnlocks(context.Context);
            });
            m_functionHooks.emplace_back(function, id);
        }

        if (auto* serverCraftFunction = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, ServerCraftRecipePath))
        {
            auto* recipeProperty = CastField<FObjectProperty>(serverCraftFunction->FindProperty(FName(TEXT("Recipe"), FNAME_Find)));
            if (!recipeProperty)
            {
                PS::Log<LogLevel::Warning>(STR("Server craft hook '{}' has no 'Recipe' parameter; modded recipes can't be crafted on dedicated servers.\n"), ServerCraftRecipePath);
            }
            else
            {
                const auto id = PS::RegisterNativePreHook(serverCraftFunction, [this, recipeProperty](UnrealScriptFunctionCallableContext& context, void*) {
                    if (!context.TheStack.Locals())
                    {
                        return;
                    }

                    UObject* recipe = nullptr;
                    FMemory::Memcpy(&recipe, recipeProperty->ContainerPtrToValuePtr<void>(context.TheStack.Locals()), sizeof(recipe));
                    if (!recipe)
                    {
                        return;
                    }

                    bool tracked = std::any_of(m_unlock.begin(), m_unlock.end(), [&](const RC::StringType& key) {
                        auto it = m_recipes.find(key);
                        return it != m_recipes.end() && it->second == recipe;
                    });
                    if (!tracked)
                    {
                        return;
                    }

                    if (auto* progressComponent = GetProgressComponentForInventoryController(context.Context))
                    {
                        std::vector<UObject*> one{ recipe };
                        AddRecipeUnlocks(progressComponent, one);
                    }
                });
                m_functionHooks.emplace_back(serverCraftFunction, id);
            }
        }
        else
        {
            PS::Log<LogLevel::Warning>(STR("Server craft hook '{}' was not found.\n"), ServerCraftRecipePath);
        }

        m_hooksActive = true;
    }

    void DragonWildsRecipeModLoader::ApplyUnlocks(UObject* progressComponent)
    {
        if (!progressComponent || m_unlock.empty())
        {
            return;
        }

        std::vector<UObject*> recipes;
        for (auto& key : m_unlock)
        {
            if(auto* recipe=LiveRecipe(key))recipes.push_back(recipe);
        }

        AddRecipeUnlocks(progressComponent, recipes);
    }

    UObject* DragonWildsRecipeModLoader::FindProgressComponent()
    {
        if (!m_progressComponentClass)
        {
            return nullptr;
        }

        TArray<UObject*> components;
        UECustom::UObjectGlobals::GetObjectsOfClass(m_progressComponentClass, components, true);

        for (auto* component : components)
        {
            if (component && !component->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                return component;
            }
        }

        return nullptr;
    }
}
