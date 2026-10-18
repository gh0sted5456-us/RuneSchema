#include "Loader/HelpyDependencyOrder.h"
#include "Core/RecipeWorkspace.h"
#include "Core/NetworkIdentityState.h"
#include "Generator/AssetSearch.h"
#include "Generator/HelpyStatKey.h"
#include "Generator/HelpyPropertyValue.h"
#include "Generator/QuickMenuDecorations.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "Runtime/AuthoredFile.h"
#include "Runtime/HelpyBundlePublish.h"
#include "Loader/AssetAuthoringMetadata.h"
#include "Loader/CharacterCustomizationLayout.h"
#include "Generator/ClonePresentation.h"
#include "SDK/Helper/CookedAssetLookup.h"
#include "SDK/Helper/ItemAppearanceMetadata.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include <memory>
#include <set>
#include <algorithm>
#include <sstream>
#include <array>
#include "Loader/AssetProvenance.h"
#include "Loader/DragonWildsRecipeModLoader.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Utility/AssetAliases.h"
#include "Loader/ItemIdentity.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <limits>
#include <string_view>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Helpers/String.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/ConsumeQueue.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsAssetModLoader.h"
#include "Loader/RegistryPatchPlan.h"
#include "Core/JsonPatchDirective.h"
#include "Core/RegistryProvenance.h"
#include "Loader/PlayerGhost.h"
#include "Loader/EquipmentWearableBridge.h"
#include "Loader/WearableBridgeDirective.h"
#include "Loader/QuestScroll.h"
#include "Loader/QuestBoardCancellation.h"
#include "Loader/QuestScrollUseContract.h"
#include "SDK/Helper/ActorHelper.h"
#include "Core/CookedPakRegistryManifest.h"
#include "SDK/Helper/RecipeAssetCatalog.h"

using namespace RC;
using namespace RC::Unreal;

namespace
{
    enum class ImportPolicy { Explicit, Required, Closure };
    enum class ImportConflict { Error, Reuse, Replace, AllowDuplicate };
    enum class ExternalImportMode { Off, Plan };

    struct ModuleImportSettings
    {
        std::vector<std::string> Modules;
        ImportPolicy Policy = ImportPolicy::Explicit;
        ImportConflict Conflict = ImportConflict::Error;
        bool Atomic = true;
        bool Dry = false;
        bool Verbose = false;
        ExternalImportMode External = ExternalImportMode::Off;
        nlohmann::json Remap = nlohmann::json::object();
    };

    bool IsModuleImportDirective(std::string_view key)
    {
        return key == "$ImpModule" || key == "$ImpPolicy" || key == "$ImpRemap"
            || key == "$ImpConflict" || key == "$ImpAtomic" || key == "$ImpDry"
            || key == "$ImpVerbose" || key == "$ImpExternal";
    }

    bool HasModuleImportDirectives(const nlohmann::json& body)
    {
        if (!body.is_object()) return false;
        for (const auto& [key, unused] : body.items())
            if (IsModuleImportDirective(key)) return true;
        return false;
    }

    ModuleImportSettings ParseModuleImportSettings(const nlohmann::json& body)
    {
        ModuleImportSettings settings;
        if (!body.contains("$ImpModule"))
        {
            if (HasModuleImportDirectives(body))
                throw std::runtime_error("$ImpModule is required when using $Imp* directives");
            return settings;
        }

        const auto& authored = body.at("$ImpModule");
        const auto add = [&](const nlohmann::json& value) {
            if (!value.is_string() || value.get_ref<const std::string&>().empty())
                throw std::runtime_error("$ImpModule entries must be non-empty object-path strings");
            settings.Modules.push_back(value.get<std::string>());
        };
        if (authored.is_string()) add(authored);
        else if (authored.is_array())
        {
            if (authored.empty() || authored.size() > 256)
                throw std::runtime_error("$ImpModule requires 1..256 entries");
            for (const auto& value : authored) add(value);
        }
        else throw std::runtime_error("$ImpModule must be a string or array of strings");

        if (body.contains("$ImpPolicy"))
        {
            if (!body.at("$ImpPolicy").is_string()) throw std::runtime_error("$ImpPolicy must be a string");
            const auto value = body.at("$ImpPolicy").get<std::string>();
            if (value == "Explicit") settings.Policy = ImportPolicy::Explicit;
            else if (value == "Required") settings.Policy = ImportPolicy::Required;
            else if (value == "Closure") settings.Policy = ImportPolicy::Closure;
            else throw std::runtime_error("$ImpPolicy must be Explicit, Required, or Closure");
        }
        if (body.contains("$ImpConflict"))
        {
            if (!body.at("$ImpConflict").is_string()) throw std::runtime_error("$ImpConflict must be a string");
            const auto value = body.at("$ImpConflict").get<std::string>();
            if (value == "Error") settings.Conflict = ImportConflict::Error;
            else if (value == "Reuse") settings.Conflict = ImportConflict::Reuse;
            else if (value == "Replace") settings.Conflict = ImportConflict::Replace;
            else if (value == "AllowDuplicate") settings.Conflict = ImportConflict::AllowDuplicate;
            else throw std::runtime_error("$ImpConflict must be Error, Reuse, Replace, or AllowDuplicate");
        }
        if (body.contains("$ImpAtomic"))
        {
            if (!body.at("$ImpAtomic").is_boolean()) throw std::runtime_error("$ImpAtomic must be a boolean");
            settings.Atomic = body.at("$ImpAtomic").get<bool>();
        }
        if (body.contains("$ImpDry"))
        {
            if (!body.at("$ImpDry").is_boolean()) throw std::runtime_error("$ImpDry must be a boolean");
            settings.Dry = body.at("$ImpDry").get<bool>();
        }
        if (body.contains("$ImpVerbose"))
        {
            if (!body.at("$ImpVerbose").is_boolean()) throw std::runtime_error("$ImpVerbose must be a boolean");
            settings.Verbose = body.at("$ImpVerbose").get<bool>();
        }
        if (body.contains("$ImpExternal"))
        {
            if (!body.at("$ImpExternal").is_string()) throw std::runtime_error("$ImpExternal must be a string");
            const auto value = body.at("$ImpExternal").get<std::string>();
            if (value == "Off") settings.External = ExternalImportMode::Off;
            else if (value == "Plan") settings.External = ExternalImportMode::Plan;
            else throw std::runtime_error("$ImpExternal must be Off or Plan");
        }
        if (body.contains("$ImpRemap"))
        {
            if (!body.at("$ImpRemap").is_object()) throw std::runtime_error("$ImpRemap must be an object");
            const auto validTarget = [](const nlohmann::json& value) {
                if (value.is_string()) return !value.get_ref<const std::string&>().empty();
                if (!value.is_array() || value.empty() || value.size() > 64) return false;
                return std::all_of(value.begin(), value.end(), [](const nlohmann::json& item) {
                    return item.is_string() && !item.get_ref<const std::string&>().empty();
                });
            };
            for (const auto& [key, value] : body.at("$ImpRemap").items())
            {
                if (key.empty()) throw std::runtime_error("$ImpRemap keys must not be empty");
                if (validTarget(value)) continue;
                if (!value.is_object())
                    throw std::runtime_error("$ImpRemap values must be a string, string array, or scoped object");
                for (const auto& [dependency, target] : value.items())
                    if (dependency.empty() || !validTarget(target))
                        throw std::runtime_error("scoped $ImpRemap entries must map dependency names to a string or string array");
            }
            settings.Remap = body.at("$ImpRemap");
        }
        return settings;
    }

    bool IsDataAssetTarget(const std::string& target)
    {
        if (target.empty() || target.front() != '/' || target.find("..") != std::string::npos)
            return false;
        const auto slash = target.find_last_of('/');
        const auto dot = target.find('.', slash == std::string::npos ? 0 : slash);
        if (slash == std::string::npos || dot == std::string::npos || dot <= slash + 1)
            return false;
        return target.substr(slash + 1, dot - slash - 1).starts_with("DA_");
    }

    bool IsDirectDataAssetPatch(const nlohmann::json& document)
    {
        if (!document.is_object()) return false;
        bool found = false;
        for (const auto& [target, body] : document.items())
        {
            if (target.starts_with('$')) continue;
            if (!IsDataAssetTarget(target) || !body.is_object()
                || body.contains("$Clone") || body.contains("$Patch") || HasModuleImportDirectives(body)) return false;
            found = true;
        }
        return found;
    }

    bool IsCharacterOptionPath(std::string_view path)
    {
        return (path.starts_with("CharacterOptionData[") || path.starts_with("CharacterOptions["))
            && path.ends_with("].OptionData");
    }

    bool IsCharacterColumnPath(std::string_view path)
    {
        return (path.starts_with("CharacterOptionData[") || path.starts_with("CharacterOptions["))
            && path.ends_with("].NumberOfColumns");
    }

    void ValidateCharacterColumnValue(const nlohmann::json& value)
    {
        if (!value.is_number_integer())
            throw std::runtime_error("character customization NumberOfColumns must be an integer");
        const auto columns=value.get<int>();
        if (columns < 1 || columns > 8)
            throw std::runtime_error("character customization NumberOfColumns must be between 1 and 8");
    }

    void NormalizeCharacterOptionValue(nlohmann::json& value)
    {
        if (value.is_array())
        {
            for (auto& item : value) NormalizeCharacterOptionValue(item);
            return;
        }
        if (!value.is_object()) return;
        const auto normalizeEnum = [&](const char* field, const char* authored,
            const char* reflected) {
            if (value.contains(field) && value.at(field).is_string()
                && value.at(field).get<std::string>() == authored)
                value[field] = reflected;
        };
        normalizeEnum("BodyTypeCompatability", "both", "Both");
        normalizeEnum("BodyTypeCompatability", "male", "Male");
        normalizeEnum("BodyTypeCompatability", "female", "Female");
        for (const auto* field : {"FaceTypeCompatibility", "EyeTypeCompatibility"})
            if (value.contains(field) && value.at(field).is_string()
                && value.at(field).get<std::string>() == "all") value[field] = -1;
    }

    nlohmann::json DirectDataAssetOperations(const nlohmann::json& body)
    {
        if (body.contains("$Operations"))
        {
            if (!body.at("$Operations").is_array() || body.at("$Operations").empty()
                || body.at("$Operations").size() > 1024)
                throw std::runtime_error("$Operations requires 1..1024 entries");
            return body.at("$Operations");
        }

        nlohmann::json operations = nlohmann::json::array();
        for (const auto& [path, authored] : body.items())
        {
            if (path.starts_with('$')) continue;
            nlohmann::json operation{{"Path", path}};
            if (authored.is_object())
            {
                static constexpr std::array<std::string_view, 5> directives{
                    "$Set", "$Merge", "$Append", "$AppendUnique", "$MergeWhere"};
                std::string selected;
                for (const auto directive : directives)
                    if (authored.contains(directive))
                    {
                        if (!selected.empty())
                            throw std::runtime_error("a DA_ field may select only one write directive");
                        selected = std::string(directive);
                    }
                if (selected.empty())
                {
                    operation["Op"] = "Merge";
                    operation["Value"] = authored;
                }
                else
                {
                    operation["Op"] = selected.substr(1);
                    if (selected == "$MergeWhere")
                    {
                        const auto& group=authored.at(selected);
                        if (!group.is_object() || !group.contains("Field") || !group.at("Field").is_string()
                            || !group.contains("Values") || !group.at("Values").is_array()
                            || group.at("Values").empty() || !group.contains("Value")
                            || !group.at("Value").is_object())
                            throw std::runtime_error("$MergeWhere requires Field, a non-empty Values array, and an object Value");
                        operation["SelectorPath"] = group.at("Field");
                        operation["SelectorValues"] = group.at("Values");
                        operation["Value"] = group.at("Value");
                    }
                    else operation["Value"] = authored.at(selected);
                    for (const auto& [key, unused] : authored.items())
                        if (key != selected && key != "$Identity" && key != "$TemplateIndex")
                            throw std::runtime_error("unknown DA_ field directive: " + key);
                    if (selected == "$AppendUnique")
                    {
                        if (authored.contains("$Identity"))
                            operation["IdentityPath"] = authored.at("$Identity");
                        else if (IsCharacterOptionPath(path))
                            operation["IdentityPath"] = "DataHandle.RowName";
                        else throw std::runtime_error("$AppendUnique requires $Identity");
                    }
                    if (authored.contains("$TemplateIndex"))
                        operation["TemplateIndex"] = authored.at("$TemplateIndex");
                    else if (selected == "$AppendUnique" && IsCharacterOptionPath(path))
                        operation["TemplateIndex"] = 0;
                }
            }
            else
            {
                operation["Op"] = "Set";
                operation["Value"] = authored;
            }
            if (IsCharacterOptionPath(path)) NormalizeCharacterOptionValue(operation["Value"]);
            if (IsCharacterColumnPath(path)) ValidateCharacterColumnValue(operation["Value"]);
            operations.push_back(std::move(operation));
            if (operations.size() > 1024)
                throw std::runtime_error("a DA_ target may contain at most 1024 field writes");
        }
        if (operations.empty()) throw std::runtime_error("a DA_ target requires at least one field write");
        return operations;
    }

    struct PathSegment
    {
        std::string Name;
        std::string Selector;
        bool HasSelector = false;
    };

    struct ResolvedMember
    {
        void* Container = nullptr;
        FProperty* Property = nullptr;
        std::string CanonicalPath;
    };

    std::vector<PathSegment> ParsePropertyPath(const std::string& path)
    {
        if (path.empty() || path.size() > 1024) throw std::runtime_error("property path length is invalid");
        std::vector<PathSegment> result;
        std::size_t offset = 0;
        while (offset < path.size())
        {
            const auto dot = path.find('.', offset);
            const auto token = path.substr(offset, dot == std::string::npos ? std::string::npos : dot - offset);
            if (token.empty()) throw std::runtime_error("property path contains an empty segment");
            PathSegment segment;
            const auto open = token.find('[');
            if (open == std::string::npos) segment.Name = token;
            else
            {
                if (token.back() != ']' || token.find('[', open + 1) != std::string::npos)
                    throw std::runtime_error("property path selector is malformed");
                segment.Name = token.substr(0, open);
                segment.Selector = token.substr(open + 1, token.size() - open - 2);
                segment.HasSelector = true;
                if (segment.Selector.empty() || segment.Selector == "*" || segment.Selector.contains(".."))
                    throw std::runtime_error("property path selector is invalid");
            }
            if (segment.Name.empty() || !std::ranges::all_of(segment.Name, [](unsigned char c) {
                    return std::isalnum(c) || c == '_';
                })) throw std::runtime_error("property path contains an invalid identifier");
            result.push_back(std::move(segment));
            if (result.size() > 32) throw std::runtime_error("property path exceeds 32 segments");
            if (dot == std::string::npos) break;
            offset = dot + 1;
        }
        return result;
    }

    FProperty* FindMember(UClass* objectType, UScriptStruct* structType, const std::string& name)
    {
        const auto wide = RC::to_generic_string(name);
        auto* found = objectType ? DragonWilds::PropertyHelper::GetPropertyByName(objectType, wide)
            : DragonWilds::PropertyHelper::GetPropertyByName(structType, wide);
        if (found || (name != "CharacterOptionData" && name != "CharacterOptions")) return found;
        FField* field = objectType ? static_cast<FField*>(objectType->GetPropertyLink())
            : structType->GetChildProperties();
        while (field)
        {
            auto* candidate=DragonWilds::PropertyHelper::CastProperty<FMapProperty>(field);
            auto* valueStruct=candidate
                ? DragonWilds::PropertyHelper::CastProperty<FStructProperty>(candidate->GetValueProp()) : nullptr;
            if (valueStruct && DragonWilds::PropertyHelper::GetPropertyByName<FArrayProperty>(
                    valueStruct->GetStruct().Get(),TEXT("OptionData"))) return candidate;
            field = DragonWilds::PropertyHelper::GetNextField(field);
        }
        return nullptr;
    }

    ResolvedMember ResolveMember(void* root, UClass* rootType, UScriptStruct* rootStruct,
        const std::string& path)
    {
        auto segments = ParsePropertyPath(path);
        void* container = root;
        UClass* objectType = rootType;
        UScriptStruct* structType = rootStruct;
        std::string canonical;
        for (std::size_t index = 0; index < segments.size(); ++index)
        {
            const auto& segment = segments[index];
            auto* property = FindMember(objectType, structType, segment.Name);
            if (!property) throw std::runtime_error("property path member was not found: " + segment.Name);
            if (!canonical.empty()) canonical.push_back('.');
            const auto actualName = RC::to_string(property->GetName());
            canonical += actualName;
            if (segment.HasSelector)
            {
                canonical.push_back('[');
                if (actualName == "CharacterOptionData" && segment.Selector == "HairPreset")
                    canonical += "ECharacterOptionType::HairPreset";
                else canonical += segment.Selector;
                canonical.push_back(']');
            }
            const bool last = index + 1 == segments.size();
            if (last)
            {
                if (segment.HasSelector)
                    throw std::runtime_error("a terminal selector is not a writable member; select a child property");
                return {container, property, canonical};
            }

            void* value = property->ContainerPtrToValuePtr<void>(container);
            FProperty* selectedType = property;
            if (segment.HasSelector)
            {
                if (auto* mapProperty = DragonWilds::PropertyHelper::CastProperty<FMapProperty>(property))
                {
                    auto* keyProperty = mapProperty->GetKeyProp();
                    auto* valueProperty = mapProperty->GetValueProp();
                    UECustom::FScriptMapHelper map(mapProperty, value);
                    UECustom::FManagedValue pair;
                    map.InitializePair(pair);
                    auto selector = segment.Selector;
                    DragonWilds::PropertyHelper::CopyJsonValueToContainer(
                        pair.GetData(), keyProperty, selector);
                    void* selected = nullptr;
                    map.ForEachPair([&](void* key, void* mapValue) {
                        if (!selected && keyProperty->Identical(key, map.GetKeyPtr(pair.GetData()))) selected = mapValue;
                    });
                    if (!selected) throw std::runtime_error("map selector did not match an existing key: " + selector);
                    value = selected;
                    selectedType = valueProperty;
                }
                else if (auto* arrayProperty = DragonWilds::PropertyHelper::CastProperty<FArrayProperty>(property))
                {
                    if (!std::ranges::all_of(segment.Selector, [](unsigned char c) { return std::isdigit(c); }))
                        throw std::runtime_error("array selector must be a non-negative integer");
                    const auto wanted = std::stoull(segment.Selector);
                    FScriptArrayHelper array(arrayProperty, value);
                    if (wanted >= static_cast<std::size_t>(array.Num()))
                        throw std::runtime_error("array selector is outside the current array");
                    value = array.GetRawPtr(static_cast<int32>(wanted));
                    selectedType = arrayProperty->GetInner();
                }
                else throw std::runtime_error("selector used on a property that is not a map or array");
            }

            if (auto* objectProperty = CastField<FObjectPropertyBase>(selectedType))
            {
                auto* object = objectProperty->GetObjectPropertyValue(value);
                if (!object) throw std::runtime_error("property path traversed a null object reference");
                container = object;
                objectType = object->GetClassPrivate();
                structType = nullptr;
            }
            else if (auto* childStruct = DragonWilds::PropertyHelper::CastProperty<FStructProperty>(selectedType))
            {
                container = value;
                objectType = nullptr;
                structType = childStruct->GetStruct().Get();
            }
            else throw std::runtime_error("property path attempted to traverse a scalar property");
        }
        throw std::runtime_error("property path did not resolve");
    }

