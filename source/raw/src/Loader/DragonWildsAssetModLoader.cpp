#include "Loader/HelpyDependencyOrder.h"
#include "Generator/HelpyStatKey.h"
#include "Generator/HelpyPropertyValue.h"
#include "Generator/QuickMenuDecorations.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "Runtime/AuthoredFile.h"
#include "Runtime/HelpyBundlePublish.h"
#include "Loader/AssetAuthoringMetadata.h"
#include "Generator/ClonePresentation.h"
#include "SDK/Helper/CookedAssetLookup.h"
#include "SDK/Helper/ItemAppearanceMetadata.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include <memory>
#include <set>
#include <algorithm>
#include "Loader/AssetProvenance.h"
#include "Loader/OwnedContentLedger.h"
#include "Runtime/HostServices.h"
#include "Utility/AssetAliases.h"
#include "Loader/ItemIdentity.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <string_view>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Engine/UDataTable.hpp"
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
#include "Loader/PlayerGhost.h"

using namespace RC;
using namespace RC::Unreal;

namespace
{
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
                || body.contains("$Clone") || body.contains("$Patch")) return false;
            found = true;
        }
        return found;
    }

    bool IsCharacterOptionPath(std::string_view path)
    {
        return (path.starts_with("CharacterOptionData[") || path.starts_with("CharacterOptions["))
            && path.ends_with("].OptionData");
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

    void AddMapEntry(FMapProperty* mapProperty, UObject* subsystem,
        const FString& key, UObject* item)
    {
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        UECustom::FManagedValue pair;
        map.InitializePair(pair);
        *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
        std::memcpy(map.GetValuePtr(pair.GetData()), &item, sizeof(item));
        map.Add(pair);
        map.Rehash();
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
        RegisterCharacterMenuPatchReplay();
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

        try { RegisterDeclarations(data,modName); }
        catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR("[SAVE-CLEANER][DECLARATION][MOD:{}] Declaration rejected; ordinary asset entries continue: {}.\n"),
                modName,PS::ToWideSafe(error.what()));
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
            // Snapshot the authored identity during the cheap JSON pass.  Clone
            // application can legitimately be deferred until GameInstanceInit;
            // save cleanup must still know that this active mod owns the item
            // before character deserialization begins.
            if(!isPatch && pending.Properties.contains("$Clone")
                && pending.Properties.contains("PersistenceID")
                && pending.Properties.at("PersistenceID").is_string()
                && pending.Properties.contains("InternalName")
                && pending.Properties.at("InternalName").is_string()) {
                const auto source=pending.Properties.at("$Clone").get<std::string>();
                const auto destination=RC::to_string(pending.ObjectPath);
                if(OwnedContent::LooksLikeItemClone(destination,source)) {
                    try {
                        OwnedContent::Record owned{"Item",RC::to_string(modName),
                            pending.Properties.at("PersistenceID").get<std::string>(),
                            pending.Properties.at("InternalName").get<std::string>(),destination};
                        OwnedContent::Validate(owned);
                        OwnedContent::Merge(OwnedContent::LedgerPath(
                            PS::HostServices::StateDirectory()),{owned});
                    } catch(const std::exception& error) {
                        PS::Log<LogLevel::Error>(STR("[SAVE-CLEANER][MOD:{}] Clone identity '{}' was not tracked: {}.\n"),
                            modName,pending.ObjectPath,PS::ToWideSafe(error.what()));
                    }
                }
            }
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
                    PropertyHelper::CopyJsonValueToContainer(operation.Member.Container,
                        operation.Member.Property,operation.Body.at("Value"));
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
                    const auto characterOption = operation.Member.CanonicalPath.starts_with(
                        "CharacterOptionData[ECharacterOptionType::")
                        && operation.Member.CanonicalPath.ends_with("].OptionData");
                    if (operation.Op=="AppendUnique" && characterOption)
                        ValidateCharacterOptionHandle(operation.Body);
                    const auto outcome=AppendValues(operation.Member,operation.Body,
                        operation.Op=="AppendUnique");
                    writes += outcome.Added;
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
                int before=-1,after=-1,added=0,existing=0,verified=0;
                for (const auto& patch : committedClassDefaults)
                {
                    if (patch.ObjectPath!=seed.ObjectPath) continue;
                    for (const auto& operation : patch.Operations)
                    {
                        if (operation.value("Op",std::string{})!="AppendUnique") continue;
                        const auto member=ResolveMember(instance,instance->GetClassPrivate(),nullptr,
                            operation.at("Path").get<std::string>());
                        if (member.CanonicalPath.starts_with("CharacterOptionData[ECharacterOptionType::")
                            && member.CanonicalPath.ends_with("].OptionData"))
                            ValidateCharacterOptionHandle(operation);
                        const auto outcome=AppendValues(member,operation,true);
                        if(before<0)before=outcome.Before;after=outcome.After;
                        added+=outcome.Added;existing+=outcome.Existing;++verified;
                    }
                }
                if (verified)
                    PS::Log<LogLevel::Normal>(STR("[CHARACTER-OPTIONS][PROPAGATED] object={} class={} count={}->{} added={} existing={} verified={} missing=0.\n"),
                        instance->GetPathName(),instance->GetClassPrivate()->GetPathName(),before,after,
                        added,existing,verified);
            }
            catch(const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(STR("[LOADER:assets][PARTIAL][CHARACTER-MENU] Existing menu object={} was not updated: {}. New menus still use the class default.\n"),
                    instance?instance->GetPathName():STR("<null>"),PS::ToWideSafe(error.what()));
            }
        }
    }

    void DragonWildsAssetModLoader::RegisterDeclarations(const nlohmann::json& data,const RC::StringType& modName)
    {
        auto declarations=OwnedContent::Declarations(data,RC::to_string(modName));
        std::vector<UObject*> verifiedObjects;
        verifiedObjects.reserve(declarations.size());
        for(auto& declaration:declarations)
        {
            const auto path=RC::to_generic_string(declaration.Source);
            auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path.c_str(),false);
            if(!object) {
                auto soft=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(path));
                object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            auto* expected=declaration.Kind=="Item"?m_itemDataClass:m_recipeDataClass;
            if(!object || !expected || !object->IsA(expected))
                throw std::runtime_error(declaration.Kind+" declaration did not resolve to the expected cooked asset class: "+declaration.Source);
            auto* idProperty=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("PersistenceID")));
            auto* nameProperty=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("InternalName")));
            if(!idProperty || !nameProperty)throw std::runtime_error("Declared cooked asset is missing PersistenceID or InternalName");
            const auto actualId=RC::to_string(*idProperty->GetPropertyValue(idProperty->ContainerPtrToValuePtr<void>(object)));
            const auto actualName=RC::to_string(*nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(object)));
            if(actualId!=declaration.PersistenceID)
                throw std::runtime_error("Declared PersistenceID does not match the cooked asset: "+declaration.Source);
            if(actualName.empty())throw std::runtime_error("Declared cooked asset has an empty InternalName: "+declaration.Source);
            // An omitted InternalName is represented by the path-derived object name.
            // Accept that convenience value, but reject any explicit conflicting value.
            const auto& raw=data.at("$declaration");
            bool explicitName=false;
            if(raw.is_object())explicitName=raw.contains("InternalName");
            else for(const auto& row:raw)if(row.is_object() && row.value("Path",std::string{})==declaration.Source)explicitName=row.contains("InternalName");
            if(explicitName && declaration.InternalName!=actualName)
                throw std::runtime_error("Declared InternalName does not match the cooked asset: "+declaration.Source);
            declaration.InternalName=actualName;
            verifiedObjects.push_back(object);
        }
        if(!declarations.empty()) {
            OwnedContent::Merge(OwnedContent::LedgerPath(PS::HostServices::StateDirectory()),declarations);
            for(std::size_t index=0;index<declarations.size();++index) {
                verifiedObjects[index]->SetRootSet();
                OwnedContent::RegisterActiveDeclarationPath(declarations[index].Source);
            }
            PS::Log<LogLevel::Normal>(STR("[SAVE-CLEANER][DECLARATION][MOD:{}] Verified and recorded {} cooked persistent asset declaration(s).\n"),
                modName,declarations.size());
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
        for (auto& [propertyName, propertyValue] : pendingAsset.Properties.items())
        {
            if (propertyName == "$Append" || propertyName == "$Clone" || propertyName == "$InheritEquipmentStats" || PS::AssetMetadata::IsKey(propertyName))
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
                PropertyHelper::CopyJsonValueToContainer(object, property, propertyValue);
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
            try {PS::AssetMetadata::Record(object,pendingAsset.Metadata,RC::to_string(pendingAsset.ModName),pendingAsset.InstalledDefinition);}
            catch(const std::exception& e){++outResult.ErrorCount;PS::Log<LogLevel::Error>(STR("Asset metadata: {}\n"),PS::ToWideSafe(e.what()));}
        }
        if (outResult.ErrorCount > errorsBefore) {
            PS::Log<LogLevel::Error>(STR("[{}] Asset edit incomplete: {} successful field writes, {} errors. Changes are not rolled back; inspect field errors and restart after correcting the mod.\n"),
                object->GetPathName(), outResult.PropertiesWritten - writesBefore,
                outResult.ErrorCount - errorsBefore);
        }
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
        for (auto& [modName, result] : batchResults)
        {
            if (result.Created || result.ClonesUpdated)
                PS::RoutineLog("assets", STR("{} $Clone: {} new, {} updated.\n"),
                    modName, result.Created, result.ClonesUpdated);
            if (result.Patched)
                PS::RoutineLog("patches", STR("{} $Patch: {} updated.\n"), modName, result.Patched);
            if (result.ErrorCount)
                PS::Log<LogLevel::Warning>(STR("{} assets: {} updated, {} properties written, {} errors.\n"),
                    modName, result.TargetsUpdated, result.PropertiesWritten, result.ErrorCount);
            else if (result.TargetsUpdated)
                PS::RoutineLog("assets", STR("{} assets: {} updated, 0 errors.\n"), modName, result.TargetsUpdated);
        }
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
        std::size_t copied = 0;
        RC::StringType inheritedUnlockFields;
        for (auto* property : TFieldRange<FProperty>(
                 source->GetClassPrivate(), EFieldIterationFlags::Default))
        {
            if (!property || property->HasAnyPropertyFlags(unsafeFlags)) continue;
            property->CopyCompleteValue_InContainer(created, source);
            ++copied;
            if (IsUnlockableAssetField(RC::to_string(property->GetName()))) {
                if (!inheritedUnlockFields.empty()) inheritedUnlockFields += TEXT(", ");
                inheritedUnlockFields += property->GetName();
            }
        }

        ClearItemIdentity(created, created->GetClassPrivate());

        if (!inheritedUnlockFields.empty())
            PS::Log<LogLevel::Verbose>(STR("{}: clone inherited unlockable field(s): {}. Explicit asset fields can replace them.\n"),
                pendingAsset.ModName, inheritedUnlockFields);

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
        PS::Log<LogLevel::Verbose>(
            STR("{}: cloned baked item '{}' to new runtime item '{}' using {} reflected properties.\n"),
            pendingAsset.ModName, source->GetPathName(), runtimePath, copied);
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
        auto* persistenceMap = PropertyHelper::CastProperty<FMapProperty>(
            PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("PersistenceIDToDataMap")));
        auto* internalMap = PropertyHelper::CastProperty<FMapProperty>(
            PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("InternalNameToDataMap")));
        if (!persistenceProperty || !internalProperty
            || !persistenceMap || !internalMap)
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': ItemSubsystem identity maps were unavailable; item was not registered.\n"),
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
        if (MapContainsOther(persistenceMap, subsystem, persistenceId, item)
            || MapContainsOther(internalMap, subsystem, persistenceId, item)
            || MapContainsOther(internalMap, subsystem, internalName, item))
        {
            ClearItemIdentity(item, itemClass);
            PS::Log<LogLevel::Error>(STR("Clone '{}': PersistenceID or InternalName collides with another item; clone was not registered.\n"),
                pendingAsset.Target);
            return false;
        }

        AddMapEntry(persistenceMap, subsystem, persistenceId, item);
        AddMapEntry(internalMap, subsystem, persistenceId, item);
        if (internalName != persistenceId)
            AddMapEntry(internalMap, subsystem, internalName, item);

        PS::Log<LogLevel::Verbose>(STR("Clone '{}': registered new item identity '{}' / '{}'.\n"),
            pendingAsset.Target, RC::StringType(*persistenceId),
            RC::StringType(*internalName));
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
