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
    auto a=declared.upper_bound(after),b=authored.upper_bound(after);auto paths=Json::object();std::string cursor=after;size_t inspected=0;
    while((a!=declared.end()||b!=authored.end())&&paths.size()<128&&inspected++<2048){
        const auto key=b==authored.end()||(a!=declared.end()&&a->first<b->first)?a->first:b->first;
        std::set<std::string> owners;
        if(a!=declared.end()&&a->first==key){owners.insert(a->second.begin(),a->second.end());++a;}
        if(b!=authored.end()&&b->first==key){owners.insert(b->second.begin(),b->second.end());++b;}
        cursor=key;
        if(!snapshot.SourceKeys.contains(HelpyCacheSources::Source(key)))continue;
        if(source.empty()||owners.contains(source.substr(5)))paths[key]=owners;
    }
    return {{"Key",snapshot.Key},{"ContentRevision",snapshot.ContentRevision},{"Sources",snapshot.SourceKeys},{"Source",source},
        {"After",after},{"Next",cursor},{"Complete",a==declared.end()&&b==authored.end()},{"Paths",paths}};
}
using Read=std::function<bool(const Json&,Json&)>;
inline HelpySourceCache::Snapshot Fetch(HelpySourceCache::Snapshot snapshot,const std::string& source,const Read& read){
    if(!source.empty()&&!source.starts_with("mods/"))return snapshot;
    if(!Scope(source))throw std::runtime_error("Invalid mod cache source");
    if(!read)return snapshot; // In-process callers already carry immutable exact ownership.
    auto owners=std::make_shared<Owners>();std::string after;
    for(size_t page=0;page<8192;++page){
        Json response;const Json request={{"Action","IconCacheOwnership"},{"Source",source},{"After",after},{"Key",snapshot.Key},{"ContentRevision",snapshot.ContentRevision},{"Sources",snapshot.SourceKeys}};
        if(!read(request,response))throw std::runtime_error("Exact cache ownership unavailable; update RuneSchema core and retry");
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
        after=next;if(complete){snapshot.AssetOwners=std::move(owners);snapshot.AuthoredAssets.reset();return snapshot;}
    }
    throw std::runtime_error("Cache ownership exceeds its paging bound");
}
}

