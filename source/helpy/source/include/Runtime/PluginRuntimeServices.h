#pragma once
#include "Runtime/RuneSchemaPluginApi.h"
#include <atomic>

namespace PS::PluginRuntimeServices {
    inline const RuneSchemaHostApi* Host = nullptr;
    inline bool ExternalDesktop=false;
    inline bool StartHidden=false;
    inline std::atomic<bool> ProfileChanged=false;
    inline std::atomic<uint32_t> GameProcessId=0;
    inline std::atomic<bool> ConnectionChanged=false;
    inline void Set(const RuneSchemaHostApi* host) noexcept { Host = host; }
    inline void Reset() noexcept { Host = nullptr; }
}
