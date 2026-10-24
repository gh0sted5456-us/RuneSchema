#pragma once
#include "Core/CacheBundleStore.h"
#include "Core/ItemCatalogFilter.h"
#include <future>
#include <optional>
#include <charconv>
// Presentation lookup on an independent file worker. Never walks UObjects,
// performs icon capture, or marks an imported catalogue complete.
namespace PS::CacheBundleItems {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline Json Page(const Json& records,const std::string& query,const std::string& cursor,size_t limit){
    if(!limit||limit>128)throw std::runtime_error("[RS-CACHE-002] Invalid bundle page size.");
    std::vector<Json> rows;for(const auto& record:records)if(record.value("Kind",std::string{})=="items"){auto row=record.at("Data");ItemCategory::Repair(row);rows.push_back(std::move(row));}
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){const auto an=HelpyCacheSources::Lower(a.value("Name",std::string{})),bn=HelpyCacheSources::Lower(b.value("Name",std::string{}));return an==bn?a.at("Path")<b.at("Path"):an<bn;});
    const auto ticket=HelpySourceCache::Hex(HelpySourceCache::Hash(records.dump()));size_t offset=0;bool reset=false;
    if(!cursor.empty()){const auto prefix="B:"+ticket+":";if(cursor.starts_with(prefix)){
        const auto text=std::string_view(cursor).substr(prefix.size());const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),offset);
        if(text.empty()||error!=std::errc{}||end!=text.data()+text.size()||offset>rows.size()){offset=0;reset=true;}
    }else reset=true;}
    const ItemCatalogFilter::Query match(query);Json found=Json::array();
    for(;offset<rows.size()&&found.size()<limit;++offset)if(match(rows[offset]))found.push_back(rows[offset]);
    Json page={{"Rows",std::move(found)},{"HasMore",offset<rows.size()},{"NextCursor","B:"+ticket+":"+std::to_string(offset)},{"AlphabeticalReady",true},{"CacheHit",true},{"CachePreview",true},{"CheckingChanges",true},{"BundleReference",true},{"PopulationStage","cached-items-verifying"},{"IndexedItems",rows.size()},{"Coverage","Partial imported references; exact paths and known persistence IDs are cached. Live availability and delivery are validated separately."}};
    if(reset)page["Reset"]=true;return page;
}
class Store {
    struct Result{std::string Key;Json Page;};
    std::future<Result> Work;std::string Key,Wanted;Json Window;Clock::time_point Next{};
    struct FileState{std::string Stamp;Json Records=Json::array();};std::shared_ptr<FileState> Files=std::make_shared<FileState>();
public:
    bool Awaiting()const{return Work.valid()&&Key!=Wanted;}
    std::optional<Json> Lookup(const std::filesystem::path& cache,const std::filesystem::path& root,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const std::string& query,const std::string& cursor,size_t limit){
        const auto key=Json::array({snapshot.Key,locale,query,cursor,limit}).dump();
        Wanted=key;
        if(Work.valid()&&Work.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Work.get();Key=std::move(result.Key);Window=std::move(result.Page);Next=Clock::now()+std::chrono::seconds(2);}
        if(!Work.valid()&&(Key!=key||Clock::now()>=Next))Work=std::async(std::launch::async,[cache,root,snapshot,locale,query,cursor,limit,key,files=Files]{
            try{
                const auto index=CacheBundleStore::Directory(cache,root)/"index.json";CacheBundleIO::Safe(index);if(!std::filesystem::exists(index))return Result{key,Json{}};
                auto stamp=snapshot.Key+"|"+locale+"|"+std::to_string(std::filesystem::last_write_time(index).time_since_epoch().count())+"|"+std::to_string(std::filesystem::file_size(index));
                const auto local=cache.parent_path()/"item-catalogues"/(HelpySourceCache::Hex(HelpySourceCache::Hash(root.generic_string()))+".json");
                if(std::filesystem::exists(local))stamp+="|"+std::to_string(std::filesystem::last_write_time(local).time_since_epoch().count());
                if(files->Stamp!=stamp){
                    auto records=CacheBundleStore::Read(cache,root,snapshot,locale,"items");
                    if(!records.empty())try{
                        const auto doc=CacheBundleStore::Parse(CacheBundleIO::Read(local,64*1024*1024));const auto scope=CacheBundleStore::Parse(doc.value("Scope",std::string("{}")));
                        if(doc.value("Version",0)==1&&scope.is_array()&&scope.size()==4&&scope[0]=="item-browse-v2"&&scope[3]==locale&&doc.at("Rows").is_array()&&doc["Rows"].size()<=CacheBundle::MaxRecords){
                            std::map<std::string,Json> merged;for(const auto& reference:records)merged[reference.at("Path").get<std::string>()]=reference;
                            for(const auto& row:doc["Rows"]){const auto reference=CacheBundle::FromRow("items",row);if(!reference.is_null()&&CacheBundleStore::ActiveOwner(CacheBundle::Parse(reference),snapshot)){
                                const auto path=reference.at("Path").get<std::string>();if(scope[1]==snapshot.CatalogKey||!merged.contains(path))merged[path]=reference;
                            }}
                            auto combined=Json::array();for(const auto& [path,reference]:merged)combined.push_back(reference);
                            (void)CacheBundle::Records(combined);records=std::move(combined);
                        }
                    }catch(...){} // Imported references still work if the local catalogue is unavailable.
                    files->Records=std::move(records);files->Stamp=stamp;
                }
                return Result{key,Page(files->Records,query,cursor,limit)};
            }catch(...){return Result{key,Json{}};}
        });
        if(Key==key&&Window.is_object()&&(!Window.at("Rows").empty()||Window.value("HasMore",false)))return Window;return {};
    }
    void Shutdown()noexcept{if(Work.valid())try{Work.get();}catch(...){}Key.clear();Window=Json{};}
};
}

