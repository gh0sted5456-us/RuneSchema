#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>

namespace PS::CookedPakRegistryManifest {
    struct Lane {
        std::string Name;
        std::vector<std::string> Paths;
    };

    struct Manifest {
        std::string Owner;
        std::string Source;
        std::vector<Lane> AssetLanes;
        std::vector<std::string> MeleeAttackClasses;
        std::vector<std::string> RangedAttackClasses;
        std::uint64_t Fingerprint = 0;
    };

    namespace Detail {
        inline std::mutex Mutex;
        inline std::vector<Manifest> Manifests;

        inline bool ValidAssetPath(const std::string& value)
        {
            if (value.empty() || value.size() > 1024 || value.front() != '/'
                || value.find("..") != std::string::npos
                || value.find_first_of("\r\n\t") != std::string::npos)
                return false;
            const auto mountEnd = value.find('/', 1);
            if (mountEnd == std::string::npos || mountEnd == 1) return false;
            const auto mount = value.substr(1, mountEnd - 1);
            if (mount == "Script" || mount == "Engine") return false;
            return std::all_of(mount.begin(), mount.end(), [](unsigned char c) {
                return std::isalnum(c) || c == '_' || c == '-' || c == '.';
            });
        }

        inline void Hash(std::uint64_t& value, std::string_view text)
        {
            constexpr std::uint64_t Prime = 1099511628211ull;
            for (const auto byte : text)
            {
                value ^= static_cast<unsigned char>(byte);
                value *= Prime;
            }
            value ^= 0xff;
            value *= Prime;
        }

        inline std::vector<std::string> ReadPaths(const nlohmann::json& root,
            const char* field, std::size_t& total)
        {
            if (!root.contains(field)) return {};
            const auto& value = root.at(field);
            if (!value.is_array() || value.size() > 4096)
                throw std::runtime_error(std::string("NativeRegistries.") + field
                    + " must be an array with at most 4096 cooked asset paths");
            std::vector<std::string> result;
            result.reserve(value.size());
            for (const auto& row : value)
            {
                if (!row.is_string() || !ValidAssetPath(row.get<std::string>()))
                    throw std::runtime_error(std::string("NativeRegistries.") + field
                        + " contains an invalid cooked asset path");
                auto path = row.get<std::string>();
                if (std::ranges::find(result, path) != result.end())
                    throw std::runtime_error(std::string("NativeRegistries.") + field
                        + " contains a duplicate cooked asset path");
                result.push_back(std::move(path));
                if (++total > 16384)
                    throw std::runtime_error(
                        "NativeRegistries exceeds the 16384-path document limit");
            }
            return result;
        }
    }

    inline bool Publish(const nlohmann::json& document, const std::string& owner,
        const std::string& source, Manifest* published = nullptr)
    {
        if (!document.is_object() || !document.contains("NativeRegistries"))
            return false;
        const auto& native = document.at("NativeRegistries");
        if (!native.is_object())
            throw std::runtime_error("NativeRegistries must be an object");
        static constexpr std::array<const char*, 8> Allowed{{
            "Items", "Recipes", "Quests", "CombatSpells", "UtilitySpells",
            "EquipmentEffects", "MeleeAttackClasses", "RangedAttackClasses"}};
        for (const auto& [key, unused] : native.items())
        {
            (void)unused;
            if (std::ranges::find(Allowed, key) == Allowed.end())
                throw std::runtime_error("NativeRegistries contains unsupported lane '"
                    + key + "'");
        }

        Manifest result;
        result.Owner = owner;
        result.Source = source;
        std::size_t total = 0;
        for (const auto* lane : {"Items", "Recipes", "Quests", "CombatSpells",
            "UtilitySpells", "EquipmentEffects"})
        {
            auto paths = Detail::ReadPaths(native, lane, total);
            if (!paths.empty()) result.AssetLanes.push_back({lane, std::move(paths)});
        }
        result.MeleeAttackClasses = Detail::ReadPaths(
            native, "MeleeAttackClasses", total);
        result.RangedAttackClasses = Detail::ReadPaths(
            native, "RangedAttackClasses", total);
        if (!total)
            throw std::runtime_error("NativeRegistries must declare at least one path");

        std::uint64_t hash = 14695981039346656037ull;
        Detail::Hash(hash, owner);
        for (const auto& lane : result.AssetLanes)
        {
            Detail::Hash(hash, lane.Name);
            for (const auto& path : lane.Paths) Detail::Hash(hash, path);
        }
        Detail::Hash(hash, "MeleeAttackClasses");
        for (const auto& path : result.MeleeAttackClasses) Detail::Hash(hash, path);
        Detail::Hash(hash, "RangedAttackClasses");
        for (const auto& path : result.RangedAttackClasses) Detail::Hash(hash, path);
        result.Fingerprint = hash;

        std::scoped_lock lock(Detail::Mutex);
        auto existing = std::ranges::find_if(Detail::Manifests,
            [&](const Manifest& value) {
                return value.Owner == owner && value.Source == source;
            });
        if (existing == Detail::Manifests.end()) Detail::Manifests.push_back(result);
        else *existing = result;
        if (published) *published = result;
        return true;
    }

    inline std::vector<Manifest> Snapshot()
    {
        std::scoped_lock lock(Detail::Mutex);
        return Detail::Manifests;
    }

    inline void Reset()
    {
        std::scoped_lock lock(Detail::Mutex);
        Detail::Manifests.clear();
    }
}
