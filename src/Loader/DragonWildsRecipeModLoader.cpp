#include <algorithm>
#include <vector>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/FWeakObjectPtr.hpp"
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
#include "Utility/Logging.h"
#include "Loader/DragonWildsRecipeModLoader.h"

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
        return body.contains("Unlock") && body.at("Unlock").is_boolean() && body.at("Unlock").get<bool>();
    }

    static void AddRecipeUnlocks(UObject* progressComponent, const std::vector<UObject*>& recipes)
    {
        if (!progressComponent || recipes.empty())
        {
            return;
        }

        for (auto* propertyName : { TEXT("RecipesUnlocked"), TEXT("RecipesUnlockedThatShouldNotPersist") })
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
            ApplyAll();
        }
    }

    void DragonWildsRecipeModLoader::OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath)
    {
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

            auto keyWide = RC::to_generic_string(recipeKey);
            if (keyWide.empty() || !body.is_object())
            {
                PS::Log<LogLevel::Error>(STR("Recipe '{}' must be a non-empty key with an object body. Skipping.\n"), keyWide);
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

    void DragonWildsRecipeModLoader::ApplyAll()
    {
        if (m_recipeDefs.empty())
        {
            return;
        }

        LoadResult result{};

        for (auto& def : m_recipeDefs)
        {
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
            }
            else
            {
                result.Edited++;
                PS::Log<LogLevel::Normal>(STR("Modified Recipe '{}'\n"), def.Key);
            }
        }

        if (result.Created || result.Edited || result.ErrorCount)
        {
            PS::Log<LogLevel::Normal>(STR("Recipes: {} created, {} edited, {} error{}.\n"),
                result.Created, result.Edited, result.ErrorCount, result.ErrorCount == 1 ? STR("") : STR("s"));
        }

        RegisterHooks();

        for (auto& def : m_recipeDefs)
        {
            auto it = m_recipes.find(def.Key);
            if (it == m_recipes.end() || !it->second)
            {
                continue;
            }

            for (auto& placement : def.Placements)
            {
                auto* datatable = TryGetDatatableByName(placement.Table);
                if (datatable && Place(it->second, placement, datatable))
                {
                    PS::Log<LogLevel::Normal>(STR("Placed Recipe '{}' into {}.{}\n"),
                        it->second->GetName(), RC::to_generic_string(placement.Table), placement.Row);
                }
            }
        }
    }

    void DragonWildsRecipeModLoader::PlaceForTable(RC::Unreal::UDataTable* datatable)
    {
        auto tableName = RC::to_string(datatable->GetName());

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
                if (placement.Table == tableName && Place(it->second, placement, datatable))
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
        for (auto* candidate : recipes)
        {
            if (candidate && candidate->GetName() == def.Key
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

        PS::Log<LogLevel::Normal>(STR("Created Recipe '{}'\n"), def.Key);

        if (WantsUnlock(def.Body)
            || std::any_of(def.Placements.begin(), def.Placements.end(),
                [](const Placement& placement) { return !placement.Category.empty(); }))
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
            placement.Row = readString(target, "Row");
            placement.Category = readString(target, "Category");
            placement.Array = readString(target, "Array");
            placement.Replaces = readString(target, "Replaces");

            if (!placement.Table.empty() && !placement.Row.empty()
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
                recipe->GetName(), RC::to_generic_string(placement.Table), placement.Row, PS::ToWideSafe(e.what()));
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

        auto* labelProp = PropertyHelper::GetPropertyByName(categoryProp->GetStruct().Get(), TEXT("Label"));
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

        int32 categoryIndex = -1;
        if (labeledArray->GetData())
        {
            auto* data = static_cast<uint8*>(labeledArray->GetData());
            for (int32 i = 0; i < labeledArray->Num(); ++i)
            {
                auto* label = labelProp->ContainerPtrToValuePtr<FText>(data + i * categorySize);
                if (label && PropertyHelper::GetTextAsString(*label) == categoryLabel)
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
            PropertyHelper::CopyJsonValueToContainer(value.GetData(), labelProp, RC::to_string(categoryLabel));
            categoryIndex = labeledArray->Num();
            helper.Add(value);
        }

        auto* categoryData = static_cast<uint8*>(labeledArray->GetData()) + categoryIndex * categorySize;
        auto* collectionArray = collectionProp->ContainerPtrToValuePtr<FScriptArray>(categoryData);
        auto collectionElementSize = collectionProp->GetInner()->GetElementSize();

        auto recipeID = UECustom::FSoftObjectPath(recipe->GetPathName());
        if (collectionArray->GetData())
        {
            auto* data = static_cast<uint8*>(collectionArray->GetData());
            for (int32 i = 0; i < collectionArray->Num(); ++i)
            {
                auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(data + i * collectionElementSize);
                if (soft->WeakPtr.Get() == recipe
                    || (soft->ObjectID.AssetPath.GetPackageName() == recipeID.AssetPath.GetPackageName()
                        && soft->ObjectID.AssetPath.GetAssetName() == recipeID.AssetPath.GetAssetName()))
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
        soft->WeakPtr = FWeakObjectPtr(recipe);
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

                if (!replaces.empty() && existing && existing->GetName() == replaces)
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

            function->RegisterPostHook([this](UnrealScriptFunctionCallableContext& context, void*) {
                ApplyUnlocks(context.Context);
            });
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
                serverCraftFunction->RegisterPreHook([this, recipeProperty](UnrealScriptFunctionCallableContext& context, void*) {
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
            auto it = m_recipes.find(key);
            if (it != m_recipes.end() && it->second)
            {
                recipes.push_back(it->second);
            }
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
