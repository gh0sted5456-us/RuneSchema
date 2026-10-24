#pragma once
#include <filesystem>
#include "Core/RuneSchemaDataRoot.h"

namespace PS::RuntimeLayout {
    inline void Initialize(const std::filesystem::path& runtime) {
        for(const auto* name:{"progress","references"}){
            const auto path=runtime/"saved"/name;
            RuneSchemaData::Safe(path);std::filesystem::create_directories(path);
        }
    }
}
