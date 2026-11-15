#pragma once
// Per-field state for the RuneSchema settings editor. The core snapshot's "Editable" map is built from
// SettingsDocument(PSConfigSettings{}), so each entry is that field's default value as well as its type.
#include <nlohmann/json.hpp>
#include <string>

namespace PS::SettingsFieldState {
struct State { bool Unsaved = false, NonDefault = false; };
inline const nlohmann::json* At(const nlohmann::json& document, const std::string& path) {
    const auto pointer = nlohmann::json::json_pointer(path);
    return document.is_object() && document.contains(pointer) ? &document.at(pointer) : nullptr;
}
// Unsaved: the draft differs from what is on disk. NonDefault: the draft differs from the shipped default.
inline State For(const nlohmann::json& snapshot, const nlohmann::json& draft, const std::string& path) {
    State state;
    const auto editable = snapshot.find("Editable");
    if (editable == snapshot.end() || !editable->contains(path)) return state;
    const auto* current = At(draft, path);
    if (!current) return state;
    const auto document = snapshot.find("Document");
    const auto* saved = document == snapshot.end() ? nullptr : At(*document, path);
    state.Unsaved = !saved || *saved != *current;
    state.NonDefault = editable->at(path) != *current;
    return state;
}
inline size_t CountNonDefault(const nlohmann::json& snapshot, const nlohmann::json& draft) {
    size_t count = 0;
    if (const auto editable = snapshot.find("Editable"); editable != snapshot.end() && editable->is_object())
        for (const auto& [path, value] : editable->items()) count += For(snapshot, draft, path).NonDefault ? 1 : 0;
    return count;
}
// Short display text for a default value. Secrets and long values are never echoed.
inline std::string DefaultText(const nlohmann::json& value, bool secret) {
    if (secret) return "hidden";
    if (value.is_boolean()) return value.get<bool>() ? "On" : "Off";
    if (value.is_string()) { const auto& text = value.get_ref<const std::string&>(); return text.empty() ? "empty" : text.size() > 48 ? text.substr(0, 45) + "..." : text; }
    if (value.is_array() && value.empty()) return "empty list";
    auto text = value.dump();
    return text.size() > 48 ? text.substr(0, 45) + "..." : text;
}
}
