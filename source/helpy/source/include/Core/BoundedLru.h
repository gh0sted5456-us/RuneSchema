#pragma once
#include <list>
#include <unordered_map>
#include <utility>
#include <cstddef>
namespace PS {
template<class Key,class Value> class BoundedLru {
    struct Entry {Value Data;typename std::list<Key>::iterator Order;};
    std::size_t m_limit;std::list<Key> m_order;std::unordered_map<Key,Entry> m_entries;
public:
    explicit BoundedLru(std::size_t limit):m_limit(limit){}
    BoundedLru(const BoundedLru&)=delete;BoundedLru& operator=(const BoundedLru&)=delete;
    Value* Find(const Key& key){auto found=m_entries.find(key);if(found==m_entries.end())return nullptr;m_order.splice(m_order.begin(),m_order,found->second.Order);return &found->second.Data;}
    void Put(const Key& key,Value value){
        if(!m_limit)return;
        if(auto* existing=Find(key)){*existing=std::move(value);return;}
        if(m_entries.size()>=m_limit){m_entries.erase(m_order.back());m_order.pop_back();}
        m_order.push_front(key);m_entries.emplace(key,Entry{std::move(value),m_order.begin()});
    }
    void Clear(){m_entries.clear();m_order.clear();}
    std::size_t Size()const{return m_entries.size();}
};
}
