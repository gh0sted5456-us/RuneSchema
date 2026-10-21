#pragma once
#include "Core/CacheBundle.h"
#include "Core/CacheBundleIO.h"
#include "Core/HelpySourceCache.h"
#include <mutex>
// File-worker only. An index is committed LAST; interrupted imports leave the
// previous index intact. Records are references, never a live asset registry.
namespace PS::CacheBundleStore {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline std::mutex CommitMutex;
inline std::string Locale(){wchar_t name[LOCALE_NAME_MAX_LENGTH]{};if(!GetUserDefaultLocaleName(name,LOCALE_NAME_MAX_LENGTH))return "und";std::string result;for(const auto c:std::wstring(name)){if(c>127)return "und";result+=static_cast<char>(c);}return result;}
inline fs::path Directory(const fs::path& cache,const fs::path& root){return cache.parent_path()/"content-bundles"/HelpySourceCache::Hex(HelpySourceCache::Hash(root.generic_string()));}
inline Json Parse(std::string_view text){return Json::parse(text,[](int depth,Json::parse_event_t,Json&){if(depth>20)throw std::runtime_error("[RS-CACHE-002] Bundle nesting exceeds its limit.");return true;});}
inline Json Index(const fs::path& folder){
    const auto file=folder/"index.json";if(!fs::exists(file))return {{"Version",1},{"Routes",Json::object()}};
    auto doc=Parse(CacheBundleIO::Read(file,1024*1024));
    if(doc.value("Version",0)!=1||!doc.contains("Routes")||!doc["Routes"].is_object()||doc["Routes"].size()>2048)throw std::runtime_error("[RS-CACHE-002] Invalid bundle index.");
    return doc;
}
inline HelpyCacheSources::Plan Plan(const HelpySourceCache::Snapshot& snapshot){
    HelpyCacheSources::Plan plan;plan.Authoritative=snapshot.Authoritative;plan.Owners=snapshot.Owners;plan.DeclaredOwners=snapshot.DeclaredOwners;plan.AuthoredAssets=snapshot.AuthoredAssets;plan.AssetOwners=snapshot.AssetOwners;
    for(const auto& [source,key]:snapshot.SourceKeys)plan.Registered.insert(source);return plan;
}
inline bool ActiveOwner(const CacheBundle::Record& row,const HelpySourceCache::Snapshot& snapshot){return snapshot.SourceKeys.contains(row.Source)&&CacheBundle::Owned(row,Plan(snapshot),snapshot.NativePlugins);}
inline Json Read(const fs::path& cache,const fs::path& root,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const std::string& kind={},bool stale=true){
    const auto folder=Directory(cache,root);auto result=Json::array();std::set<std::string> seen;size_t bytes=0;
    const auto index=Index(folder);
    for(const auto& [route,entry]:index["Routes"].items()){
        if(!entry.is_object()||entry.value("Locale",std::string{})!=locale||(!kind.empty()&&entry.value("Kind",std::string{})!=kind))continue;
        const auto digest=entry.value("SHA256",std::string{}),source=entry.value("Source",std::string{});
        const auto key=snapshot.SourceKeys.find(source);if(key==snapshot.SourceKeys.end()||!CacheBundleIO::Digest(digest))continue;
        const bool current=entry.value("SourceKey",std::string{})==key->second;if(!current&&!stale)continue;
        try{
            const auto text=CacheBundleIO::Read(folder/(digest+".json"),CacheBundle::MaxMetadata);
            bytes+=text.size();if(bytes>CacheBundle::MaxMetadata)throw std::runtime_error("[RS-CACHE-005] Active bundle references exceed the metadata limit.");
            if(CacheBundleIO::Hash(text)!=digest)continue;
            for(auto row:CacheBundle::Records(Parse(text))){
                if(row.Kind!=entry.value("Kind",std::string{})||row.Source!=source||row.Owner!=entry.value("Owner",std::string{})||!ActiveOwner(row,snapshot)||(!kind.empty()&&row.Kind!=kind))continue;
                const auto identity=row.Kind+"|"+row.Path;if(!seen.insert(identity).second)continue;
                row.Data["CacheSourceVerified"]=current;row.Data["CheckingChanges"]=true;
                if(!CacheBundle::OriginalOwnerProven(row,Plan(snapshot),snapshot.NativePlugins)){
                    row.Data["CachedSourceName"]=row.Data["SourceName"];row.Data["SourceKind"]="unknown";row.Data["SourceName"]="";
                    row.Data["SourceEvidence"]="Shared namespace reference; original provider awaits native exact-path attribution";
                }
                result.push_back(CacheBundle::Document(row));if(result.size()>CacheBundle::MaxRecords)throw std::runtime_error("[RS-CACHE-005] Too many active references.");
            }
        }catch(const fs::filesystem_error&){continue;}
    }return result;
}
inline size_t Commit(const fs::path& cache,const fs::path& root,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const std::vector<CacheBundle::Record>& records,const std::function<void()>& check={}){
    std::lock_guard lock(CommitMutex);const auto folder=Directory(cache,root);auto index=Index(folder);
    std::map<std::string,std::vector<CacheBundle::Record>> routes;
    for(const auto& row:records){if(!ActiveOwner(row,snapshot))throw std::runtime_error("[RS-CACHE-008] Record owner is not active or cannot be proven.");routes[CacheBundle::Route(row)].push_back(row);}
    if(routes.size()+index["Routes"].size()>2048)throw std::runtime_error("[RS-CACHE-005] Too many bundle routes.");
    size_t changed=0;
    for(const auto& [route,rows]:routes){
        if(check)check();const auto identity=route+"|"+locale;auto merged=std::map<std::string,Json>{};
        // Partial bundles merge; they cannot erase other cached records.
        const auto previous=index["Routes"].find(identity);
        if(previous!=index["Routes"].end()&&previous->value("SourceKey",std::string{})==snapshot.SourceKeys.at(rows.front().Source)){
            const auto digest=previous->value("SHA256",std::string{});
            if(CacheBundleIO::Digest(digest))try{const auto old=CacheBundleIO::Read(folder/(digest+".json"),CacheBundle::MaxMetadata);if(CacheBundleIO::Hash(old)==digest)for(const auto& row:CacheBundle::Records(Parse(old)))merged[row.Path]=CacheBundle::Document(row);}catch(...){}
        }
        for(const auto& row:rows){const auto doc=CacheBundle::Document(row);if(!merged.contains(row.Path)||merged.at(row.Path)!=doc)++changed;merged[row.Path]=doc;}
        auto data=Json::array();for(const auto& [path,row]:merged)data.push_back(row);(void)CacheBundle::Records(data);
        const auto text=data.dump(),digest=CacheBundleIO::Hash(text);
        if(!fs::exists(folder/(digest+".json")))CacheBundleIO::Publish(folder/(digest+".json"),text);
        const auto& first=rows.front();index["Routes"][identity]={{"Owner",first.Owner},{"Kind",first.Kind},{"Source",first.Source},{"SourceKey",snapshot.SourceKeys.at(first.Source)},{"Locale",locale},{"SHA256",digest},{"Count",data.size()},{"Bytes",text.size()}};
    }
    size_t totalBytes=0,totalRecords=0;for(const auto& [route,entry]:index["Routes"].items()){totalBytes+=entry.value("Bytes",CacheBundle::MaxMetadata);totalRecords+=entry.value("Count",CacheBundle::MaxRecords);}
    if(totalBytes>CacheBundle::MaxMetadata||totalRecords>CacheBundle::MaxRecords)throw std::runtime_error("[RS-CACHE-005] Committed metadata exceeds the 64 MiB / 65536-record limit; previous index retained.");
    const auto text=index.dump();if(text.size()>1024*1024)throw std::runtime_error("[RS-CACHE-005] Bundle index exceeds its limit.");
    if(check)check();if(!routes.empty()&&Index(folder)!=index)CacheBundleIO::Publish(folder/"index.json",text);
    return changed;
}
inline Json Collect(const fs::path& cache,const fs::path& root,const fs::path& journal,const HelpySourceCache::Snapshot& snapshot){
    std::map<std::string,Json> unique;
    const auto add=[&](const std::string& kind,const Json& rows){if(!rows.is_array()||rows.size()>CacheBundle::MaxRecords)return;for(const auto& data:rows){
        const auto row=CacheBundle::FromRow(kind,data);if(row.is_null())continue;const auto parsed=CacheBundle::Parse(row);if(ActiveOwner(parsed,snapshot))unique[kind+"|"+parsed.Path]=row;
    }};
    try{for(const auto& row:Read(cache,root,snapshot,Locale(),{},false))unique[row.at("Kind").get<std::string>()+"|"+row.at("Path").get<std::string>()]=row;}catch(...){}
    const auto manifest=cache.parent_path()/"item-catalogues"/(HelpySourceCache::Hex(HelpySourceCache::Hash(root.generic_string()))+".json");
    try{const auto doc=Parse(CacheBundleIO::Read(manifest,64*1024*1024));const auto scope=Parse(doc.value("Scope",std::string("{}")));
        if(doc.value("Version",0)==1&&scope.is_array()&&scope.size()==4&&scope[0]=="item-browse-v2"&&scope[1]==snapshot.CatalogKey&&scope[3]==Locale())add("items",doc.at("Rows"));}catch(...){}
    // Native journal entries are keyed by catalogue content, not icon settings.
    try{const auto doc=Parse(CacheBundleIO::Read(journal/"catalog.json",16*1024*1024));if(doc.value("Version",0)==1&&doc.value("SourceKey",std::string{})==snapshot.CatalogKey)add("journal",doc.at("Entries"));}catch(...){}
    auto result=Json::array();for(const auto& [key,row]:unique)result.push_back(row);(void)CacheBundle::Records(result);return result;
}
}

