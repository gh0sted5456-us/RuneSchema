#pragma once

#include <map>
#include <string>
#include <vector>
#include "Core/JsonLoadOrderMerge.h"

namespace DragonWilds {
    // Authoring-time provenance only; never retains game objects or runs on tick.
    class PatchConflicts {
    public:
        struct Conflict { std::string Field, Earlier, Later; };
        std::vector<Conflict> Record(const std::string& record, const nlohmann::json& changes,
            const std::string& source, bool keyedArrays = true) {
            std::vector<std::string> paths;
            Collect(changes, "", keyedArrays, paths);
            std::vector<Conflict> conflicts;
            auto& fields = m_records[record];
            const auto writer = source + " (patch #" + std::to_string(++m_sequence) + ")";
            for (const auto& path : paths) {
                for (auto it = fields.begin(); it != fields.end();) {
                    if (Overlaps(path, it->first)) {
                        if (it->second != writer) conflicts.push_back({path, it->second, writer});
                        // A child write does not remove ownership of the rest of its parent.
                        if (it->first == path || it->first.starts_with(path + "/")) {
                            it = fields.erase(it);
                            continue;
                        }
                    }
                    ++it;
                }
                fields[path] = writer;
            }
            return conflicts;
        }
        void Clear() { m_records.clear(); m_sequence = 0; }
    private:
        std::map<std::string, std::map<std::string, std::string>> m_records;
        size_t m_sequence = 0;
        static bool Overlaps(const std::string& a, const std::string& b) {
            return a == b || a.starts_with(b + "/") || b.starts_with(a + "/");
        }
        static std::string Escape(std::string text) {
            std::string escaped;
            for (char c : text) {
                if (c == '~') escaped += "~0";
                else if (c == '/') escaped += "~1";
                else escaped += c;
            }
            return escaped;
        }
        static void Collect(const nlohmann::json& value, const std::string& path,
            bool keyed, std::vector<std::string>& paths) {
            if (value.is_object() && value.size() == 1 && value.contains("$Patch")
                && value.at("$Patch").is_array()) {
                for (const auto& edit : value.at("$Patch")) {
                    if (!edit.is_object() || !edit.contains("$Target")) continue;
                    const auto selector = edit.contains("$Index") ? "$Index" : "$Match";
                    if (!edit.contains(selector)) continue;
                    Collect(edit.at("$Target"), path + "/" + Escape(std::string(selector) + "=" + edit.at(selector).dump()), keyed, paths);
                }
                return;
            }
            if (value.is_object()) {
                for (const auto& [key, child] : value.items()) {
                    if (key == "$Comment" || key == "$Append") continue;
                    Collect(child, path + "/" + Escape(key), keyed, paths);
                }
                return;
            }
            if (keyed && value.is_array() && !value.empty()) {
                bool identified = true;
                for (const auto& row : value) if (!JsonLoadOrderMerge::LineItemIdentity(row)) { identified = false; break; }
                if (identified) {
                    for (const auto& row : value) {
                        const auto identity = *JsonLoadOrderMerge::LineItemIdentity(row);
                        // Identity fields select a line item; they are not conflicting writes.
                        auto payload = row;
                        payload.erase(identity.substr(0, identity.find(':')));
                        if (!payload.empty()) Collect(payload, path + "/" + Escape(identity), keyed, paths);
                    }
                    return;
                }
            }
            if (!path.empty()) paths.push_back(path);
        }
    };
}
