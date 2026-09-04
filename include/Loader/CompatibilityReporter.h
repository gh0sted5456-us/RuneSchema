#pragma once

#include <filesystem>
#include <vector>

#include "Unreal/NameTypes.hpp"

namespace DragonWilds {
    class CompatibilityReporter {
    public:
        static void Generate(
            const std::filesystem::path& modsFolderPath,
            const std::vector<RC::StringType>& orderedModNames);
    };
}
