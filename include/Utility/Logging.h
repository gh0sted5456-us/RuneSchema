#pragma once

#include <HAL/Platform.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include "Utility/Config.h"
#include <string_view>
#include <utility>

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

            auto formatted_log = std::format(STR("[RuneSchema] [debug] {}"), content);
            RC::Output::send<optional_arg>(formatted_log, std::forward<FmtArgs>(fmt_args)...);
        }
        else
        {
            auto formatted_log = std::format(STR("[RuneSchema] {}"), content);
            RC::Output::send<optional_arg>(formatted_log, std::forward<FmtArgs>(fmt_args)...);
        }
    }

    template <typename... FmtArgs>
    auto RoutineLog(std::string_view channel, RC::File::StringViewType content,
        FmtArgs&&... fmt_args) -> void
    {
        if (!PS::PSConfig::Get()->IsRoutineNotificationEnabled(channel)) return;
        Log<RC::LogLevel::Normal>(content, std::forward<FmtArgs>(fmt_args)...);
    }
}
