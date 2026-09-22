#pragma once
#include "Utility/Logging.h"
#include <Windows.h>
#include <atomic>

namespace PS::UE4SSCompatibility {
    inline bool HasHashTableApi()
    {
        const auto host = GetModuleHandleW(L"UE4SS.dll");
        return host && GetProcAddress(host, "?ClearGlobalObjectCache@Unreal@RC@@YAXXZ") != nullptr;
    }

    inline const char* DetectedBuild()
    {
        return HasHashTableApi() ? "UE4SS hash-table API line" : "UE4SS classic API line";
    }

    inline bool MatchesTarget() { return HasHashTableApi(); }

    inline void Report()
    {
        static std::atomic_flag reported = ATOMIC_FLAG_INIT;
        if (reported.test_and_set()) return;

        const auto host = GetModuleHandleW(L"UE4SS.dll");
        if (!host)
        {
            PS::Log<RC::LogLevel::Warning>(STR("UE4SS compatibility: host module could not be inspected.\n"));
            return;
        }

        // The hash-table object cache was added after the 946 line. Detect a
        // capability instead of binding RuneSchema behavior to a file version.
        const bool hashTables = HasHashTableApi();
        RC::Output::send<RC::LogLevel::Normal>(
            STR("[RuneSchema] UE4SS compatibility: host={} | weak-serial=local-safe | lifecycle=InitGameState\n"),
            hashTables ? STR("hash-table line") : STR("classic line"));
    }
}
