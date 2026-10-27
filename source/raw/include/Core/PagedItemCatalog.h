#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/AlphabeticalItemCatalog.h"
#include "Core/ItemCatalogFilter.h"
#include <filesystem>
#include <fstream>
#include <future>
#include <chrono>
#include <optional>
#include <windows.h>
// Presentation-only sorted disk pages. Every gameplay action still resolves a
// live asset. No Unreal, UI or mutable source state is accessed by the worker.
namespace PS::PagedItemCatalog {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline constexpr uint64_t MaxBytes=64*1024*1024;
inline fs::path File(fs::path manifest){manifest+=L".pages";return manifest;}
inline uint64_t Hash(std::string_view text){uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}return hash;}
template<class T> inline T ReadNumber(std::istream& input){T value{};input.read(reinterpret_cast<char*>(&value),sizeof(value));if(!input)throw std::runtime_error("Truncated item page index");return value;}
template<class T> inline void WriteNumber(std::ostream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof(value));}
inline size_t Offset(const std::string& cursor,uint64_t ticket){
    if(cursor.empty())return 0;const auto split=cursor.find(':',2);
    if(!cursor.starts_with("P:")||split==cursor.npos)throw AlphabeticalItemCatalog::Expired("Item page index changed");
    uint64_t identity=0;size_t offset=0;const auto parse=[](auto& value,std::string_view text){auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);return !text.empty()&&error==std::errc{}&&end==text.data()+text.size();};
    if(!parse(identity,std::string_view(cursor).substr(2,split-2))||identity!=ticket||!parse(offset,std::string_view(cursor).substr(split+1)))
        throw AlphabeticalItemCatalog::Expired("Item page index changed");return offset;
}
inline void Save(const fs::path& manifest,const std::string& scope,const std::vector<std::shared_ptr<const Json>>& rows){
    const auto file=File(manifest);auto temp=file;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    try{
        if(rows.size()>AlphabeticalItemCatalog::Cache::MaxRows||scope.size()>16384)throw std::runtime_error("Item page index exceeds bounds");
        std::vector<std::string> records;records.reserve(rows.size());uint64_t bytes=0;
        for(const auto& row:rows){auto data=row->dump();if(data.size()>32768||bytes+data.size()>MaxBytes)throw std::runtime_error("Item page data exceeds bounds");bytes+=data.size();records.push_back(std::move(data));}
        const auto header=Json{{"Version",2},{"Scope",scope},{"Count",records.size()},{"Bytes",bytes}}.dump();
        if(header.size()>32768)throw std::runtime_error("Item page header exceeds bounds");
        fs::create_directories(file.parent_path());std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        out.write("RSCAT002",8);WriteNumber(out,uint32_t(header.size()));out.write(header.data(),header.size());
        uint64_t offset=0;for(const auto& data:records){WriteNumber(out,offset);WriteNumber(out,uint32_t(data.size()));offset+=data.size();}
        for(const auto& data:records)out.write(data.data(),data.size());out.close();if(!out)throw std::runtime_error("Item pages write failed");
        if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Item pages commit failed");
    }catch(...){std::error_code ec;fs::remove(temp,ec);throw;}
}
inline Json Read(const fs::path& file,const std::string& scope,const std::string& query,const std::string& cursor,size_t limit,bool preview=false,const std::string& locale={},const std::set<std::string>& nativeProviders={}){
    if(!limit||limit>128)throw std::runtime_error("Item page limit must be 1..128");
    const auto stamp=fs::last_write_time(file);const auto size=fs::file_size(file);
    if(size<12||size>MaxBytes+1024*1024)throw std::runtime_error("Invalid item page file size");
    std::ifstream input(file,std::ios::binary);char magic[8]{};input.read(magic,8);
    if(std::string_view(magic,8)!="RSCAT002")throw std::runtime_error("Invalid item page format");
    const auto headerBytes=ReadNumber<uint32_t>(input);if(headerBytes>32768||headerBytes>size-12)throw std::runtime_error("Invalid item page header size");
    std::string text(headerBytes,'\0');input.read(text.data(),text.size());if(!input)throw std::runtime_error("Incomplete item page header");
    const auto header=Json::parse(text);if(header.at("Version")!=2)throw std::runtime_error("Item page format changed");
    const auto savedScope=header.at("Scope").get<std::string>();
    if(preview){const auto identity=Json::parse(savedScope);
        if(!identity.is_array()||identity.size()!=4||identity[0]!="item-browse-v2"||identity[3]!=locale)throw std::runtime_error("Cached preview locale or policy changed");
    }else if(savedScope!=scope)throw std::runtime_error("Item page source scope changed");
    const auto count=header.at("Count").get<uint64_t>(),bytes=header.at("Bytes").get<uint64_t>();
    if(count>AlphabeticalItemCatalog::Cache::MaxRows||bytes>MaxBytes||12+headerBytes+count*12+bytes!=size)throw std::runtime_error("Invalid item page bounds");
    const auto ticket=Hash(savedScope+"|"+std::to_string(stamp.time_since_epoch().count())+"|"+std::to_string(size));
    bool reset=false;size_t at=0;try{at=Offset(cursor,ticket);}catch(const AlphabeticalItemCatalog::Expired&){reset=true;}
    if(at>count)throw std::runtime_error("Item page offset exceeds count");
    std::vector<std::pair<uint64_t,uint32_t>> index;index.reserve(count);uint64_t expected=0;
    for(uint64_t i=0;i<count;++i){const auto offset=ReadNumber<uint64_t>(input);const auto length=ReadNumber<uint32_t>(input);
        if(offset!=expected||!length||length>32768||length>bytes-offset)throw std::runtime_error("Invalid item page record bounds");index.emplace_back(offset,length);expected+=length;}
    if(expected!=bytes)throw std::runtime_error("Incomplete item page index");
    const uint64_t start=12+headerBytes+count*12;const ItemCatalogFilter::Query match(query);Json rows=Json::array();size_t scanned=0;uint64_t readBytes=start;
    for(;at<count&&rows.size()<limit&&scanned<2048;++at,++scanned){
        const auto [offset,length]=index[at];input.seekg(start+offset);std::string data(length,'\0');input.read(data.data(),length);if(!input)throw std::runtime_error("Incomplete item page record");
        readBytes+=length;auto row=Json::parse(data);AlphabeticalItemCatalog::Cache checked;checked.Restore(Json::array({row}));
        if(!row.value("RuntimeClone",false)&&(row.contains("SourceKind")||!nativeProviders.empty())){
            const auto info=ItemSource::Reattribute(row,{},nativeProviders);
            if(info.Group==ItemSource::Kind::Vanilla||info.Group==ItemSource::Kind::Expansion)row.update(ItemSource::Fields(info));
        }
        ItemCategory::Repair(row);if(match(row))rows.push_back(std::move(row));
    }
    if(fs::file_size(file)!=size||fs::last_write_time(file)!=stamp)throw std::runtime_error("Item pages changed while reading");
    const bool filtering=at<count&&scanned==2048&&rows.size()<limit;
    Json result={{"Rows",std::move(rows)},{"HasMore",at<count},{"NextCursor","P:"+std::to_string(ticket)+":"+std::to_string(at)},
        {"AlphabeticalReady",true},{"IndexedItems",count},{"Scanned",at},{"CacheHit",true},{"PopulationStage",filtering?"filtering-cache":"cached-items"},
        {"PopulationDone",at},{"PopulationTotal",count},{"ReadRecords",scanned},{"ReadBytes",readBytes},{"Coverage","Source-verified paged item cache; live assets checked on use"}};
    if(reset){result["Reset"]=true;result["CursorRecovered"]=true;}return result;
}
class Store {
    struct Result {std::string Key,Scope;Json Window;std::string Error;};PS::ProcessExitFuture<Result> m_read;std::string m_scope,m_key,m_failed;Json m_window;
public:
    std::string Failure;
    std::optional<Json> Lookup(const fs::path& manifest,const std::string& scope,const std::string& query,const std::string& cursor,size_t limit,bool preview=false,const std::string& locale={},const std::set<std::string>& nativeProviders={}){
        if(m_scope!=scope){m_scope=scope;m_key.clear();m_failed.clear();m_window=Json{};Failure.clear();}
        if(m_read.valid()&&m_read.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=m_read.get();
            if(result.Scope==scope){m_key=result.Key;m_window=std::move(result.Window);Failure=std::move(result.Error);if(m_window.is_null())m_failed=scope;}}
        if(m_failed==scope)return {};
        const auto key=Json::array({scope,query,cursor,limit,preview,locale,nativeProviders}).dump();if(m_key==key&&!m_window.is_null())return m_window;
        if(!m_read.valid())m_read=std::async(std::launch::async,[manifest,scope,query,cursor,limit,key,preview,locale,nativeProviders]{
            try{
                auto page=Read(File(manifest),scope,query,cursor,limit,preview,locale,nativeProviders);
                const bool recovered=page.value("CursorRecovered",false);size_t records=page.value("ReadRecords",size_t{});uint64_t bytes=page.value("ReadBytes",uint64_t{});
                // A filtered query need not bounce across the host/UI boundary
                // for every empty 2048-record disk slice. Stay on this worker
                // for up to eight slices, stopping as soon as a match appears.
                for(int slices=1;!query.empty()&&slices<8&&page["Rows"].empty()&&page.value("HasMore",false);++slices){
                    page=Read(File(manifest),scope,query,page.at("NextCursor").get<std::string>(),limit,preview,locale,nativeProviders);
                    records+=page.value("ReadRecords",size_t{});bytes+=page.value("ReadBytes",uint64_t{});
                }
                page["ReadRecords"]=records;page["ReadBytes"]=bytes;
                if(recovered){page["Reset"]=true;page["CursorRecovered"]=true;}
                if(preview){page["CachePreview"]=true;page["CheckingChanges"]=true;page["PopulationStage"]="cached-items-verifying";page["Coverage"]="Saved presentation cache; mounted content verification is still pending. Gameplay actions always resolve live assets.";
                    if(page["Rows"].empty()&&!page.value("HasMore",false)){page["HasMore"]=true;page["AlphabeticalReady"]=false;}
                }
                return Result{key,scope,std::move(page)};
            }catch(const std::exception& error){return Result{key,scope,Json{},error.what()};}catch(...){return Result{key,scope,Json{},"Saved item pages could not be read"};}});
        return Json{{"Rows",Json::array()},{"HasMore",true},{"NextCursor",cursor},{"AlphabeticalReady",false},{"PopulationStage",query.empty()?"reading-catalogue":"filtering-cache"},{"Coverage",query.empty()?"Reading source-verified item cache":"Searching the verified local item catalogue on a file worker"}};
    }
    std::optional<Json> Preview(const fs::path& manifest,const std::string& locale,const std::string& query,const std::string& cursor,size_t limit,const std::set<std::string>& nativeProviders={}){return Lookup(manifest,"preview:"+locale,query,cursor,limit,true,locale,nativeProviders);}
    void Shutdown()noexcept{if(m_read.valid())try{(void)m_read.get();}catch(...){}m_scope.clear();m_key.clear();m_failed.clear();m_window=Json{};}
};
}
