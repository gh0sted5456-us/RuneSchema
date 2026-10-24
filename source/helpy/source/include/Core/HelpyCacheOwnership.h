#pragma once
#include "Core/HelpySourceCache.h"
namespace PS::HelpyCacheOwnership {
using Json=nlohmann::json;
using Owners=HelpyCacheSources::Plan::AssetOwnerMap;
inline bool Scope(const std::string& source){return source.empty()||(source.starts_with("mods/")&&ContentSource::Mod(source.substr(5))==source&&ContentSource::Valid(source));}
inline Json Page(const HelpySourceCache::Snapshot& snapshot,const Json& request){
    const auto source=request.value("Source",std::string{}),after=request.value("After",std::string{});
    if(!Scope(source)||(!after.empty()&&HelpyCacheSources::AssetKey(after)!=after))throw std::runtime_error("Invalid cache ownership scope/cursor");
    if(request.at("Key")!=snapshot.Key||request.at("ContentRevision")!=snapshot.ContentRevision||request.at("Sources")!=Json(snapshot.SourceKeys))
        throw std::runtime_error("Cache ownership changed; retry the operation");
    const Owners empty;const auto& declared=snapshot.AssetOwners?*snapshot.AssetOwners:empty;
    const auto& authored=snapshot.AuthoredAssets?*snapshot.AuthoredAssets:empty;
    const auto& containers=snapshot.ContainerAssets?*snapshot.ContainerAssets:empty;
    auto a=declared.upper_bound(after),b=authored.upper_bound(after),c=containers.upper_bound(after);
    auto paths=Json::object(),containerPaths=Json::object();std::string cursor=after;size_t inspected=0;
    while((a!=declared.end()||b!=authored.end()||c!=containers.end())&&paths.size()+containerPaths.size()<127&&inspected++<2048){
        std::string key;
        for(const auto& candidate:{a==declared.end()?std::string{}:a->first,b==authored.end()?std::string{}:b->first,c==containers.end()?std::string{}:c->first})
            if(!candidate.empty()&&(key.empty()||candidate<key))key=candidate;
        std::set<std::string> owners;
        if(a!=declared.end()&&a->first==key){owners.insert(a->second.begin(),a->second.end());++a;}
        if(b!=authored.end()&&b->first==key){owners.insert(b->second.begin(),b->second.end());++b;}
        std::set<std::string> suppliers;
        if(c!=containers.end()&&c->first==key){suppliers=c->second;++c;}
        cursor=key;
        if(!snapshot.SourceKeys.contains(HelpyCacheSources::Source(key)))continue;
        if(source.empty()||owners.contains(source.substr(5)))paths[key]=owners;
        if(!suppliers.empty()&&(source.empty()||suppliers.contains(source)||owners.contains(source.substr(5))))containerPaths[key]=suppliers;
        if(paths.contains(key)&&owners.empty())paths.erase(key);
    }
    return {{"Key",snapshot.Key},{"ContentRevision",snapshot.ContentRevision},{"Sources",snapshot.SourceKeys},{"Source",source},
        {"After",after},{"Next",cursor},{"Complete",a==declared.end()&&b==authored.end()&&c==containers.end()},{"Paths",paths},{"ContainerPaths",containerPaths}};
}
using Read=std::function<bool(const Json&,Json&)>;
struct Unavailable final:std::runtime_error {using std::runtime_error::runtime_error;};
inline HelpySourceCache::Snapshot Fetch(HelpySourceCache::Snapshot snapshot,const std::string& source,const Read& read){
    if(snapshot.Provisional)throw Unavailable("Waiting for live cache ownership verification; cached content is retained.");
    if(!source.empty()&&!source.starts_with("mods/"))return snapshot;
    if(!Scope(source))throw std::runtime_error("Invalid mod cache source");
    if(!read)return snapshot; // In-process callers already carry immutable exact ownership.
    auto owners=std::make_shared<Owners>(),containers=std::make_shared<Owners>();std::string after;
    for(size_t page=0;page<8192;++page){
        Json response;const Json request={{"Action","IconCacheOwnership"},{"Source",source},{"After",after},{"Key",snapshot.Key},{"ContentRevision",snapshot.ContentRevision},{"Sources",snapshot.SourceKeys}};
        if(!read(request,response))throw Unavailable("Cache ownership is not ready or changed; retry after live source verification.");
        if(!response.is_object()||response.at("Key")!=snapshot.Key||response.at("ContentRevision")!=snapshot.ContentRevision
            ||response.at("Sources")!=Json(snapshot.SourceKeys)||response.at("Source")!=source||response.at("After")!=after
            ||!response.at("Paths").is_object()||response.at("Paths").size()>128||!response.at("Complete").is_boolean())
            throw std::runtime_error("Invalid/stale cache ownership proof");
        const auto next=response.at("Next").get<std::string>();const bool complete=response.at("Complete").get<bool>();
        if((!complete&&next<=after)||(!next.empty()&&HelpyCacheSources::AssetKey(next)!=next)||next<after)
            throw std::runtime_error("Invalid cache ownership continuation");
        for(const auto& [path,values]:response.at("Paths").items()){
            if(HelpyCacheSources::AssetKey(path)!=path||path<=after||path>next||!snapshot.SourceKeys.contains(HelpyCacheSources::Source(path))
                ||!values.is_array()||values.empty()||values.size()>32||owners->contains(path)||owners->size()>=524288)
                throw std::runtime_error("Invalid exact cache asset proof");
            std::set<std::string> names;for(const auto& value:values){
                const auto name=value.get<std::string>();if(!ContentSource::Valid(ContentSource::Mod(name))||ContentSource::Mod(name)!="mods/"+name||!names.insert(name).second)
                    throw std::runtime_error("Invalid exact cache owner");
            }
            if(!source.empty()&&!names.contains(source.substr(5)))throw std::runtime_error("Cache ownership escaped the requested mod");
            owners->emplace(path,std::move(names));
        }
        if(response.contains("ContainerPaths")){
            const auto& rows=response.at("ContainerPaths");if(!rows.is_object()||rows.size()+response.at("Paths").size()>128)throw std::runtime_error("Invalid container ownership page");
            for(const auto& [path,values]:rows.items()){
                if(HelpyCacheSources::AssetKey(path)!=path||path<=after||path>next||!snapshot.SourceKeys.contains(HelpyCacheSources::Source(path))
                    ||!values.is_array()||values.empty()||values.size()>32||containers->contains(path)||containers->size()>=524288)throw std::runtime_error("Invalid exact container proof");
                std::set<std::string> tokens;for(const auto& value:values){const auto token=value.get<std::string>();if(!ContentSource::Valid(token)||!tokens.insert(token).second)throw std::runtime_error("Invalid container provider");}
                if(!source.empty()&&!tokens.contains(source)&&(!owners->contains(path)||!owners->at(path).contains(source.substr(5))))throw std::runtime_error("Container proof escaped its source");
                containers->emplace(path,std::move(tokens));
            }
        }
        after=next;if(complete){snapshot.AssetOwners=std::move(owners);snapshot.AuthoredAssets.reset();snapshot.ContainerAssets=std::move(containers);return snapshot;}
    }
    throw std::runtime_error("Cache ownership exceeds its paging bound");
}
}

