#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
namespace PS::UE4SSBuildIdentity {
struct Identity {std::string Version,Commit;};
inline std::optional<Identity> ParseStartup(std::string_view log) {
    if(log.size()>16384)return {};
    const auto marker=log.find("UE4SS - v");if(marker==log.npos)return {};
    const auto end=log.find_first_of("\r\n",marker);
    auto line=log.substr(marker,end==log.npos?log.size()-marker:end-marker);
    if(line.size()>256)return {};
    line.remove_prefix(9);const auto separator=line.find(" - Git SHA #");if(separator==line.npos)return {};
    Identity value{std::string(line.substr(0,separator)),std::string(line.substr(separator+12))};
    while(!value.Commit.empty()&&value.Commit.back()==' ')value.Commit.pop_back();
    if(value.Version.empty()||value.Version.size()>100||value.Version.front()<'0'||value.Version.front()>'9'
        ||value.Commit.size()<7||value.Commit.size()>40)return {};
    for(unsigned char ch:value.Version)if(ch<32||ch>126)return {};
    for(auto& ch:value.Commit){if(ch>='A'&&ch<='F')ch+=32;if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return {};}
    return value;
}
enum class Relation { Pinned, Older, Newer, Unknown };
inline std::optional<std::array<unsigned,3>> VersionSeries(std::string_view text) {
    std::array<unsigned,3> parts{};
    for(std::size_t i=0;i<parts.size();++i) {
        if(text.empty())return {};
        const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),parts[i]);
        if(error!=std::errc{})return {};
        text.remove_prefix(static_cast<std::size_t>(end-text.data()));
        if(i+1<parts.size()) {
            if(text.empty()||text.front()!='.')return {};
            text.remove_prefix(1);
        }
    }
    if(!text.empty()&&text.front()!=' ')return {};
    return parts;
}
inline Relation Compare(const Identity& installed,std::string_view pin,std::string_view pinnedVersion) {
    if(pin.starts_with(installed.Commit)&&installed.Commit.size()>=7)return Relation::Pinned;
    const auto version=VersionSeries(installed.Version),expected=VersionSeries(pinnedVersion);
    if(version&&expected&&*version!=*expected)return *version<*expected?Relation::Older:Relation::Newer;
    // GitHub compare verified these upstream revisions relative to this pin.
    if(pin.starts_with("f6d5f942")) {
        if(installed.Commit.starts_with("d935b5b"))return Relation::Older;
        if(installed.Commit.starts_with("1ba38db"))return Relation::Newer;
    }
    return Relation::Unknown;
}
}
