#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace PS::ItemCart {
using json=nlohmann::json;
struct Entry { std::string Path; json Row; int Quantity=1; };
class Model {
    std::vector<Entry> m_entries;
public:
    static constexpr std::size_t Limit=64;
    const std::vector<Entry>& Entries() const {return m_entries;}
    bool Add(const json& row) {
        if(!row.is_object())return false;
        const auto it=row.find("Path");
        if(it==row.end() || !it->is_string())return false;
        const auto path=it->get<std::string>();
        if(path.empty() || path.front()!='/' || path.size()>=1024)return false;
        for(auto& entry:m_entries)if(entry.Path==path) {
            entry.Quantity=std::min(10000,entry.Quantity+1);return true;
        }
        if(m_entries.size()>=Limit)return false;
        m_entries.push_back({path,row,1});return true;
    }
    bool Remove(const std::string& path) {
        return std::erase_if(m_entries,[&](const auto& entry){return entry.Path==path;})!=0;
    }
    void Clear(){m_entries.clear();}
    void SetQuantity(const std::string& path,int quantity) {
        for(auto& entry:m_entries)if(entry.Path==path){entry.Quantity=std::clamp(quantity,1,10000);return;}
    }
    json Payload(int mode=0) const {
        json items=json::array();
        for(const auto& entry:m_entries) {
            int stack=1;
            const auto it=entry.Row.find("MaxStackSize");
            if(it!=entry.Row.end() && it->is_number_unsigned())stack=static_cast<int>(std::clamp<std::uint64_t>(it->get<std::uint64_t>(),1,10000));
            else if(it!=entry.Row.end() && it->is_number_integer())stack=static_cast<int>(std::clamp<std::int64_t>(it->get<std::int64_t>(),1,10000));
            const int quantity=mode==1?1:mode==2?std::max(1,stack/2):mode==3?stack:entry.Quantity;
            items.push_back({{"Item",entry.Path},{"Count",quantity}});
        }
        return items;
    }
};
}
