#pragma once
#include "Runtime/RuneSchemaPluginApi.h"

namespace PS::PluginRuntimeServices {
    inline const RuneSchemaHostApi* Host = nullptr;
    inline void Set(const RuneSchemaHostApi* host) noexcept { Host = host; }
    inline void Reset() noexcept { Host = nullptr; }
}
