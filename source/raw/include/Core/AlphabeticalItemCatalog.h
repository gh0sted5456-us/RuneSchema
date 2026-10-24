#pragma once
#include "Core/ItemNameOrder.h"
#include "Core/ItemCategory.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
namespace PS::AlphabeticalItemCatalog {
using Json=nlohmann::json;
struct Expired:std::runtime_error {using std::runtime_error::runtime_error;};
class Cache {
    using Rows=std::map<std::pair<std::string,std::string>,std::shared_ptr<const Json>>;
    struct Entry {std::string Key;uint64_t Ticket=0,Used=0;size_t Scan=0;bool Ready=false;std::vector<size_t> Matches;};
    std::vector<Entry> m_entries;std::string m_scope;uint64_t m_ticket=0,m_clock=0,m_progress=0;
    Rows m_items;std::set<std::string> m_paths;std::vector<const Json*> m_order;
    Rows::const_iterator m_indexAt{};size_t m_scan=0;bool m_sourceReady=false,m_ready=false,m_persistable=true;
    static size_t Number(std::string_view text){
        size_t value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
        if(error!=std::errc{}||end!=text.data()+text.size()||text.empty())throw std::runtime_error("Invalid alphabetical catalogue cursor");return value;
    }
    void Trim(const std::string& keep){
        while(m_entries.size()>MaxQueries){
            auto oldest=m_entries.end();for(auto it=m_entries.begin();it!=m_entries.end();++it)if(it->Key!=keep&&(oldest==m_entries.end()||it->Used<oldest->Used))oldest=it;
            if(oldest==m_entries.end())throw std::runtime_error("Alphabetical query cache exceeds its safe limit");m_entries.erase(oldest);
        }
    }
    struct All {bool operator()(const Json&)const{return true;}};
public:
    static constexpr size_t MaxQueries=8,MaxRows=65536,MatchBudget=2048;
    Cache()=default;Cache(const Cache&)=delete;Cache& operator=(const Cache&)=delete;
    void Scope(const std::string& scope){if(scope!=m_scope){
        m_entries.clear();m_order.clear();m_items.clear();m_paths.clear();m_scan=0;m_sourceReady=false;m_ready=false;m_persistable=true;m_scope=scope;++m_progress;
    }}
    size_t Queries()const{return m_entries.size();}
    bool Ready()const{return m_ready;}
    bool Persistable()const{return m_ready&&m_persistable;}
    // Immutable rows may be serialized on a file worker, never Unreal objects.
    std::vector<std::shared_ptr<const Json>> Snapshot()const{
        std::vector<std::shared_ptr<const Json>> rows;if(!m_ready)return rows;
        rows.reserve(m_items.size());for(const auto& [key,row]:m_items)rows.push_back(row);return rows;
    }
    void Restore(const Json& rows){
        if(!rows.is_array()||rows.size()>MaxRows)throw std::runtime_error("Invalid item manifest row count");
        for(const auto& row:rows){
            if(!row.is_object()||row.dump().size()>32768)throw std::runtime_error("Invalid item manifest row");
            const auto path=row.at("Path").get<std::string>(),name=row.at("Name").get<std::string>();
            if(path.empty()||path.front()!='/'||path.size()>1024||path.find("..")!=path.npos||name.size()>4096||!m_paths.insert(path).second)
                throw std::runtime_error("Invalid item manifest path");
            auto repaired=row;ItemCategory::Repair(repaired);
            m_items.emplace(ItemNameOrder::Key(name,path),std::make_shared<const Json>(std::move(repaired)));
        }
        m_order.reserve(m_items.size());for(const auto& [key,row]:m_items)m_order.push_back(row.get());
        m_scan=m_items.size();m_ready=m_sourceReady=true;++m_progress;
    }
    template<class Source,class Match=All> Json Read(const std::string& key,const std::string& cursor,size_t limit,Source source,Match matches={}){
        if(!limit||limit>128)throw std::runtime_error("Alphabetical window limit must be 1..128");
        auto found=std::find_if(m_entries.begin(),m_entries.end(),[&](const auto& entry){return entry.Key==key;});
        if(found==m_entries.end()){
            if(!cursor.empty())throw Expired("Alphabetical catalogue expired; refresh the list");
            m_entries.push_back({key,++m_ticket});found=m_entries.end()-1;
        }
        auto& entry=*found;entry.Used=++m_clock;size_t offset=0;
        if(!cursor.empty()){
            const auto a=cursor.find(':',2),b=a==cursor.npos?cursor.npos:cursor.find(':',a+1);
            if(!cursor.starts_with("A:")||a==cursor.npos||b==cursor.npos||Number(std::string_view(cursor).substr(2,a-2))!=entry.Ticket)
                throw Expired("Alphabetical catalogue changed; refresh the list");
            offset=Number(std::string_view(cursor).substr(a+1,b-a-1));(void)Number(std::string_view(cursor).substr(b+1));
        }
        if(offset>MaxRows)throw std::runtime_error("Alphabetical catalogue cursor is out of range");
        if(!m_sourceReady){
            // One native index for every filter. Queries retain row indices,
            // not duplicate JSON or repeated visits to the UObject roster.
            auto slice=source(m_scan);const auto next=slice.at("NextOffset").template get<size_t>();
            m_persistable=m_persistable&&slice.value("MountedMetadataReady",true);
            for(auto& row:slice.at("Rows")){
                if(!row.is_object())continue;const auto path=row.value("Path",std::string{});if(path.empty()||!m_paths.insert(path).second)continue;
                if(m_items.size()>=MaxRows)throw std::runtime_error("Alphabetical item catalogue exceeds its safe row limit");
                const auto key=ItemNameOrder::Key(row.value("Name",path),path);
                ItemCategory::Repair(row);
                m_items.emplace(key,std::make_shared<const Json>(std::move(row)));
            }
            m_sourceReady=!slice.value("HasMore",false);
            if(!m_sourceReady&&next<=m_scan)throw std::runtime_error("Alphabetical catalogue scan stopped advancing");m_scan=next;++m_progress;
            if(m_sourceReady){m_order.reserve(m_items.size());m_indexAt=m_items.begin();}
        }
        if(m_sourceReady&&!m_ready){
            size_t indexed=0;for(;m_indexAt!=m_items.end()&&indexed<MatchBudget;++m_indexAt,++indexed)m_order.push_back(m_indexAt->second.get());
            m_ready=m_indexAt==m_items.end();if(indexed)++m_progress;
        }
        if(m_ready&&!entry.Ready){
            size_t checked=0;
            while(entry.Scan<m_order.size()&&checked<MatchBudget&&entry.Matches.size()<offset+limit+1){
                if(matches(*m_order[entry.Scan]))entry.Matches.push_back(entry.Scan);++entry.Scan;++checked;
            }
            entry.Ready=entry.Scan==m_order.size();if(checked)++m_progress;
        }
        Json rows=Json::array();size_t next=offset;
        if(m_ready){
            if(entry.Ready&&offset>entry.Matches.size())throw std::runtime_error("Alphabetical catalogue cursor is out of range");
            for(;next<entry.Matches.size()&&rows.size()<limit;++next)rows.push_back(*m_order[entry.Matches[next]]);
        }
        Json result={{"Rows",std::move(rows)},{"HasMore",!m_ready||!entry.Ready||next<entry.Matches.size()},
            {"NextCursor","A:"+std::to_string(entry.Ticket)+":"+std::to_string(next)+":"+std::to_string(m_progress)},
            {"AlphabeticalReady",m_ready},{"IndexedItems",m_items.size()},{"Scanned",m_scan}};
        Trim(key);return result;
    }
    template<class Source,class Match=All> Json ReadRecovering(const std::string& key,const std::string& cursor,size_t limit,Source source,Match matches={}){
        try{return Read(key,cursor,limit,source,matches);}
        catch(const Expired&){
            auto result=Read(key,"",limit,source,matches);result["Reset"]=true;result["CursorRecovered"]=true;return result;
        }
    }
};
}
