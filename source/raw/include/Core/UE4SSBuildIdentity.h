#pragma once
#include <algorithm>
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
        ||value.Commit.size()<8||value.Commit.size()>40)return {};
    for(unsigned char ch:value.Version)if(ch<32||ch>126)return {};
    for(auto& ch:value.Commit){if(ch>='A'&&ch<='F')ch+=32;if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return {};}
    return value;
}
}
