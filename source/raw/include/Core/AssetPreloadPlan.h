#pragma once
#include <string>
#include <unordered_set>
#include <vector>

namespace PS::AssetPreload {
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
