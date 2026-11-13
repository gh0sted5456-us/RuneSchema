#pragma once
#include "Core/CacheBundleStore.h"
#include <future>

namespace PS::LearnedItemCatalog {
class Store {
    using Clock=std::chrono::steady_clock;
    std::future<size_t> Work;
    Clock::time_point NextWrite{};
    std::map<std::string,CacheBundle::Record> Pending;
    std::string Context;
    std::function<size_t(std::vector<CacheBundle::Record>)> Write;
public:
    std::string Error;
    bool HasWork()const{return Work.valid()||!Pending.empty();}
    bool Flush(bool force=false){
        bool changed=false;
        if(Work.valid()){
            if(Work.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
            try{changed=Work.get()!=0;Error.clear();}catch(const std::exception& error){Error=error.what();}
        }
        if(Pending.empty()||(!force&&Clock::now()<NextWrite))return changed;
        std::vector<CacheBundle::Record> rows;rows.reserve(Pending.size());
        for(auto& [path,row]:Pending)rows.push_back(std::move(row));
        Pending.clear();
        Work=std::async(std::launch::async,[write=Write,rows=std::move(rows)]()mutable{return write(std::move(rows));});
        return changed;
    }
    void Observe(const std::filesystem::path& cache,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const nlohmann::json& rows){
        if(!snapshot.Authoritative||snapshot.Provisional||!rows.is_array())return;
        const auto context=cache.generic_string()+"|"+snapshot.CatalogKey+"|"+locale;
        if(Context!=context){Pending.clear();Context=context;}
        if(Pending.empty()){
            NextWrite=Clock::now()+std::chrono::milliseconds(250);
            Write=[cache,snapshot,locale](auto records){return CacheBundleStore::Commit(cache,cache.parent_path(),snapshot,locale,records);};
        }
        const auto plan=CacheBundleStore::Plan(snapshot);
        for(const auto& data:rows){
            if(!data.value("Available",true))continue;
            const auto document=CacheBundle::FromRow("items",data);if(document.is_null())continue;
            auto row=CacheBundle::Parse(document);
            if(!snapshot.SourceKeys.contains(row.Source)||!CacheBundle::Owned(row,plan,snapshot.NativePlugins))continue;
            const auto previous=Pending.find(row.Path);
            if(previous!=Pending.end()){
                if(row.PersistenceID.empty())row.PersistenceID=previous->second.PersistenceID;
                row.Data=ItemName::Merge(previous->second.Data,row.Data);
                previous->second=std::move(row);
            }else if(Pending.size()<2048)Pending.emplace(row.Path,std::move(row));
        }
    }
    void Shutdown()noexcept{
        try{if(Work.valid())Work.get();Flush(true);if(Work.valid())Work.get();}catch(const std::exception& error){Error=error.what();}
    }
    ~Store(){Shutdown();}
};
}
