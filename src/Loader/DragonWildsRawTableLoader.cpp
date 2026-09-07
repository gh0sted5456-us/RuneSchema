#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "Unreal/NameTypes.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "SDK/Classes/UCompositeDataTable.h"
#include "SDK/Classes/Custom/UDataTableStore.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/Logging.h"
#include "Utility/JsonHelpers.h"
#include "Loader/DragonWildsRawTableLoader.h"
#include "Core/JsonPatchDirective.h"
#include "Loader/WildcardFilter/WildcardFilters.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace DragonWilds {
    DragonWildsRawTableLoader::DragonWildsRawTableLoader() : DragonWildsModLoaderBase("raw")
    {
        SetDisplayName(TEXT("Raw Table Loader"));
    }


    void DragonWildsRawTableLoader::Apply(const RC::StringType& tableName, RC::Unreal::UDataTable* datatable)
    {
        auto it = m_tableDataMap.find(tableName);
        if (it != m_tableDataMap.end())
        {
            LoadResult result{};

            for (auto& data : it->second)
            {
                Apply(data, datatable, result);
            }

            PS::RoutineLog("raw", STR("{}: {} rows updated, {} rows added, {} rows deleted, {} error{}.\n"),
                datatable->GetName(), result.SuccessfulModifications, result.SuccessfulAdditions,
                result.SuccessfulDeletions, result.ErrorCount, result.ErrorCount > 1 || result.ErrorCount == 0 ? STR("s") : STR(""));
            if (result.Patched) PS::RoutineLog("patches", STR("{} $Patch: {} updated.\n"), datatable->GetName(), result.Patched);
        }
    }

    void DragonWildsRawTableLoader::Apply(UECustom::UCompositeDataTable* compositeDatatable)
    {
        auto parentTables = compositeDatatable->GetParentTables();
        for (auto& parentTable : parentTables)
        {
            auto parentTableName = parentTable->GetName();
            if (parentTableName.ends_with(STR("_Common")))
            {
                Apply(compositeDatatable->GetName(), parentTable.Get());
            }
        }
    }

    void DragonWildsRawTableLoader::Apply(const nlohmann::json& data, RC::Unreal::UDataTable* datatable, LoadResult& outResult)
    {
        for (auto& [dataKey, dataRow] : data.items())
        {
            if (dataKey == "Rows")
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("When copying entries from FModel, make sure to not include the 'Rows' field and instead add your row entries directly.\n"));
                continue;
            }

            if (dataKey.contains("*"))
            {
                HandleFilters(datatable, dataRow, outResult);
                continue;
            }

            auto rowKeyName = FName(RC::to_generic_string(dataKey), FNAME_Add);
            if (dataRow.is_null())
            {
                DeleteRow(datatable, rowKeyName, outResult);
                continue;
            }

            auto row = datatable->FindRowUnchecked(rowKeyName);
            const bool patchOnly = dataRow.is_object() && dataRow.value("$PatchOnly", false);
            if (!row)
            {
                if (patchOnly)
                {
                    outResult.ErrorCount++;
                    PS::Log<LogLevel::Error>(STR("Raw $Patch target '{}:{}' was not found; no row was created.\n"),
                        datatable->GetName(), rowKeyName.ToString());
                    continue;
                }
                AddRow(datatable, rowKeyName, dataRow, outResult);
                continue;
            }
            auto effective = dataRow;
            if (patchOnly) effective.erase("$PatchOnly");
            const auto priorErrors = outResult.ErrorCount;
            EditRow(datatable, rowKeyName, row, effective, outResult);
            if (patchOnly && outResult.ErrorCount == priorErrors) ++outResult.Patched;
        }
    }

    void DragonWildsRawTableLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::PostEngineInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            LoadDocument(data, modName);
        });
    }

    void DragonWildsRawTableLoader::LoadDocument(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (data.is_array()) { for (const auto& entry : data) LoadDocument(entry, modName); return; }
            try
            {
                static constexpr std::array<std::string_view, 0> noProtected{};
                if (const auto patch = JsonPatchDirective::Parse(data, noProtected, "raw"))
                {
                    const auto colon = patch->Reference.find(':');
                    if (colon == std::string::npos || colon == 0 || colon + 1 == patch->Reference.size())
                        throw std::runtime_error("raw $Patch identity must be DataTable:RowName");
                    m_pendingPatches.push_back({patch->Reference.substr(0, colon),
                        patch->Reference.substr(colon + 1), patch->Changes, modName});
                    return;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Raw patch from {}: {}. Skipping.\n"),
                    modName, PS::ToWideSafe(error.what()));
                return;
            }
            for (auto& [Key, Value] : data.items())
            {
                if (Key.starts_with("$"))
                {
                    continue;
                }

                AddToTableDataMap(Key, Value);
            }
    }

    void DragonWildsRawTableLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase)
    {
        if (phase != EEngineLifecyclePhase::PostEngineInit) return;
        for (auto& patch : m_pendingPatches)
        {
            patch.Changes["$PatchOnly"] = true;
            AddToTableDataMap(patch.Table,
                nlohmann::json{{patch.Row, std::move(patch.Changes)}});
            PS::Log<LogLevel::Verbose>(STR("{} queued raw patch '{}:{}'.\n"),
                patch.ModName, RC::to_generic_string(patch.Table),
                RC::to_generic_string(patch.Row));
        }
        m_pendingPatches.clear();
    }

    void DragonWildsRawTableLoader::OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            ReloadDocument(data, modName);
        });
    }

    void DragonWildsRawTableLoader::ReloadDocument(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (data.is_array()) { for (const auto& entry : data) ReloadDocument(entry, modName); return; }
        static constexpr std::array<std::string_view, 0> noProtected{};
        if (const auto patch = JsonPatchDirective::Parse(data, noProtected, "raw"))
        {
            const auto colon = patch->Reference.find(':');
            if (colon == std::string::npos || colon == 0 || colon + 1 == patch->Reference.size())
                throw std::runtime_error("raw $Patch identity must be DataTable:RowName");
            auto changes = patch->Changes;
            changes["$PatchOnly"] = true;
            ReloadDocument(nlohmann::json{{patch->Reference.substr(0, colon),
                {{patch->Reference.substr(colon + 1), changes}}}}, modName);
            return;
        }
            for (auto& [key, value] : data.items())
            {
                if (key.starts_with("$"))
                {
                    continue;
                }

                auto datatable = TryGetDatatableByName(key);
                if (!datatable)
                {
                    PS::Log<LogLevel::Error>(STR("Failed to auto-reload {}, data table {} doesn't exist.\n"),
                        modName, RC::to_generic_string(key));
                    return;
                }

                auto name = datatable->GetNamePrivate().ToString();
                LoadResult result;
                Apply(value, datatable, result);

                PS::RoutineLog("raw", STR("{}: {} rows updated, {} rows added, {} rows deleted, {} error{}.\n"),
                    name, result.SuccessfulModifications, result.SuccessfulAdditions,
                    result.SuccessfulDeletions, result.ErrorCount, result.ErrorCount > 1 || result.ErrorCount == 0 ? STR("s") : STR(""));
            if (result.Patched) PS::RoutineLog("patches", STR("{} $Patch: {} updated.\n"), datatable->GetName(), result.Patched);
            }
    }

    bool DragonWildsRawTableLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            return true;
        }

        return false;
    }

    bool DragonWildsRawTableLoader::OnInitialize()
    {
        return true;
    }

    void DragonWildsRawTableLoader::OnDatatableSerialized(RC::Unreal::UDataTable* datatable)
    {
        if (!datatable) return;

        if (datatable->GetClassPrivate() == UECustom::UCompositeDataTable::StaticClass())
        {
            auto compositeDatatable = static_cast<UECustom::UCompositeDataTable*>(datatable);
            Apply(compositeDatatable);
        }
        else
        {
            Apply(datatable->GetName(), datatable);
        }
    }

    void DragonWildsRawTableLoader::HandleFilters(RC::Unreal::UDataTable* datatable, const nlohmann::json& data, LoadResult& outResult)
    {
        try
        {
            if (data.is_null())
            {
                outResult.SuccessfulDeletions += datatable->GetRowMap().Num();
                datatable->EmptyTable();
            }
            else
            {
                PS::WildcardFilters wildcardFilters;
                if (data.contains("$Filters"))
                {
                    wildcardFilters.Parse(data.at("$Filters"), datatable->GetRowStruct().Get());
                }

                for (auto& [key, row] : datatable->GetRowMap())
                {
                    if (wildcardFilters.IsEmpty() || wildcardFilters.Match(row))
                    {
                        if (ModifyRowProperties(datatable, key, row, data, outResult))
                        {
                            outResult.SuccessfulModifications++;
                        }
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            outResult.ErrorCount++;
            PS::Log<LogLevel::Error>(STR("Failed to do wildcard modification in {}: {}\n"),
                datatable->GetNamePrivate().ToString(), PS::ToWideSafe(e.what()));
        }
    }

    void DragonWildsRawTableLoader::AddRow(RC::Unreal::UDataTable* datatable, const FName& rowName, const nlohmann::json& data, LoadResult& outResult)
    {
        auto rowStruct = datatable->GetRowStruct().Get();
        FManagedStruct newRowData(rowStruct);

        try
        {
            if (ModifyRowProperties(datatable, rowName, newRowData.GetData(), data, outResult))
            {
                datatable->AddRow(rowName, *reinterpret_cast<RC::Unreal::FTableRowBase*>(newRowData.GetData()));
                outResult.SuccessfulAdditions++;
            }
        }
        catch (const std::exception& e)
        {
            auto tableName = datatable->GetNamePrivate().ToString();
            outResult.ErrorCount++;
            PS::Log<LogLevel::Error>(STR("Failed to add Row '{}' in {}: {}\n"),
                rowName.ToString(), tableName, PS::ToWideSafe(e.what()));
        }
    }

    void DragonWildsRawTableLoader::EditRow(RC::Unreal::UDataTable* datatable, const FName& rowName, uint8* row, const nlohmann::json& data, LoadResult& outResult)
    {
        try
        {
            if (ModifyRowProperties(datatable, rowName, row, data, outResult))
            {
                outResult.SuccessfulModifications++;
            }
        }
        catch (const std::exception& e)
        {
            auto tableName = datatable->GetNamePrivate().ToString();
            outResult.ErrorCount++;
            PS::Log<LogLevel::Error>(STR("Failed to edit Row '{}' in {}: {}\n"),
                rowName.ToString(), tableName, PS::ToWideSafe(e.what()));
        }
    }

    void DragonWildsRawTableLoader::DeleteRow(RC::Unreal::UDataTable* datatable, const RC::Unreal::FName& rowName, LoadResult& outResult)
    {
        datatable->RemoveRow(rowName);
        outResult.SuccessfulDeletions++;
    }

    bool DragonWildsRawTableLoader::ModifyRowProperties(RC::Unreal::UDataTable* datatable, const FName& rowName, void* rowPtr, const nlohmann::json& data,
                                                        LoadResult& outResult)
    {
        if (!data.is_object())
        {
            throw std::runtime_error(std::format("Value for {} must be an object", RC::to_string(rowName.ToString())));
        }

        auto rowStruct = datatable->GetRowStruct().Get();
        bool wasRowModified = false;

        for (auto& [key, value] : data.items())
        {
            if (key == "$Filters" || key == "$Append")
            {
                continue;
            }

            auto keyWide = RC::to_generic_string(key);
            auto property = PropertyHelper::GetPropertyByName(rowStruct, keyWide);
            if (property)
            {
                PropertyHelper::CopyJsonValueToContainer(rowPtr, property, value);
                wasRowModified = true;
            }
            else
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("Property '{}' not found in Row '{}' in {}.\n"),
                    keyWide, rowName.ToString(), datatable->GetNamePrivate().ToString());
            }
        }

        if (!data.contains("$Append"))
        {
            return wasRowModified;
        }

        auto& appendData = data.at("$Append");
        if (!appendData.is_object())
        {
            throw std::runtime_error("$Append must be an object");
        }

        for (auto& [arrayKey, items] : appendData.items())
        {
            if (!items.is_array())
            {
                throw std::runtime_error(std::format("$Append.{} must be an array", arrayKey));
            }

            auto arrayKeyWide = RC::to_generic_string(arrayKey);
            auto arrayProperty = CastField<FArrayProperty>(
                PropertyHelper::GetPropertyByName(rowStruct, arrayKeyWide));

            if (!arrayProperty)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("Append target '{}' not found or is not an array in Row '{}' in {}.\n"),
                    arrayKeyWide, rowName.ToString(), datatable->GetNamePrivate().ToString());
                continue;
            }

            auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(rowPtr);
            if (!array)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("Append target '{}' has no array data in Row '{}' in {}.\n"),
                    arrayKeyWide, rowName.ToString(), datatable->GetNamePrivate().ToString());
                continue;
            }

            UECustom::FScriptArrayHelper helper(array, arrayProperty);
            for (auto& item : items)
            {
                UECustom::FManagedValue value;
                helper.InitializeValue(value);

                PropertyHelper::CopyJsonValueToContainer(value.GetData(), arrayProperty->GetInner(), item);
                helper.Add(value);
                wasRowModified = true;
            }
        }

        return wasRowModified;
    }

    void DragonWildsRawTableLoader::AddToTableDataMap(const std::string& datatableName, const nlohmann::json& data)
    {
        auto datatableNameWide = RC::to_generic_string(datatableName);
        auto it = m_tableDataMap.find(datatableNameWide);
        if (it != m_tableDataMap.end())
        {
            it->second.push_back(data);
        }
        else
        {
            std::vector<nlohmann::json> newDataArray{
                data
            };
            m_tableDataMap.emplace(datatableNameWide, newDataArray);
        }
    }
}
