#pragma once
// Origin rules for host-owned plugin web pages (RuneSchemaWebPageV1).
#include <algorithm>
#include <string>
#include <string_view>

namespace PS::HelpyWebOrigin {
// "https://host" only: lowercase letters, digits, dots and hyphens; no port, path, user or query.
inline bool Valid(std::string_view origin) {
    constexpr std::string_view scheme = "https://";
    if (!origin.starts_with(scheme) || origin.size() <= scheme.size() || origin.size() > 128) return false;
    const auto host = origin.substr(scheme.size());
    if (host.front() == '.' || host.back() == '.' || host.front() == '-' || host.find("..") != host.npos || host.find('.') == host.npos) return false;
    return std::all_of(host.begin(), host.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-'; });
}
// True when `url` is the origin itself or a page on it. Never matches "https://host.evil" or "https://host@evil".
template <class Text> bool Inside(std::basic_string_view<Text> url, std::basic_string_view<Text> origin) {
    if (url.size() < origin.size() || url.substr(0, origin.size()) != origin) return false;
    if (url.size() == origin.size()) return true;
    const auto next = url[origin.size()];
    return next == Text('/') || next == Text('?') || next == Text('#');
}
inline bool Inside(std::string_view url, std::string_view origin) { return Inside<char>(url, origin); }
inline bool Inside(std::wstring_view url, std::wstring_view origin) { return Inside<wchar_t>(url, origin); }
// A link the player clicked may open in their own browser only if it is an ordinary web address.
inline bool External(std::wstring_view url) { return url.starts_with(L"https://") || url.starts_with(L"http://"); }
}
