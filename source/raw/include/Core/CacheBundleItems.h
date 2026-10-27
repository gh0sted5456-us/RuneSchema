#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/CacheBundleStore.h"
#include "Core/ItemCatalogFilter.h"
#include "Core/PagedItemCatalog.h"
#include <future>
#include <optional>
#include <charconv>
// Presentation lookup on an independent file worker. Never walks UObjects,
// performs icon capture, or marks an imported catalogue complete.
namespace PS::CacheBundleItems {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
struct Index {
    std::vector<Json> Rows;std::string Ticket;
    explicit Index(const Json& records){
        for(const auto& record:records)if(record.value("Kind",std::string{})=="items"){auto row=record.at("Data");ItemCategory::Repair(row);Rows.push_back(std::move(row));}
        std::sort(Rows.begin(),Rows.end(),[](const auto& a,const auto& b){const auto an=HelpyCacheSources::Lower(a.value("Name",std::string{})),bn=HelpyCacheSources::Lower(b.value("Name",std::string{}));return an==bn?a.at("Path")<b.at("Path"):an<bn;});
        Ticket=HelpySourceCache::Hex(HelpySourceCache::Hash(records.dump()));
    }
    Json Page(const std::string& query,const std::string& cursor,size_t limit)const{
    if(!limit||limit>128)throw std::runtime_error("[RS-CACHE-002] Invalid bundle page size.");
    const auto& rows=Rows;const auto& ticket=Ticket;size_t offset=0;bool reset=false;
    if(!cursor.empty()){const auto prefix="B:"+ticket+":";if(cursor.starts_with(prefix)){
        const auto text=std::string_view(cursor).substr(prefix.size());const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),offset);
        if(text.empty()||error!=std::errc{}||end!=text.data()+text.size()||offset>rows.size()){offset=0;reset=true;}
    }else reset=true;}
    const ItemCatalogFilter::Query match(query);Json found=Json::array();
    for(;offset<rows.size()&&found.size()<limit;++offset)if(match(rows[offset]))found.push_back(rows[offset]);
    Json page={{"Rows",std::move(found)},{"HasMore",offset<rows.size()},{"NextCursor","B:"+ticket+":"+std::to_string(offset)},{"AlphabeticalReady",true},{"CacheHit",true},{"CachePreview",true},{"CheckingChanges",true},{"BundleReference",true},{"PopulationStage","cached-items-verifying"},{"IndexedItems",rows.size()},{"Coverage","Partial imported references; exact paths and known persistence IDs are cached. Live availability and delivery are validated separately."}};
    if(reset)page["Reset"]=true;return page;
    }
};
inline Json Page(const Json& records,const std::string& query,const std::string& cursor,size_t limit){return Index(records).Page(query,cursor,limit);}
class Store {
    struct Result{std::string Key;Json Page;};
    PS::ProcessExitFuture<Result> Work;std::string Key,Wanted;Json Window;Clock::time_point Next{};uint64_t Epoch=0;
    struct FileState{std::string Stamp,PageKey;std::unique_ptr<Index> Items;Json Window;};std::shared_ptr<FileState> Files=std::make_shared<FileState>();
public:
    void Invalidate(){++Epoch;Next={};Files=std::make_shared<FileState>();}
    bool Awaiting()const{return Work.valid()&&Key!=Wanted;}
    std::optional<Json> Lookup(const std::filesystem::path& cache,const std::filesystem::path& root,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const std::string& query,const std::string& cursor,size_t limit){
        const auto key=Json::array({snapshot.Key,snapshot.Provisional,locale,query,cursor,limit,Epoch}).dump();
        Wanted=key;
        if(Work.valid()&&Work.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Work.get();if(result.Page.is_object()||Key!=result.Key)Window=std::move(result.Page);Key=std::move(result.Key);Next=Clock::now()+std::chrono::seconds(2);}
        if(!Work.valid()&&(Key!=key||Clock::now()>=Next))Work=std::async(std::launch::async,[cache,root,snapshot,locale,query,cursor,limit,key,files=Files]{
            try{
                const auto published=HelpyPresentationCache::Directory(cache)/"index.json",legacy=CacheBundleStore::Directory(cache,root)/"index.json";
                const auto learned=CacheBundleStore::Directory(cache,cache.parent_path())/"index.json";
                const auto local=cache.parent_path()/"item-catalogues"/"native-items.json";
                bool nativeChecked=false;
                // Learned rows may be absent from the older complete sidecar.
                if(cursor.starts_with("P:")||(cursor.empty()&&(query.empty()||!std::filesystem::exists(learned))))try{
                    CacheBundleIO::Safe(PagedItemCatalog::File(local));auto next=cursor;
                    for(int slice=0;slice<8;++slice){
                        auto page=PagedItemCatalog::Read(PagedItemCatalog::File(local),{},query,next,limit,true,locale,snapshot.NativePlugins);
                        Json rows=Json::array();for(const auto& row:page.at("Rows")){
                            auto reference=CacheBundle::FromRow("items",row);if(reference.is_null()||!CacheBundleStore::Browsable(reference,snapshot))continue;
                            rows.push_back(std::move(reference["Data"]));
                        }
                        page["Rows"]=std::move(rows);nativeChecked=!page.value("HasMore",false);
                        if(!page["Rows"].empty()||!page.value("HasMore",false)||slice==7){
                            if(!page["Rows"].empty()||page.value("HasMore",false)||cursor.starts_with("P:")){
                                page["CachePreview"]=true;page["CheckingChanges"]=true;page["BundleReference"]=true;
                                page["Coverage"]="Saved presentation rows filtered by enabled sources; gameplay remains live-validated.";
                                return Result{key,std::move(page)};
                            }break;
                        }
                        next=page.at("NextCursor").get<std::string>();
                    }
                }catch(...){} // A missing or stale sidecar falls back to bounded reference metadata.
                const auto index=std::filesystem::exists(published)?published:std::filesystem::exists(learned)?learned:std::filesystem::exists(legacy)?legacy:local;CacheBundleIO::Safe(index);if(!std::filesystem::exists(index))return Result{key,Json{}};
                auto stamp=snapshot.Key+(snapshot.Provisional?"|preview|":"|live|")+locale+"|"+std::to_string(std::filesystem::last_write_time(index).time_since_epoch().count())+"|"+std::to_string(std::filesystem::file_size(index));
                if(std::filesystem::exists(local))stamp+="|"+std::to_string(std::filesystem::last_write_time(local).time_since_epoch().count());
                if(index!=legacy&&std::filesystem::exists(legacy))stamp+="|learned|"+std::to_string(std::filesystem::last_write_time(legacy).time_since_epoch().count());
                if(index!=learned&&std::filesystem::exists(learned))stamp+="|observed|"+std::to_string(std::filesystem::last_write_time(learned).time_since_epoch().count());
                if(files->Stamp!=stamp){
                    auto records=CacheBundleStore::Read(cache,root,snapshot,locale,"items");
                    if(!nativeChecked&&(!std::filesystem::exists(learned)||!std::filesystem::exists(PagedItemCatalog::File(local))))try{
                        const auto doc=CacheBundleStore::Parse(CacheBundleIO::Read(local,64*1024*1024));const auto scope=CacheBundleStore::Parse(doc.value("Scope",std::string("{}")));
                        if(doc.value("Version",0)==1&&scope.is_array()&&scope.size()==4&&scope[0]=="item-browse-v2"&&scope[3]==locale&&doc.at("Rows").is_array()&&doc["Rows"].size()<=CacheBundle::MaxRecords){
                            std::map<std::string,Json> merged;for(const auto& reference:records)merged[reference.at("Path").get<std::string>()]=reference;
                            for(const auto& row:doc["Rows"]){auto reference=CacheBundle::FromRow("items",row);if(!reference.is_null()&&CacheBundleStore::Browsable(reference,snapshot)){
                                reference["Data"]["CacheSourceVerified"]=false;reference["Data"]["ObservedCurrent"]=false;
                                const auto path=reference.at("Path").get<std::string>();if(scope[1]==snapshot.CatalogKey||!merged.contains(path))merged[path]=reference;
                            }}
                            auto combined=Json::array();for(const auto& [path,reference]:merged)combined.push_back(reference);
                            (void)CacheBundle::Records(combined);records=std::move(combined);
                        }
                    }catch(...){} // Imported references still work if the local catalogue is unavailable.
                    files->Items=std::make_unique<Index>(records);files->Stamp=stamp;files->PageKey.clear();
                }
                if(files->PageKey!=key){
                    files->Window=files->Items->Page(query,cursor,limit);files->PageKey=key;
                    if(!query.empty()&&!files->Window.value("HasMore",true)&&std::filesystem::exists(learned))try{
                        // Show learned rows first, then continue through the older native sidecar.
                        const auto head=PagedItemCatalog::Read(PagedItemCatalog::File(local),{},"","",1,true,locale,snapshot.NativePlugins);
                        auto next=head.at("NextCursor").get<std::string>();next.replace(next.rfind(':')+1,std::string::npos,"0");
                        files->Window["NextCursor"]=next;files->Window["HasMore"]=true;
                    }catch(...){} // References remain browsable without a compatible sidecar.
                }
                return Result{key,files->Window};
            }catch(...){return Result{key,Json{}};}
        });
        if(Key==key&&Window.is_object()&&(!Window.at("Rows").empty()||Window.value("HasMore",false)||cursor.starts_with("P:")))return Window;return {};
    }
    void Shutdown()noexcept{if(Work.valid())try{Work.get();}catch(...){}Key.clear();Window=Json{};}
};
}

