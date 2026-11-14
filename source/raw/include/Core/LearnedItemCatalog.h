#pragma once
#include "Core/CacheBundleStore.h"
#include <deque>
#include <future>

namespace PS::LearnedItemCatalog {
// Keep retries ahead of newer observations so old data cannot overwrite a later result.
class Store {
    using Clock=std::chrono::steady_clock;
    using Writer=std::function<size_t(std::vector<CacheBundle::Record>)>;
    struct Batch{std::string Context;Writer Write;std::map<std::string,CacheBundle::Record> Rows;unsigned Failures=0;};
    std::future<size_t> Work;
    Batch Running,Current;              // Running is the batch the worker holds; Current collects new observations.
    std::deque<Batch> Closed;           // Earlier contexts waiting for their own write, oldest first.
    Clock::time_point NextWrite{};
    static constexpr unsigned MaximumAttempts=5;
    static constexpr size_t MaximumClosed=4;
    // Newer observations win; an older persistence ID fills a missing one, as Observe merges.
    static void Merge(std::map<std::string,CacheBundle::Record>& into,CacheBundle::Record row){
        const auto previous=into.find(row.Path);
        if(previous==into.end()){into.emplace(row.Path,std::move(row));return;}
        if(row.PersistenceID.empty())row.PersistenceID=previous->second.PersistenceID;
        row.Data=ItemName::Merge(previous->second.Data,row.Data);previous->second=std::move(row);
    }
    void Abandon(Batch& batch){Dropped+=batch.Rows.size();batch.Rows.clear();}
    void Close(Batch batch){
        if(batch.Rows.empty())return;
        if(Closed.size()>=MaximumClosed){Abandon(Closed.front());Closed.pop_front();}
        Closed.push_back(std::move(batch));
    }
    void Failed(std::string message){
        Error=std::move(message);
        if(++Running.Failures>=MaximumAttempts){
            Abandon(Running);
            Running={};
            return;
        }
        NextWrite=Clock::now()+std::chrono::milliseconds(250u<<Running.Failures);
    }
    void Start(){
        try{
            std::vector<CacheBundle::Record> rows;
            rows.reserve(Running.Rows.size());
            for(const auto& [path,row]:Running.Rows)rows.push_back(row);
            Work=std::async(std::launch::async,[write=Running.Write,rows=std::move(rows)]()mutable{
                return write(std::move(rows));
            });
        }catch(const std::exception& error){Failed(error.what());}
    }
public:
    static constexpr size_t MaximumPending=2048;
    std::string Error;   // Last persistence failure, cleared by the next successful commit.
    size_t Dropped=0;    // Discoveries not persisted: saturation, abandoned retries or overflowing contexts.
    bool HasWork()const{return Work.valid()||!Running.Rows.empty()||!Current.Rows.empty()||!Closed.empty();}
    bool Flush(bool force=false){
        bool changed=false;
        if(Work.valid()){
            if(Work.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
            try{changed=Work.get()!=0;Error.clear();Running={};}
            catch(const std::exception& error){Failed(error.what());}
            catch(...){Failed("Learned item write failed.");}
        }
        if(!force&&Clock::now()<NextWrite)return changed;
        if(Running.Rows.empty()){
            if(!Closed.empty()){
                Running=std::move(Closed.front());
                Closed.pop_front();
            }else if(!Current.Rows.empty()){
                Running=std::move(Current);
                Current={Running.Context,{},{},0};
            }else return changed;
        }
        Start();
        return changed;
    }
    void Observe(const std::filesystem::path& cache,const HelpySourceCache::Snapshot& snapshot,const std::string& locale,const nlohmann::json& rows){
        if(!snapshot.Authoritative||snapshot.Provisional||!rows.is_array())return;
        const auto context=cache.generic_string()+"|"+snapshot.CatalogKey+"|"+locale;
        // Rows from an earlier context keep their own writer instead of being discarded.
        if(Current.Context!=context){Close(std::move(Current));Current={context,{},{},0};}
        if(Current.Rows.empty()){
            NextWrite=std::max(NextWrite,Clock::now()+std::chrono::milliseconds(250));
            Current.Write=[cache,snapshot,locale](auto records){return CacheBundleStore::Commit(cache,cache.parent_path(),snapshot,locale,records);};
        }
        const auto plan=CacheBundleStore::Plan(snapshot);
        for(const auto& data:rows){
            if(!data.value("Available",true))continue;
            const auto document=CacheBundle::FromRow("items",data);if(document.is_null())continue;
            auto row=CacheBundle::Parse(document);
            if(!snapshot.SourceKeys.contains(row.Source)||!CacheBundle::Owned(row,plan,snapshot.NativePlugins))continue;
            if(Current.Rows.size()>=MaximumPending&&!Current.Rows.contains(row.Path)){++Dropped;continue;}
            Merge(Current.Rows,std::move(row));
        }
    }
    // Drains every queued batch. A failed earlier worker no longer prevents later rows from writing.
    void Shutdown()noexcept{
        try{
            while(HasWork()){
                if(Work.valid())Work.wait();
                Flush(true);
            }
        }catch(const std::exception& error){Error=error.what();}catch(...){Error="Learned item shutdown failed.";}
    }
    ~Store(){Shutdown();}
};
}
