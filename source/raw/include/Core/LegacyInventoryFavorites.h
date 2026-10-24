#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
namespace PS::LegacyInventoryFavorites {
inline std::set<std::string> Parse(std::string_view text){
    if(text.size()>1024*1024)throw std::runtime_error("Legacy favorites exceed 1 MiB; original retained");
    std::set<std::string> result;size_t lines=0;
    while(!text.empty()){
        if(++lines>16384)throw std::runtime_error("Legacy favorites exceed the entry bound; original retained");
        const auto end=text.find('\n');auto line=text.substr(0,end);text=end==text.npos?std::string_view{}:text.substr(end+1);
        const auto first=line.find_first_not_of(" \t\r");if(first==line.npos)continue;
        line=line.substr(first,line.find_last_not_of(" \t\r")-first+1);
        if(line.size()!=32)throw std::runtime_error("Invalid legacy favorite identity; original retained");
        std::string key;for(char c:line){if(c>='a'&&c<='f')c-=32;if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))throw std::runtime_error("Invalid legacy favorite identity; original retained");key+=c;}
        if(key==std::string(32,'0'))throw std::runtime_error("Empty legacy favorite identity; original retained");result.insert(std::move(key));
    }return result;
}
inline std::string Key(const std::array<uint32_t,4>& words){std::string result;for(auto word:words){char part[9]{};std::snprintf(part,sizeof(part),"%08X",word);result+=part;}return result;}
}
