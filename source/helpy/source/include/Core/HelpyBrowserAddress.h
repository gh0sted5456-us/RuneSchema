#pragma once
// Helpy's web browser: what the address bar turns typed text into, and which addresses it opens.
// Engine-free and tested on its own.
#include <string>
#include <string_view>

namespace PS::HelpyBrowserAddress {
inline constexpr size_t MaxLength = 2048;
inline constexpr std::string_view Home = "https://duckduckgo.com/";
inline constexpr std::string_view Search = "https://duckduckgo.com/?q=";

inline bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        const char a = text[i] >= 'A' && text[i] <= 'Z' ? static_cast<char>(text[i] + 32) : text[i];
        if (a != prefix[i]) return false;
    }
    return true;
}
inline bool Printable(std::string_view text) {
    for (const unsigned char c : text) if (c < 33 || c == 127) return false; // No spaces or controls.
    return true;
}
// Only web addresses: http or https, no spaces or control characters, at most MaxLength.
inline bool Web(std::string_view url) {
    if (url.empty() || url.size() > MaxLength || !Printable(url)) return false;
    const auto rest = StartsWithNoCase(url, "https://") ? url.substr(8) : StartsWithNoCase(url, "http://") ? url.substr(7) : std::string_view{};
    return !rest.empty() && rest.front() != '/' && rest.front() != '@';
}
inline std::string Encode(std::string_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out.push_back(static_cast<char>(c));
        else if (c == ' ') out.push_back('+');
        else { out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); }
    }
    return out;
}
// "example.com/page" or "localhost:8080" style: no spaces, a dot or a port, host characters first.
inline bool LooksLikeHost(std::string_view text) {
    if (text.empty() || !Printable(text)) return false;
    const auto end = text.find_first_of("/?#");
    const auto host = text.substr(0, end);
    if (host.empty() || host.front() == '.' || host.front() == '-') return false;
    bool dot = false, port = false;
    for (size_t i = 0; i < host.size(); ++i) {
        const char c = host[i];
        if (c == '.') { dot = true; continue; }
        if (c == ':') { port = i + 1 < host.size(); for (size_t j = i + 1; j < host.size(); ++j) if (host[j] < '0' || host[j] > '9') return false; break; }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return dot || port;
}
// What the address bar opens: the address itself when it is a web address, https:// added to a bare
// host, and a search for anything else (including other schemes such as javascript: or file:).
inline std::string Resolve(std::string_view input) {
    while (!input.empty() && (input.front() == ' ' || input.front() == '\t')) input.remove_prefix(1);
    while (!input.empty() && (input.back() == ' ' || input.back() == '\t')) input.remove_suffix(1);
    if (input.empty() || input.size() > MaxLength) return {};
    if (Web(input)) return std::string(input);
    if (input.find("://") == std::string_view::npos && LooksLikeHost(input)) {
        auto url = "https://" + std::string(input);
        if (Web(url)) return url;
    }
    return std::string(Search) + Encode(input);
}
}
