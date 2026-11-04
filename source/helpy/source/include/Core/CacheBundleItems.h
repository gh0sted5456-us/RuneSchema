#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/CacheBundleStore.h"
#include "Core/ItemCatalogFilter.h"
#include "Core/PagedItemCatalog.h"
#include "Core/ItemGroupings.h"
#include <future>
#include <optional>
#include <charconv>
// Presentation lookup on an independent file worker. Never walks UObjects,
// performs icon capture, or marks an imported catalogue complete.
namespace PS::CacheBundleItems {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
struct Index {
    struct Entry{Json Row;std::string Name,Search,Category,Family;};
    std::vector<Entry> Rows;std::string Ticket;
    std::vector<size_t> All;std::map<std::string,std::vector<size_t>> Categories,Families;
    explicit Index(const Json& records,const ItemGroupings::Plan& groups={}){
        for(const auto& record:records)if(record.value("Kind",std::string{})=="items"){
            auto row=record.at("Data");if(!row.contains("Path"))row["Path"]=record.at("Path");ItemGroupings::Apply(row,groups,record.value("Owner",std::string{}));ItemCategory::Repair(row);
            auto name=HelpyCacheSources::Lower(row.value("Name",std::string{})),search=ItemCatalogFilter::Searchable(row,row.at("Category"));
            const auto category=HelpyCacheSources::Lower(row.at("Category").get<std::string>()),family=ItemFamily::RowKey(row);
            Rows.push_back({std::move(row),std::move(name),std::move(search),category,family});
        }
        std::sort(Rows.begin(),Rows.end(),[](const auto& a,const auto& b){return a.Name==b.Name?a.Row.at("Path")<b.Row.at("Path"):a.Name<b.Name;});
        for(size_t i=0;i<Rows.size();++i){All.push_back(i);Categories[Rows[i].Category].push_back(i);if(!Rows[i].Family.empty())Families[Rows[i].Family].push_back(i);}
        auto hash=HelpySourceCache::Hash(records.dump());hash=HelpySourceCache::Hash(ItemGroupings::Document(groups).dump(),hash);Ticket=HelpySourceCache::Hex(hash);
    }
    Json Page(const std::string& query,const std::string& cursor,size_t limit)const{
    if(!limit||limit>128)throw std::runtime_error("[RS-CACHE-002] Invalid bundle page size.");
    const auto& rows=Rows;const auto ticket=Ticket+"-"+HelpySourceCache::Hex(HelpySourceCache::Hash(query));size_t offset=0;bool reset=false;
    ItemCatalogFilter::Query match(query);const auto category=std::move(match.Category),family=std::move(match.Family);match.Category.clear();match.Family.clear();
    const auto words=std::move(match.Words);match.Words.clear();const std::vector<size_t> empty;const std::vector<size_t>* candidates=&All;
    if(!category.empty()){const auto found=Categories.find(category);candidates=found==Categories.end()?&empty:&found->second;}
    if(!family.empty()){const auto found=Families.find(family);const auto* familyRows=found==Families.end()?&empty:&found->second;if(familyRows->size()<candidates->size())candidates=familyRows;}
    if(!cursor.empty()){const auto prefix="B:"+ticket+":";if(cursor.starts_with(prefix)){
        const auto text=std::string_view(cursor).substr(prefix.size());const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),offset);
        if(text.empty()||error!=std::errc{}||end!=text.data()+text.size()||offset>candidates->size()){offset=0;reset=true;}
    }else reset=true;}
    Json found=Json::array();
    for(;offset<candidates->size()&&found.size()<limit;++offset){
        const auto& entry=rows[(*candidates)[offset]];
        if((category.empty()||category==entry.Category)&&(family.empty()||family==entry.Family)&&std::all_of(words.begin(),words.end(),[&](const auto& word){return entry.Search.find(word)!=entry.Search.npos;})&&match(entry.Row))found.push_back(entry.Row);
    }
    Json page={{"Rows",std::move(found)},{"HasMore",offset<candidates->size()},{"NextCursor","B:"+ticket+":"+std::to_string(offset)},{"AlphabeticalReady",true},{"CacheHit",true},{"CachePreview",true},{"CheckingChanges",true},{"BundleReference",true},{"PopulationStage","cached-items-verifying"},{"IndexedItems",rows.size()},{"Coverage","Partial imported references; exact paths and known persistence IDs are cached. Live availability and delivery are validated separately."}};
    if(reset)page["Reset"]=true;return page;
    }
};
inline Json Page(const Json& records,const std::string& query,const std::string& cursor,size_t limit){return Index(records).Page(query,cursor,limit);}
class Store {
    struct Result{std::string Key;Json Page;std::shared_ptr<const Index> Items;std::string Scope,GroupingStatus;bool Fast=false;};
    PS::ProcessExitFuture<Result> Work;std::string Key,Wanted;Json Window;Clock::time_point Next{};uint64_t Epoch=0;
    struct FileState{std::string Stamp,PageKey,GroupingStatus;std::shared_ptr<const Index> Items;ItemGroupings::Plan Groups;Json Window;bool NativeIndexed=false;};std::shared_ptr<FileState> Files=std::make_shared<FileState>();
    std::shared_ptr<const Index> Published;std::string PublishedScope;bool Fast=false;
public:
    std::string GroupingStatus;
    std::string ContentKey()const{return Published?Published->Ticket:std::string{};}
    void Invalidate(){++Epoch;Next={};Files=std::make_shared<FileState>();Published.reset();PublishedScope.clear();Fast=false;}
    bool Awaiting()const{return Work.valid()&&Key!=Wanted;}
    std::optional<Json> Lookup(const std::filesystem::path& cache,const std::filesystem::path& root,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const std::string& query,const std::string& cursor,size_t limit,bool prepare=false){
        const auto scope=Json::array({cache.generic_string(),root.generic_string(),snapshot.Key,snapshot.CatalogKey,snapshot.Authoritative,snapshot.Provisional,snapshot.PresentationOnly,snapshot.Owners,snapshot.DeclaredOwners,snapshot.SourceKeys,snapshot.NativePlugins,locale,Epoch}).dump();
        const auto key=Json::array({scope,query,cursor,limit,prepare}).dump();
        Wanted=key;
        if(Work.valid()&&Work.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Work.get();if(result.Page.is_object()||Key!=result.Key)Window=std::move(result.Page);Key=std::move(result.Key);Next=Clock::now()+std::chrono::seconds(2);if(result.Items){Published=std::move(result.Items);PublishedScope=std::move(result.Scope);Fast=result.Fast;GroupingStatus=std::move(result.GroupingStatus);}}
        const bool ready=Published&&Fast&&PublishedScope==scope&&!cursor.starts_with("P:");
        if(!Work.valid()&&((!ready&&Key!=key)||Clock::now()>=Next))Work=std::async(std::launch::async,[cache,root,snapshot,locale,query,cursor,limit,key,scope,prepare,files=Files]{
            SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
            try{
                const auto published=HelpyPresentationCache::Directory(cache)/"index.json",legacy=CacheBundleStore::Directory(cache,root)/"index.json";
                const auto learned=CacheBundleStore::Directory(cache,cache.parent_path())/"index.json";
                const auto local=cache.parent_path()/"item-catalogues"/"native-items.json";
                bool nativeChecked=false;
                // Learned rows may be absent from the older complete sidecar.
                const bool currentPages=PagedItemCatalog::Current(PagedItemCatalog::File(local));
                const auto nativePage=[&](std::string next)->std::optional<Json>{
                    try{CacheBundleIO::Safe(PagedItemCatalog::File(local));
                        for(int slice=0;slice<8;++slice){
                            auto page=PagedItemCatalog::Read(PagedItemCatalog::File(local),{},query,next,limit,true,locale,snapshot.NativePlugins);
                            Json rows=Json::array();for(const auto& row:page.at("Rows")){
                                auto reference=CacheBundle::FromRow("items",row);if(reference.is_null()||!CacheBundleStore::Browsable(reference,snapshot))continue;
                                rows.push_back(std::move(reference["Data"]));
                            }
                            page["Rows"]=std::move(rows);nativeChecked=!page.value("HasMore",false);
                            if(!page["Rows"].empty()||!page.value("HasMore",false)||slice==7){
                                page["CachePreview"]=true;page["CheckingChanges"]=true;page["BundleReference"]=true;
                                page["Coverage"]="Saved presentation rows filtered by enabled sources; gameplay remains live-validated.";
                                return page;
                            }
                            next=page.at("NextCursor").get<std::string>();
                        }
                    }catch(...){}return {};
                };
                if(!prepare&&currentPages&&(cursor.starts_with("P:")||(cursor.empty()&&!std::filesystem::exists(published)&&(query.empty()||!std::filesystem::exists(local)))))
                    if(auto page=nativePage(cursor);page&&(!page->at("Rows").empty()||page->value("HasMore",false)||cursor.starts_with("P:")))return Result{key,std::move(*page)};
                const auto index=std::filesystem::exists(published)?published:std::filesystem::exists(learned)?learned:std::filesystem::exists(legacy)?legacy:local;CacheBundleIO::Safe(index);if(!std::filesystem::exists(index))return Result{key,Json{}};
                auto stamp=scope+"|"+std::to_string(std::filesystem::last_write_time(index).time_since_epoch().count())+"|"+std::to_string(std::filesystem::file_size(index));
                if(std::filesystem::exists(local))stamp+="|"+std::to_string(std::filesystem::last_write_time(local).time_since_epoch().count());
                if(index!=legacy&&std::filesystem::exists(legacy))stamp+="|learned|"+std::to_string(std::filesystem::last_write_time(legacy).time_since_epoch().count());
                if(index!=learned&&std::filesystem::exists(learned))stamp+="|observed|"+std::to_string(std::filesystem::last_write_time(learned).time_since_epoch().count());
                std::string groupingError;
                try{for(const auto& [grouping,owner]:ItemGroupings::Files(cache,root)){CacheBundleIO::Safe(grouping);if(std::filesystem::is_regular_file(grouping))stamp+="|groups|"+grouping.generic_string()+"|"+std::to_string(std::filesystem::last_write_time(grouping).time_since_epoch().count())+"|"+std::to_string(std::filesystem::file_size(grouping));}}catch(const std::exception& error){groupingError=error.what();stamp+="|groups-refused";}
                if(files->Stamp!=stamp){
                    try{if(!groupingError.empty())throw std::runtime_error(groupingError);files->Groups=ItemGroupings::Load(cache,root);files->GroupingStatus.clear();}catch(const std::exception& error){files->GroupingStatus="[RS-CACHE-002] Grouping edits were ignored; cached items remain available. "+std::string(error.what());}
                    files->NativeIndexed=false;
                    auto records=CacheBundleStore::Read(cache,root,snapshot,locale,"items");
                    if(!nativeChecked)try{
                        const auto nativeStamp=std::filesystem::last_write_time(local);
                        const auto doc=CacheBundleStore::Parse(CacheBundleIO::Read(local,64*1024*1024));const auto scope=CacheBundleStore::Parse(doc.value("Scope",std::string("{}")));
                        if(doc.value("Version",0)==1&&scope.is_array()&&scope.size()==4&&scope[0]=="item-browse-v2"&&scope[3]==locale&&doc.at("Rows").is_array()&&doc["Rows"].size()<=CacheBundle::MaxRecords){
                            if(!currentPages)try{
                                AlphabeticalItemCatalog::Cache rebuilt;rebuilt.Restore(doc["Rows"]);
                                if(std::filesystem::last_write_time(local)==nativeStamp&&!PagedItemCatalog::Current(PagedItemCatalog::File(local))){
                                    CacheBundleIO::Safe(PagedItemCatalog::File(local));PagedItemCatalog::Save(local,doc.at("Scope"),rebuilt.Snapshot());
                                    // A concurrent native refresh must leave this older sidecar stale.
                                    std::filesystem::last_write_time(PagedItemCatalog::File(local),nativeStamp);
                                }
                            }catch(...){} // Readable references remain available if the index cannot be repaired.
                            // Prepare native observations once alongside imported references, rather than reparsing disk pages for every search.
                            std::map<std::string,Json> merged;for(const auto& reference:records)merged[reference.at("Path").get<std::string>()]=reference;
                            for(const auto& row:doc["Rows"]){auto reference=CacheBundle::FromRow("items",row);if(!reference.is_null()&&CacheBundleStore::Browsable(reference,snapshot)){
                                reference["Data"]["CacheSourceVerified"]=false;reference["Data"]["ObservedCurrent"]=false;
                                const auto path=reference.at("Path").get<std::string>();if(scope[1]==snapshot.CatalogKey||!merged.contains(path)){if(merged.contains(path))reference["Data"]=ItemName::Merge(merged.at(path).at("Data"),reference.at("Data"));merged[path]=reference;}
                                else if(!ItemName::Resolved(merged.at(path).at("Data"))&&ItemName::Resolved(reference.at("Data"))){
                                    auto& data=merged.at(path).at("Data");data["Name"]=reference.at("Data").at("Name");data["DisplayName"]=data.at("Name");
                                }
                            }}
                            auto combined=Json::array();for(const auto& [path,reference]:merged)combined.push_back(reference);
                            // Each input was validated above; the path-keyed merge already guarantees unique identities.
                            if(combined.size()>CacheBundle::MaxRecords||combined.dump().size()>CacheBundle::MaxMetadata)throw std::runtime_error("[RS-CACHE-002] Combined item cache exceeds metadata bounds.");
                            records=std::move(combined);files->NativeIndexed=true;
                        }
                    }catch(...){} // Imported references still work if the local catalogue is unavailable.
                    files->Items=std::make_shared<const Index>(records,files->Groups);files->Stamp=stamp;files->PageKey.clear();
                }
                if(files->PageKey!=key){
                    files->Window=files->Items->Page(query,cursor,limit);files->PageKey=key;
                    if(!files->NativeIndexed&&!files->Window.value("HasMore",true)&&currentPages)try{
                        // Imported rows lead; saved native pages continue without a full catalogue rebuild.
                        if(files->Window.at("Rows").empty()){if(auto page=nativePage(""))files->Window=std::move(*page);}
                        else{const auto head=PagedItemCatalog::Read(PagedItemCatalog::File(local),{},"","",1,true,locale,snapshot.NativePlugins);
                            auto next=head.at("NextCursor").get<std::string>();next.replace(next.rfind(':')+1,std::string::npos,"0");
                            files->Window["NextCursor"]=next;files->Window["HasMore"]=true;}
                    }catch(...){} // References remain browsable without a compatible sidecar.
                }
                return Result{key,files->Window,files->Items,scope,files->GroupingStatus,files->NativeIndexed||!currentPages};
            }catch(...){return Result{key,Json{}};}
        });
        if(ready){Key=key;Window=Published->Page(query,cursor,limit);return Window;}
        if(Key==key&&Window.is_object()&&(!Window.at("Rows").empty()||Window.value("HasMore",false)||cursor.starts_with("P:")||cursor.starts_with("B:")))return Window;return {};
    }
    void Shutdown()noexcept{if(Work.valid())try{Work.get();}catch(...){}Key.clear();Window=Json{};Published.reset();PublishedScope.clear();Fast=false;}
};
}

