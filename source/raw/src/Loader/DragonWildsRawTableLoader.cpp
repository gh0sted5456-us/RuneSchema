#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "Unreal/NameTypes.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "SDK/Classes/UCompositeDataTable.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/Custom/UDataTableStore.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/Logging.h"
#include "Utility/JsonHelpers.h"
#include "Loader/DragonWildsRawTableLoader.h"
#include "Loader/CharacterCustomizationPlan.h"
#include "Core/JsonPatchDirective.h"
#include "Loader/WildcardFilter/WildcardFilters.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include <fstream>
#include <memory>
#include <set>

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    nlohmann::json ParseStrict(const fs::path& path) {
        if (fs::file_size(path) > 2 * 1024 * 1024) throw std::runtime_error("definition exceeds the 2 MiB limit");
        std::ifstream input(path, std::ios::binary); if (!input) throw std::runtime_error("definition could not be opened");
        std::vector<std::set<std::string>> objectKeys;
        const auto callback=[&](int, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
            if(event==nlohmann::json::parse_event_t::object_start)objectKeys.emplace_back();
            else if(event==nlohmann::json::parse_event_t::key) {if(objectKeys.empty()||!objectKeys.back().insert(parsed.get<std::string>()).second)throw std::runtime_error("duplicate JSON key: "+parsed.get<std::string>());}
            else if(event==nlohmann::json::parse_event_t::object_end&&!objectKeys.empty())objectKeys.pop_back();
            return true;
        };
        return nlohmann::json::parse(input,callback,true,true);
    }
}

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
        LoadRegistryDirectory(loaderPath / "patches", modName, false);
        LoadRegistryDirectory(loaderPath / "character_customization", modName, true);
    }

    void DragonWildsRawTableLoader::LoadDocument(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (data.is_array()) { for (const auto& entry : data) LoadDocument(entry, modName); return; }
        if (data.is_object() && data.value("schema", "") == RegistryPatch::Schema)
        {
            try { m_registryDocuments.push_back(RegistryPatch::ParseDocument(data, RC::to_string(modName), "raw")); }
            catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][REJECTED][MOD:{}] {}.\n"),modName,PS::ToWideSafe(error.what()));}
            return;
        }
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
            WarnPatchConflicts(m_patchConflicts, "raw:" + patch.Table + ":" + patch.Row,
                patch.Changes, RC::to_string(patch.ModName), false);
            patch.Changes["$PatchOnly"] = true;
            AddToTableDataMap(patch.Table,
                nlohmann::json{{patch.Row, std::move(patch.Changes)}});
            PS::Log<LogLevel::Verbose>(STR("{} queued raw patch '{}:{}'.\n"),
                patch.ModName, RC::to_generic_string(patch.Table),
                RC::to_generic_string(patch.Row));
        }
        m_pendingPatches.clear();
        try {
            m_registryPlan=RegistryPatch::BuildPlan(m_registryDocuments);
            if(!m_registryPlan.empty())PS::Log<LogLevel::Normal>(STR("[REGISTRY-PATCH][READY] {} operation{} planned with ownership, dependency, and capability validation.\n"),
                m_registryPlan.size(),m_registryPlan.size()==1?STR(""):STR("s"));
            LoadAndApplyRegistryTargets();
        } catch(const std::exception& error) {
            m_registryPlan.clear();
            PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][DISABLED] Plan rejected before mutation: {}. Legacy /raw records continue.\n"),PS::ToWideSafe(error.what()));
        }
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
            WarnPatchConflicts(m_patchConflicts, "raw:" + patch->Reference,
                changes, RC::to_string(modName), false);
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
        ApplyRegistryPatches(datatable);
    }

    void DragonWildsRawTableLoader::LoadRegistryDirectory(const fs::path& path, const RC::StringType& modName, bool customization)
    {
        if(!fs::is_directory(path))return;
        std::vector<fs::path> files;for(const auto& entry:fs::directory_iterator(path))if(entry.is_regular_file()&&(entry.path().extension()==".json"||entry.path().extension()==".jsonc"))files.push_back(entry.path());
        std::ranges::sort(files);
        for(const auto& file:files)try {
            auto data=ParseStrict(file);const auto source=file.generic_string();
            if(customization) {
                RegistryPatch::Document document;document.Owner=RegistryPatch::NormalizeId(RC::to_string(modName));document.Source=source;document.Profile="dragonwilds.characterCustomization.v1";
                document.Priority=data.value("priority",0);document.Patches=CharacterCustomization::Expand(data,RC::to_string(modName),source);m_registryDocuments.push_back(std::move(document));
            } else m_registryDocuments.push_back(RegistryPatch::ParseDocument(data,RC::to_string(modName),source));
        } catch(const std::exception& error) {PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][REJECTED][MOD:{}][FILE:{}] {}; remaining definitions continue.\n"),modName,file.native(),PS::ToWideSafe(error.what()));}
    }

    bool DragonWildsRawTableLoader::ProfileAllows(const RegistryPatch::Patch& patch) const
    {
        using enum RegistryPatch::Operation;
        if(patch.Profile=="dragonwilds.characterCustomization.v1") {
            const auto& path=patch.TargetSpec.ObjectPath;
            const auto table=patch.TargetSpec.Kind==RegistryPatch::TargetKind::DataTable&&patch.Op==AddRow&&
                (path==CharacterCustomization::HairZones||path==CharacterCustomization::HairPresets);
            const auto menu=patch.TargetSpec.Kind==RegistryPatch::TargetKind::ClassDefaultObject&&patch.Op==AppendUnique&&
                path==CharacterCustomization::CharacterOptions&&patch.Property=="CharacterOptions[HairPreset].OptionData";
            return table||menu;
        }
        if(patch.Profile=="dragonwilds.dataTableOwnedRows.v1")
            return patch.TargetSpec.Kind==RegistryPatch::TargetKind::DataTable&&patch.Op!=PatchExistingRow&&patch.Op<=MergeOwnedRow;
        if(patch.Profile=="dragonwilds.dataTablePatch.v1")
            return patch.TargetSpec.Kind==RegistryPatch::TargetKind::DataTable&&patch.Op==PatchExistingRow&&
                !patch.TargetSpec.ExpectedRowStruct.empty()&&patch.Preconditions.is_object()&&patch.Preconditions.contains("requiredProperties");
        return false;
    }

    void DragonWildsRawTableLoader::LoadAndApplyRegistryTargets()
    {
        std::vector<std::string> paths;
        for(const auto& patch:m_registryPlan)if(patch.TargetSpec.Kind==RegistryPatch::TargetKind::DataTable&&!patch.TargetSpec.ObjectPath.empty()&&
            std::ranges::find(paths,patch.TargetSpec.ObjectPath)==paths.end())paths.push_back(patch.TargetSpec.ObjectPath);
        for(const auto& path:paths)try {
            auto* object=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(path).c_str(),false);
            if(!object){UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(RC::to_generic_string(path))};object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);}
            if(!object||!object->IsA(UDataTable::StaticClass()))throw std::runtime_error("target did not resolve to UDataTable: "+path);
            ApplyRegistryPatches(static_cast<UDataTable*>(object));
        }catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][TARGET] {} could not be prepared: {}.\n"),RC::to_generic_string(path),PS::ToWideSafe(error.what()));}
        ApplyObjectRegistryPatches();
    }

    void DragonWildsRawTableLoader::ApplyObjectRegistryPatches()
    {
        for(const auto& patch:m_registryPlan)try {
            if(m_appliedRegistryPatches.contains(patch.CanonicalId)||patch.TargetSpec.Kind==RegistryPatch::TargetKind::DataTable)continue;
            if(!ProfileAllows(patch))throw std::runtime_error("capability profile rejected the object target or operation");
            if(std::ranges::any_of(patch.DependsOn,[&](const auto& dependency){
                const auto found=std::ranges::find_if(m_registryPlan,[&](const auto& candidate){return candidate.Owner==patch.Owner&&candidate.Id==dependency;});
                return found==m_registryPlan.end()||!m_appliedRegistryPatches.contains(found->CanonicalId);
            }))
                throw std::runtime_error("a required operation was not committed");
            auto* asset=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(patch.TargetSpec.ObjectPath).c_str(),false);
            if(!asset){UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(RC::to_generic_string(patch.TargetSpec.ObjectPath))};asset=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);}
            if(!asset)throw std::runtime_error("object target could not be loaded");
            UObject* target=asset;
            if(patch.TargetSpec.Kind==RegistryPatch::TargetKind::ClassDefaultObject) {
                if(!asset->IsA(UClass::StaticClass()))throw std::runtime_error("classDefaultObject target did not resolve to UClass");
                target=static_cast<UClass*>(asset)->GetClassDefaultObject().Get();if(!target)throw std::runtime_error("class default object is unavailable");
            }

            // v1 exposes one narrowly allowlisted reflected collection. Resolve
            // the map by its declared name, with a structural fallback for game
            // builds that renamed the outer field but retained the native shape.
            FMapProperty* options=PropertyHelper::GetPropertyByName<FMapProperty>(target->GetClassPrivate(),TEXT("CharacterOptions"));
            if(!options)for(FProperty* field=target->GetClassPrivate()->GetPropertyLink();field;field=field->GetPropertyLinkNext()) {
                auto* candidate=PropertyHelper::CastProperty<FMapProperty>(field);if(!candidate)continue;
                auto* valueStruct=PropertyHelper::CastProperty<FStructProperty>(candidate->GetValueProp());
                if(valueStruct&&PropertyHelper::GetPropertyByName<FArrayProperty>(valueStruct->GetStruct().Get(),TEXT("OptionData"))){options=candidate;break;}
            }
            if(!options)throw std::runtime_error("character-option map layout was not found");
            auto* keyProperty=options->GetKeyProp();auto* valueProperty=PropertyHelper::CastProperty<FStructProperty>(options->GetValueProp());
            if(!keyProperty||!valueProperty)throw std::runtime_error("character-option map key/value layout changed");
            const auto keySize=keyProperty->GetSize();const auto keyAlign=keyProperty->GetMinAlignment();
            if(keySize<=0||keyAlign<=0||(keyAlign&(keyAlign-1)))throw std::runtime_error("character-option map key layout is invalid");
            void* key=FMemory::Malloc(keySize,keyAlign);if(!key)throw std::bad_alloc();keyProperty->InitializeValue(key);
            struct KeyGuard{FProperty* Property;void* Data;~KeyGuard(){if(Data){Property->DestroyValue(Data);FMemory::Free(Data);}}} keyGuard{keyProperty,key};
            PropertyHelper::CopyJsonValueToContainer(key,keyProperty,"HairPreset");
            void* category=nullptr;UECustom::FScriptMapHelper map(options,options->ContainerPtrToValuePtr<void>(target));
            map.ForEachPair([&](void* candidateKey,void* value){if(!category&&keyProperty->Identical(candidateKey,key))category=value;});
            if(!category)throw std::runtime_error("HairPreset character-option category is missing");
            auto* arrayProperty=PropertyHelper::GetPropertyByName<FArrayProperty>(valueProperty->GetStruct().Get(),TEXT("OptionData"));
            if(!arrayProperty)throw std::runtime_error("HairPreset OptionData array is missing");
            auto* elementStruct=PropertyHelper::CastProperty<FStructProperty>(arrayProperty->GetInner());if(!elementStruct)throw std::runtime_error("HairPreset OptionData element is not a struct");
            auto* handleProperty=PropertyHelper::GetPropertyByName<FStructProperty>(elementStruct->GetStruct().Get(),TEXT("DataHandle"));
            auto* rowNameProperty=handleProperty?PropertyHelper::GetPropertyByName<FNameProperty>(handleProperty->GetStruct().Get(),TEXT("RowName")):nullptr;
            if(!handleProperty||!rowNameProperty)throw std::runtime_error("HairPreset option identity layout changed");
            const auto wanted=patch.Identity.value("value","");if(wanted.empty())throw std::runtime_error("appendUnique identity is missing");
            UECustom::FScriptArrayHelper array(arrayProperty->ContainerPtrToValuePtr<void>(category),arrayProperty);void* templateValue=nullptr;bool exists=false;
            array.ForEachElement([&](void* element){if(!templateValue)templateValue=element;auto* handle=handleProperty->ContainerPtrToValuePtr<void>(element);auto* name=rowNameProperty->ContainerPtrToValuePtr<FName>(handle);if(name&&RC::to_string(name->ToString())==wanted)exists=true;});
            if(exists){m_appliedRegistryPatches.insert(patch.CanonicalId);continue;}
            if(!templateValue)throw std::runtime_error("HairPreset category has no structural template element");
            UECustom::FManagedValue prepared;array.InitializeValue(prepared);
            struct ValueGuard{FProperty* Property;void* Data;~ValueGuard(){if(Property&&Data)Property->DestroyValue(Data);}} valueGuard{arrayProperty->GetInner(),prepared.GetData()};
            arrayProperty->GetInner()->CopySingleValue(prepared.GetData(),templateValue);
            auto values=ResolveRegistryValue(patch.Value,patch);
            if(values.value("BodyTypeCompatability","")=="both")values["BodyTypeCompatability"]="Both";
            else if(values.value("BodyTypeCompatability","")=="male")values["BodyTypeCompatability"]="Male";
            else if(values.value("BodyTypeCompatability","")=="female")values["BodyTypeCompatability"]="Female";
            if(values.contains("FaceTypeCompatibility")&&values["FaceTypeCompatibility"]=="all")values["FaceTypeCompatibility"]=-1;
            if(values.contains("EyeTypeCompatibility")&&values["EyeTypeCompatibility"]=="all")values["EyeTypeCompatibility"]=-1;
            for(const auto& [field,value]:values.items()){auto* property=PropertyHelper::GetPropertyByName(elementStruct->GetStruct().Get(),RC::to_generic_string(field));if(!property)throw std::runtime_error("HairPreset option property is missing: "+field);PropertyHelper::CopyJsonValueToContainer(prepared.GetData(),property,value);}
            array.Add(prepared);m_appliedRegistryPatches.insert(patch.CanonicalId);
            PS::Log<LogLevel::Normal>(STR("[REGISTRY-PATCH][COMMITTED][{}] Added unique HairPreset option '{}'.\n"),RC::to_generic_string(patch.Transaction),RC::to_generic_string(wanted));
        }catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][OBJECT][{}] Object operation was not committed: {}.\n"),RC::to_generic_string(patch.CanonicalId),PS::ToWideSafe(error.what()));}
    }

    nlohmann::json DragonWildsRawTableLoader::ResolveRegistryValue(const nlohmann::json& value, const RegistryPatch::Patch& patch) const
    {
        if(value.is_array()){auto result=nlohmann::json::array();for(const auto& item:value)result.push_back(ResolveRegistryValue(item,patch));return result;}
        if(!value.is_object())return value;
        if(value.contains("$ref")) {
            const auto reference=value.at("$ref").get<std::string>();const auto prefix=std::string("patch:");const auto separator=reference.find('#');
            if(!reference.starts_with(prefix)||separator==std::string::npos)throw std::runtime_error("invalid cross-patch reference");
            const auto id=reference.substr(prefix.size(),separator-prefix.size());const auto field=reference.substr(separator+1);
            const auto found=std::ranges::find_if(m_registryPlan,[&](const auto& candidate){return candidate.Owner==patch.Owner&&candidate.Id==id;});
            if(found==m_registryPlan.end()||field!="rowName")throw std::runtime_error("unresolved cross-patch reference");
            const auto name=found->Row.value("name","");return name=="auto"?RegistryPatch::StableRowName("RS_ROW",found->CanonicalId):name;
        }
        if(value.contains("$type")) {
            const auto type=value.at("$type").get<std::string>();
            if(type=="softObject"||type=="softClass")return value.at("path");
            if(type=="enum"||type=="name")return value.at("value");
            if(type=="text")return value.value("source","");
            if(type=="dataTableRowHandle")return nlohmann::json{{"DataTable",value.at("table")},{"RowName",ResolveRegistryValue(value.at("row"),patch)}};
            throw std::runtime_error("unsupported tagged registry value");
        }
        auto result=nlohmann::json::object();for(const auto& [key,item]:value.items())result[key]=ResolveRegistryValue(item,patch);return result;
    }

    void DragonWildsRawTableLoader::ApplyRegistryPatches(UDataTable* datatable)
    {
        if(!datatable||m_registryPlan.empty())return;const auto tablePath=RC::to_string(datatable->GetPathName());const auto tableName=RC::to_string(datatable->GetName());
        std::map<std::string,std::vector<const RegistryPatch::Patch*>> transactions;
        for(const auto& patch:m_registryPlan) {
            if(m_appliedRegistryPatches.contains(patch.CanonicalId)||patch.TargetSpec.Kind!=RegistryPatch::TargetKind::DataTable)continue;
            bool matches=!patch.TargetSpec.ObjectPath.empty()?patch.TargetSpec.ObjectPath==tablePath:patch.TargetSpec.ShortName==tableName;
            if(!matches)continue;
            if(!patch.TargetSpec.ShortName.empty()) {
                auto candidates=GetDatatablesByName(patch.TargetSpec.ShortName);std::erase_if(candidates,[&](auto* item){const auto path=RC::to_string(item->GetPathName());
                    if(!patch.TargetSpec.ExpectedRowStruct.empty()&&RC::to_string(item->GetRowStruct()->GetPathName())!=patch.TargetSpec.ExpectedRowStruct)return true;
                    return !patch.TargetSpec.SearchRoots.empty()&&std::ranges::none_of(patch.TargetSpec.SearchRoots,[&](const auto& root){return path.starts_with(root);});});
                if(candidates.size()!=1){PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][AMBIGUOUS] '{}': {} matching DataTables; use objectPath.\n"),RC::to_generic_string(patch.TargetSpec.ShortName),candidates.size());continue;}
            }
            transactions[patch.Owner+":"+patch.Transaction].push_back(&patch);
        }
        for(const auto& [transaction,patches]:transactions)try {
            struct Prepared{const RegistryPatch::Patch* Patch{};FName Name;std::unique_ptr<FManagedStruct> Data;bool Existing=false;};std::vector<Prepared> prepared;
            auto* rowStruct=datatable->GetRowStruct().Get();if(!rowStruct)throw std::runtime_error("target DataTable has no row struct");
            for(const auto* patch:patches) {
                if(!ProfileAllows(*patch))throw std::runtime_error("capability profile rejected the target or operation");
                if(!patch->TargetSpec.ExpectedRowStruct.empty()&&RC::to_string(rowStruct->GetPathName())!=patch->TargetSpec.ExpectedRowStruct)throw std::runtime_error("target row struct differs from expectedRowStruct");
                if(patch->Preconditions.is_object()&&patch->Preconditions.contains("requiredProperties"))for(const auto& [name,type]:patch->Preconditions.at("requiredProperties").items()) {
                    auto* property=PropertyHelper::GetPropertyByName(rowStruct,RC::to_generic_string(name));if(!property)throw std::runtime_error("required reflected property is missing: "+name);
                    const auto actual=PropertyHelper::GetPropertyTypeAsUTF8String(property);if(type.is_string()&&actual.find(type.get<std::string>())==std::string::npos)throw std::runtime_error("required reflected property changed type: "+name);
                }
                if(!patch->Row.is_object()||!patch->Row.contains("values")||!patch->Row.at("values").is_object())throw std::runtime_error("row requires values");
                auto name=patch->Row.value("name","");if(name=="auto")name=RegistryPatch::StableRowName("RS_ROW",patch->CanonicalId);if(name.empty())throw std::runtime_error("row name is empty");
                const FName rowName(RC::to_generic_string(name),FNAME_Add);auto* existing=datatable->FindRowUnchecked(rowName);const auto ownerKey=tablePath+":"+name;const auto owned=m_ownedRows.find(ownerKey);
                using enum RegistryPatch::Operation;
                if(existing&&(patch->Op==AddRow||patch->Op==CopyRow)) {if(owned!=m_ownedRows.end()&&owned->second==std::pair{patch->Owner,patch->Digest}){m_appliedRegistryPatches.insert(patch->CanonicalId);continue;}throw std::runtime_error("row already exists with different or external ownership: "+name);}
                if(existing&&(patch->Op==UpsertOwnedRow||patch->Op==MergeOwnedRow)&&(owned==m_ownedRows.end()||owned->second.first!=patch->Owner))throw std::runtime_error("owned-row update refused external row: "+name);
                if(!existing&&patch->Op==PatchExistingRow)throw std::runtime_error("patchExistingRow target is missing: "+name);
                auto data=std::make_unique<FManagedStruct>(rowStruct);void* sourceRow=existing;
                if(patch->Row.contains("template")) {const auto templateName=patch->Row.at("template").value("rowName","");sourceRow=datatable->FindRowUnchecked(FName(RC::to_generic_string(templateName),FNAME_Find));if(!sourceRow)throw std::runtime_error("template row is missing: "+templateName);}
                if(sourceRow)rowStruct->CopyScriptStruct(data->GetData(),sourceRow);
                auto values=ResolveRegistryValue(patch->Row.at("values"),*patch);
                if(values.contains("ZoneStyleIndex")&&values.at("ZoneStyleIndex").is_string()&&values.at("ZoneStyleIndex")=="auto") {
                    auto* number=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(rowStruct,TEXT("ZoneStyleIndex")));if(!number||!number->IsInteger())throw std::runtime_error("ZoneStyleIndex is not an integer property");
                    std::set<int64> used;for(const auto& [unused,row]:datatable->GetRowMap())used.insert(number->GetSignedIntPropertyValue(number->ContainerPtrToValuePtr<void>(row)));
                    int64 next=0;while(used.contains(next))++next;values["ZoneStyleIndex"]=next;
                }
                for(const auto& [field,value]:values.items()) {auto* property=PropertyHelper::GetPropertyByName(rowStruct,RC::to_generic_string(field));if(!property)throw std::runtime_error("row property is missing: "+field);PropertyHelper::CopyJsonValueToContainer(data->GetData(),property,value);}
                prepared.push_back({patch,rowName,std::move(data),existing!=nullptr});
            }
            for(auto& item:prepared){datatable->AddRow(item.Name,*reinterpret_cast<FTableRowBase*>(item.Data->GetData()));const auto name=RC::to_string(item.Name.ToString());m_ownedRows[tablePath+":"+name]={item.Patch->Owner,item.Patch->Digest};m_appliedRegistryPatches.insert(item.Patch->CanonicalId);}
            if(!prepared.empty())PS::Log<LogLevel::Normal>(STR("[REGISTRY-PATCH][COMMITTED][{}] {} owned row operation{} applied to {}.\n"),RC::to_generic_string(transaction),prepared.size(),prepared.size()==1?STR(""):STR("s"),datatable->GetName());
        } catch(const std::exception& error){PS::Log<LogLevel::Error>(STR("[REGISTRY-PATCH][REJECTED][{}] No rows from this table transaction were committed: {}.\n"),RC::to_generic_string(transaction),PS::ToWideSafe(error.what()));}
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
