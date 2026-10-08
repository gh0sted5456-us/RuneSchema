#pragma once

#include <HAL/Platform.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include "Utility/Config.h"
#include <string_view>
#include <utility>
#include <cstddef>

namespace PS {
    inline auto ToWideSafe(const char* text) -> RC::StringType
    {
        RC::StringType wide;
        if (!text)
        {
            return wide;
        }

        for (auto* byte = reinterpret_cast<const unsigned char*>(text); *byte; ++byte)
        {
            wide.push_back(*byte < 0x80 ? static_cast<RC::CharType>(*byte) : STR('?'));
        }
        return wide;
    }

    inline auto ToWideSafe(std::string_view text) -> RC::StringType
    {
        RC::StringType wide;
        wide.reserve(text.size());
        for (const auto byte : text)
            wide.push_back(static_cast<unsigned char>(byte) < 0x80
                ? static_cast<RC::CharType>(static_cast<unsigned char>(byte))
                : STR('?'));
        return wide;
    }

    template <RC::Unreal::int32 optional_arg, typename... FmtArgs>
    auto Log(RC::File::StringViewType content, FmtArgs&&... fmt_args) -> void
    {
        if (optional_arg == RC::LogLevel::Error)
        {
            auto formatted_log = std::format(STR("[RuneSchema] [error] {}"), content);
            RC::Output::send<optional_arg>(formatted_log, std::forward<FmtArgs>(fmt_args)...);
        }
        else if (optional_arg == RC::LogLevel::Warning)
        {
            auto formatted_log = std::format(STR("[RuneSchema] [warning] {}"), content);
            RC::Output::send<optional_arg>(formatted_log, std::forward<FmtArgs>(fmt_args)...);
        }
        else if (optional_arg == RC::LogLevel::Verbose)
        {
            auto config = PS::PSConfig::Get();
            if (!config->IsDebugLoggingEnabled()) return;

            auto formatted_log = std::format(STR("[RuneSchema] [diagnostic] {}"), content);
            RC::Output::send<RC::LogLevel::Normal>(formatted_log, std::forward<FmtArgs>(fmt_args)...);
        }
        else
        {
            if (!PS::PSConfig::Get()->IsDebugLoggingEnabled()) return;
            auto formatted_log = std::format(STR("[RuneSchema] {}"), content);
            RC::Output::send<optional_arg>(formatted_log, std::forward<FmtArgs>(fmt_args)...);
        }
    }

    template <typename... FmtArgs>
    auto RoutineLog(std::string_view channel, RC::File::StringViewType content,
        FmtArgs&&... fmt_args) -> void
    {
        if (!PS::PSConfig::Get()->IsRoutineNotificationEnabled(channel)) return;
        RC::StringType decorated = STR("[loader=");
        decorated += ToWideSafe(channel);
        decorated += STR("] ");
        decorated.append(content.data(), content.size());
        Log<RC::LogLevel::Normal>(decorated,
            std::forward<FmtArgs>(fmt_args)...);
    }

    // One stable, always-visible row per completed loader pass. Individual
    // objects and property writes remain diagnostic-only.
    inline void LoaderSummary(std::string_view loader, std::size_t loaded,
        std::size_t created, std::size_t updated, std::size_t placed,
        std::size_t errors)
    {
        RC::Output::send<RC::LogLevel::Normal>(
            STR("[RuneSchema] [summary] {} | loaded={} | created={} | updated={} | placed={} | errors={}\n"),
            ToWideSafe(loader), loaded, created, updated, placed, errors);
    }
}
