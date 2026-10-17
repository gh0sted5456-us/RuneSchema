#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <mutex>
#include "nlohmann/json.hpp"
#include "Loader/ItemIdentity.h"

namespace PS::RecipeItemRouting {
// Store paths, never unleased UObject pointers. Resolution revalidates the
// live asset and its identity, including after GC/unload or an asset replacement.
class Index {
public:
    struct Builder {
        std::unordered_map<std::string,std::string> Paths;
        std::unordered_set<std::string> Ambiguous;
        void Add(const std::string& id,const std::string& path) {
            if(id.empty())return;
            if(path.empty())throw std::runtime_error("ItemData route has no object path");
            if(Ambiguous.contains(id))return;
            const auto [entry,inserted]=Paths.emplace(id,path);
            if(!inserted&&entry->second!=path){Paths.erase(entry);Ambiguous.insert(id);}
        }
    };
private:
    enum class State {Empty,Building,Ready,Failed};
    State m_state=State::Empty;
    mutable std::recursive_mutex m_gate;
    Builder m_data;
    std::string m_failure;
public:
    void Reset(){std::lock_guard lock(m_gate);m_state=State::Empty;m_data={};m_failure.clear();}
    template<class Scan> void Ensure(Scan&& scan) {
        std::lock_guard lock(m_gate);
        if(m_state==State::Ready)return;
        if(m_state==State::Failed)throw std::runtime_error(m_failure);
        if(m_state==State::Building)throw std::runtime_error("Recursive ItemData route discovery");
        m_state=State::Building;
        try{Builder next;scan(next);m_data=std::move(next);m_state=State::Ready;}
        catch(const std::exception& error){m_failure=error.what();m_state=State::Failed;throw;}
        catch(...){m_failure="ItemData route discovery failed";m_state=State::Failed;throw;}
    }
    std::string Resolve(const std::string& id) const {
        std::lock_guard lock(m_gate);
        if(m_state!=State::Ready)throw std::runtime_error("ItemData routes are not ready");
        if(m_data.Ambiguous.contains(id))throw std::runtime_error("Recipe ItemData PersistenceID is ambiguous: "+id);
        const auto found=m_data.Paths.find(id);
        if(found==m_data.Paths.end())throw std::runtime_error("Recipe ItemData PersistenceID is not registered: "+id);
        return found->second;
    }
    bool Matches(const std::string& id,const std::string& path) const {
        std::lock_guard lock(m_gate);
        return m_state==State::Ready&&!m_data.Ambiguous.contains(id)
            &&m_data.Paths.contains(id)&&m_data.Paths.at(id)==path;
    }
    bool Ambiguous(const std::string& id) const {std::lock_guard lock(m_gate);return m_data.Ambiguous.contains(id);}
};

template<class Prepare,class ResolvePath>
nlohmann::json Route(std::string_view propertyName,const nlohmann::json& authored,
    Index& index,Prepare&& prepare,ResolvePath&& resolvePath) {
    if(propertyName!="ItemsConsumed"&&propertyName!="ItemsCreated")return authored;
    auto routed=authored;if(!routed.is_array())return routed;
    for(auto& row:routed) {
        if(!row.is_object()||!row.contains("ItemData")||!row.at("ItemData").is_string())continue;
        const auto reference=row.at("ItemData").get<std::string>();
        std::string path=reference,identity;
        if(DragonWilds::IsCanonicalPersistenceId(reference)) {
            prepare();path=index.Resolve(reference);identity=reference;
        }else if(!reference.starts_with('/')||reference.find('.')==std::string::npos)
            throw std::runtime_error("Recipe ItemData must be a full object path");
        // Exact paths never depend on a whole-roster scan or a failed PID cache.
        row["ItemData"]=resolvePath(path,identity);
    }
    return routed;
}
}
