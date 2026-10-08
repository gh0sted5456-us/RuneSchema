#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace PS::NetworkIdentityPlan {
    struct Candidate {
        std::string Identity;
        std::string AssetPath;
        std::size_t SourceIndex = 0;
    };

    inline std::vector<Candidate> Canonicalize(
        std::span<const Candidate> candidates, std::size_t preservedPrefix)
    {
        if (preservedPrefix > std::numeric_limits<std::uint16_t>::max()
            || candidates.size()
                > std::numeric_limits<std::uint16_t>::max() - preservedPrefix)
            throw std::runtime_error("Network identity registry exceeds uint16 capacity");

        std::vector<Candidate> ordered(candidates.begin(), candidates.end());
        std::ranges::sort(ordered, {}, [](const Candidate& value) {
            return std::pair{value.Identity, value.AssetPath};
        });
        std::unordered_set<std::string> identities;
        for (const auto& candidate : ordered)
        {
            if (candidate.Identity.empty() || candidate.AssetPath.empty())
                throw std::runtime_error("Network identity candidate is incomplete");
            if (!identities.emplace(candidate.Identity).second)
                throw std::runtime_error(
                    "Duplicate persistence identity in network registration plan: "
                    + candidate.Identity);
        }
        return ordered;
    }

    inline std::string Fingerprint(std::span<const std::string> orderedPaths)
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (std::size_t index = 0; index < orderedPaths.size(); ++index)
        {
            const auto row = std::to_string(index) + "=" + orderedPaths[index] + "\n";
            for (const unsigned char byte : row)
            {
                hash ^= byte;
                hash *= 1099511628211ULL;
            }
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
