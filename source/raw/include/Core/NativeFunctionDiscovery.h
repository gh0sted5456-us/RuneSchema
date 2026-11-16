#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace PS::NativeFunctionDiscovery {
struct Region { std::uint32_t Rva; std::span<const std::uint8_t> Bytes; };
inline std::vector<int> Parse(std::string_view pattern) {
    std::vector<int> bytes;
    const auto hex=[](char ch)->int {if(ch>='0'&&ch<='9')return ch-'0';if(ch>='a'&&ch<='f')return ch-'a'+10;if(ch>='A'&&ch<='F')return ch-'A'+10;return -1;};
    while(!pattern.empty()) {
        const auto begin=pattern.find_first_not_of(' ');if(begin==pattern.npos)break;pattern.remove_prefix(begin);
        const auto end=pattern.find(' ');const auto token=pattern.substr(0,end);
        if(token=="??")bytes.push_back(-1);
        else {if(token.size()!=2||hex(token[0])<0||hex(token[1])<0)throw std::runtime_error("Invalid native discovery pattern");bytes.push_back(hex(token[0])*16+hex(token[1]));}
        if(end==pattern.npos)break;pattern.remove_prefix(end+1);
    }
    if(bytes.size()<24||bytes.size()>256||bytes.front()<0)throw std::runtime_error("Native discovery pattern has invalid bounds");
    return bytes;
}
// Only executable regions and exact x64 unwind function entries are eligible.
// A second match is a failure, not a license to choose the first address.
inline std::optional<std::uint32_t> FindUnique(std::span<const Region> regions,
    std::span<const std::uint32_t> sortedEntries,std::string_view signature) {
    const auto pattern=Parse(signature);std::optional<std::uint32_t> result;
    for(const auto& region:regions) {
        if(region.Bytes.size()<pattern.size())continue;
        const auto limit=region.Bytes.size()-pattern.size();
        const auto first=std::lower_bound(sortedEntries.begin(),sortedEntries.end(),region.Rva);
        for(auto entry=first;entry!=sortedEntries.end();++entry) {
            const auto offset=static_cast<size_t>(*entry-region.Rva);
            if(offset>limit)break;
            if(entry!=first&&*entry==entry[-1])continue;
            const auto* bytes=region.Bytes.data()+offset;
            if(bytes[0]!=pattern[0])continue;
            bool matched=true;
            for(size_t i=1;i<pattern.size();++i)if(pattern[i]>=0&&bytes[i]!=pattern[i]){matched=false;break;}
            if(matched) {
                if(result)return {};
                result=*entry;
            }
        }
    }
    return result;
}
}
