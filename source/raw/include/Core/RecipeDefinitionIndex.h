#pragma once
// Lookup helpers for the recipe loader's definition list. They replace linear
// scans that made loading N recipes cost O(N^2) string comparisons, without
// changing which definition a lookup returns.
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace PS::RecipeDefinitionIndex {

// Owner/key lookups over an append-only definition list. A definition is
// identified by its owner and key separately (never by a joined string), and
// entries are only ever appended or replaced in place with the same identity,
// so the index stays exact by indexing new entries as the list grows.
template<class String> class Definitions {
    struct PairHash {
        std::size_t operator()(const std::pair<String,String>& value) const noexcept {
            const auto first=std::hash<String>{}(value.first);
            return first^(std::hash<String>{}(value.second)+0x9e3779b97f4a7c15ull+(first<<6)+(first>>2));
        }
    };
    struct Slot { std::size_t First=0; std::size_t Count=0; };
    std::unordered_map<std::pair<String,String>,Slot,PairHash> m_owned;
    std::unordered_map<String,Slot> m_keys;
    std::size_t m_indexed=0;
public:
    struct Match {
        std::optional<std::size_t> Index; // Set only when exactly one definition matches.
        bool Ambiguous=false;             // More than one definition matches.
    };

    // Indexes entries appended since the last call. `owner(i)` and `key(i)`
    // return definition i's owner and key.
    template<class Owner,class Key>
    void Sync(std::size_t size,Owner&& owner,Key&& key) {
        if(size<m_indexed)Reset();
        for(;m_indexed<size;++m_indexed) {
            auto& slot=m_owned[{owner(m_indexed),key(m_indexed)}];
            if(!slot.Count++)slot.First=m_indexed;
            auto& byKey=m_keys[key(m_indexed)];
            if(!byKey.Count++)byKey.First=m_indexed;
        }
    }
    void Reset() { m_owned.clear(); m_keys.clear(); m_indexed=0; }
    std::size_t Indexed() const noexcept { return m_indexed; }

    // The first definition with this owner and key, like std::find_if.
    std::optional<std::size_t> First(const String& owner,const String& key) const {
        const auto found=m_owned.find({owner,key});
        if(found==m_owned.end())return std::nullopt;
        return found->second.First;
    }
    Match Owned(const String& owner,const String& key) const {
        const auto found=m_owned.find({owner,key});
        if(found==m_owned.end())return {};
        if(found->second.Count>1)return {std::nullopt,true};
        return {found->second.First,false};
    }
    // A key that must be unique across every owner.
    Match AnyOwner(const String& key) const {
        const auto found=m_keys.find(key);
        if(found==m_keys.end())return {};
        if(found->second.Count>1)return {std::nullopt,true};
        return {found->second.First,false};
    }
};

// The DataTables any recipe placement targets, by short table name or by full
// object path. Lets the serialize hook skip unrelated tables with one lookup
// instead of walking every recipe's placements.
class PlacementTargets {
    std::unordered_set<std::string> m_names,m_paths;
    bool m_dirty=true;
public:
    void MarkDirty() noexcept { m_dirty=true; }
    bool Dirty() const noexcept { return m_dirty; }
    // `each(add)` must call add(table,dataTable) for every placement, where
    // dataTable is the full object path or empty for a name-only target.
    template<class Each> void Rebuild(Each&& each) {
        m_names.clear(); m_paths.clear();
        each([&](const std::string& table,const std::string& dataTable) {
            if(dataTable.empty())m_names.insert(table);
            else m_paths.insert(dataTable);
        });
        m_dirty=false;
    }
    bool Empty() const noexcept { return m_names.empty()&&m_paths.empty(); }
    bool TargetsName(const std::string& name) const { return m_names.contains(name); }
    bool TargetsPath(const std::string& path) const { return m_paths.contains(path); }
    bool HasPathTargets() const noexcept { return !m_paths.empty(); }
};

// Results of resolving the same reference repeatedly within one synchronous
// pass (for example a common ingredient used by hundreds of recipes). Only
// successes are kept; a failure is resolved again, and reported, every time.
template<class Value> class PassCache {
    std::unordered_map<std::string,Value> m_values;
public:
    std::size_t Hits=0,Misses=0;
    template<class Resolve> Value Get(const std::string& key,Resolve&& resolve) {
        if(const auto found=m_values.find(key);found!=m_values.end()){++Hits;return found->second;}
        ++Misses;
        auto value=resolve();
        m_values.emplace(key,value);
        return value;
    }
};

}
