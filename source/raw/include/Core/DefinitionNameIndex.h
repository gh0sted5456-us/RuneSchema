#pragma once
#include <cstddef>
#include <unordered_map>
namespace PS {
template<class String> class DefinitionNameIndex {
    std::unordered_map<String,std::size_t> m_counts;
public:
    void Clear(){m_counts.clear();}
    void Add(const String& name){++m_counts[name];}
    bool Ambiguous(const String& name) const {const auto it=m_counts.find(name);return it!=m_counts.end() && it->second>1;}
};
}
