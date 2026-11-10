#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace PS::NetworkIdentityPlan {
    inline constexpr std::size_t Capacity =
        std::numeric_limits<std::uint16_t>::max();

    struct Candidate {
        std::string Identity;
        std::string AssetPath;
        std::size_t SourceIndex = 0;
        // Supplied from the audited network object's membership, not merely
        // its presence in the persistence-ID map. Both registries can need repair.
        bool NetworkRegistered = false;
    };

    inline std::vector<Candidate> Canonicalize(
        std::span<const Candidate> candidates, std::size_t preservedPrefix)
    {
        if (preservedPrefix > Capacity)
            throw std::runtime_error("Network identity registry exceeds uint16 capacity");

        std::vector<Candidate> ordered(candidates.begin(), candidates.end());
        std::ranges::sort(ordered, {}, [](const Candidate& value) {
            return std::tie(value.Identity, value.AssetPath);
        });
        std::string_view previousIdentity;
        std::size_t additions = 0;
        std::size_t reused = 0;
        for (const auto& candidate : ordered)
        {
            if (candidate.Identity.empty() || candidate.AssetPath.empty())
                throw std::runtime_error("Network identity candidate is incomplete");
            if (candidate.Identity == previousIdentity)
                throw std::runtime_error(
                    "Duplicate persistence identity in network registration plan: "
                    + candidate.Identity);
            previousIdentity = candidate.Identity;
            if (candidate.NetworkRegistered) ++reused;
            else ++additions;
        }
        if (reused > preservedPrefix)
            throw std::runtime_error(
                "Reused network identity count exceeds the audited registry prefix");
        // Discovery includes already registered assets on every pass. Keep them
        // in the plan for validation/repair, but never reserve another ID for them.
        if (additions > Capacity - preservedPrefix)
            throw std::runtime_error(
                "Network identity registry exceeds uint16 capacity (existing="
                + std::to_string(preservedPrefix) + ", new="
                + std::to_string(additions) + ", maximum="
                + std::to_string(Capacity) + ")");
        return ordered;
    }

    inline std::string Fingerprint(std::span<const std::string> orderedPaths)
    {
        std::uint64_t hash = 14695981039346656037ULL;
        const auto append = [&](std::string_view bytes) {
            for (const unsigned char byte : bytes)
            {
                hash ^= byte;
                hash *= 1099511628211ULL;
            }
        };
        for (std::size_t index = 0; index < orderedPaths.size(); ++index)
        {
            char indexText[std::numeric_limits<std::size_t>::digits10 + 1];
            const auto end = std::to_chars(indexText, indexText + sizeof(indexText), index).ptr;
            append(std::string_view(indexText, end));
            append("=");
            append(orderedPaths[index]);
            append("\n");
        }
        constexpr char Hex[] = "0123456789abcdef";
        std::string result(16, '0');
        for (int offset = 15; offset >= 0; --offset)
        {
            result[static_cast<std::size_t>(offset)] = Hex[hash & 0xf];
            hash >>= 4;
        }
        return result;
    }
}
