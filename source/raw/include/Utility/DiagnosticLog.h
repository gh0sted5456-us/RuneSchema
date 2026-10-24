#pragma once
#include <format>
#include <string_view>

namespace PS::DiagnosticLog {
void Initialize(bool archiveLogs=false,bool freshSession=true) noexcept;
void Write(std::wstring_view text,bool urgent=false) noexcept;
void WriteUtf8(std::string_view text,bool urgent=false) noexcept;
void Shutdown() noexcept;
template<class... Args>
void Formatted(std::wstring_view format,bool urgent,Args&&... args) noexcept {
    try { Write(std::vformat(format,std::make_wformat_args(args...)),urgent); }
    catch(...) {} // A diagnostic must never interrupt a loader or grant.
}
}