    void MergeStruct(void* value, UScriptStruct* type, const nlohmann::json& patch)
    {
        if (!patch.is_object()) throw std::runtime_error("Merge requires an object value");
        for (const auto& [name, fieldValue] : patch.items())
        {
            auto* field = DragonWilds::PropertyHelper::GetPropertyByName(type, RC::to_generic_string(name));
            if (!field) throw std::runtime_error("merge member was not found: " + name);
            DragonWilds::PropertyHelper::CopyJsonValueToContainer(value, field, fieldValue);
        }
    }

    bool SameIdentity(void* left, void* right, UScriptStruct* type, const std::string& identityPath)
    {
        const auto a = ResolveMember(left, nullptr, type, identityPath);
        const auto b = ResolveMember(right, nullptr, type, identityPath);
        if (a.Property != b.Property) return false;
        return a.Property->Identical(a.Property->ContainerPtrToValuePtr<void>(a.Container),
            b.Property->ContainerPtrToValuePtr<void>(b.Container));
    }

    struct AppendResult
    {
        int Before = 0;
        int After = 0;
        int Added = 0;
        int Existing = 0;
        std::string Identity;
    };

    void ValidateCharacterOptionHandle(const nlohmann::json& operation)
    {
        if (!operation.contains("Value"))
            throw std::runtime_error("character option append requires Value");
        const auto validate=[](const nlohmann::json& value) {
        if (!value.is_object()) throw std::runtime_error("character option value must be an object");
        if (!value.contains("DataHandle") || !value.at("DataHandle").is_object())
            throw std::runtime_error("character option append requires DataHandle");
        const auto& handle=value.at("DataHandle");
        if (!handle.contains("DataTable") || !handle.at("DataTable").is_string()
            || !handle.contains("RowName") || !handle.at("RowName").is_string())
            throw std::runtime_error("character option DataHandle requires string DataTable and RowName");
        const auto tablePath=RC::to_generic_string(handle.at("DataTable").get<std::string>());
        auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,tablePath.c_str(),false);
        if (!object)
        {
            UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(tablePath)};
            object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
        }
        if (!object || !object->IsA(UDataTable::StaticClass()))
            throw std::runtime_error("character option DataHandle table did not resolve to UDataTable");
        const auto row=FName(RC::to_generic_string(handle.at("RowName").get<std::string>()),FNAME_Find);
        if (!static_cast<UDataTable*>(object)->FindRowUnchecked(row))
            throw std::runtime_error("character option DataHandle row is unavailable");
        };
        const auto& value=operation.at("Value");
        if (value.is_array()) for (const auto& item : value) validate(item);
        else validate(value);
    }

    AppendResult AppendValues(const ResolvedMember& member, const nlohmann::json& operation, bool unique)
    {
        auto* arrayProperty = DragonWilds::PropertyHelper::CastProperty<FArrayProperty>(member.Property);
        if (!arrayProperty) throw std::runtime_error("Append operation requires an array property");
        auto* elementStruct = DragonWilds::PropertyHelper::CastProperty<FStructProperty>(arrayProperty->GetInner());
        if (unique && (!elementStruct || !operation.contains("IdentityPath")
            || !operation.at("IdentityPath").is_string()))
            throw std::runtime_error("AppendUnique requires a struct array and IdentityPath");
        if (!operation.contains("Value")) throw std::runtime_error("Append operation requires Value");
        nlohmann::json values = operation.at("Value").is_array()
            ? operation.at("Value") : nlohmann::json::array({operation.at("Value")});
        auto* arrayAddress = arrayProperty->ContainerPtrToValuePtr<void>(member.Container);
        UECustom::FScriptArrayHelper destination(arrayAddress, arrayProperty);
        FScriptArrayHelper inspect(arrayProperty, arrayAddress);
        AppendResult result;
        result.Before = inspect.Num();
        for (const auto& value : values)
        {
            UECustom::FManagedValue prepared;
            destination.InitializeValue(prepared);
            if (operation.contains("TemplateIndex"))
            {
                const auto templateIndex = operation.at("TemplateIndex").get<int>();
                if (templateIndex < 0 || templateIndex >= inspect.Num())
                    throw std::runtime_error("TemplateIndex is outside the current array");
                arrayProperty->GetInner()->CopySingleValue(prepared.GetData(), inspect.GetRawPtr(templateIndex));
            }
            DragonWilds::PropertyHelper::CopyJsonValueToContainer(
                prepared.GetData(), arrayProperty->GetInner(), value);
            bool exists = false;
            int identityMatches = 0;
            if (unique)
            {
                const auto identity = operation.at("IdentityPath").get<std::string>();
                if (value.is_object() && value.contains("DataHandle")
                    && value.at("DataHandle").is_object()
                    && value.at("DataHandle").contains("RowName")
                    && value.at("DataHandle").at("RowName").is_string())
                    result.Identity = value.at("DataHandle").at("RowName").get<std::string>();
                for (int32 index = 0; index < inspect.Num(); ++index)
                    if (SameIdentity(prepared.GetData(), inspect.GetRawPtr(index),
                        elementStruct->GetStruct().Get(), identity))
                    {
                        ++identityMatches;
                        if (!arrayProperty->GetInner()->Identical(prepared.GetData(),inspect.GetRawPtr(index)))
                            throw std::runtime_error("AppendUnique identity conflicts with a different existing value");
                        exists=true;
                    }
                if (identityMatches > 1)
                    throw std::runtime_error("AppendUnique identity already exists more than once");
            }
            if (!exists) { destination.Add(prepared); ++result.Added; }
            else ++result.Existing;
            if (unique)
            {
                const auto identity = operation.at("IdentityPath").get<std::string>();
                int verified = 0;
                for (int32 index = 0; index < inspect.Num(); ++index)
                    if (SameIdentity(prepared.GetData(), inspect.GetRawPtr(index),
                        elementStruct->GetStruct().Get(), identity)) ++verified;
                if (verified != 1)
                    throw std::runtime_error("AppendUnique post-commit identity verification failed");
            }
        }
        result.After = inspect.Num();
        if (result.After != result.Before + result.Added)
            throw std::runtime_error("Append operation count verification failed");
        return result;
    }

    struct CharacterLayoutResult
    {
        int Options = 0;
        int Before = 0;
        int After = 0;
    };

    CharacterLayoutResult AdjustCharacterOptionColumns(
        UObject* target, const std::string& optionPath)
    {
        if (!target || !IsCharacterOptionPath(optionPath)) return {};
        auto options=ResolveMember(target,target->GetClassPrivate(),nullptr,optionPath);
        auto* arrayProperty=DragonWilds::PropertyHelper::CastProperty<FArrayProperty>(
            options.Property);
        if (!arrayProperty)
            throw std::runtime_error("character customization OptionData is no longer an array");
        auto columnPath=optionPath.substr(0,
            optionPath.size()-std::string_view("OptionData").size())+"NumberOfColumns";
        auto columns=ResolveMember(target,target->GetClassPrivate(),nullptr,columnPath);
        auto* numeric=DragonWilds::PropertyHelper::CastProperty<FNumericProperty>(
            columns.Property);
        if (!numeric || !numeric->IsInteger())
            throw std::runtime_error(
                "character customization NumberOfColumns is no longer an integer property");
        auto* arrayAddress=arrayProperty->ContainerPtrToValuePtr<void>(options.Container);
        FScriptArrayHelper inspect(arrayProperty,arrayAddress);
        if (inspect.Num()<0)
            throw std::runtime_error(
                "character customization OptionData has an invalid element count");
        auto* columnAddress=numeric->ContainerPtrToValuePtr<void>(columns.Container);
        const auto before=static_cast<int>(numeric->GetSignedIntPropertyValue(columnAddress));
        const auto after=DragonWilds::CharacterCustomizationLayout::RequiredColumns(
            static_cast<std::size_t>(inspect.Num()),before);
        if (after!=before)
        {
            numeric->SetIntPropertyValue(columnAddress,static_cast<RC::Unreal::int64>(after));
            if (numeric->GetSignedIntPropertyValue(columnAddress)!=after)
                throw std::runtime_error(
                    "dynamic character customization column write did not persist");
        }
        return {inspect.Num(),before,after};
    }

    int MergeWhere(const ResolvedMember& member, const nlohmann::json& operation)
    {
        auto* arrayProperty=DragonWilds::PropertyHelper::CastProperty<FArrayProperty>(member.Property);
        auto* elementStruct=arrayProperty
            ? DragonWilds::PropertyHelper::CastProperty<FStructProperty>(arrayProperty->GetInner()) : nullptr;
        if (!arrayProperty || !elementStruct)
            throw std::runtime_error("$MergeWhere requires an array of reflected structs");
        if (!operation.contains("SelectorPath") || !operation.at("SelectorPath").is_string()
            || !operation.contains("SelectorValues") || !operation.at("SelectorValues").is_array()
            || operation.at("SelectorValues").empty() || !operation.contains("Value")
            || !operation.at("Value").is_object())
            throw std::runtime_error("$MergeWhere selector contract is invalid");

        const auto selectorPath=operation.at("SelectorPath").get<std::string>();
        auto* arrayAddress=arrayProperty->ContainerPtrToValuePtr<void>(member.Container);
        UECustom::FScriptArrayHelper managed(arrayAddress,arrayProperty);
        FScriptArrayHelper inspect(arrayProperty,arrayAddress);
        std::vector<void*> selectedElements;
        std::set<void*> uniqueElements;
        for (const auto& selectorValue : operation.at("SelectorValues"))
        {
            UECustom::FManagedValue expected;
            managed.InitializeValue(expected);
            const auto expectedMember=ResolveMember(expected.GetData(),nullptr,
                elementStruct->GetStruct().Get(),selectorPath);
            DragonWilds::PropertyHelper::CopyJsonValueToContainer(
                expectedMember.Container,expectedMember.Property,selectorValue);
            int matches=0;
            for (int32 index=0;index<inspect.Num();++index)
            {
                auto* element=inspect.GetRawPtr(index);
                if (!SameIdentity(expected.GetData(),element,
                    elementStruct->GetStruct().Get(),selectorPath)) continue;
                ++matches;
                if (!uniqueElements.insert(element).second)
                    throw std::runtime_error("$MergeWhere contains duplicate selectors for one array entry");
                selectedElements.push_back(element);
            }
            if (matches!=1)
                throw std::runtime_error("$MergeWhere selector must match exactly one array entry");
        }
        for (const auto& [name,value] : operation.at("Value").items())
        {
            auto* field=DragonWilds::PropertyHelper::GetPropertyByName(
                elementStruct->GetStruct().Get(),RC::to_generic_string(name));
            if (!field) throw std::runtime_error("$MergeWhere value member was not found: "+name);
            DragonWilds::PropertyHelper::ValidateJsonValueType(field,value);
        }
        for (auto* element : selectedElements)
            MergeStruct(element,elementStruct->GetStruct().Get(),operation.at("Value"));
        return static_cast<int>(selectedElements.size());
    }

    bool ReadRequiredString(const nlohmann::json& body, const char* name,
        std::string& out)
    {
        if (!body.contains(name) || !body.at(name).is_string()) return false;
        out = body.at(name).get<std::string>();
        return !out.empty();
    }

    using DragonWilds::IsCanonicalPersistenceId;

    std::string SanitizePackageSegment(std::string_view value,
        std::string_view fallback)
    {
        std::string result;
        result.reserve(value.size());
        bool previousUnderscore = false;
        for (const auto character : value)
        {
            const auto byte = static_cast<unsigned char>(character);
            const auto normalized = (std::isalnum(byte) || character == '_')
                ? character : '_';
            if (normalized == '_' && previousUnderscore) continue;
            result.push_back(normalized);
            previousUnderscore = normalized == '_';
        }
        while (!result.empty() && result.front() == '_') result.erase(result.begin());
        while (!result.empty() && result.back() == '_') result.pop_back();
        if (result.empty()) result.assign(fallback);
        return result;
    }

    bool IsUnlockableAssetField(std::string_view name)
    {
        return name == "RecipesToUnlock"
            || name == "BuildingPieceToUnlock";
    }

    void ValidateDominionSpheres(const nlohmann::json& definitions)
    {
        if (!definitions.is_object() || definitions.empty() || definitions.size() > 32)
            throw std::runtime_error("$DominionSpheres requires 1..32 named subobjects");
        for (const auto& [path, body] : definitions.items())
        {
            if (path.empty() || path.size() > 512)
                throw std::runtime_error("Dominion sphere subobject path length is invalid");
            bool segmentStart = true;
            for (const auto character : path)
            {
                const auto byte = static_cast<unsigned char>(character);
                if (character == '.')
                {
                    if (segmentStart) throw std::runtime_error("Dominion sphere subobject path has an empty segment");
                    segmentStart = true;
                }
                else
                {
                    if (!std::isalnum(byte) && character != '_')
                        throw std::runtime_error("Dominion sphere subobject path contains an invalid character");
                    segmentStart = false;
                }
            }
            if (segmentStart) throw std::runtime_error("Dominion sphere subobject path has an empty segment");
            if (!body.is_object() || body.size() != 1 || !body.contains("Radius")
                || !body.at("Radius").is_number())
                throw std::runtime_error("Dominion sphere entry requires only numeric Radius");
            const auto radius = body.at("Radius").get<double>();
            if (!std::isfinite(radius) || radius <= 0.0 || radius > 100000.0)
                throw std::runtime_error("Dominion sphere Radius must be greater than 0 and at most 100000 cm");
        }
    }

    void ClearItemIdentity(UObject* object, UClass* objectClass)
    {
        if (!object || !objectClass) return;
        for (const auto* name : { TEXT("PersistenceID"), TEXT("InternalName") })
        {
            auto* property = DragonWilds::PropertyHelper::CastProperty<FStrProperty>(
                DragonWilds::PropertyHelper::GetPropertyByName(objectClass, name));
            if (property)
                property->SetPropertyValue(
                    property->ContainerPtrToValuePtr<void>(object), FString{});
        }
    }

    bool MapContainsOther(FMapProperty* mapProperty, UObject* subsystem,
        const FString& key, UObject* item)
    {
        if (!mapProperty || !subsystem || key.GetCharArray().Num() <= 1)
            return false;
        bool collision = false;
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        map.ForEachPair([&](void* keyPointer, void* valuePointer) {
            if (collision || *static_cast<FString*>(keyPointer) != key) return;
            UObject* mapped = nullptr;
            std::memcpy(&mapped, valuePointer, sizeof(mapped));
            collision = mapped && mapped != item;
        });
        return collision;
    }

    bool AddMapEntry(FMapProperty* mapProperty, UObject* subsystem,
        const FString& key, UObject* item)
    {
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        UECustom::FManagedValue pair;
        map.InitializePair(pair);
        *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
        std::memcpy(map.GetValuePtr(pair.GetData()), &item, sizeof(item));
        map.Add(pair);
        auto* stored = map.FindValue(&key);
        UObject* mapped = nullptr;
        if (stored) std::memcpy(&mapped, stored, sizeof(mapped));
        return mapped == item;
    }

    int32_t EnsureItemNetworkIdentity(UObject* item, UObject* subsystem)
    {
        if (!item || !subsystem) return -1;
        auto* subsystemClass = subsystem->GetClassPrivate();
        auto* reverseProperty = CastField<FMapProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(subsystemClass, TEXT("DataToNetIdMap")));
        auto* arrayProperty = CastField<FArrayProperty>(
            DragonWilds::PropertyHelper::GetPropertyByName(subsystemClass, TEXT("NetIdToData")));
        if (!reverseProperty || !arrayProperty) return -1;

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        int32_t existingId = -1;
        reverse.ForEachPair([&](void* keyPtr, void* valuePtr) {
            UObject* existing = nullptr;
            std::memcpy(&existing, keyPtr, sizeof(existing));
            if (existing == item)
            {
                uint16 netId = 0;
                std::memcpy(&netId, valuePtr, sizeof(netId));
                existingId = static_cast<int32_t>(netId);
            }
        });
        if (existingId >= 0) return existingId;

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        if (!array || array->Num() < 0
            || array->Num() >= std::numeric_limits<uint16>::max())
            return -1;

        const auto netId = static_cast<uint16>(array->Num());
        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        UECustom::FManagedValue value;
        arrayHelper.InitializeValue(value);
        std::memcpy(value.GetData(), &item, sizeof(item));
        arrayHelper.Add(value);

        UECustom::FManagedValue reversePair;
        reverse.InitializePair(reversePair);
        std::memcpy(reverse.GetKeyPtr(reversePair.GetData()), &item, sizeof(item));
        std::memcpy(reverse.GetValuePtr(reversePair.GetData()), &netId, sizeof(netId));
        reverse.Add(reversePair);

        // Verify both directions before the item is exposed through string maps.
        bool reverseVerified = false;
        if (auto* stored = reverse.FindValue(&item))
        {
            uint16 existingId = 0;
            std::memcpy(&existingId, stored, sizeof(existingId));
            reverseVerified = existingId == netId;
        }
        FScriptArrayHelper inspect(arrayProperty, array);
        if (!reverseVerified || netId >= inspect.Num())
            return -1;
        UObject* arrayItem = nullptr;
        std::memcpy(&arrayItem, inspect.GetRawPtr(netId), sizeof(arrayItem));
        return arrayItem == item ? static_cast<int32_t>(netId) : -1;
    }
}

namespace DragonWilds {
    DragonWildsAssetModLoader::DragonWildsAssetModLoader() : DragonWildsModLoaderBase("assets")
    {
        SetDisplayName(TEXT("Asset Mod Loader"));
        AuthoringInstance=this;
    }

