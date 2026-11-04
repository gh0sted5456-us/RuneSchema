#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace PS::AssetPreload {
inline bool IsMountedPackage(std::string_view package,const std::unordered_set<std::string>& roots) {
    std::string lower(package);
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(lower.starts_with("/game/mods/")||lower.starts_with("/game/runeschema/"))return true;
    if(lower.size()<3||lower.front()!='/')return false;
    const auto slash=lower.find('/',1);
    return slash!=lower.npos&&roots.contains(lower.substr(1,slash-1));
}
template<class String> class Plan {
    std::unordered_set<String> m_seen;
    std::vector<String> m_paths;
public:
    bool Add(const String& path) {
        using Char=typename String::value_type;
        const auto prefix=[&](const char* text){std::size_t i=0;for(;text[i];++i)if(i>=path.size() || path[i]!=static_cast<Char>(text[i]))return false;return true;};
        if(path.empty() || path.front()!=static_cast<Char>('/') || path.size()>4096
            || path.find(static_cast<Char>('.'))==String::npos || prefix("/Script/")
            || prefix("/Game/RuneSchema/") || prefix("/Engine/Transient"))return false;
        if(!m_seen.insert(path).second)return false;
        m_paths.push_back(path);return true;
    }
    const std::vector<String>& Paths() const{return m_paths;}
};
}
