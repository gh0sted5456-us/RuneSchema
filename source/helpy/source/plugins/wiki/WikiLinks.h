#pragma once
// RuneSchema.Wiki: addresses on the RuneScape: Dragonwilds Wiki (dragonwilds.runescape.wiki).
#include <string>
#include <string_view>

namespace PS::WikiLinks {
inline constexpr std::string_view Origin = "https://dragonwilds.runescape.wiki";
inline constexpr std::string_view Home = "https://dragonwilds.runescape.wiki/";
inline constexpr size_t MaxQueryBytes = 400;
// Percent-encodes UTF-8 for a query value: unreserved characters stay, everything else is %XX.
inline std::string Encode(std::string_view text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') result += static_cast<char>(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}
// The wiki's search with "go": an exact page title opens that page, anything else shows results.
// Empty or whitespace-only text opens the wiki's main page.
inline std::string Lookup(std::string_view query) {
    while (!query.empty() && (query.front() == ' ' || query.front() == '\t')) query.remove_prefix(1);
    while (!query.empty() && (query.back() == ' ' || query.back() == '\t')) query.remove_suffix(1);
    if (query.empty()) return std::string(Home);
    if (query.size() > MaxQueryBytes) query = query.substr(0, MaxQueryBytes);
    return std::string(Origin) + "/w/Special:Search?search=" + Encode(query) + "&go=Go";
}
}