    DragonWildsAssetModLoader::~DragonWildsAssetModLoader()
    {
        if (m_characterMenuPatchHook != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_characterMenuPatchHook);
            m_characterMenuPatchHook = Hook::ERROR_ID;
        }
        if(AuthoringInstance==this)AuthoringInstance=nullptr;
        std::scoped_lock lock{m_mutex};
        m_pendingAssets.clear();
        m_pendingPatches.clear();
        m_pendingObjectPatches.clear();
        m_retainedObjectPatches.clear();
        m_createdAssetsByTarget.clear();
        PS::AssetAliases::Clear();
        m_createdAssets.clear();
        PS::AssetProvenance::Clear();
        PS::AssetMetadata::Clear();
    }

    void DragonWildsAssetModLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPathWithSource(loaderPath,
                [&](const nlohmann::json& data, const std::filesystem::path& relative) {
                QueueData(data, modName, relative.generic_string());
            });
        }
        else if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            TryApplyPending();
            ApplyPendingPatches();
            ApplyObjectPatches();
            ReportUnresolvedAssets();
        }
    }

    void DragonWildsAssetModLoader::OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath)
    {
        // A live authoring transaction has already applied this exact file.
        // Reapplying it could mutate an inventory item during a watcher callback.
        {
            std::scoped_lock lock{m_mutex};
            if(m_toolAssetFiles.contains(modFilePath.lexically_normal())) {
                PS::Log<LogLevel::Warning>(TEXT("Live-authored asset changes require restart: {}\n"),modFilePath.wstring());return;
            }
        }
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            QueueData(data, modName, modFilePath.filename().generic_string());
        });

        TryApplyPending();
        ApplyPendingPatches();
        ApplyObjectPatches();
        ReportUnresolvedAssets();
    }

    bool DragonWildsAssetModLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit;
    }

    bool DragonWildsAssetModLoader::OnInitialize()
    {
        if (!PS::Storefront::IsDedicatedServer())
            RegisterCharacterMenuPatchReplay();
        else
            PS::Log<LogLevel::Normal>(STR(
                "[SERVER][ASSET-CLONING] Runtime item cloning and live registry insertion remain enabled; character-menu patch replay is suppressed.\n"));
        m_dataAssetClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.DataAsset"), false);

        if (!m_dataAssetClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Engine.DataAsset.\n"), GetDisplayName());
            return false;
        }

        m_curveBaseClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.CurveBase"), false);

        if (!m_curveBaseClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Engine.CurveBase.\n"), GetDisplayName());
            return false;
        }

        m_itemDataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemData"), false);

        if (!m_itemDataClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Dominion.ItemData.\n"), GetDisplayName());
            return false;
        }

        m_recipeDataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.RecipeData"), false);
        if (!m_recipeDataClass)
        {
            PS::Log<LogLevel::Error>(STR("Unable to initialize {}, failed to find /Script/Dominion.RecipeData.\n"), GetDisplayName());
            return false;
        }

        return true;
    }

    void DragonWildsAssetModLoader::QueueData(const nlohmann::json& data,
        const RC::StringType& modName, const std::string& source)
    {
        if (!data.is_object())
        {
            PS::Log<LogLevel::Warning>(STR("[LOADER:assets][PARTIAL][MOD:{}][FILE:{}] Root must be an object; this file was skipped.\n"),
                modName,RC::to_generic_string(source));
            return;
        }

        try { if (QueueObjectPatch(data, modName, source)) return; }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(STR("[LOADER:assets][PARTIAL][MOD:{}][FILE:{}] Field edit skipped: {}. Other asset files continue.\n"),
                modName,RC::to_generic_string(source),PS::ToWideSafe(error.what()));
            return;
        }

        PS::AssetMetadata::Declaration documentMetadata;
        try { documentMetadata=PS::AssetMetadata::Read(data); }
        catch(const std::exception& e) {
            PS::Log<LogLevel::Error>(STR("Asset file metadata from {}: {}. File skipped.\n"),modName,PS::ToWideSafe(e.what()));
            return;
        }
        std::scoped_lock lock{m_mutex};
        for (auto& [target, properties] : data.items())
        {
            if (target.starts_with("$") || PS::AssetMetadata::IsKey(target))
            {
                continue;
            }

            if (!properties.is_object())
            {
                PS::Log<LogLevel::Error>(STR("Target '{}' must contain an object of properties. Skipping.\n"),
                    RC::to_generic_string(target));
                continue;
            }

            nlohmann::json normalizedProperties = properties;
            std::string effectiveTarget = target;
            bool isPatch = false;
            auto metadata=documentMetadata;
            try
            {
                metadata=PS::AssetMetadata::Merge(metadata,PS::AssetMetadata::Read(properties));
                static constexpr std::array<std::string_view, 2> protectedIdentity{
                    "PersistenceID", "InternalName"};
                if (const auto patch = JsonPatchDirective::Parse(
                        properties, protectedIdentity, "asset"))
                {
                    effectiveTarget = patch->Reference;
                    normalizedProperties = patch->Changes;
                    isPatch = true;
                    metadata=PS::AssetMetadata::Merge(metadata,PS::AssetMetadata::Read(normalizedProperties));
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Asset patch '{}': {}. Skipping.\n"),
                    RC::to_generic_string(target), PS::ToWideSafe(error.what()));
                continue;
            }

            normalizedProperties.erase("Modded");normalizedProperties.erase("RuneSchema");
            if (normalizedProperties.contains("$clone") || normalizedProperties.contains("$CloneFrom"))
            {
                PS::Log<LogLevel::Error>(STR("Target '{}': use the exact case-sensitive $Clone directive. Skipping.\n"),
                    RC::to_generic_string(target));
                continue;
            }
            if (normalizedProperties.contains("$Clone")
                && (!normalizedProperties.at("$Clone").is_string()
                    || normalizedProperties.at("$Clone").get_ref<const std::string&>().empty()))
            {
                PS::Log<LogLevel::Error>(STR("Target '{}': $Clone must contain a baked ItemData object path. Skipping.\n"),
                    RC::to_generic_string(target));
                continue;
            }
            try
            {
                (void)PS::WearableBridgeDirective::Requested(normalizedProperties);
                if(normalizedProperties.contains("$QuestScroll"))
                    (void)QuestScrolls::Parse(RC::to_string(modName),normalizedProperties.at("$QuestScroll"));
                if (normalizedProperties.contains("$DominionSpheres"))
                {
                    if (normalizedProperties.contains("$Clone"))
                        throw std::runtime_error("$DominionSpheres cannot be combined with $Clone");
                    ValidateDominionSpheres(normalizedProperties.at("$DominionSpheres"));
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Target '{}': {}. Skipping.\n"),
                    RC::to_generic_string(target), PS::ToWideSafe(error.what()));
                continue;
            }

            auto targetWide = RC::to_generic_string(effectiveTarget);
            auto pending = PendingAsset{
                targetWide,
                NormalizeObjectPath(targetWide),
                modName,
                std::move(normalizedProperties), isPatch,metadata,true
            };
            if (isPatch) WarnPatchConflicts(m_patchConflicts, "assets:" + RC::to_string(pending.ObjectPath),
                pending.Properties, RC::to_string(modName), false);
            (isPatch ? m_pendingPatches : m_pendingAssets).push_back(std::move(pending));
        }
    }

    bool DragonWildsAssetModLoader::QueueObjectPatch(const nlohmann::json& data,
        const RC::StringType& modName, const std::string& source)
    {
        // New DA_ files are identified by their target paths. Older schema
        // envelopes remain readable so installed mods do not break.
        const auto runtimeSchema = data.value("schema", std::string{});
        const auto schema = runtimeSchema.empty()
            ? data.value("$schema", std::string{}) : runtimeSchema;
        std::vector<PendingObjectPatch> pending;
        if (IsDirectDataAssetPatch(data) || schema == "RuneSchema.AssetPatch.v2")
        {
            for (const auto& [target, body] : data.items())
            {
                if (target.starts_with("$")) continue;
                if (!IsDataAssetTarget(target) || !body.is_object())
                    throw std::runtime_error("DA_ patches require full cooked DA_ paths with object bodies");
                const auto generatedClass = target.ends_with("_C");
                const auto mode = body.value("$Target",
                    generatedClass ? "ClassDefaultObject" : "Object");
                if (mode != "Object" && mode != "ClassDefaultObject")
                    throw std::runtime_error("$Target must be Object or ClassDefaultObject");
                const auto expected = body.value("$ExpectedClass",
                    mode == "ClassDefaultObject" ? target : std::string{});
                auto operations = DirectDataAssetOperations(body);
                for (auto& operation : operations)
                    if (operation.is_object() && operation.contains("Path")
                        && operation.at("Path").is_string()
                        && IsCharacterOptionPath(operation.at("Path").get_ref<const std::string&>())
                        && operation.contains("Value"))
                        NormalizeCharacterOptionValue(operation["Value"]);
                pending.push_back({NormalizeObjectPath(RC::to_generic_string(target)),
                    RC::to_generic_string(expected),modName,source,"da-fields",
                    mode == "ClassDefaultObject" ? PatchTargetMode::ClassDefaultObject : PatchTargetMode::Object,
                    std::move(operations)});
            }
        }
        else if (schema == RegistryPatch::Schema)
        {
            auto document = RegistryPatch::ParseDocument(data, RC::to_string(modName), source);
            for (const auto& patch : document.Patches)
            {
                if (patch.Op <= RegistryPatch::Operation::PatchExistingRow) continue;
                if (patch.TargetSpec.ObjectPath.empty())
                    throw std::runtime_error("/assets registry object patches require full objectPath targets");
                std::string operation;
                switch (patch.Op)
                {
                case RegistryPatch::Operation::Set: operation="Set"; break;
                case RegistryPatch::Operation::Merge: operation="Merge"; break;
                case RegistryPatch::Operation::Append: operation="Append"; break;
                case RegistryPatch::Operation::AppendUnique: operation="AppendUnique"; break;
                case RegistryPatch::Operation::UpsertOwned: operation="Merge"; break;
                default: throw std::runtime_error("unsupported /assets registry object operation");
                }
                auto property=patch.Property;
                auto value = patch.Value;
                nlohmann::json item;
                const auto selectorOpen=property.find('{');
                if (operation=="Merge" && selectorOpen!=std::string::npos && property.ends_with('}'))
                {
                    const auto selector=property.substr(selectorOpen+1,property.size()-selectorOpen-2);
                    const auto equals=selector.find('=');
                    if (equals==std::string::npos || equals==0 || equals+1>=selector.size())
                        throw std::runtime_error("legacy merge selector must use {Field=Value}");
                    property.resize(selectorOpen);
                    item={{"Op","MergeWhere"},{"Path",property},{"Value",value},
                        {"SelectorPath",selector.substr(0,equals)},
                        {"SelectorValues",nlohmann::json::array({selector.substr(equals+1)})}};
                }
                else item={{"Op",operation},{"Path",property},{"Value",value}};
                if (IsCharacterOptionPath(property)) NormalizeCharacterOptionValue(item["Value"]);
                if (IsCharacterColumnPath(property)) ValidateCharacterColumnValue(item["Value"]);
                if (patch.Identity.is_object() && patch.Identity.contains("property"))
                    item["IdentityPath"]=patch.Identity.at("property");
                if (patch.Template.is_object() && patch.Template.contains("fromIndex"))
                    item["TemplateIndex"]=patch.Template.at("fromIndex");
                pending.push_back({NormalizeObjectPath(RC::to_generic_string(patch.TargetSpec.ObjectPath)),
                    RC::to_generic_string(patch.TargetSpec.ExpectedClass),modName,source,patch.Id,
                    patch.TargetSpec.Kind == RegistryPatch::TargetKind::ClassDefaultObject
                        ? PatchTargetMode::ClassDefaultObject : PatchTargetMode::Object,
                    nlohmann::json::array({std::move(item)})});
            }
        }
        else return false;

        if (pending.empty())
            throw std::runtime_error("asset patch document contains no object operations");
        std::scoped_lock lock{m_mutex};
        m_pendingObjectPatches.insert(m_pendingObjectPatches.end(),
            std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
        return true;
    }

    void DragonWildsAssetModLoader::RegisterCharacterMenuPatchReplay()
    {
        if (m_characterMenuPatchHook != Hook::ERROR_ID) return;
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("CharacterMenuAssetPatchReplay");
        m_characterMenuPatchHook = Hook::RegisterProcessEventPreCallback(
            [this](Hook::TCallbackIterationData<void>&, UObject* source, UFunction* function, void*)
            {
                if (!source || !function || !source->GetClassPrivate() || m_replayingCharacterMenuPatches) return;
                const auto classPath = RC::to_string(source->GetClassPrivate()->GetPathName());
                if (classPath.find("WBP_CharacterOptionSelect_C") == std::string::npos) return;
                const auto functionPath = RC::to_string(function->GetPathName());
                if (!functionPath.ends_with(":Construct") && !functionPath.ends_with(":BP_OnOpen")) return;
                m_replayingCharacterMenuPatches = true;
                try { ApplyObjectPatches(true); }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Warning>(STR("[LOADER:assets][PARTIAL][CHARACTER-MENU] Replay skipped: {}. Other assets remain active.\n"),
                        PS::ToWideSafe(error.what()));
                }
                m_replayingCharacterMenuPatches = false;
            }, options);
        if (m_characterMenuPatchHook == Hook::ERROR_ID)
            PS::Log<LogLevel::Warning>(TEXT("[LOADER:assets][PARTIAL][CHARACTER-MENU] Pre-open replay is unavailable; other asset edits remain active.\n"));
        else
            PS::Log<LogLevel::Normal>(TEXT("[LOADER:assets][OK][CHARACTER-MENU] Pre-open replay enabled.\n"));
    }

    void DragonWildsAssetModLoader::ApplyObjectPatches(bool characterMenuReplay)
    {
        std::vector<PendingObjectPatch> pending;
        {
            std::scoped_lock lock{m_mutex};
            if (characterMenuReplay)
            {
                std::ranges::copy_if(m_retainedObjectPatches, std::back_inserter(pending), [](const auto& patch) {
                    return RC::to_string(patch.ObjectPath).find("DA_CharacterOptionData") != std::string::npos;
                });
            }
            else
            {
                pending.swap(m_pendingObjectPatches);
                m_retainedObjectPatches.insert(m_retainedObjectPatches.end(), pending.begin(), pending.end());
            }
        }
        std::ranges::sort(pending, [](const auto& left, const auto& right) {
            return std::tie(left.ModName,left.Source,left.PatchId,left.ObjectPath)
                < std::tie(right.ModName,right.Source,right.PatchId,right.ObjectPath);
        });
        struct VerificationSummary
        {
            RC::StringType ModName;
            RC::StringType Target;
            std::string Category;
            int Baseline = -1;
            int Final = -1;
            int Added = 0;
            int Existing = 0;
            int Verified = 0;
        };
        std::map<std::string,VerificationSummary> summaries;
        std::vector<PendingObjectPatch> committedClassDefaults;
        for (const auto& patch : pending) try
        {
            const auto pathText = RC::to_string(patch.ObjectPath);
            const auto slash = pathText.find_last_of('/');
            const auto dot = pathText.find('.', slash == std::string::npos ? 0 : slash);
            const auto assetName = pathText.substr(slash + 1,
                dot == std::string::npos ? std::string::npos : dot - slash - 1);
            if (!assetName.starts_with("DA_"))
                throw std::runtime_error("generic object patches are limited to explicit DA_ targets");

            auto* resolved = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                nullptr,nullptr,patch.ObjectPath.c_str(),false);
            if (!resolved)
            {
                UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(patch.ObjectPath)};
                resolved=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            if (!resolved) throw std::runtime_error("target could not be loaded");
            UObject* target = resolved;
            UClass* requestedClass = nullptr;
            if (patch.Mode == PatchTargetMode::ClassDefaultObject)
            {
                if (!resolved->IsA(UClass::StaticClass()))
                    throw std::runtime_error("ClassDefaultObject target did not resolve to UClass");
                requestedClass = static_cast<UClass*>(resolved);
                target = requestedClass->GetClassDefaultObject().Get();
                if (!target || !target->HasAnyFlags(RF_ClassDefaultObject))
                    throw std::runtime_error("class default object is unavailable or invalid");
            }
            else if (!IsSupportedTarget(target))
                throw std::runtime_error("object target is not a DataAsset, curve, or DataAsset subobject");
            if (!IsReadyForPatch(target)) throw std::runtime_error("target is not ready for reflected editing");

            if (!patch.ExpectedClass.empty())
            {
                auto* expectedObject = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                    nullptr,nullptr,patch.ExpectedClass.c_str(),false);
                if (!expectedObject)
                {
                    UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(patch.ExpectedClass)};
                    expectedObject=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
                }
                if (!expectedObject || !expectedObject->IsA(UClass::StaticClass()))
                    throw std::runtime_error("$ExpectedClass did not resolve to UClass");
                auto* expected=static_cast<UClass*>(expectedObject);
                if (!target->IsA(expected))
                    throw std::runtime_error("target does not match $ExpectedClass");
                if (requestedClass && requestedClass != expected && !requestedClass->IsChildOf(expected))
                    throw std::runtime_error("resolved generated class does not match $ExpectedClass");
            }

            struct Prepared { std::string Op; ResolvedMember Member; nlohmann::json Body; };
            std::vector<Prepared> prepared;
            for (const auto& operation : patch.Operations)
            {
                if (!operation.is_object() || !operation.contains("Op") || !operation.contains("Path")
                    || !operation.at("Op").is_string() || !operation.at("Path").is_string())
                    throw std::runtime_error("each operation requires string Op and Path");
                const auto op = operation.at("Op").get<std::string>();
                if (op!="Set" && op!="Merge" && op!="Append" && op!="AppendUnique" && op!="MergeWhere")
                    throw std::runtime_error("operation must be Set, Merge, Append, AppendUnique, or MergeWhere");
                auto member=ResolveMember(target,target->GetClassPrivate(),nullptr,
                    operation.at("Path").get<std::string>());
                if (IsCharacterColumnPath(member.CanonicalPath))
                {
                    ValidateCharacterColumnValue(operation.at("Value"));
                    auto* numeric=PropertyHelper::CastProperty<FNumericProperty>(member.Property);
                    if (!numeric || !numeric->IsInteger())
                        throw std::runtime_error("character customization NumberOfColumns is no longer an integer property");
                }
                if ((op=="Append"||op=="AppendUnique"||op=="MergeWhere")
                    && !PropertyHelper::CastProperty<FArrayProperty>(member.Property))
                    throw std::runtime_error("array operation path does not resolve to an array");
                if (op=="Set" && (PropertyHelper::CastProperty<FArrayProperty>(member.Property)
                    || PropertyHelper::CastProperty<FMapProperty>(member.Property)))
                    throw std::runtime_error("Set cannot replace complete arrays or maps; use Append or Merge");
                prepared.push_back({op,std::move(member),operation});
            }

            int writes=0;
            for (const auto& operation : prepared)
            {
                if (!operation.Body.contains("Value"))
                    throw std::runtime_error("operation requires Value");
                if (operation.Op=="Set")
                {
                    if (IsCharacterColumnPath(operation.Member.CanonicalPath))
                    {
                        auto* numeric=PropertyHelper::CastProperty<FNumericProperty>(
                            operation.Member.Property);
                        auto* address=numeric->ContainerPtrToValuePtr<void>(
                            operation.Member.Container);
                        const auto current=numeric->GetSignedIntPropertyValue(address);
                        const auto requested=operation.Body.at("Value").get<int64_t>();
                        numeric->SetIntPropertyValue(address,std::max(current,requested));
                    }
                    else PropertyHelper::CopyJsonValueToContainer(
                        operation.Member.Container,operation.Member.Property,
                        operation.Body.at("Value"));
                    ++writes;
                }
                else if (operation.Op=="Merge")
                {
                    if (auto* structure=PropertyHelper::CastProperty<FStructProperty>(operation.Member.Property))
                        MergeStruct(structure->ContainerPtrToValuePtr<void>(operation.Member.Container),
                            structure->GetStruct().Get(),operation.Body.at("Value"));
                    else if (PropertyHelper::CastProperty<FMapProperty>(operation.Member.Property))
                        PropertyHelper::CopyJsonValueToContainer(operation.Member.Container,
                            operation.Member.Property,operation.Body.at("Value"));
                    else throw std::runtime_error("Merge requires a struct or map property");
                    ++writes;
                }
                else if (operation.Op=="MergeWhere")
                {
                    writes += MergeWhere(operation.Member,operation.Body);
                }
                else
                {
                    const auto characterOption = IsCharacterOptionPath(
                        operation.Member.CanonicalPath);
                    if (operation.Op=="AppendUnique" && characterOption)
                        ValidateCharacterOptionHandle(operation.Body);
                    const auto outcome=AppendValues(operation.Member,operation.Body,
                        operation.Op=="AppendUnique");
                    writes += outcome.Added;
                    if (characterOption)
                    {
                        const auto layout=AdjustCharacterOptionColumns(
                            target,operation.Member.CanonicalPath);
                        if (layout.After!=layout.Before)
                            PS::Log<LogLevel::Normal>(STR(
                                "[CHARACTER-LAYOUT][DYNAMIC] target={} field={} options={} columns={}->{} maxRowsPerColumn={}.\n"),
                                target->GetPathName(),
                                RC::to_generic_string(operation.Member.CanonicalPath),
                                layout.Options,layout.Before,layout.After,
                                DragonWilds::CharacterCustomizationLayout::MaximumRowsPerColumn);
                    }
                    if (operation.Op=="AppendUnique")
                    {
                        PS::Log<LogLevel::Normal>(STR("[LOADER:assets][OK][MOD:{}] target={} field={} identity=DataHandle.RowName:{} count={}->{} added={} existing={}.\n"),
                            patch.ModName,target->GetPathName(),
                            RC::to_generic_string(operation.Member.CanonicalPath),RC::to_generic_string(outcome.Identity),
                            outcome.Before,outcome.After,outcome.Added,outcome.Existing);
                        const auto key=RC::to_string(patch.ModName)+"|"+RC::to_string(patch.ObjectPath)
                            +"|"+operation.Member.CanonicalPath;
                        auto& summary=summaries[key];
                        summary.ModName=patch.ModName;summary.Target=target->GetPathName();
                        const auto begin=operation.Member.CanonicalPath.find("::")+2;
                        const auto end=operation.Member.CanonicalPath.find(']',begin);
                        summary.Category=operation.Member.CanonicalPath.substr(begin,end-begin);
                        if(summary.Baseline<0)summary.Baseline=outcome.Before;
                        summary.Final=outcome.After;summary.Added+=outcome.Added;
                        summary.Existing+=outcome.Existing;++summary.Verified;
                    }
                }
            }
            if (patch.Mode==PatchTargetMode::ClassDefaultObject)
                committedClassDefaults.push_back(patch);
            if (prepared.empty() || std::ranges::none_of(prepared,[](const auto& operation){return operation.Op=="AppendUnique";}))
                PS::Log<LogLevel::Normal>(STR("[LOADER:assets][OK][MOD:{}][FILE:{}] target={} writes={}.\n"),
                    patch.ModName,RC::to_generic_string(patch.Source),patch.ObjectPath,writes);
        }
        catch(const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(STR("[LOADER:assets][PARTIAL][MOD:{}][FILE:{}][TARGET:{}] Field edit skipped: {}. Other targets continue.\n"),
                patch.ModName,RC::to_generic_string(patch.Source),patch.ObjectPath,PS::ToWideSafe(error.what()));
        }

        for (const auto& [unused,summary] : summaries)
            PS::Log<LogLevel::Normal>(STR("[CHARACTER-OPTIONS][VERIFIED][MOD:{}] target={} category={} baseline={} added={} existing={} final={} verified={} missing=0 duplicates=0.\n"),
                summary.ModName,summary.Target,RC::to_generic_string(summary.Category),summary.Baseline,summary.Added,summary.Existing,
                summary.Final,summary.Verified);

        std::set<std::string> propagatedTargets;
        for (const auto& seed : committedClassDefaults)
        {
            const auto path=RC::to_string(seed.ObjectPath);
            if (!propagatedTargets.insert(path).second) continue;
            auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,
                seed.ObjectPath.c_str(),false);
            if (!object || !object->IsA(UClass::StaticClass())) continue;
            auto* objectClass=static_cast<UClass*>(object);
            TArray<UObject*> instances;
            const auto invalid=static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject
                |RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed);
            UECustom::UObjectGlobals::GetObjectsOfClass(objectClass,instances,true,invalid);
            if (instances.Num()==0)
                PS::Log<LogLevel::Normal>(STR("[CHARACTER-OPTIONS][PROPAGATED] class={} instances=0; the updated class default will be used for new menus.\n"),
                    objectClass->GetPathName());
            for (auto* instance : instances) try
            {
                if (!instance || !IsReadyForPatch(instance)) continue;
                int before=-1,after=-1,added=0,existing=0,verified=0,columnWrites=0;
                for (const auto& patch : committedClassDefaults)
                {
                    if (patch.ObjectPath!=seed.ObjectPath) continue;
                    for (const auto& operation : patch.Operations)
                    {
                        const auto op=operation.value("Op",std::string{});
                        const auto authoredPath=operation.value("Path",std::string{});
                        if (op=="Set" && IsCharacterColumnPath(authoredPath))
                        {
                            ValidateCharacterColumnValue(operation.at("Value"));
                            const auto member=ResolveMember(instance,instance->GetClassPrivate(),nullptr,authoredPath);
                            auto* numeric=PropertyHelper::CastProperty<FNumericProperty>(member.Property);
                            if (!numeric || !numeric->IsInteger())
                                throw std::runtime_error("character customization NumberOfColumns is no longer an integer property");
                            auto* address=numeric->ContainerPtrToValuePtr<void>(member.Container);
                            const auto current=numeric->GetSignedIntPropertyValue(address);
                            const auto requested=operation.at("Value").get<int64_t>();
                            numeric->SetIntPropertyValue(address,std::max(current,requested));
                            ++columnWrites;
                            continue;
                        }
                        if (op!="AppendUnique") continue;
                        const auto member=ResolveMember(instance,instance->GetClassPrivate(),nullptr,
                            authoredPath);
                        if (IsCharacterOptionPath(member.CanonicalPath))
                            ValidateCharacterOptionHandle(operation);
                        const auto outcome=AppendValues(member,operation,true);
                        const auto layout=AdjustCharacterOptionColumns(
                            instance,member.CanonicalPath);
                        if(layout.After!=layout.Before)++columnWrites;
                        if(before<0)before=outcome.Before;after=outcome.After;
                        added+=outcome.Added;existing+=outcome.Existing;++verified;
                    }
                }
                if (verified)
                    PS::Log<LogLevel::Normal>(STR("[CHARACTER-OPTIONS][PROPAGATED] object={} class={} count={}->{} added={} existing={} verified={} missing=0.\n"),
                        instance->GetPathName(),instance->GetClassPrivate()->GetPathName(),before,after,
                        added,existing,verified);
                if (columnWrites)
                    PS::Log<LogLevel::Normal>(STR("[CHARACTER-LAYOUT][PROPAGATED] object={} class={} columnWrites={}.\n"),
                        instance->GetPathName(),instance->GetClassPrivate()->GetPathName(),columnWrites);
            }
            catch(const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(STR("[LOADER:assets][PARTIAL][CHARACTER-MENU] Existing menu object={} was not updated: {}. New menus still use the class default.\n"),
                    instance?instance->GetPathName():STR("<null>"),PS::ToWideSafe(error.what()));
            }
        }
    }

    void DragonWildsAssetModLoader::ApplyPendingPatches()
    {
        {
            std::scoped_lock lock{m_mutex};
            if (m_pendingPatches.empty()) return;
            PS::Log<LogLevel::Verbose>(STR("Applying {} deferred asset $Patch document(s).\n"),
                m_pendingPatches.size());
            m_pendingAssets.insert(m_pendingAssets.end(),
                std::make_move_iterator(m_pendingPatches.begin()),
                std::make_move_iterator(m_pendingPatches.end()));
            m_pendingPatches.clear();
        }
        TryApplyPending();
    }

    void DragonWildsAssetModLoader::Apply(UObject* object, const PendingAsset& pendingAsset, LoadResult& outResult)
    {
        if (!object)
        {
            outResult.ErrorCount++;
            return;
        }

        auto* objectClass = object->GetClassPrivate();
        if (!objectClass)
        {
            outResult.ErrorCount++;
            return;
        }

        const auto errorsBefore = outResult.ErrorCount;
        const auto writesBefore = outResult.PropertiesWritten;
        bool wearableRefresh=false;
        try {
            wearableRefresh=PS::WearableBridgeDirective::Requested(pendingAsset.Properties);
            if(wearableRefresh&&!EquipmentWearableBridge::IsWearable(object))throw std::runtime_error("$WearableBridge requires WearableEquipmentData, not a generic item or mesh asset");
        }catch(const std::exception& error){++outResult.ErrorCount;PS::Log<LogLevel::Error>(STR("Asset wearable directive: {}\n"),PS::ToWideSafe(error.what()));return;}
        if (HasModuleImportDirectives(pendingAsset.Properties)
            && !ApplyModuleImports(object, pendingAsset, outResult))
            return;
        for (auto& [propertyName, propertyValue] : pendingAsset.Properties.items())
        {
            if (propertyName == "$Append" || propertyName == "$Clone" || propertyName == "$InheritEquipmentStats" || propertyName == "$WearableBridge" || propertyName == "$QuestScroll" || IsModuleImportDirective(propertyName) || PS::AssetMetadata::IsKey(propertyName))
            {
                continue;
            }

            if (propertyName == "$DominionSpheres")
            {
                ApplyDominionSpheres(object, propertyValue, outResult);
                continue;
            }

            if(propertyName == "$VisualEffect") {
                try {
                    const auto sourceHint = pendingAsset.Properties.contains("$Clone")
                        ? pendingAsset.Properties.at("$Clone").get<std::string>()
                        : RC::to_string(object->GetPathName());
                    PlayerGhost::SetItemEffect(object,propertyValue,sourceHint);
                    ++outResult.PropertiesWritten;
                }
                catch(const std::exception& error) {
                    ++outResult.ErrorCount;
                    PS::Log<LogLevel::Error>(TEXT("[{}] Invalid equipment visual: {}\n"),object->GetName(),PS::ToWideSafe(error.what()));
                }
                continue;
            }
            auto propertyNameWide = RC::to_generic_string(propertyName);
            auto* property = PropertyHelper::GetPropertyByName(objectClass, propertyNameWide);
            if (!property)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("[{}] Property '{}' was not found.\n"),
                    object->GetName(), propertyNameWide);
                continue;
            }

            try
            {
                auto routed=propertyValue;
                if(propertyName=="RecipesToUnlock") {
                    if(!m_recipeService)throw std::runtime_error("recipe reference service is unavailable");
                    if(!routed.is_array())throw std::runtime_error("RecipesToUnlock must be an array");
                    for(auto& reference:routed) {
                        if(!reference.is_string())throw std::runtime_error("RecipesToUnlock entries must be strings");
                        auto* recipe=m_recipeService->ResolveReference(pendingAsset.ModName,reference.get<std::string>());
                        if(!recipe)throw std::runtime_error("RecipesToUnlock target is unavailable or ambiguous: "+reference.get<std::string>());
                        reference=RC::to_string(recipe->GetPathName());
                    }
                }
                PropertyHelper::CopyJsonValueToContainer(object, property, routed);
                outResult.PropertiesWritten++;
                if (IsUnlockableAssetField(propertyName))
                    PS::Log<LogLevel::Verbose>(STR("[{}] Applied unlockable asset field '{}'.\n"),
                        object->GetName(), propertyNameWide);
            }
            catch (const std::exception& e)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("[{}] Failed writing '{}': {}\n"),
                    object->GetName(), propertyNameWide, PS::ToWideSafe(e.what()));
            }
        }

        if (pendingAsset.Properties.contains("$Append"))
        {
            AppendProperties(object, objectClass, pendingAsset.Properties.at("$Append"), outResult);
        }
        // Metadata is recorded only against an object actually handled by this loader.
        // A native field-write failure cannot grant new clone permissions.
        if(outResult.ErrorCount==errorsBefore) {
            if(wearableRefresh&&!pendingAsset.Properties.contains("$Clone"))
                (void)EquipmentWearableBridge::RefreshRegistered(object);
            try {PS::AssetMetadata::Record(object,pendingAsset.Metadata,RC::to_string(pendingAsset.ModName),pendingAsset.InstalledDefinition);}
            catch(const std::exception& e){++outResult.ErrorCount;PS::Log<LogLevel::Error>(STR("Asset metadata: {}\n"),PS::ToWideSafe(e.what()));}
            if(outResult.ErrorCount==errorsBefore && pendingAsset.Properties.contains("$QuestScroll"))try {
                const auto scroll=QuestScrolls::Parse(RC::to_string(pendingAsset.ModName),pendingAsset.Properties.at("$QuestScroll"));
                auto* consumable=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ConsumableItemData"));
                auto* identity=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(objectClass,TEXT("PersistenceID")));
                if(!consumable||!object->IsA(consumable)||!identity||identity->GetArrayDim()!=1
                    ||RC::to_string(RC::StringType(*identity->GetPropertyValue(identity->ContainerPtrToValuePtr<void>(object))))!=scroll.PersistenceId)
                    throw std::runtime_error("$QuestScroll requires ConsumableItemData with the exact declared stable item PersistenceID");
                (void)QuestScrolls::RequireUseEvent(object);
                QuestScrolls::Bind(RC::to_string(object->GetPathName()),scroll);
            }catch(const std::exception& e){++outResult.ErrorCount;PS::Log<LogLevel::Error>(STR("Quest scroll: {}\n"),PS::ToWideSafe(e.what()));}
        }
        if (outResult.ErrorCount > errorsBefore) {
            PS::Log<LogLevel::Error>(STR("[{}] Asset edit incomplete: {} successful field writes, {} errors. Changes are not rolled back; inspect field errors and restart after correcting the mod.\n"),
                object->GetPathName(), outResult.PropertiesWritten - writesBefore,
                outResult.ErrorCount - errorsBefore);
        }
    }

    bool DragonWildsAssetModLoader::ApplyModuleImports(UObject* object,
        const PendingAsset& pendingAsset, LoadResult& outResult)
    {
        ModuleImportSettings settings;
        try { settings = ParseModuleImportSettings(pendingAsset.Properties); }
        catch (const std::exception& error)
        {
            ++outResult.ErrorCount;
            PS::Log<LogLevel::Error>(STR("[{}] Module import rejected: {}.\n"),
                object->GetName(), PS::ToWideSafe(error.what()));
            return false;
        }
        if (settings.Modules.empty()) return true;
        if (settings.Policy == ImportPolicy::Explicit && !settings.Remap.empty())
            PS::Log<LogLevel::Warning>(STR("[{}] $ImpRemap is ignored when $ImpPolicy is Explicit.\n"), object->GetName());
        ResolvedMember modulesMember;
        FArrayProperty* arrayProperty = nullptr;
        FStructProperty* entryProperty = nullptr;
        FObjectProperty* moduleProperty = nullptr;
        void* arrayAddress = nullptr;
        try
        {
            modulesMember = ResolveMember(object, object->GetClassPrivate(), nullptr, "Modules.Modules");
            arrayProperty = PropertyHelper::CastProperty<FArrayProperty>(modulesMember.Property);
            entryProperty = arrayProperty
                ? PropertyHelper::CastProperty<FStructProperty>(arrayProperty->GetInner()) : nullptr;
            moduleProperty = entryProperty
                ? CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(
                    entryProperty->GetStruct().Get(), TEXT("Module"))) : nullptr;
            if (!arrayProperty || !entryProperty || !moduleProperty)
                throw std::runtime_error("target does not expose a compatible Modules.Modules array");
            if (moduleProperty->GetElementSize() != sizeof(UObject*))
                throw std::runtime_error("Modules.Modules.Module has an unexpected object-reference layout");
            arrayAddress = arrayProperty->ContainerPtrToValuePtr<void>(modulesMember.Container);
        }
        catch (const std::exception& error)
        {
            ++outResult.ErrorCount;
            PS::Log<LogLevel::Error>(STR("[{}] Module import target rejected: {}.\n"),
                object->GetName(), PS::ToWideSafe(error.what()));
            return false;
        }

        const auto resolveImport = [&](const std::string& authored) -> UObject* {
            const auto normalizeUnrealPath = [](std::string path) {
                std::replace(path.begin(), path.end(), '\\', '/');
                if (path.rfind("Game/", 0) == 0)
                    path.insert(path.begin(), '/');
                return path;
            };

            const auto fullPathText = normalizeUnrealPath(authored);
            const auto fullPath = RC::to_generic_string(fullPathText);
            auto* resolved = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                nullptr, nullptr, fullPath.c_str(), false);
            if (resolved) return resolved;

            UECustom::TSoftObjectPtr<UObject> fullSoft{UECustom::FSoftObjectPath(fullPath)};
            resolved = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(fullSoft);
            if (resolved) return resolved;

            const auto colon = fullPathText.find(':');
            const auto assetText = colon == std::string::npos ? fullPathText : fullPathText.substr(0, colon);
            const auto assetPath = RC::to_generic_string(assetText);
            if (assetPath.empty()) return nullptr;

            auto* parent = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                nullptr, nullptr, assetPath.c_str(), false);
            if (!parent)
            {
                UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(assetPath)};
                parent = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }

            resolved = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                nullptr, nullptr, fullPath.c_str(), false);
            if (resolved || !parent || colon == std::string::npos) return resolved;

            const auto childName = fullPathText.substr(colon + 1);
            UObjectGlobals::ForEachUObject([&](UObject* candidate, int32_t, int32_t) -> LoopAction {
                if (candidate && candidate->GetOuterPrivate() == parent
                    && RC::to_string(candidate->GetName()) == childName
                    && !candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
                {
                    resolved = candidate;
                    return LoopAction::Break;
                }
                return LoopAction::Continue;
            });
            if (resolved) return resolved;

            try
            {
                auto sourceModules = ResolveMember(parent, parent->GetClassPrivate(), nullptr, "Modules.Modules");
                auto* sourceArrayProperty = PropertyHelper::CastProperty<FArrayProperty>(sourceModules.Property);
                auto* sourceEntryProperty = sourceArrayProperty
                    ? PropertyHelper::CastProperty<FStructProperty>(sourceArrayProperty->GetInner()) : nullptr;
                auto* sourceModuleProperty = sourceEntryProperty
                    ? CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(
                        sourceEntryProperty->GetStruct().Get(), TEXT("Module"))) : nullptr;
                if (!sourceArrayProperty || !sourceEntryProperty || !sourceModuleProperty
                    || sourceModuleProperty->GetElementSize() != sizeof(UObject*))
                    return nullptr;

                FScriptArrayHelper sourceArray(sourceArrayProperty,
                    sourceArrayProperty->ContainerPtrToValuePtr<void>(sourceModules.Container));
                for (int32 index = 0; index < sourceArray.Num(); ++index)
                {
                    auto* entry = sourceArray.GetRawPtr(index);
                    UObject* candidate = nullptr;
                    std::memcpy(&candidate, sourceModuleProperty->ContainerPtrToValuePtr<void>(entry), sizeof(candidate));
                    if (candidate && RC::to_string(candidate->GetName()) == childName
                        && !candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
                        return candidate;
                }
            }
            catch (...)
            {
            }
            return nullptr;
        };
        const auto moduleName = [](UObject* module, FName& out) -> bool {
            if (!module || !module->GetClassPrivate()) return false;
            auto* property = CastField<FNameProperty>(PropertyHelper::GetPropertyByName(
                module->GetClassPrivate(), TEXT("ModuleName")));
            if (!property || property->GetArrayDim() != 1 || property->GetElementSize() != sizeof(FName))
                return false;
            out = property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(module));
            return out != FName{};
        };

        struct DependencyRef
        {
            FName Name{};
            std::string Path;
            bool Optional = false;
        };
        const auto scanDependencies = [](UObject* module) {
            std::vector<DependencyRef> found;
            if (!module || !module->GetClassPrivate()) return found;
            const FName moduleNameField(TEXT("ModuleName"), FNAME_Add);
            std::size_t visitedFields = 0;
            const auto walk = [&](auto&& self, void* container, FProperty* first,
                const std::string& prefix, bool inheritedOptional, int depth) -> void {
                if (!container || !first || depth > 8) return;
                for (auto* property = first; property; property = property->GetPropertyLinkNext())
                {
                    if (++visitedFields > 1024)
                        throw std::runtime_error("dependency scan exceeded 1024 reflected fields");
                    const auto propertyName = RC::to_string(property->GetName());
                    const auto path = prefix.empty() ? propertyName : prefix + "." + propertyName;
                    const bool optional = inheritedOptional || propertyName.starts_with("Optional");
                    if (property->GetFName() == moduleNameField)
                    {
                        if (depth == 0) continue;
                        auto* nameProperty = CastField<FNameProperty>(property);
                        if (!nameProperty || nameProperty->GetArrayDim() != 1
                            || nameProperty->GetElementSize() != sizeof(FName))
                            throw std::runtime_error("dependency ModuleName field has an unexpected layout at " + path);
                        const auto value = nameProperty->GetPropertyValue(
                            nameProperty->ContainerPtrToValuePtr<void>(container));
                        if (value != FName{}) found.push_back({value, path, optional});
                        continue;
                    }
                    if (auto* structProperty = PropertyHelper::CastProperty<FStructProperty>(property))
                    {
                        auto* nested = structProperty->ContainerPtrToValuePtr<void>(container);
                        auto* type = structProperty->GetStruct().Get();
                        if (nested && type)
                            self(self, nested, type->GetPropertyLink(), path, optional, depth + 1);
                    }
                    else if (auto* arrayProperty = PropertyHelper::CastProperty<FArrayProperty>(property))
                    {
                        auto* innerStruct = PropertyHelper::CastProperty<FStructProperty>(arrayProperty->GetInner());
                        auto* type = innerStruct ? innerStruct->GetStruct().Get() : nullptr;
                        if (!type) continue;
                        FScriptArrayHelper array(arrayProperty, arrayProperty->ContainerPtrToValuePtr<void>(container));
                        if (array.Num() > 256)
                            throw std::runtime_error("dependency scan refused a struct array larger than 256 entries at " + path);
                        for (int32 index = 0; index < array.Num(); ++index)
                            self(self, array.GetRawPtr(index), type->GetPropertyLink(),
                                path + "[" + std::to_string(index) + "]", optional, depth + 1);
                    }
                }
            };
            walk(walk, module, module->GetClassPrivate()->GetPropertyLink(), "", false, 0);
            return found;
        };

        auto* expectedModuleClass = moduleProperty->GetPropertyClass().Get();

        enum class ExternalReferenceKind { Asset, BehavioralClass, Object, OwnedSubobject, SoftReference, Structural };
        enum class ExternalReferenceProvenance { Shared, Source, Target, Owned };
        enum class ExternalDependencyDisposition { Available, Candidate, Owned, Contextual, Unresolved };
        struct ExternalReference
        {
            std::string Module;
            std::string Path;
            std::string Target;
            std::string Class;
            std::string ClassPath;
            ExternalReferenceKind Kind = ExternalReferenceKind::Object;
            ExternalReferenceProvenance Provenance = ExternalReferenceProvenance::Shared;
            bool Soft = false;
        };
        const auto scanExternalReferences = [&](UObject* module) {
            std::vector<ExternalReference> found;
            if (!module || !module->GetClassPrivate()) return found;
            std::size_t visitedFields = 0;
            const auto modulePath = RC::to_string(module->GetPathName());
            const auto assetPathOf = [](std::string path) {
                if (const auto colon = path.find(':'); colon != std::string::npos) path.resize(colon);
                return path;
            };
            const auto directoryOf = [](const std::string& path) {
                const auto slash = path.rfind('/');
                return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
            };
            const auto sourceDirectory = directoryOf(assetPathOf(modulePath));
            const auto targetDirectory = directoryOf(assetPathOf(RC::to_string(object->GetPathName())));
            const auto provenanceOf = [&](const std::string& targetPath, bool owned) {
                if (owned) return ExternalReferenceProvenance::Owned;
                const auto assetPath = assetPathOf(targetPath);
                if (!sourceDirectory.empty() && assetPath.starts_with(sourceDirectory))
                    return ExternalReferenceProvenance::Source;
                if (!targetDirectory.empty() && assetPath.starts_with(targetDirectory))
                    return ExternalReferenceProvenance::Target;
                return ExternalReferenceProvenance::Shared;
            };
            const auto isStructuralPath = [](const std::string& path) {
                return path == "ModuleType" || path.ends_with(".ModuleType")
                    || path.ends_with(".PropertyClass") || path.ends_with(".MetaClass");
            };
            const auto isOwnedByModule = [&](UObject* target) {
                for (auto* outer = target ? target->GetOuterPrivate() : nullptr; outer; outer = outer->GetOuterPrivate())
                {
                    if (outer == module) return true;
                }
                return false;
            };
            const auto classifyHard = [&](const std::string& path, UObject* target) {
                if (isStructuralPath(path)) return ExternalReferenceKind::Structural;
                if (isOwnedByModule(target)) return ExternalReferenceKind::OwnedSubobject;
                if (target && target->IsA<UClass>()) return ExternalReferenceKind::BehavioralClass;
                const auto targetPath = target ? RC::to_string(target->GetPathName()) : std::string{};
                if (targetPath.starts_with("/Game/") && targetPath.find(':') == std::string::npos)
                    return ExternalReferenceKind::Asset;
                return ExternalReferenceKind::Object;
            };
            const auto addHardReference = [&](const std::string& path, UObject* target) {
                if (!target || target == module) return;
                if (expectedModuleClass && target->IsA(expectedModuleClass)) return;
                const auto targetPath = RC::to_string(target->GetPathName());
                if (targetPath.empty()) return;
                const auto* targetClass = target->GetClassPrivate();
                const auto kind = classifyHard(path, target);
                found.push_back({modulePath, path, targetPath,
                    targetClass ? RC::to_string(targetClass->GetName()) : std::string{},
                    targetClass ? RC::to_string(targetClass->GetPathName()) : std::string{},
                    kind, provenanceOf(targetPath, kind == ExternalReferenceKind::OwnedSubobject), false});
            };
            const auto addSoftReference = [&](const std::string& path, FProperty* property, void* address) {
                if (!property || !address || property->GetElementSize() != sizeof(UECustom::FSoftObjectPtr)) return;
                const auto& objectId = static_cast<UECustom::FSoftObjectPtr*>(address)->ObjectID;
                const auto package = RC::to_string(objectId.GetLongPackageFName().ToString());
                const auto asset = RC::to_string(objectId.GetAssetFName().ToString());
                if (package.empty() || package == "None" || asset.empty() || asset == "None") return;
                std::string target = package + "." + asset;
                const auto& chars = objectId.SubPathString.GetCharArray();
                if (chars.Num() > 1 && chars.GetData())
                {
                    const auto sub = RC::to_string(RC::StringType(chars.GetData(), chars.Num() - 1));
                    if (!sub.empty()) target += ":" + sub;
                }
                const bool isClass = CastField<FSoftClassProperty>(property) != nullptr;
                const auto kind = isStructuralPath(path) ? ExternalReferenceKind::Structural
                                                          : ExternalReferenceKind::SoftReference;
                found.push_back({modulePath, path, target,
                    isClass ? "SoftClass" : "SoftObject", std::string{},
                    kind, provenanceOf(target, false), true});
            };
            const auto walk = [&](auto&& self, void* container, FProperty* first,
                const std::string& prefix, int depth) -> void {
                if (!container || !first || depth > 8) return;
                for (auto* property = first; property; property = property->GetPropertyLinkNext())
                {
                    if (++visitedFields > 2048)
                        throw std::runtime_error("external reference scan exceeded 2048 reflected fields");
                    const auto propertyName = RC::to_string(property->GetName());
                    const auto path = prefix.empty() ? propertyName : prefix + "." + propertyName;
                    auto* address = property->ContainerPtrToValuePtr<void>(container);
                    if (CastField<FSoftObjectProperty>(property) || CastField<FSoftClassProperty>(property))
                    {
                        addSoftReference(path, property, address);
                        continue;
                    }
                    if (auto* objectProperty = CastField<FObjectPropertyBase>(property))
                    {
                        addHardReference(path, objectProperty->GetObjectPropertyValue(address));
                        continue;
                    }
                    if (auto* structProperty = PropertyHelper::CastProperty<FStructProperty>(property))
                    {
                        auto* nested = structProperty->ContainerPtrToValuePtr<void>(container);
                        auto* type = structProperty->GetStruct().Get();
                        if (nested && type)
                            self(self, nested, type->GetPropertyLink(), path, depth + 1);
                        continue;
                    }
                    if (auto* arrayProperty = PropertyHelper::CastProperty<FArrayProperty>(property))
                    {
                        FScriptArrayHelper array(arrayProperty, arrayProperty->ContainerPtrToValuePtr<void>(container));
                        if (array.Num() > 512)
                            throw std::runtime_error("external reference scan refused an array larger than 512 entries at " + path);
                        auto* inner = arrayProperty->GetInner();
                        if (CastField<FSoftObjectProperty>(inner) || CastField<FSoftClassProperty>(inner))
                        {
                            for (int32 index = 0; index < array.Num(); ++index)
                                addSoftReference(path + "[" + std::to_string(index) + "]", inner, array.GetRawPtr(index));
                            continue;
                        }
                        if (auto* innerObject = CastField<FObjectPropertyBase>(inner))
                        {
                            for (int32 index = 0; index < array.Num(); ++index)
                                addHardReference(path + "[" + std::to_string(index) + "]",
                                    innerObject->GetObjectPropertyValue(array.GetRawPtr(index)));
                            continue;
                        }
                        auto* innerStruct = PropertyHelper::CastProperty<FStructProperty>(inner);
                        auto* type = innerStruct ? innerStruct->GetStruct().Get() : nullptr;
                        if (!type) continue;
                        for (int32 index = 0; index < array.Num(); ++index)
                            self(self, array.GetRawPtr(index), type->GetPropertyLink(),
                                path + "[" + std::to_string(index) + "]", depth + 1);
                    }
                }
            };
            walk(walk, module, module->GetClassPrivate()->GetPropertyLink(), "", 0);
            std::ranges::sort(found, [](const ExternalReference& left, const ExternalReference& right) {
                if (left.Kind != right.Kind) return left.Kind < right.Kind;
                if (left.Module != right.Module) return left.Module < right.Module;
                if (left.Path != right.Path) return left.Path < right.Path;
                return left.Target < right.Target;
            });
            found.erase(std::unique(found.begin(), found.end(), [](const ExternalReference& left, const ExternalReference& right) {
                return left.Module == right.Module && left.Path == right.Path && left.Target == right.Target && left.Kind == right.Kind;
            }), found.end());
            return found;
        };

        struct ImportRequest
        {
            std::string Source;
            UObject* Module = nullptr;
            bool Requested = true;
            bool Forced = false;
        };
        struct DependencyTrace
        {
            std::string Parent;
            std::string Path;
            std::string Name;
            std::string Resolved;
            bool Optional = false;
            bool Remapped = false;
        };

        const auto remapTargets = [&](UObject* parent, const std::string& dependency) {
            std::vector<std::string> targets;
            if (settings.Remap.empty() || !parent) return targets;
            const auto read = [&](const nlohmann::json& value) {
                if (value.is_string()) targets.push_back(value.get<std::string>());
                else if (value.is_array())
                    for (const auto& item : value) targets.push_back(item.get<std::string>());
            };
            const auto full = RC::to_string(parent->GetPathName());
            const auto shortName = RC::to_string(parent->GetName());
            if (const auto scoped = settings.Remap.find(full); scoped != settings.Remap.end() && scoped->is_object())
                if (const auto mapped = scoped->find(dependency); mapped != scoped->end()) read(*mapped);
            if (targets.empty())
                if (const auto scoped = settings.Remap.find(shortName); scoped != settings.Remap.end() && scoped->is_object())
                    if (const auto mapped = scoped->find(dependency); mapped != scoped->end()) read(*mapped);
            if (targets.empty())
                if (const auto mapped = settings.Remap.find(dependency); mapped != settings.Remap.end() && !mapped->is_object())
                    read(*mapped);
            return targets;
        };

        std::vector<ImportRequest> requests;
        std::vector<DependencyTrace> dependencyTrace;
        int dependencyErrors = 0;
        if (settings.Policy == ImportPolicy::Explicit)
        {
            requests.reserve(settings.Modules.size());
            for (const auto& source : settings.Modules)
                requests.push_back({source, resolveImport(source), true, false});
        }
        else
        {
            std::map<UObject*, std::size_t> emitted;
            auto* expectedClass = moduleProperty->GetPropertyClass().Get();
            const auto sourceCandidates = [&](UObject* parent, const FName& wanted) {
                std::vector<UObject*> matches;
                if (!parent || !parent->GetOuterPrivate())
                    throw std::runtime_error("dependency source module has no owning asset");
                UObjectGlobals::ForEachUObject([&](UObject* candidate, int32_t, int32_t) -> LoopAction {
                    if (!candidate || candidate == parent || candidate->GetOuterPrivate() != parent->GetOuterPrivate()
                        || (expectedClass && !candidate->IsA(expectedClass))
                        || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
                        return LoopAction::Continue;
                    FName candidateName{};
                    if (moduleName(candidate, candidateName) && candidateName == wanted)
                        matches.push_back(candidate);
                    return LoopAction::Continue;
                });
                return matches;
            };
            const auto resolveRemapTarget = [&](UObject* parent, const std::string& authored) -> UObject* {
                if (authored.find('/') != std::string::npos || authored.find(':') != std::string::npos)
                {
                    auto* resolved = resolveImport(authored);
                    if (!resolved) throw std::runtime_error("$ImpRemap target could not be resolved: " + authored);
                    if (expectedClass && !resolved->IsA(expectedClass))
                        throw std::runtime_error("$ImpRemap target is not a compatible module: " + authored);
                    return resolved;
                }
                if (!parent || !parent->GetOuterPrivate())
                    throw std::runtime_error("$ImpRemap shorthand requires a source asset");
                std::vector<UObject*> matches;
                UObjectGlobals::ForEachUObject([&](UObject* candidate, int32_t, int32_t) -> LoopAction {
                    if (!candidate || candidate->GetOuterPrivate() != parent->GetOuterPrivate()
                        || (expectedClass && !candidate->IsA(expectedClass))
                        || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
                        return LoopAction::Continue;
                    const auto objectName = RC::to_string(candidate->GetName());
                    FName candidateModuleName{};
                    const bool moduleMatch = moduleName(candidate, candidateModuleName)
                        && RC::to_string(candidateModuleName.ToString()) == authored;
                    if (objectName == authored || moduleMatch) matches.push_back(candidate);
                    return LoopAction::Continue;
                });
                if (matches.empty()) throw std::runtime_error("$ImpRemap target was not found in the source asset: " + authored);
                if (matches.size() != 1)
                    throw std::runtime_error("$ImpRemap target is ambiguous; use an exact object path: " + authored);
                return matches.front();
            };
            const auto resolveDependency = [&](UObject* parent, const FName& wanted,
                const std::string& dependencyPath) {
                std::vector<UObject*> resolved;
                const auto wantedText = RC::to_string(wanted.ToString());
                const auto mapped = remapTargets(parent, wantedText);
                if (!mapped.empty())
                {
                    bool satisfiesReference = false;
                    std::set<UObject*> unique;
                    for (const auto& authored : mapped)
                    {
                        auto* target = resolveRemapTarget(parent, authored);
                        if (!unique.insert(target).second) continue;
                        resolved.push_back(target);
                        FName targetName{};
                        if (moduleName(target, targetName) && targetName == wanted)
                            satisfiesReference = true;
                    }
                    if (!satisfiesReference)
                        throw std::runtime_error("$ImpRemap for '" + wantedText
                            + "' must include a module whose ModuleName still satisfies that reference at " + dependencyPath);
                    return resolved;
                }

                auto matches = sourceCandidates(parent, wanted);
                if (matches.empty())
                    throw std::runtime_error("dependency '" + wantedText
                        + "' was not found in the source asset at " + dependencyPath);
                if (matches.size() != 1)
                {
                    std::string candidates;
                    for (auto* candidate : matches)
                    {
                        if (!candidates.empty()) candidates += ", ";
                        candidates += RC::to_string(candidate->GetName());
                    }
                    throw std::runtime_error("dependency '" + wantedText
                        + "' is ambiguous at " + dependencyPath + "; candidates: " + candidates
                        + "; select one with $ImpRemap");
                }
                resolved.push_back(matches.front());
                return resolved;
            };

            for (const auto& authoredRoot : settings.Modules)
            {
                auto* root = resolveImport(authoredRoot);
                if (!root)
                {
                    ++dependencyErrors;
                    PS::Log<LogLevel::Error>(STR("[{}] $ImpModule '{}': source object could not be resolved.\n"),
                        object->GetName(), RC::to_generic_string(authoredRoot));
                    if (settings.Atomic) break;
                    continue;
                }
                if (expectedClass && !root->IsA(expectedClass))
                {
                    ++dependencyErrors;
                    PS::Log<LogLevel::Error>(STR("[{}] $ImpModule '{}': source object is not compatible with Modules.Modules.Module.\n"),
                        object->GetName(), RC::to_generic_string(authoredRoot));
                    if (settings.Atomic) break;
                    continue;
                }
                if (auto existing = emitted.find(root); existing != emitted.end())
                {
                    requests[existing->second].Requested = true;
                    continue;
                }

                std::vector<ImportRequest> staged;
                std::vector<DependencyTrace> stagedTrace;
                std::set<UObject*> stagedDone;
                std::set<UObject*> visiting;
                try
                {
                    const auto visit = [&](auto&& self, UObject* current, const std::string& sourcePath,
                        bool explicitlyRequested, bool forcedImport, int depth) -> void {
                        if (!current) throw std::runtime_error("dependency resolver received a null module");
                        if (depth > 64) throw std::runtime_error("dependency graph exceeded 64 levels");
                        if (emitted.contains(current))
                        {
                            if (explicitlyRequested) requests[emitted.at(current)].Requested = true;
                            if (forcedImport) requests[emitted.at(current)].Forced = true;
                            return;
                        }
                        if (stagedDone.contains(current))
                        {
                            for (auto& item : staged)
                            {
                                if (item.Module != current) continue;
                                if (explicitlyRequested) item.Requested = true;
                                if (forcedImport) item.Forced = true;
                            }
                            return;
                        }
                        if (visiting.contains(current)) return;
                        visiting.insert(current);
                        const auto currentDisplay = RC::to_string(current->GetPathName());
                        for (const auto& dependency : scanDependencies(current))
                        {
                            const auto dependencyName = RC::to_string(dependency.Name.ToString());
                            const bool remapped = !remapTargets(current, dependencyName).empty();
                            if (dependency.Optional && settings.Policy != ImportPolicy::Closure)
                            {
                                stagedTrace.push_back({currentDisplay, dependency.Path, dependencyName, {}, true, remapped});
                                continue;
                            }
                            if (dependency.Optional)
                            {
                                try
                                {
                                    const auto resolvedModules = resolveDependency(current, dependency.Name, dependency.Path);
                                    for (auto* resolved : resolvedModules)
                                    {
                                        const auto resolvedPath = RC::to_string(resolved->GetPathName());
                                        stagedTrace.push_back({currentDisplay, dependency.Path, dependencyName, resolvedPath, true, remapped});
                                        self(self, resolved, resolvedPath, false, remapped, depth + 1);
                                    }
                                }
                                catch (const std::exception& error)
                                {
                                    stagedTrace.push_back({currentDisplay, dependency.Path, dependencyName, {}, true, remapped});
                                    PS::Log<LogLevel::Warning>(STR("[{}] $ImpPolicy Closure skipped optional dependency '{}': {}.\n"),
                                        object->GetName(), RC::to_generic_string(dependencyName), PS::ToWideSafe(error.what()));
                                }
                                continue;
                            }
                            const auto resolvedModules = resolveDependency(current, dependency.Name, dependency.Path);
                            for (auto* resolved : resolvedModules)
                            {
                                const auto resolvedPath = RC::to_string(resolved->GetPathName());
                                stagedTrace.push_back({currentDisplay, dependency.Path, dependencyName, resolvedPath, false, remapped});
                                self(self, resolved, resolvedPath, false, remapped, depth + 1);
                            }
                        }
                        visiting.erase(current);
                        stagedDone.insert(current);
                        if (emitted.size() + staged.size() >= 512)
                            throw std::runtime_error("dependency closure exceeds 512 modules");
                        staged.push_back({sourcePath, current, explicitlyRequested, forcedImport});
                    };
                    visit(visit, root, authoredRoot, true, false, 0);
                }
                catch (const std::exception& error)
                {
                    ++dependencyErrors;
                    PS::Log<LogLevel::Error>(STR("[{}] $ImpModule '{}': dependency resolution failed: {}.\n"),
                        object->GetName(), RC::to_generic_string(authoredRoot), PS::ToWideSafe(error.what()));
                    if (settings.Atomic) break;
                    continue;
                }
                for (auto& item : staged)
                {
                    if (auto existing = emitted.find(item.Module); existing != emitted.end())
                    {
                        if (item.Requested) requests[existing->second].Requested = true;
                        if (item.Forced) requests[existing->second].Forced = true;
                        continue;
                    }
                    emitted[item.Module] = requests.size();
                    requests.push_back(std::move(item));
                }
                dependencyTrace.insert(dependencyTrace.end(), stagedTrace.begin(), stagedTrace.end());
            }
        }

        if (dependencyErrors && settings.Atomic)
        {
            ++outResult.ErrorCount;
            PS::Log<LogLevel::Error>(STR("[{}] $ImpAtomic blocked the complete module-import batch during dependency resolution; destination was not changed.\n"),
                object->GetName());
            return false;
        }
        outResult.ErrorCount += dependencyErrors;

        enum class ActionKind { Append, Reuse, Replace };
        struct PlannedAction
        {
            ActionKind Kind = ActionKind::Append;
            std::string Source;
            UObject* Module = nullptr;
            int32 ExistingIndex = -1;
        };

        FScriptArrayHelper inspect(arrayProperty, arrayAddress);
        const int32 originalModuleCount = inspect.Num();
        std::vector<UObject*> virtualModules;
        virtualModules.reserve(static_cast<std::size_t>(originalModuleCount) + requests.size());
        for (int32 index = 0; index < originalModuleCount; ++index)
        {
            auto* element = inspect.GetRawPtr(index);
            auto* value = moduleProperty->ContainerPtrToValuePtr<void>(element);
            virtualModules.push_back(moduleProperty->GetObjectPropertyValue(value));
        }

        std::vector<PlannedAction> plan;
        int validationErrors = 0;
        for (const auto& request : requests)
        {
            const auto& source = request.Source;
            auto* imported = request.Module;
            try
            {
                if (!imported) throw std::runtime_error("source object could not be resolved: " + source);
                auto* expectedClass = moduleProperty->GetPropertyClass().Get();
                if (expectedClass && !imported->IsA(expectedClass))
                    throw std::runtime_error("source object is not compatible with Modules.Modules.Module: " + source);

                FName wantedName{};
                const bool hasName = moduleName(imported, wantedName);
                std::vector<int32> exactMatches;
                std::vector<int32> nameMatches;
                for (std::size_t index = 0; index < virtualModules.size(); ++index)
                {
                    auto* existing = virtualModules[index];
                    if (existing == imported)
                    {
                        exactMatches.push_back(static_cast<int32>(index));
                        continue;
                    }
                    if (hasName)
                    {
                        FName existingName{};
                        if (moduleName(existing, existingName) && existingName == wantedName)
                            nameMatches.push_back(static_cast<int32>(index));
                    }
                }

                PlannedAction action{ActionKind::Append, source, imported, -1};
                if (!exactMatches.empty())
                {
                    action.ExistingIndex = exactMatches.front();
                    action.Kind = ActionKind::Reuse;
                }
                else if (!request.Requested && !request.Forced && !nameMatches.empty())
                {
                    if (nameMatches.size() != 1)
                        throw std::runtime_error("required dependency is ambiguous in destination: " + source);
                    action.ExistingIndex = nameMatches.front();
                    auto* reused = virtualModules[static_cast<std::size_t>(action.ExistingIndex)];
                    if (!reused || reused->GetClassPrivate() != imported->GetClassPrivate())
                        throw std::runtime_error("required dependency has a same-name destination module with an incompatible class: " + source);
                    action.Kind = ActionKind::Reuse;
                }
                else if (settings.Conflict == ImportConflict::AllowDuplicate)
                {
                    virtualModules.push_back(imported);
                }
                else if (nameMatches.empty())
                {
                    virtualModules.push_back(imported);
                }
                else
                {
                    if (nameMatches.size() != 1)
                        throw std::runtime_error("module identity is ambiguous in destination: " + source);
                    action.ExistingIndex = nameMatches.front();
                    if (settings.Conflict == ImportConflict::Error)
                        throw std::runtime_error("module conflicts with an existing destination module: " + source);
                    if (settings.Conflict == ImportConflict::Reuse)
                    {
                        auto* reused = virtualModules[static_cast<std::size_t>(action.ExistingIndex)];
                        if (!reused || reused->GetClassPrivate() != imported->GetClassPrivate())
                            throw std::runtime_error("$ImpConflict Reuse found the same ModuleName on an incompatible module class: " + source);
                        action.Kind = ActionKind::Reuse;
                    }
                    else
                    {
                        if (action.ExistingIndex >= originalModuleCount)
                            throw std::runtime_error("$ImpConflict Replace cannot target another module from the same import batch; remove the duplicate request");
                        action.Kind = ActionKind::Replace;
                        virtualModules[static_cast<std::size_t>(action.ExistingIndex)] = imported;
                    }
                }
                plan.push_back(std::move(action));
            }
            catch (const std::exception& error)
            {
                ++validationErrors;
                PS::Log<LogLevel::Error>(STR("[{}] $ImpModule '{}': {}.\n"),
                    object->GetName(), RC::to_generic_string(source), PS::ToWideSafe(error.what()));
                if (settings.Atomic) break;
            }
        }

        if (validationErrors && settings.Atomic)
        {
            ++outResult.ErrorCount;
            PS::Log<LogLevel::Error>(STR("[{}] $ImpAtomic blocked the complete module-import batch; destination was not changed.\n"),
                object->GetName());
            return false;
        }
        outResult.ErrorCount += validationErrors;

        int appendCount = 0;
        int replaceCount = 0;
        int reuseCount = 0;
        for (const auto& action : plan)
        {
            if (action.Kind == ActionKind::Reuse) { ++reuseCount; continue; }
            if (settings.Dry) continue;
            if (action.Kind == ActionKind::Replace)
            {
                auto* value = moduleProperty->ContainerPtrToValuePtr<void>(
                    inspect.GetRawPtr(action.ExistingIndex));
                auto* replacement = action.Module;
                FMemory::Memcpy(value, &replacement, sizeof(replacement));
                ++replaceCount;
                ++outResult.PropertiesWritten;
                continue;
            }
            UECustom::FScriptArrayHelper destination(arrayAddress, arrayProperty);
            UECustom::FManagedValue prepared;
            destination.InitializeValue(prepared);
            auto* value = moduleProperty->ContainerPtrToValuePtr<void>(prepared.GetData());
            auto* imported = action.Module;
            FMemory::Memcpy(value, &imported, sizeof(imported));
            destination.Add(prepared);
            ++appendCount;
            ++outResult.PropertiesWritten;
        }

        const auto shortObjectName = [](const std::string& path) -> std::string {
            if (const auto colon = path.rfind(':'); colon != std::string::npos && colon + 1 < path.size())
                return path.substr(colon + 1);
            if (const auto dot = path.rfind('.'); dot != std::string::npos && dot + 1 < path.size())
                return path.substr(dot + 1);
            if (const auto slash = path.rfind('/'); slash != std::string::npos && slash + 1 < path.size())
                return path.substr(slash + 1);
            return path;
        };
        const auto actionLabel = [](ActionKind kind) -> const char* {
            switch (kind)
            {
            case ActionKind::Append: return "append";
            case ActionKind::Reuse: return "reuse";
            case ActionKind::Replace: return "replace";
            }
            return "unknown";
        };

        if (settings.Dry)
        {
            const auto totalErrors = dependencyErrors + validationErrors;
            std::ostringstream report;
            report << "[" << RC::to_string(object->GetName()) << "] [$ImpDry]\n\n";
            report << "IMPORT MAP\n";

            if (!settings.Verbose)
            {
                report << "  " << (totalErrors == 0 ? "READY" : "BLOCKED")
                       << " | requested " << settings.Modules.size()
                       << " | resolved " << requests.size()
                       << " | planned " << plan.size()
                       << " | reused " << reuseCount
                       << " | errors " << totalErrors << "\n";
                report << "  Requested: ";
                if (settings.Modules.empty())
                {
                    report << "none\n";
                }
                else
                {
                    for (std::size_t index = 0; index < settings.Modules.size(); ++index)
                    {
                        if (index > 0) report << ", ";
                        report << shortObjectName(settings.Modules[index]);
                    }
                    report << "\n";
                }
            }
            else
            {
                report << "  Requested : ";
                if (settings.Modules.empty())
                {
                    report << "none\n";
                }
                else if (settings.Modules.size() == 1)
                {
                    report << shortObjectName(settings.Modules.front()) << "\n";
                }
                else
                {
                    report << settings.Modules.size() << " modules\n";
                    for (const auto& requested : settings.Modules)
                        report << "    + " << shortObjectName(requested) << "\n";
                }
            }

            if (settings.Policy == ImportPolicy::Required || settings.Policy == ImportPolicy::Closure)
            {
                std::vector<const DependencyTrace*> required;
                std::vector<const DependencyTrace*> optional;
                required.reserve(dependencyTrace.size());
                optional.reserve(dependencyTrace.size());
                for (const auto& dependency : dependencyTrace)
                    (dependency.Optional ? optional : required).push_back(&dependency);

                if (settings.Verbose)
                {
                    report << "\n  Required\n";
                    if (required.empty())
                    {
                        report << "    none\n";
                    }
                    else
                    {
                        std::size_t width = 0;
                        for (const auto* dependency : required)
                            width = std::max(width, dependency->Name.size());
                        for (const auto* dependency : required)
                        {
                            report << "    + " << dependency->Name;
                            if (dependency->Name.size() < width)
                                report << std::string(width - dependency->Name.size(), ' ');
                            report << " -> " << shortObjectName(dependency->Resolved);
                            if (dependency->Remapped) report << " [remap]";
                            report << "\n";
                        }
                    }

                    report << "\n  Optional\n";
                    if (optional.empty())
                    {
                        report << "    none\n";
                    }
                    else
                    {
                        std::size_t width = 0;
                        for (const auto* dependency : optional)
                            width = std::max(width, dependency->Name.size());
                        for (const auto* dependency : optional)
                        {
                            report << "    - " << dependency->Name;
                            if (dependency->Name.size() < width)
                                report << std::string(width - dependency->Name.size(), ' ');
                            if (dependency->Resolved.empty())
                            {
                                report << " [skipped]\n";
                            }
                            else
                            {
                                report << " -> " << shortObjectName(dependency->Resolved) << " [included";
                                if (dependency->Remapped) report << ", remap";
                                report << "]\n";
                            }
                        }
                    }
                }
                else
                {
                    report << "\n  Dependencies\n";
                    const auto printCollapsed = [&](const std::vector<const DependencyTrace*>& traces, bool isOptional) {
                        std::map<std::string, std::pair<const DependencyTrace*, std::size_t>> collapsed;
                        for (const auto* dependency : traces)
                        {
                            const auto key = dependency->Name + "\x1f" + dependency->Resolved + "\x1f"
                                + (dependency->Remapped ? "1" : "0") + "\x1f"
                                + (dependency->Resolved.empty() ? "1" : "0");
                            auto& entry = collapsed[key];
                            if (!entry.first) entry.first = dependency;
                            ++entry.second;
                        }
                        if (collapsed.empty())
                        {
                            report << "    " << (isOptional ? "optional" : "required") << ": none\n";
                            return;
                        }
                        for (const auto& [unused, entry] : collapsed)
                        {
                            const auto* dependency = entry.first;
                            report << "    " << (isOptional ? "- " : "+ ") << dependency->Name;
                            if (dependency->Resolved.empty())
                                report << " [skipped]";
                            else
                                report << " -> " << shortObjectName(dependency->Resolved);
                            if (dependency->Remapped) report << " [remap]";
                            if (entry.second > 1) report << " x" << entry.second;
                            report << "\n";
                        }
                    };
                    printCollapsed(required, false);
                    printCollapsed(optional, true);
                }
            }

            std::vector<ExternalReference> externalReferences;
            std::set<UObject*> externalScanned;
            std::string externalScanError;
            try
            {
                for (const auto& request : requests)
                {
                    if (!request.Module || !externalScanned.insert(request.Module).second) continue;
                    auto moduleReferences = scanExternalReferences(request.Module);
                    externalReferences.insert(externalReferences.end(),
                        moduleReferences.begin(), moduleReferences.end());
                }
            }
            catch (const std::exception& error)
            {
                externalScanError = error.what();
            }

            const auto countKind = [&](ExternalReferenceKind kind) {
                return std::ranges::count_if(externalReferences, [kind](const ExternalReference& reference) {
                    return reference.Kind == kind;
                });
            };
            const auto assetCount = countKind(ExternalReferenceKind::Asset);
            const auto behavioralClassCount = countKind(ExternalReferenceKind::BehavioralClass);
            const auto objectCount = countKind(ExternalReferenceKind::Object);
            const auto ownedCount = countKind(ExternalReferenceKind::OwnedSubobject);
            const auto softCount = countKind(ExternalReferenceKind::SoftReference);
            const auto structuralCount = countKind(ExternalReferenceKind::Structural);
            const auto countProvenance = [&](ExternalReferenceProvenance provenance) {
                return std::ranges::count_if(externalReferences, [provenance](const ExternalReference& reference) {
                    return reference.Kind != ExternalReferenceKind::Structural
                        && reference.Provenance == provenance;
                });
            };
            const auto sourceDependencyCount = countProvenance(ExternalReferenceProvenance::Source);
            const auto sharedDependencyCount = countProvenance(ExternalReferenceProvenance::Shared);
            const auto targetDependencyCount = countProvenance(ExternalReferenceProvenance::Target);
            std::set<std::string> modulesWithVisibleDependencies;
            std::set<std::string> sourceCoupledModules;
            for (const auto& reference : externalReferences)
            {
                if (reference.Kind == ExternalReferenceKind::Structural
                    || reference.Kind == ExternalReferenceKind::OwnedSubobject)
                    continue;
                modulesWithVisibleDependencies.insert(reference.Module);
                if (reference.Provenance == ExternalReferenceProvenance::Source)
                    sourceCoupledModules.insert(reference.Module);
            }
            const auto selfContainedCount = externalScanned.size() >= modulesWithVisibleDependencies.size()
                ? externalScanned.size() - modulesWithVisibleDependencies.size() : 0;

            report << "\n  External\n";
            if (!externalScanError.empty())
            {
                report << "    scan failed: " << externalScanError << "\n";
            }
            const auto provenanceLabel = [](ExternalReferenceProvenance provenance) -> const char* {
                switch (provenance)
                {
                case ExternalReferenceProvenance::Source: return "source";
                case ExternalReferenceProvenance::Target: return "target";
                case ExternalReferenceProvenance::Owned: return "owned";
                case ExternalReferenceProvenance::Shared: return "shared";
                }
                return "unknown";
            };
            const auto compactClass = [](const std::string& className) -> std::string {
                if (className == "BlueprintGeneratedClass") return "BGC";
                return className;
            };
            const auto dependencyDisposition = [&](const ExternalReference& reference) {
                if (reference.Kind == ExternalReferenceKind::OwnedSubobject
                    || reference.Provenance == ExternalReferenceProvenance::Owned)
                    return ExternalDependencyDisposition::Owned;
                if (reference.Soft)
                    return ExternalDependencyDisposition::Unresolved;
                if (reference.Kind == ExternalReferenceKind::Object)
                    return ExternalDependencyDisposition::Contextual;
                if (reference.Kind == ExternalReferenceKind::Asset
                    || reference.Kind == ExternalReferenceKind::BehavioralClass)
                {
                    if (reference.Provenance == ExternalReferenceProvenance::Source)
                        return ExternalDependencyDisposition::Candidate;
                    return ExternalDependencyDisposition::Available;
                }
                return ExternalDependencyDisposition::Contextual;
            };
            const auto dispositionLabel = [](ExternalDependencyDisposition disposition) -> const char* {
                switch (disposition)
                {
                case ExternalDependencyDisposition::Available: return "available";
                case ExternalDependencyDisposition::Candidate: return "candidate";
                case ExternalDependencyDisposition::Owned: return "owned";
                case ExternalDependencyDisposition::Contextual: return "contextual";
                case ExternalDependencyDisposition::Unresolved: return "unresolved";
                }
                return "unknown";
            };
            const auto printExternalKind = [&](ExternalReferenceKind kind, const char* label) {
                std::vector<const ExternalReference*> matches;
                for (const auto& reference : externalReferences)
                    if (reference.Kind == kind) matches.push_back(&reference);
                if (matches.empty()) return;
                if (settings.Verbose)
                {
                    report << "\n    " << label << "\n";
                    std::string currentModule;
                    for (const auto* reference : matches)
                    {
                        if (reference->Module != currentModule)
                        {
                            currentModule = reference->Module;
                            report << "      " << shortObjectName(currentModule) << "\n";
                        }
                        report << "        + " << reference->Path << " -> " << shortObjectName(reference->Target);
                        if (!reference->Class.empty()) report << " [" << reference->Class << "]";
                        report << " [" << provenanceLabel(reference->Provenance) << "]"
                               << " [" << dispositionLabel(dependencyDisposition(*reference)) << "]\n";
                        report << "          path: " << reference->Target << "\n";
                        if (!reference->ClassPath.empty())
                            report << "          class: " << reference->ClassPath << "\n";
                        report << "          state: " << (reference->Soft ? "soft/unloaded" : "loaded") << "\n";
                    }
                    return;
                }
                if (kind == ExternalReferenceKind::OwnedSubobject) return;
                for (const auto* reference : matches)
                {
                    report << "    + " << shortObjectName(reference->Module) << "." << reference->Path
                           << " -> " << shortObjectName(reference->Target);
                    if (!reference->Class.empty()) report << " [" << compactClass(reference->Class) << "]";
                    report << " [" << provenanceLabel(reference->Provenance) << "]"
                           << " [" << (reference->Soft ? "soft" : "loaded") << "]"
                           << " [" << dispositionLabel(dependencyDisposition(*reference)) << "]\n";
                    if (reference->Soft)
                        report << "      path: " << reference->Target << "\n";
                }
            };
            printExternalKind(ExternalReferenceKind::Asset, "Assets");
            printExternalKind(ExternalReferenceKind::BehavioralClass, "Behavioral Classes");
            printExternalKind(ExternalReferenceKind::Object, "External Objects / Components");
            printExternalKind(ExternalReferenceKind::OwnedSubobject, "Owned Subobjects");
            printExternalKind(ExternalReferenceKind::SoftReference, "Soft References");
            const bool visibleExternal = std::ranges::any_of(externalReferences, [&](const ExternalReference& reference) {
                return reference.Kind != ExternalReferenceKind::Structural
                    && (settings.Verbose || reference.Kind != ExternalReferenceKind::OwnedSubobject);
            });
            if (!visibleExternal && externalScanError.empty()) report << "    none\n";

            report << "\n  External deps: assets " << assetCount
                   << " | classes " << behavioralClassCount
                   << " | objects " << objectCount
                   << " | owned " << ownedCount
                   << " | soft " << softCount << "\n";

            std::size_t availableDependencyCount = 0;
            std::size_t candidateDependencyCount = 0;
            std::size_t ownedDependencyCount = 0;
            std::size_t contextualDependencyCount = 0;
            std::size_t unresolvedDependencyCount = 0;
            for (const auto& reference : externalReferences)
            {
                if (reference.Kind == ExternalReferenceKind::Structural) continue;
                switch (dependencyDisposition(reference))
                {
                case ExternalDependencyDisposition::Available: ++availableDependencyCount; break;
                case ExternalDependencyDisposition::Candidate: ++candidateDependencyCount; break;
                case ExternalDependencyDisposition::Owned: ++ownedDependencyCount; break;
                case ExternalDependencyDisposition::Contextual: ++contextualDependencyCount; break;
                case ExternalDependencyDisposition::Unresolved: ++unresolvedDependencyCount; break;
                }
            }
            report << "  Safety: available " << availableDependencyCount
                   << " | candidates " << candidateDependencyCount
                   << " | contextual " << contextualDependencyCount
                   << " | unresolved " << unresolvedDependencyCount
                   << " | owned " << ownedDependencyCount << "\n";

            if (settings.External == ExternalImportMode::Plan)
            {
                struct ExternalPlanEntry
                {
                    const ExternalReference* Reference = nullptr;
                    ExternalDependencyDisposition Disposition = ExternalDependencyDisposition::Available;
                    std::size_t Uses = 0;
                };
                std::map<std::string, ExternalPlanEntry> externalPlan;
                for (const auto& reference : externalReferences)
                {
                    if (reference.Kind == ExternalReferenceKind::Structural) continue;
                    const auto disposition = dependencyDisposition(reference);
                    const auto key = std::to_string(static_cast<int>(disposition)) + "\x1f" + reference.Target;
                    auto& entry = externalPlan[key];
                    if (!entry.Reference)
                    {
                        entry.Reference = &reference;
                        entry.Disposition = disposition;
                    }
                    ++entry.Uses;
                }
                std::size_t planImportCount = 0;
                std::size_t planReuseCount = 0;
                std::size_t planOwnedCount = 0;
                std::size_t planBlockedCount = 0;
                for (const auto& [unused, entry] : externalPlan)
                {
                    switch (entry.Disposition)
                    {
                    case ExternalDependencyDisposition::Candidate: ++planImportCount; break;
                    case ExternalDependencyDisposition::Available: ++planReuseCount; break;
                    case ExternalDependencyDisposition::Owned: ++planOwnedCount; break;
                    case ExternalDependencyDisposition::Contextual:
                    case ExternalDependencyDisposition::Unresolved: ++planBlockedCount; break;
                    }
                }
                report << "  External plan: import " << planImportCount
                       << " | reuse " << planReuseCount
                       << " | owned " << planOwnedCount
                       << " | blocked " << planBlockedCount << "\n";
                for (const auto& [unused, entry] : externalPlan)
                {
                    if (!entry.Reference) continue;
                    const auto& reference = *entry.Reference;
                    const char* operation = "reuse";
                    switch (entry.Disposition)
                    {
                    case ExternalDependencyDisposition::Candidate: operation = "would-import"; break;
                    case ExternalDependencyDisposition::Available: operation = "reuse"; break;
                    case ExternalDependencyDisposition::Owned: operation = "travels"; break;
                    case ExternalDependencyDisposition::Contextual: operation = "blocked-context"; break;
                    case ExternalDependencyDisposition::Unresolved: operation = "blocked-unresolved"; break;
                    }
                    if (!settings.Verbose && entry.Disposition == ExternalDependencyDisposition::Available) continue;
                    if (!settings.Verbose && entry.Disposition == ExternalDependencyDisposition::Owned) continue;
                    report << "    " << operation << ": " << shortObjectName(reference.Target);
                    if (!reference.Class.empty()) report << " [" << compactClass(reference.Class) << "]";
                    report << " [" << provenanceLabel(reference.Provenance) << "]";
                    if (entry.Uses > 1) report << " x" << entry.Uses;
                    report << "\n";
                    if (settings.Verbose)
                    {
                        report << "      from: " << shortObjectName(reference.Module) << "." << reference.Path << "\n";
                        report << "      path: " << reference.Target << "\n";
                    }
                }
            }

            const bool hasAttentionDependencies = candidateDependencyCount > 0
                || contextualDependencyCount > 0 || unresolvedDependencyCount > 0;
            if (hasAttentionDependencies)
            {
                report << "  Review\n";
                for (const auto& reference : externalReferences)
                {
                    if (reference.Kind == ExternalReferenceKind::Structural) continue;
                    const auto disposition = dependencyDisposition(reference);
                    if (disposition != ExternalDependencyDisposition::Candidate
                        && disposition != ExternalDependencyDisposition::Contextual
                        && disposition != ExternalDependencyDisposition::Unresolved)
                        continue;
                    report << "    " << dispositionLabel(disposition) << ": "
                           << shortObjectName(reference.Module) << "." << reference.Path
                           << " -> " << shortObjectName(reference.Target);
                    if (!reference.Class.empty())
                        report << " [" << compactClass(reference.Class) << "]";
                    report << " [" << provenanceLabel(reference.Provenance) << "]\n";
                    if (reference.Soft || settings.Verbose)
                        report << "      path: " << reference.Target << "\n";
                }
            }

            report << "  Portability: self " << selfContainedCount
                   << " | source " << sourceCoupledModules.size()
                   << " | shared " << sharedDependencyCount
                   << " | target " << targetDependencyCount << "\n";
            if (structuralCount > 0)
                report << "  Structural hidden: " << structuralCount << "\n";

            report << "\n  Plan\n";
            if (plan.empty())
            {
                report << "    none\n";
            }
            else
            {
                std::size_t step = 0;
                for (const auto& action : plan)
                {
                    ++step;
                    const auto display = action.Module
                        ? RC::to_string(action.Module->GetName())
                        : shortObjectName(action.Source);
                    report << "    " << step << ". " << display << " ["
                           << actionLabel(action.Kind) << "]\n";
                }
            }

            if (settings.Verbose)
            {
                report << "\n  " << (totalErrors == 0 ? "READY" : "BLOCKED") << "\n";
                report << "  requested " << settings.Modules.size()
                       << " | resolved " << requests.size()
                       << " | planned " << plan.size()
                       << " | reused " << reuseCount
                       << " | errors " << totalErrors << "\n";
            }



            if (totalErrors == 0)
                PS::Log<LogLevel::Normal>(STR("{}"), RC::to_generic_string(report.str()));
            else
                PS::Log<LogLevel::Error>(STR("{}"), RC::to_generic_string(report.str()));
        }
        else
            PS::Log<LogLevel::Normal>(STR("[{}] [$ImpModule] APPLIED | requested {} | resolved {} | appended {} | replaced {} | reused {} | errors {}\n"),
                object->GetName(), settings.Modules.size(), requests.size(), appendCount, replaceCount, reuseCount,
                dependencyErrors + validationErrors);
        return !settings.Atomic || (dependencyErrors + validationErrors) == 0;
    }

    void DragonWildsAssetModLoader::ApplyDominionSpheres(UObject* owner,
        const nlohmann::json& definitions, LoadResult& outResult)
    {
        ValidateDominionSpheres(definitions);
        auto* sphereClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionShape_Sphere"), false);
        if (!sphereClass) throw std::runtime_error("DominionShape_Sphere class is unavailable");

        for (const auto& [path, body] : definitions.items())
        {
            UObject* current = owner;
            size_t begin = 0;
            while (begin < path.size())
            {
                const auto end = path.find('.', begin);
                const auto segment = path.substr(begin, end == std::string::npos ? path.size() - begin : end - begin);
                const FName wanted(RC::to_generic_string(segment), FNAME_Add);
                UObject* found = nullptr;
                UObjectGlobals::ForEachUObject([&](UObject* candidate, int32_t, int32_t) -> LoopAction {
                    if (!candidate || candidate->GetOuterPrivate() != current || candidate->GetFName() != wanted
                        || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)))
                        return LoopAction::Continue;
                    if (found && found != candidate)
                        throw std::runtime_error("Dominion sphere subobject path is ambiguous: " + path);
                    found = candidate;
                    return LoopAction::Continue;
                });
                if (!found) throw std::runtime_error("Dominion sphere subobject was not found: " + path);
                current = found;
                if (end == std::string::npos) break;
                begin = end + 1;
            }
            if (!current || !current->IsA(sphereClass))
                throw std::runtime_error("Dominion sphere target has the wrong class: " + path);
            auto* radius = CastField<FFloatProperty>(
                PropertyHelper::GetPropertyByName(current->GetClassPrivate(), TEXT("Radius")));
            if (!radius || radius->GetArrayDim() != 1 || radius->GetElementSize() != sizeof(float))
                throw std::runtime_error("DominionShape_Sphere Radius layout changed");
            const auto requested = body.at("Radius").get<double>();
            PropertyHelper::CopyJsonValueToContainer(current, radius, body.at("Radius"));
            const auto written = radius->GetFloatingPointPropertyValue(
                radius->ContainerPtrToValuePtr<void>(current));
            if (!std::isfinite(written) || std::abs(written - requested) > 0.001)
                throw std::runtime_error("DominionShape_Sphere Radius did not match after update");
            ++outResult.PropertiesWritten;
        }
    }

    void DragonWildsAssetModLoader::AppendProperties(UObject* object, UClass* objectClass, const nlohmann::json& appendData, LoadResult& outResult)
    {
        if (!appendData.is_object())
        {
            outResult.ErrorCount++;
            PS::Log<LogLevel::Error>(STR("[{}] $Append must be an object.\n"), object->GetName());
            return;
        }

        for (auto& [propertyName, items] : appendData.items())
        {
            auto propertyNameWide = RC::to_generic_string(propertyName);
            if (!items.is_array())
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("[{}] $Append.{} must be an array.\n"),
                    object->GetName(), propertyNameWide);
                continue;
            }

            auto* property = PropertyHelper::GetPropertyByName(objectClass, propertyNameWide);
            if (!property)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Warning>(STR("[{}] Append target '{}' was not found.\n"),
                    object->GetName(), propertyNameWide);
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(object, property, PropertyHelper::BuildAppendValue(property, items));
                outResult.PropertiesWritten++;
            }
            catch (const std::exception& e)
            {
                outResult.ErrorCount++;
                PS::Log<LogLevel::Error>(STR("[{}] Failed appending to '{}': {}\n"),
                    object->GetName(), propertyNameWide, PS::ToWideSafe(e.what()));
            }
        }
    }

    std::vector<RC::StringType> DragonWildsAssetModLoader::CookedReferencePaths()
    {
        std::scoped_lock lock{m_mutex};
        std::vector<RC::StringType> paths;
        const auto collect=[&](const auto& pending){
            for(const auto& asset:pending){
                if(asset.Properties.contains("$Clone")){
                    const auto& source=asset.Properties.at("$Clone");
                    if(source.is_string())paths.push_back(NormalizeObjectPath(RC::to_generic_string(source.get<std::string>())));
                }else if(!asset.ObjectPath.empty())paths.push_back(asset.ObjectPath);
            }
        };
        collect(m_pendingAssets);collect(m_pendingPatches);return paths;
    }

    UObject* DragonWildsAssetModLoader::EnsureQuestCancellationScroll(const std::string& path,const nlohmann::json& properties)
    {
        std::scoped_lock lock{m_mutex};
        const auto contract=QuestScrolls::Parse(QuestCancellations::Owner,properties.at("$QuestScroll"));
        if(contract.Action!="Abandon")throw std::runtime_error("Generated cancellation must bind Abandon");
        PendingAsset pending;
        pending.Target=RC::to_generic_string(path);pending.ObjectPath=pending.Target;
        pending.ModName=RC::to_generic_string(QuestCancellations::Owner);pending.Properties=properties;
        if(const auto bound=QuestScrolls::Find(path);bound && *bound!=contract)
            throw std::runtime_error("Generated cancellation binding conflicts with an existing item");
        auto* subsystem=FindItemSubsystem();
        auto* object=Resolve(pending);bool created=false;
        if(object && std::find(m_createdAssets.begin(),m_createdAssets.end(),object)==m_createdAssets.end())
            throw std::runtime_error("Generated cancellation path is occupied by another asset");
        if(!object){object=CreateFromClone(pending,subsystem);created=object!=nullptr;}
        if(!object || !IsReadyForPatch(object) || !IsSupportedTarget(object))throw std::runtime_error("Cancellation scroll is not ready");
        LoadResult result{};Apply(object,pending,result);
        const bool registered=result.ErrorCount==0 && RegisterCreatedItem(object,pending,subsystem);
        PS::AssetProvenance::Record(object,properties.at("$Clone").get<std::string>(),QuestCancellations::Owner,created,registered,result.ErrorCount,properties);
        if(!registered || result.ErrorCount)throw std::runtime_error("Cancellation scroll failed native item registration");
        PS::AssetAliases::Add(pending.Target,object->GetPathName());
        return object;
    }

    void DragonWildsAssetModLoader::TryApplyPending()
    {
        struct BatchResult {
            int TargetsUpdated = 0;
            int Created = 0;
            int ClonesUpdated = 0;
            int Patched = 0;
            int PropertiesWritten = 0;
            int ErrorCount = 0;
        };

        std::map<RC::StringType, BatchResult> batchResults;
        std::scoped_lock lock{m_mutex};

        // Helpy may clone another installed runtime item. Its parent must be
        // created/registered first even when JSON filenames put the child first.
        std::vector<PS::HelpyDependencies::Node> dependencies;
        dependencies.reserve(m_pendingAssets.size());
        for(const auto& pending:m_pendingAssets) {
            PS::HelpyDependencies::Node node;
            if(pending.Properties.contains("$Clone")) {
                node.source=RC::to_string(NormalizeObjectPath(RC::to_generic_string(pending.Properties.at("$Clone").get<std::string>())));
                node.aliases.push_back(RC::to_string(pending.ObjectPath));
                std::string internal;
                if(ReadRequiredString(pending.Properties,"InternalName",internal)) {
                    const auto name=SanitizePackageSegment(internal,"ITEM_RS_Unnamed");
                    node.aliases.push_back("/Game/RuneSchema/"+SanitizePackageSegment(RC::to_string(pending.ModName),"UnnamedMod")+"/Items/"+name+"."+name);
                }
            }
            dependencies.push_back(std::move(node));
        }
        const auto order=PS::HelpyDependencies::Order(dependencies);
        for(const auto i:order.blocked) {
            const auto& pending=m_pendingAssets[i];++batchResults[pending.ModName].ErrorCount;
            PS::Log<LogLevel::Error>(STR("Clone '{}' has a cyclic clone-source dependency; skipped without changing unrelated definitions.\n"),pending.Target);
        }
        std::vector<PendingAsset> sorted;sorted.reserve(order.ordered.size());
        for(const auto i:order.ordered)sorted.push_back(std::move(m_pendingAssets[i]));
        m_pendingAssets=std::move(sorted);

        UObject* itemSubsystem = nullptr;
        bool subsystemSearched = false;
        constexpr size_t detailLimit = 8;
        size_t detailLines = 0;
        size_t omittedDetails = 0;
        PS::ConsumeQueue(m_pendingAssets, [&](PendingAsset& pending)
        {
            auto* it = &pending;
            UObject* object = nullptr;
            const bool isClone = it->Properties.contains("$Clone");
            bool createdNow = false;
            if (isClone && !subsystemSearched) {
                itemSubsystem = FindItemSubsystem();
                subsystemSearched = true;
            }
            try
            {
                object = Resolve(*it);
                if (object && isClone
                    && std::find(m_createdAssets.begin(), m_createdAssets.end(), object)
                        == m_createdAssets.end())
                    throw std::runtime_error(
                        "the clone target collides with an existing loaded asset");
                if (!object && isClone) {
                    object = CreateFromClone(*it, itemSubsystem);
                    createdNow = object != nullptr;
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Clone '{}' from {} rejected: {}\n"),
                    it->Target, it->ModName, PS::ToWideSafe(error.what()));
                batchResults[it->ModName].ErrorCount++;
                return true;
            }
            if (!object || !IsReadyForPatch(object))
            {
                return false;
            }

            if (!IsSupportedTarget(object))
            {
                auto* objectClass = object->GetClassPrivate();
                auto className = objectClass ? objectClass->GetName() : TEXT("<unknown>");
                PS::Log<LogLevel::Error>(STR("'{}' resolved to '{}' (class '{}'), which is not a DataAsset, a Curve, or a subobject owned by one. Skipping.\n"),
                    it->Target, object->GetName(), className);
                batchResults[it->ModName].ErrorCount++;
                return true;
            }

            LoadResult result{};
            Apply(object, *it, result);
            const bool registered=!isClone || (result.ErrorCount==0 && RegisterCreatedItem(object, *it, itemSubsystem));
            if (!registered) result.ErrorCount++;
            if(isClone && registered && result.ErrorCount==0) {
                try {
                    const auto authored=NormalizeObjectPath(it->Target);
                    if(!authored.empty() && authored.front()==TEXT('/'))
                        PS::AssetAliases::Add(authored,object->GetPathName());
                }catch(const std::exception& error) {
                    ++result.ErrorCount;
                    PS::Log<LogLevel::Error>(STR("Clone alias '{}': {}\n"),it->Target,PS::ToWideSafe(error.what()));
                }
            }
            if (isClone) PS::AssetProvenance::Record(object,it->Properties.at("$Clone").get<std::string>(),
                RC::to_string(it->ModName),createdNow,registered,result.ErrorCount,it->Properties);
            if (isClone && registered && result.ErrorCount == 0) {
                if (detailLines < detailLimit) {
                    PS::Log<LogLevel::Verbose>(STR("{}: {} clone '{}' as '{}'.\n"),
                        it->ModName, createdNow ? STR("created") : STR("updated"),
                        it->Target, object->GetPathName());
                    ++detailLines;
                } else ++omittedDetails;
            }

            auto& batchResult = batchResults[it->ModName];
            if (isClone) {
                if (createdNow) ++batchResult.Created;
                else ++batchResult.ClonesUpdated;
            } else if (it->IsPatch) ++batchResult.Patched;
            else ++batchResult.TargetsUpdated;
            batchResult.PropertiesWritten += result.PropertiesWritten;
            batchResult.ErrorCount += result.ErrorCount;

            return true;
        });

        PS::AssetProvenance::Flush();
        if (omittedDetails)
            PS::Log<LogLevel::Verbose>(STR("Assets: {} additional successful clone detail(s) omitted.\n"), omittedDetails);
        std::size_t totalCreated=0,totalUpdated=0,totalErrors=0;
        for (auto& [modName, result] : batchResults)
        {
            totalCreated+=result.Created;
            totalUpdated+=result.ClonesUpdated+result.Patched+result.TargetsUpdated;
            totalErrors+=result.ErrorCount;
            if (result.Created || result.ClonesUpdated)
                PS::Log<LogLevel::Verbose>(STR("[loader=assets][mod={}] clones: created={} updated={}.\n"),
                    modName, result.Created, result.ClonesUpdated);
            if (result.Patched)
                PS::Log<LogLevel::Verbose>(STR("[loader=assets][mod={}] patches: updated={}.\n"), modName, result.Patched);
            if (result.ErrorCount)
                PS::Log<LogLevel::Warning>(STR("{} assets: {} updated, {} properties written, {} errors.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten, result.ErrorCount);
            else if (result.TargetsUpdated)
                PS::Log<LogLevel::Verbose>(STR("[loader=assets][mod={}] targets: updated={} properties={}.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten);
        }
        if(!batchResults.empty())
            PS::LoaderSummary("assets", totalCreated+totalUpdated,
                totalCreated,totalUpdated,0,totalErrors);
    }

    void DragonWildsAssetModLoader::ReportUnresolvedAssets()
    {
        std::scoped_lock lock{m_mutex};
        for (auto& pendingAsset : m_pendingAssets)
        {
            PS::Log<LogLevel::Error>(STR("Gave up resolving '{}'; the asset never loaded.\n"),
                pendingAsset.Target);
        }

        m_pendingAssets.clear();
    }

    UObject* DragonWildsAssetModLoader::Resolve(const PendingAsset& pendingAsset)
    {
        if (const auto created = m_createdAssetsByTarget.find(pendingAsset.Target);
            created != m_createdAssetsByTarget.end())
            return created->second;

        if (!pendingAsset.ObjectPath.empty())
        {
            auto* found = UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, pendingAsset.ObjectPath.c_str(), false);
            if (!found && !pendingAsset.Properties.contains("$Clone"))
            {
                auto soft = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(pendingAsset.ObjectPath));
                found = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            return found;
        }

        UObject* found = nullptr;
        const FName targetName(pendingAsset.Target,FNAME_Add);
        UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
            if (object && object->GetFName() == targetName && IsSupportedTarget(object))
            {
                found = object;
                return LoopAction::Break;
            }

            return LoopAction::Continue;
        });

        return found;
    }

    UObject* DragonWildsAssetModLoader::FindItemSubsystem() const
    {
        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemSubsystem"), false);
        if (!subsystemClass) return nullptr;

        TArray<UObject*> subsystems;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, subsystems, true);
        for (auto* subsystem : subsystems)
        {
            if (subsystem && !subsystem->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject)))
                return subsystem;
        }
        return nullptr;
    }

    UObject* DragonWildsAssetModLoader::CreateFromClone(
        const PendingAsset& pendingAsset, UObject* subsystem)
    {
        std::string internalName;
        std::string persistenceId;
        if (!ReadRequiredString(pendingAsset.Properties, "InternalName", internalName))
            throw std::runtime_error("$Clone requires a non-empty InternalName");
        if (!ReadRequiredString(pendingAsset.Properties, "PersistenceID", persistenceId)
            || !IsCanonicalPersistenceId(persistenceId))
            throw std::runtime_error(
                "$Clone requires a unique canonical 22-character PersistenceID");
        if (!subsystem)
            throw std::runtime_error("ItemSubsystem is not ready for clone registration");

        const auto sourceText = pendingAsset.Properties.at("$Clone").get<std::string>();
        const auto sourcePath = NormalizeObjectPath(
            RC::to_generic_string(sourceText));
        if (sourcePath.empty())
            throw std::runtime_error("$Clone source must be a full baked object path");

        UObject* source=nullptr;
        if(const auto known=m_createdAssetsByTarget.find(sourcePath);known!=m_createdAssetsByTarget.end())source=known->second;
        if(!source)source=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,sourcePath.c_str(),false);
        if (!source)
        {
            auto soft = UECustom::TSoftObjectPtr<UObject>(
                UECustom::FSoftObjectPath(sourcePath));
            source = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
        }
        if (!source || !source->GetClassPrivate())
            throw std::runtime_error("$Clone source baked asset could not be loaded");
        if (!m_itemDataClass || !source->IsA(m_itemDataClass))
            throw std::runtime_error("$Clone source must derive from ItemData");
        if(!IsReadyForPatch(source)||PS::AssetMetadata::IsIncomplete(source))
            throw std::runtime_error("$Clone source is not ready or has incomplete authored files");
        const auto sourceOrigin=PS::AssetProvenance::Lookup(source);
        if(sourceOrigin.is_object()&&!sourceOrigin.value("Registered",false))
            throw std::runtime_error("$Clone runtime source has not completed registration");

        const auto runtimePath = RC::to_generic_string(
            "/Game/RuneSchema/" + SanitizePackageSegment(
                RC::to_string(pendingAsset.ModName), "UnnamedMod")
            + "/Items/" + SanitizePackageSegment(internalName, "ITEM_RS_Unnamed")
            + "." + SanitizePackageSegment(internalName, "ITEM_RS_Unnamed"));
        const auto dot = runtimePath.rfind(TEXT('.'));
        if (dot == RC::StringType::npos)
            throw std::runtime_error("failed to form the runtime clone path");
        const auto packagePath = runtimePath.substr(0, dot);
        const auto objectName = runtimePath.substr(dot + 1);

        if (UECustom::UObjectGlobals::StaticFindObject(
                nullptr, nullptr, runtimePath.c_str(), false))
            throw std::runtime_error("the runtime clone path is already occupied");

        auto* package = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, packagePath.c_str(), false);
        if (!package)
        {
            auto* packageClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/CoreUObject.Package"), false);
            if (!packageClass)
                throw std::runtime_error("the Unreal package class is unavailable");
            FStaticConstructObjectParameters packageParams(packageClass, nullptr);
            packageParams.Name = FName(packagePath, FNAME_Add);
            packageParams.SetFlags = static_cast<EObjectFlags>(
                RF_Public | RF_Standalone | RF_Transactional);
            package = UObjectGlobals::StaticConstructObject<UObject*>(packageParams);
        }
        if (!package)
            throw std::runtime_error("failed to create the runtime clone package");
        package->SetRootSet();
        m_createdAssets.push_back(package);

        FStaticConstructObjectParameters params(source->GetClassPrivate(), package);
        params.Name = FName(objectName, FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(
            RF_Public | RF_Standalone | RF_Transactional);
        auto* created = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!created)
            throw std::runtime_error("Dragonwilds refused to construct the clone");

        // Establish ownership at the creation site, including assets created by JSON loaders.
        // Do not wait for item registration to allocate a weak-reference serial.
        PS::AssetProvenance::Record(created,pendingAsset.Properties.at("$Clone").get<std::string>(),
            RC::to_string(pendingAsset.ModName),true,false,0,pendingAsset.Properties);

        constexpr std::uint64_t unsafeFlags =
            CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient
            | CPF_InstancedReference | CPF_ContainsInstancedReference
            | CPF_Deprecated | CPF_EditorOnly;
        for (auto* property : TFieldRange<FProperty>(
                 source->GetClassPrivate(), EFieldIterationFlags::Default))
        {
            if (!property || property->HasAnyPropertyFlags(unsafeFlags)) continue;
            property->CopyCompleteValue_InContainer(created, source);
        }

        ClearItemIdentity(created, created->GetClassPrivate());

        // Existing mods keep their prior row-name policy. Live authored clones
        // opt in to inheriting the original row and never create a dangling row.
        if (!pendingAsset.Properties.value("$InheritEquipmentStats",false))
        if (auto* rowHandle = PropertyHelper::GetPropertyByName(
                created->GetClassPrivate(),
                TEXT("WearableEquipmentDataTableRowHandle")))
        {
            PropertyHelper::CopyJsonValueToContainer(
                created, rowHandle,
                nlohmann::json{{"RowName", internalName}});
        }

        created->SetRootSet();
        m_createdAssets.push_back(created);
        m_createdAssetsByTarget[pendingAsset.Target] = created;
        m_createdAssetsByTarget[runtimePath] = created;
        return created;
    }

    bool DragonWildsAssetModLoader::RegisterCreatedItem(
        UObject* item, const PendingAsset& pendingAsset, UObject* subsystem)
    {
        if (!item || !subsystem) return false;

        auto* itemClass = item->GetClassPrivate();
        auto* persistenceProperty = PropertyHelper::CastProperty<FStrProperty>(
            PropertyHelper::GetPropertyByName(itemClass, TEXT("PersistenceID")));
        auto* internalProperty = PropertyHelper::CastProperty<FStrProperty>(
            PropertyHelper::GetPropertyByName(itemClass, TEXT("InternalName")));
        if (!persistenceProperty || !internalProperty)
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': identity fields were unavailable; item was not registered.\n"),
                pendingAsset.Target);
            return false;
        }

        const auto persistenceId = persistenceProperty->GetPropertyValue(
            persistenceProperty->ContainerPtrToValuePtr<void>(item));
        const auto internalName = internalProperty->GetPropertyValue(
            internalProperty->ContainerPtrToValuePtr<void>(item));
        if (persistenceId.GetCharArray().Num() <= 1
            || internalName.GetCharArray().Num() <= 1)
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': identity fields were not written; item was not registered.\n"),
                pendingAsset.Target);
            return false;
        }

        // Record authored provenance before native registration. The startup
        // registrar uses this durable identity set to distinguish RuneSchema
        // JSON clones from cooked/base-or-PAK ItemData without relying on
        // transient UObject addresses or package-name guesses.
        PS::RegistryProvenance::AnnounceRuneSchemaItem(
            RC::to_string(RC::StringType(*persistenceId)));

        // WinGDK can retain an outgoing ItemSubsystem while the next world is
        // starting. Register the clone in every live ItemSubsystem, matching
        // DragonWildsDataRegistrar's cross-world policy, instead of trusting
        // whichever instance GetObjectsOfClass happens to return first.
        auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.ItemSubsystem"), false);
        TArray<UObject*> discovered;
        if (subsystemClass)
            UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, discovered, true);

        std::vector<UObject*> candidates;
        candidates.push_back(subsystem);
        for (auto* candidate : discovered)
        {
            if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject
                    | RF_BeginDestroyed | RF_FinishDestroyed)))
                continue;
            if (std::find(candidates.begin(), candidates.end(), candidate)
                == candidates.end())
                candidates.push_back(candidate);
        }

        size_t registeredCount = 0;
        int32_t firstNetId = -1;
        for (auto* candidate : candidates)
        {
            auto* persistenceMap = PropertyHelper::CastProperty<FMapProperty>(
                PropertyHelper::GetPropertyByName(
                    candidate->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
            auto* internalMap = PropertyHelper::CastProperty<FMapProperty>(
                PropertyHelper::GetPropertyByName(
                    candidate->GetClassPrivate(), TEXT("InternalNameToDataMap")));
            if (!persistenceMap || !internalMap) continue;

            if (MapContainsOther(persistenceMap, candidate, persistenceId, item)
                || MapContainsOther(internalMap, candidate, persistenceId, item)
                || MapContainsOther(internalMap, candidate, internalName, item))
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Clone '{}': skipped one live ItemSubsystem because its identity maps point at a different object.\n"),
                    pendingAsset.Target);
                continue;
            }

            // All peers publish startup network IDs after every clone has
            // completed identity/property construction. Interleaving TArray
            // and reverse-map growth with repeated UObject cloning can expose
            // an unsettled ItemSubsystem to its own registration
            // callbacks. Persistence/internal maps are installed now so
            // recipes can resolve outputs; DragonWildsDataRegistrar assigns
            // and verifies all network IDs in one settled startup batch. A
            // client-only early append changes its preserved prefix relative
            // to the server, despite identical final item counts/mods. Clones
            // created after this boundary still enroll immediately as before.
            const bool networkDeferred = PS::NetworkIdentityState::DeferItemEnrollment();
            const auto netId = networkDeferred
                ? -1 : EnsureItemNetworkIdentity(item, candidate);
            if (!networkDeferred && netId < 0)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Clone '{}': skipped one live ItemSubsystem because network identity registration failed.\n"),
                    pendingAsset.Target);
                continue;
            }

            const bool primaryVerified = AddMapEntry(
                persistenceMap, candidate, persistenceId, item);
            bool internalVerified = AddMapEntry(
                internalMap, candidate, persistenceId, item);
            if (internalName != persistenceId)
                internalVerified = AddMapEntry(
                    internalMap, candidate, internalName, item)
                    && internalVerified;
            if (!primaryVerified || !internalVerified)
            {
                PS::Log<LogLevel::Error>(STR(
                    "Clone '{}': native item identity maps failed post-insert round-trip verification.\n"),
                    pendingAsset.Target);
                continue;
            }

            if (!networkDeferred && firstNetId < 0) firstNetId = netId;
            if(!networkDeferred)EquipmentWearableBridge::TryRegister(candidate,item);
            ++registeredCount;
        }

        if (!registeredCount)
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR(
                "Clone '{}': no live ItemSubsystem accepted its identity/network registration.\n"),
                pendingAsset.Target);
            return false;
        }

        if (PS::Storefront::IsDedicatedServer())
            PS::Log<LogLevel::Verbose>(STR(
                "[REGISTRY][ITEM][IDENTITY-READY] source=runeschema asset='{}' PersistenceID='{}' InternalName='{}' subsystemCount={} networkId=deferred-to-settled-registrar verified=true.\n"),
                pendingAsset.Target, RC::StringType(*persistenceId),
                RC::StringType(*internalName), registeredCount);
        else
            PS::Log<LogLevel::Verbose>(STR(
                "[REGISTRY][ITEM][ADDED] source=runeschema asset='{}' PersistenceID='{}' InternalName='{}' subsystemCount={} firstNetworkId={} verified=true.\n"),
                pendingAsset.Target, RC::StringType(*persistenceId),
                RC::StringType(*internalName), registeredCount, firstNetId);
        return true;
    }

    RC::StringType DragonWildsAssetModLoader::NormalizeObjectPath(const RC::StringType& target) const
    {
        if (!target.starts_with(TEXT("/")))
        {
            return TEXT("");
        }

        auto slash = target.find_last_of(TEXT('/'));
        auto dot = target.find(TEXT('.'), slash == RC::StringType::npos ? 0 : slash + 1);
        if (dot != RC::StringType::npos)
        {
            return target;
        }

        auto assetName = target.substr(slash + 1);
        return std::format(TEXT("{}.{}"), target, assetName);
    }

    bool DragonWildsAssetModLoader::IsSupportedTarget(UObject* object) const
    {
        if (!object || !m_dataAssetClass || !m_curveBaseClass)
        {
            return false;
        }

        return object->IsA(m_dataAssetClass)
            || object->IsA(m_curveBaseClass)
            || object->GetTypedOuter(m_dataAssetClass) != nullptr;
    }

    bool DragonWildsAssetModLoader::IsReadyForPatch(UObject* object) const
    {
        if (!object)
        {
            return false;
        }

        return !object->HasAnyFlags(RF_NeedInitialization)
            && !object->HasAnyFlags(RF_NeedLoad)
            && !object->HasAnyFlags(RF_NeedPostLoad)
            && !object->HasAnyFlags(RF_NeedPostLoadSubobjects)
            && !object->HasAnyFlags(RF_BeginDestroyed)
            && !object->HasAnyFlags(RF_FinishDestroyed);
    }
}

#include "LiveCloneAuthoring.inl"
