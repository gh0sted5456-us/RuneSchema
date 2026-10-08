#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

namespace PS::RegistryManifestSummary {
    using Fingerprints = std::map<std::string, std::string>;

    struct Negotiation
    {
        std::vector<std::string> Matched;
        std::vector<std::string> Different;
        std::vector<std::string> ClientOnly;
        std::vector<std::string> AuthorityOnly;

        [[nodiscard]] bool Exact() const
        {
            return Different.empty() && ClientOnly.empty()
                && AuthorityOnly.empty();
        }

        [[nodiscard]] std::string Summary() const
        {
            return "matched=" + std::to_string(Matched.size())
                + ", changed=" + std::to_string(Different.size())
                + ", client-extras=" + std::to_string(ClientOnly.size())
                + ", unavailable-locally=" + std::to_string(AuthorityOnly.size());
        }
    };

    inline std::string Fingerprint(const std::string& bytes)
    {
        std::uint64_t value = 14695981039346656037ULL;
        for (const unsigned char byte : bytes)
        {
            value ^= byte;
            value *= 1099511628211ULL;
        }
        constexpr char Hex[] = "0123456789abcdef";
        std::string result(16, '0');
        for (int offset = 15; offset >= 0; --offset)
        {
            result[static_cast<std::size_t>(offset)] = Hex[value & 0xf];
            value >>= 4;
        }
        return result;
    }

    inline Fingerprints Owners(const std::string& snapshot)
    {
        const auto manifest = nlohmann::json::parse(snapshot);
        if (!manifest.is_object() || !manifest.contains("entries")
            || !manifest.at("entries").is_array())
            throw std::runtime_error("Registry owner summary requires a manifest entries array");

        std::map<std::string, nlohmann::json> grouped;
        for (const auto& entry : manifest.at("entries"))
        {
            if (!entry.is_object())
                throw std::runtime_error("Registry owner summary found a malformed entry");
            auto owner = entry.value("owner", std::string{});
            if (owner.empty())
            {
                const auto key = entry.value("key", std::string{});
                const auto colon = key.find(':');
                if (colon != std::string::npos) owner = key.substr(0, colon);
            }
            if (owner.empty() || owner.size() > 96)
                throw std::runtime_error("Registry owner summary found an invalid owner");
            grouped[owner].push_back(entry);
        }
        if (grouped.size() > 256)
            throw std::runtime_error("Registry owner summary exceeds 256 mods");

        Fingerprints result;
        for (const auto& [owner, entries] : grouped)
            result.emplace(owner, "fnv1a64:" + Fingerprint(entries.dump()));
        return result;
    }

    inline std::string Difference(const Fingerprints& local,
        const Fingerprints& authority)
    {
        std::vector<std::string> different;
        std::vector<std::string> clientOnly;
        std::vector<std::string> authorityOnly;
        for (const auto& [owner, fingerprint] : local)
        {
            const auto found = authority.find(owner);
            if (found == authority.end()) clientOnly.push_back(owner);
            else if (found->second != fingerprint) different.push_back(owner);
        }
        for (const auto& [owner, fingerprint] : authority)
            if (!local.contains(owner)) authorityOnly.push_back(owner);

        const auto join = [](const std::vector<std::string>& values) {
            std::ostringstream out;
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index) out << ',';
                out << values[index];
            }
            return out.str();
        };
        std::vector<std::string> parts;
        if (!different.empty()) parts.push_back("different=" + join(different));
        if (!clientOnly.empty()) parts.push_back("client-only=" + join(clientOnly));
        if (!authorityOnly.empty()) parts.push_back("authority-only=" + join(authorityOnly));
        if (parts.empty()) return "match";
        return join(parts);
    }

    inline Negotiation Negotiate(const Fingerprints& client,
        const Fingerprints& authority)
    {
        Negotiation result;
        for (const auto& [owner, fingerprint] : client)
        {
            const auto found = authority.find(owner);
            if (found == authority.end()) result.ClientOnly.push_back(owner);
            else if (found->second == fingerprint) result.Matched.push_back(owner);
            else result.Different.push_back(owner);
        }
        for (const auto& [owner, fingerprint] : authority)
            if (!client.contains(owner)) result.AuthorityOnly.push_back(owner);
        return result;
    }
}
