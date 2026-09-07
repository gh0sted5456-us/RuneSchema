#include "Generator/FocusedCapture.h"
#include "Generator/CaptureTraversal.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Helpers/Casting.hpp"
#include "Helpers/String.hpp"
#include <algorithm>
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;
using nlohmann::json;
namespace PS::InspectionTools {
namespace {
json AllProperties(UObject* object, CaptureBudget& budget, unsigned depth);
json Read(FProperty* field, void* address, CaptureBudget& budget, unsigned depth) {
    if (!budget.remaining || depth > budget.maxDepth) return {{"omitted", "capture budget/depth limit"}};
    --budget.remaining;
    if (!field || !address || field->GetArrayDim() != 1) return {{"omitted", "unsupported layout"}};
    if (auto* p = CastField<FBoolProperty>(field)) return p->GetPropertyValue(address);
    if (auto* p = CastField<FEnumProperty>(field)) return Read(p->GetUnderlyingProp(), address, budget, depth + 1);
    if (auto* p = CastField<FNumericProperty>(field)) {
        if (p->IsInteger()) return p->GetSignedIntPropertyValue(address);
        return p->GetFloatingPointPropertyValue(address);
    }
    if (CastField<FNameProperty>(field)) return to_string(static_cast<FName*>(address)->ToString());
    if (CastField<FStrProperty>(field)) {
        const auto& chars = static_cast<FString*>(address)->GetCharArray();
        const auto count = chars.Num();
        if (count < 0 || (count && !chars.GetData())) throw std::runtime_error("Invalid string header");
        if (!count) return "";
        const auto length = std::min<int32>(count - 1, 4096);
        const auto text = to_string(StringType(chars.GetData(), length));
        if (count - 1 > length) return {{"text", text}, {"truncated", true}};
        return text;
    }
    // Follow resolved references only; never load assets during traversal.
    if (auto* p = CastField<FObjectPropertyBase>(field)) {
        auto* object = p->GetObjectPropertyValue(address);
        if (object && budget.followReferences) return AllProperties(object, budget, depth + 1);
        return object ? json(to_string(object->GetPathName())) : json(nullptr);
    }
    if (auto* p = CastField<FStructProperty>(field)) {
        json result = json::object();
        unsigned count = 0;
        for (auto* child : TFieldRange<FProperty>(p->GetStruct().Get(), EFieldIterationFlags::Default)) {
            if (count++ >= budget.maxEntries || !budget.remaining) { result["$truncated"] = true; break; }
            if (child->GetOffset_Internal() < 0) continue;
            result[to_string(child->GetName())] = Read(child, child->ContainerPtrToValuePtr<void>(address), budget, depth + 1);
        }
        return result;
    }
    if (auto* p = CastField<FArrayProperty>(field)) {
        auto* array = static_cast<FScriptArray*>(address);
        const auto count = array->Num();
        auto* inner = p->GetInner();
        const auto stride = inner ? inner->GetElementSize() : 0;
        if (count < 0 || stride <= 0 || stride > 16384 || (count && !array->GetData()))
            throw std::runtime_error("Invalid array header or element size");
        json entries = json::array();
        for (int32 i = 0; i < std::min<int32>(count, budget.maxEntries) && budget.remaining; ++i)
            entries.push_back(Read(inner, static_cast<uint8*>(array->GetData()) + static_cast<size_t>(i) * stride, budget, depth + 1));
        const bool truncated = entries.size() != static_cast<size_t>(count);
        return {{"count", count}, {"entries", entries}, {"truncated", truncated}};
    }
    if (auto* p = CastField<FMapProperty>(field)) {
        auto* key = p->GetKeyProp();
        auto* value = p->GetValueProp();
        auto* map = static_cast<FScriptMap*>(address);
        const auto count = map->Num(), maxIndex = map->GetMaxIndex();
        if (!key || !value || count < 0 || maxIndex < count || maxIndex > static_cast<int32>(budget.maxSparseSlots)
            || key->GetSize() <= 0 || value->GetSize() <= 0 || key->GetSize() > 16384 || value->GetSize() > 16384)
            throw std::runtime_error("Invalid sparse map layout or MaxSparseSlots exceeded");
        const auto layout = FScriptMap::GetScriptLayout(key->GetSize(), key->GetMinAlignment(), value->GetSize(), value->GetMinAlignment());
        json entries = json::array();
        // Num() is a count, not the highest sparse index.
        VisitCaptureSlots(count, maxIndex, budget.maxSparseSlots, budget.maxEntries,
          [&](int32 i) { return map->IsValidIndex(i); }, [&](int32 i) {
            if (budget.remaining < 2) return false;
            auto* pair = static_cast<uint8*>(map->GetData(i, layout));
            if (!pair) throw std::runtime_error("Null sparse map pair");
            entries.push_back({{"index", i}, {"key", Read(key, pair, budget, depth + 1)},
                {"value", Read(value, pair + layout.ValueOffset, budget, depth + 1)}});
            return true;
        });
        const bool truncated = entries.size() != static_cast<size_t>(count);
        return {{"count", count}, {"maxIndex", maxIndex}, {"entries", entries}, {"truncated", truncated}};
    }
    return {{"metadataOnly", PropertyHelper::GetPropertyTypeAsUTF8String(field)}};
}
json AllProperties(UObject* object, CaptureBudget& budget, unsigned depth) {
    if (!object || !object->GetClassPrivate()) return nullptr;
    json result = {{"path", to_string(object->GetPathName())}, {"class", to_string(object->GetClassPrivate()->GetPathName())}};
    if (!budget.remaining || depth > budget.maxDepth) { result["omitted"] = "capture budget/depth limit"; return result; }
    --budget.remaining;
    if (object->IsA(UClass::StaticClass()) || object->IsA(UScriptStruct::StaticClass()) || object->IsA(UFunction::StaticClass())) {
        result["omitted"] = "type object: use existing schema inspection"; return result;
    }
    if (!budget.visited.insert(object).second) { result["alreadyVisited"] = true; return result; }
    result["properties"] = json::object();
    unsigned count = 0;
    for (auto* field : TFieldRange<FProperty>(object->GetClassPrivate(), EFieldIterationFlags::Default)) {
        if (count++ >= budget.maxEntries || !budget.remaining) { result["truncated"] = true; break; }
        const auto name = to_string(field->GetName());
        try {
            if (field->GetOffset_Internal() < 0) throw std::runtime_error("Negative property offset");
            result["properties"][name] = Read(field, field->ContainerPtrToValuePtr<void>(object), budget, depth + 1);
        } catch (const std::exception& e) { result["properties"][name] = {{"error", e.what()}}; }
    }
    return result;
}
}
json CaptureProperty(UObject* root, const json& path, CaptureBudget& budget) {
    if (!root) throw std::runtime_error("Property capture root unavailable");
    const auto rootPath = to_string(root->GetPathName());
    auto* object = root;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto name = path[i].get<std::string>();
        if (name == "*" && i + 1 == path.size())
            return {{"root", rootPath}, {"path", path}, {"value", AllProperties(object, budget, 0)}};
        auto* field = PropertyHelper::GetPropertyByName(object->GetClassPrivate(), to_wstring(name));
        if (!field || field->GetOffset_Internal() < 0 || field->GetArrayDim() != 1)
            throw std::runtime_error("Missing or unsupported property: " + name);
        auto* address = field->ContainerPtrToValuePtr<void>(object);
        if (i + 1 == path.size())
            return {{"root", rootPath}, {"owner", to_string(object->GetPathName())}, {"path", path},
                {"type", PropertyHelper::GetPropertyTypeAsUTF8String(field)}, {"value", Read(field, address, budget, 0)}};
        auto* reference = CastField<FObjectProperty>(field);
        if (!reference || field->GetElementSize() != sizeof(UObject*))
            throw std::runtime_error("Intermediate property must be a strong object reference: " + name);
        object = reference->GetObjectPropertyValue(address);
        if (!object) throw std::runtime_error("Null intermediate reference: " + name);
    }
    throw std::runtime_error("Empty property path");
}
}
