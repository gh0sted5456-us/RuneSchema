#pragma once
#include "Core/CacheBundleStore.h"
#include <future>

namespace PS::LearnedItemCatalog {
class Store {
    std::future<void> Work;
    std::map<std::string,CacheBundle::Record> Pending;
    std::string Context;
    std::function<void(std::vector<CacheBundle::Record>)> Write;
public:
    std::string Error;
    void Flush(){
        if(Work.valid()){
            if(Work.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
            try{Work.get();Error.clear();}catch(const std::exception& error){Error=error.what();}
        }
        if(Pending.empty())return;
        std::vector<CacheBundle::Record> rows;for(auto& [path,row]:Pending)rows.push_back(std::move(row));Pending.clear();
        Work=std::async(std::launch::async,[write=Write,rows=std::move(rows)]()mutable{write(std::move(rows));});
    }
    void Observe(const std::filesystem::path& cache,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const nlohmann::json& rows){
        if(!snapshot.Authoritative||snapshot.Provisional||!rows.is_array())return;
        const auto context=cache.generic_string()+snapshot.Key+locale;
        if(Context!=context){Pending.clear();Context=context;}
        const auto plan=CacheBundleStore::Plan(snapshot);
        for(const auto& data:rows){
            if(!data.value("Available",true))continue;
            const auto document=CacheBundle::FromRow("items",data);if(document.is_null())continue;
            auto row=CacheBundle::Parse(document);
            if(snapshot.SourceKeys.contains(row.Source)&&CacheBundle::Owned(row,plan,snapshot.NativePlugins)
                &&(Pending.size()<2048||Pending.contains(row.Path)))Pending.insert_or_assign(row.Path,std::move(row));
        }
        Write=[cache,snapshot,locale](auto records){CacheBundleStore::Commit(cache,cache.parent_path(),snapshot,locale,records);};Flush();
    }
    void Shutdown()noexcept{
        try{if(Work.valid())Work.get();Flush();if(Work.valid())Work.get();}catch(const std::exception& error){Error=error.what();}
    }
    ~Store(){Shutdown();}
};
}
