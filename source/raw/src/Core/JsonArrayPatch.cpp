#include "Core/JsonArrayPatch.h"
#include <limits>
#include <stdexcept>

namespace DragonWilds::JsonArrayPatch {
    int Select(const Edit& edit, int count, const std::function<bool(int)>& matches) {
        if (edit.Index) {
            if (*edit.Index < 0 || *edit.Index >= count)
                throw std::runtime_error("Array $Index is out of range; no entries changed");
            return *edit.Index;
        }
        int selected = -1;
        for (int index = 0; index < count; ++index) {
            if (!matches(index)) continue;
            if (selected != -1) throw std::runtime_error("Array $Match is ambiguous; no entries changed");
            selected = index;
        }
        if (selected == -1) throw std::runtime_error("Array $Match found no entry; no entries changed");
        return selected;
    }

    std::vector<Edit> Parse(const nlohmann::json& value) {
        if (!value.is_object() || value.size() != 1 || !value.contains("$Patch")
            || !value.at("$Patch").is_array() || value.at("$Patch").empty())
            throw std::runtime_error("Array patch must contain only a non-empty $Patch array");
        std::vector<Edit> edits;
        for (const auto& item : value.at("$Patch")) {
            if (!item.is_object() || item.size() != 2 || !item.contains("$Target")
                || !item.at("$Target").is_object() || item.at("$Target").empty()
                || item.contains("$Index") == item.contains("$Match"))
                throw std::runtime_error("Array edit requires exactly one $Index or $Match and a non-empty $Target object");
            Edit edit{};
            edit.Target = item.at("$Target");
            if (item.contains("$Index")) {
                const auto& index = item.at("$Index");
                if (!index.is_number_integer() || index < 0 || index > std::numeric_limits<int>::max())
                    throw std::runtime_error("Array $Index must be a non-negative int32");
                edit.Index = index.get<int>();
            } else {
                edit.Match = item.at("$Match");
                if (!edit.Match.is_object() || edit.Match.empty())
                    throw std::runtime_error("Array $Match must be a non-empty field object");
            }
            edits.push_back(std::move(edit));
        }
        return edits;
    }
}
