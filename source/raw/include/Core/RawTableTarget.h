#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace PS::RawTableTarget {
inline bool Qualified(const std::string& path){return path.starts_with('/')&&path.find('.')!=path.npos;}
inline std::vector<size_t> CommonParents(const std::vector<std::string>& names){
    std::vector<size_t> result;for(size_t i=0;i<names.size();++i)if(names[i].ends_with("_Common"))result.push_back(i);
    if(result.empty())throw std::runtime_error("Composite DataTable has no editable _Common parent");return result;
}
struct Cursor {
    size_t Applied=0;
    template<class Apply>void Consume(size_t total,Apply&& apply){
        if(total<Applied)throw std::runtime_error("Raw table rules changed during application; restart required");
        const auto first=Applied;Applied=total; // Reserve before callbacks: native loads can reenter serialization.
        for(size_t i=first;i<total;++i)apply(i);
    }
};
}
