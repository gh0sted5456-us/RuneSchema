#pragma once

#include "Core/ModDataRelay.h"

namespace PS::StandaloneNetworkRegistry {
using ModDataRelay::Json;

inline Json Select(const Json& document, const std::string& owner, bool appearanceOnly,
    const std::set<std::string>& registered, const Json& merged) {
    using ModDataRelay::Require;
    Require(document.is_object() && document.contains("Entries")
        && document.size() == 1 + static_cast<size_t>(document.contains("SchemaVersion"))
        && (!document.contains("SchemaVersion")
            || (document.at("SchemaVersion").is_number_integer() && document.at("SchemaVersion") == 1)),
        "Standalone network registry accepts only Entries and optional SchemaVersion 1");
    const auto& entries = document.at("Entries");
    Require(entries.is_array() && entries.size() <= 256,
        "Standalone network registry requires an array of at most 256 entries");

    std::vector<ModDataRelay::Channel> channels;
    if (!appearanceOnly) {
        for (const auto& entry : merged)
            if (entry.value("kind", std::string{}) == "DataRelay")
                channels.push_back(ModDataRelay::Declaration(entry.at("owner"), entry.at("relay")));
    }
    std::set<std::string> selectedKeys;
    Json accepted = Json::array();
    for (const auto& entry : entries) {
        Require(entry.is_object() && entry.size() == 3 && entry.contains("Id") && entry.contains("Kind"),
            "Standalone network entry must have Id, Kind and its route declaration");
        const auto kind = entry.at("Kind").get<std::string>();
        const auto id = entry.at("Id").get<std::string>();
        Require(ModDataRelay::Token(id, 96) && id.find(':') == id.npos, "Standalone network entry Id is invalid");
        const bool relay = !appearanceOnly && kind == "DataRelay" && entry.contains("Relay");
        Require(relay || (kind == "CharacterAppearance" && entry.contains("Appearance")),
            "Standalone network registry accepts only declared transport routes");
        const auto key = owner + ":" + id;
        if (registered.contains(key)) continue;
        Require(selectedKeys.insert(key).second, "Duplicate standalone network entry Id");
        if (relay) {
            const auto channel = ModDataRelay::Declaration(owner, entry.at("Relay"));
            const auto existing = std::ranges::find(channels, channel.Key, &ModDataRelay::Channel::Key);
            if (existing != channels.end()) {
                Require(*existing == channel, "Conflicting standalone relay channel registration");
                continue;
            }
            Require(channels.size() < ModDataRelay::MaxChannels, "Too many relay channels");
            channels.push_back(channel);
        }
        accepted.push_back(entry);
    }
    return {{"SchemaVersion", 1}, {"Entries", std::move(accepted)}};
}
}
