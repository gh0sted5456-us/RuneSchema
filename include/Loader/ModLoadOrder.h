#pragma once

#include <filesystem>
#include <vector>
#include "Unreal/NameTypes.hpp"

namespace DragonWilds {
    struct ModOrderEntry { RC::StringType Name; bool Enabled = true; };

    class ModLoadOrder {
    public:
        static std::vector<RC::StringType> Resolve(
            const std::filesystem::path& modsFolderPath,
            const std::vector<RC::StringType>& discoveredModNames);
    private:
        static std::filesystem::path GetOrderPath(const std::filesystem::path& modsFolderPath);
        static std::vector<ModOrderEntry> Load(const std::filesystem::path& path,
            bool strictValues);
        static void Save(const std::filesystem::path& path,
            const std::vector<ModOrderEntry>& entries);
        static void SavePreservingComments(const std::filesystem::path& path,
            const std::vector<ModOrderEntry>& entries);
    };
}
