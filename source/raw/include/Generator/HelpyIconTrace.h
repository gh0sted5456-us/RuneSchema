#pragma once

#include "Runtime/HostServices.h"
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace PS::HelpyIconTrace {
// Detailed icon tracing is opt-in: no filesystem work on the normal browse or
// game-thread thumbnail path. Set before launching the game when diagnosing.
inline bool Enabled(){static const bool enabled=[] {const auto* value=std::getenv("RUNESCHEMA_HELPY_ICON_TRACE");return value && std::string_view(value)=="1";}();return enabled;}
namespace detail {
    inline std::mutex& Mutex(){static auto* value=new std::mutex();return *value;}
    inline std::unordered_set<std::string>& Seen(){static auto* value=new std::unordered_set<std::string>();return *value;}
    inline std::size_t& EventCount(){static std::size_t value=0;return value;}
    inline bool& HeaderWritten(){static bool value=false;return value;}
    inline constexpr std::size_t MaxEvents=3000;
    inline constexpr std::uintmax_t MaxLogBytes=3ull*1024ull*1024ull;
    inline std::filesystem::path File(){return HostServices::CacheDirectory()/"HelpyIconTrace.log";}
    inline std::string Clean(std::string value){
        for(auto& c:value)if(c=='\r'||c=='\n'||c=='\t')c=' ';
        if(value.size()>1400)value.resize(1400);
        return value;
    }
    inline void EnsureHeader(std::ofstream& out){
        if(HeaderWritten())return;
        HeaderWritten()=true;
        out << "\n===== RuneSchema Helpy icon trace session =====\n";
        out << "Stages: row -> plugin-row -> render-row -> visible-request -> queue/disk -> submit -> core -> resolve -> readback -> plugin-result -> ui-install\n";
    }
}

inline void Event(std::string_view stage,const std::string& correlation,const std::string& details={}){
    if(!Enabled()||stage.empty()||correlation.empty())return;
    std::lock_guard lock(detail::Mutex());
    if(detail::EventCount()>=detail::MaxEvents)return;
    const std::string unique=std::string(stage)+"|"+correlation;
    if(!detail::Seen().insert(unique).second)return;
    const auto file=detail::File();std::error_code ec;
    if(std::filesystem::exists(file,ec)&&!ec&&std::filesystem::file_size(file,ec)>detail::MaxLogBytes){
        std::filesystem::remove(file,ec);detail::HeaderWritten()=false;
    }
    std::filesystem::create_directories(file.parent_path(),ec);
    std::ofstream out(file,std::ios::app);if(!out)return;
    detail::EnsureHeader(out);++detail::EventCount();
    out << '[' << stage << "] key='" << detail::Clean(correlation) << "'";
    if(!details.empty())out << " " << detail::Clean(details);
    out << '\n';
}

inline std::filesystem::path Path(){return detail::File();}
}
