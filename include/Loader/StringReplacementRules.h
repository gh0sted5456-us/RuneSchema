#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace DragonWilds::StringReplacementRules {
template<class Entry, class Text, class Owner>
inline void Replace(Entry& entry, const Text& text, const Owner& owner) {
    entry.To = text;
    entry.Owner = owner;
    entry.Matched = false;
}

inline std::string Read(const nlohmann::json& value) {
    if (value.is_string()) {
        auto text = value.get<std::string>();
        if (!text.empty()) return text;
    } else if (value.is_array() && !value.empty()) {
        std::string text;
        bool first = true;
        for (const auto& line : value) {
            if (!line.is_string()) throw std::runtime_error("replacement lines must be strings");
            if (!first) text += "\r\n";
            first = false;
            text += line.get_ref<const std::string&>();
        }
        if (!text.empty()) return text;
    }
    throw std::runtime_error("replacement must be a non-empty string or string array; blanking text is unsupported");
}

inline void Validate(const nlohmann::json& document) {
    if (!document.is_object()) throw std::runtime_error("strings document must be an object");
    const auto validate = [](const std::string& scope, const std::string& source, const nlohmann::json& value) {
        try {
            if (source.empty()) throw std::runtime_error("source text must not be empty");
            (void)Read(value);
        } catch (const std::exception& error) {
            throw std::runtime_error("strings " + scope + "/" + source + ": " + error.what());
        }
    };
    for (const auto& [key, value] : document.items()) {
        if (value.is_object()) {
            if (key.empty()) throw std::runtime_error("string-table scope must not be empty");
            for (const auto& [source, replacement] : value.items()) validate(key, source, replacement);
        } else validate("global", key, value);
    }
}
}
