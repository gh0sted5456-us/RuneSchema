#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PS::RuntimeWidgetEvents {

template<class MatchName>
bool Match(const nlohmann::json& rule, MatchName matchName)
{
    const auto when = rule.find("$When");
    if (when == rule.end()) return true;

    const auto matches = [&](const nlohmann::json& value) {
        const auto& expected = value.get_ref<const std::string&>();
        const auto equal = [&](std::string_view token) {
            return std::ranges::equal(expected, token, [](unsigned char a, unsigned char b) {
                return std::tolower(a) == b;
            });
        };
        return equal("any") || equal("always") || equal("resolved") || matchName(expected);
    };

    if (when->is_string()) return matches(*when);
    if (!when->is_object())
        throw std::runtime_error("Blueprint $RuntimeWidget $When must be a function name or object");

    const auto filter = when->find("Function");
    if (filter == when->end())
        throw std::runtime_error("Blueprint $RuntimeWidget $When object requires Function");
    if (filter->is_string()) return matches(*filter);
    if (!filter->is_array())
        throw std::runtime_error("Blueprint $RuntimeWidget $When Function must be a string or array");
    if (filter->empty())
        throw std::runtime_error("Blueprint $RuntimeWidget $When Function array cannot be empty");

    for (const auto& entry : *filter) {
        if (!entry.is_string())
            throw std::runtime_error("Blueprint $RuntimeWidget $When Function array must contain strings");
        if (matches(entry)) return true;
    }
    return false;
}

template<class AddName>
bool Collect(const nlohmann::json& rule, AddName addName)
{
    try {
        return Match(rule, [&](const std::string& name) {
            addName(name);
            return false;
        });
    } catch (const std::runtime_error&) {
        // Let the owner-specific matcher report malformed rules as before.
        return true;
    }
}

}
