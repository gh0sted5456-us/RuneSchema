#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PS::RecipeBatchWork {
// A startup-scope snapshot, not a persistent raw-object cache. Missing/stale
// objects still go through the native loader, which also handles dependencies
// loaded since the snapshot. Callers retain normal type/lifetime checks.
template<class Key,class Handle> class References {
    std::unordered_map<Key,Handle> m_objects;
public:
    std::size_t Hits=0,Loads=0;
    void Remember(const Key& key,Handle handle){m_objects.insert_or_assign(key,handle);}
    template<class Load,class Validate,class Capture>
    auto Resolve(const Key& key,Load&& load,Validate&& validate,Capture&& capture) {
        if(const auto found=m_objects.find(key);found!=m_objects.end())
            if(auto* object=found->second.Get();object&&validate(object)){++Hits;return object;}
        ++Loads;
        auto* object=load();
        if(!object||!validate(object))return decltype(object){};
        Remember(key,capture(object));return object;
    }
};

// Membership of a soft-reference collection during one synchronous append
// batch. No live array address survives the batch. A foreign size/storage
// change rebuilds membership; our own append must be read back before Commit.
// In-place row edits require Invalidate(); state never survives the batch.
template<class Collection,class Item,class Hash=std::hash<Item>> class AppendIndex {
    struct Entry {const void* Data=nullptr;std::size_t Count=0;std::unordered_set<Item,Hash> Items;};
    std::unordered_map<Collection,Entry> m_entries;
public:
    std::size_t Scanned=0,Rebuilds=0;
    void Invalidate(){m_entries.clear();}
    void Clear(){m_entries.clear();Scanned=0;Rebuilds=0;}
    template<class Read>
    bool Contains(const Collection& collection,const void* data,std::size_t count,const Item& item,Read&& read) {
        auto found=m_entries.find(collection);
        if(found==m_entries.end()||found->second.Data!=data||found->second.Count!=count){
            Entry next;next.Data=data;next.Count=count;next.Items.reserve(count);
            for(std::size_t index=0;index<count;++index){next.Items.insert(read(index));++Scanned;}
            found=m_entries.insert_or_assign(collection,std::move(next)).first;++Rebuilds;
        }
        return found->second.Items.contains(item);
    }
    void CommitAppend(const Collection& collection,const void* data,std::size_t count,const Item& item) {
        auto found=m_entries.find(collection);
        if(found==m_entries.end()||count!=found->second.Count+1||found->second.Items.contains(item))
            throw std::runtime_error("Recipe collection changed outside its append batch");
        found->second.Items.insert(item);found->second.Data=data;found->second.Count=count;
    }
};

// Scoped processing-array transaction ledger. Keep the entire expected native
// sequence (including pre-existing duplicates/nulls); validate each appended
// slot immediately and compare every slot before a batch can pass. No UObject
// is dereferenced here, and no array address survives the calling load/replay.
struct ArrayStorage {const void* Data=nullptr;std::size_t Count=0;};
template<class Collection,class Item,class Hash=std::hash<Item>> class ProcessingAppends {
    struct Entry {ArrayStorage Storage;std::vector<Item> Expected;std::unordered_set<Item,Hash> Items;bool Changed=false;};
    std::unordered_map<Collection,Entry> m_entries;
    static void SameStorage(const Entry& entry,ArrayStorage actual) {
        if(actual.Data!=entry.Storage.Data||actual.Count!=entry.Expected.size())
            throw std::runtime_error("Processing recipe collection changed outside its batch");
    }
public:
    std::size_t Calls=0,Appends=0,SnapshotReads=0,VerificationReads=0;
    // Called after serialization: old row storage may already have been freed.
    bool Invalidate(){
        const bool pending=std::any_of(m_entries.begin(),m_entries.end(),[](const auto& value){return value.second.Changed;});
        m_entries.clear();return pending;
    }
    void Forget(const Collection& collection){m_entries.erase(collection);}
    template<class Read>
    bool Contains(const Collection& collection,ArrayStorage storage,const Item& item,Read&& read) {
        ++Calls;
        auto found=m_entries.find(collection);
        if(found==m_entries.end()){
            Entry next;next.Storage=storage;next.Expected.reserve(storage.Count);next.Items.reserve(storage.Count);
            for(std::size_t index=0;index<storage.Count;++index){
                const auto value=read(index);next.Expected.push_back(value);next.Items.insert(value);++SnapshotReads;
            }
            found=m_entries.emplace(collection,std::move(next)).first;
        }else SameStorage(found->second,storage);
        return found->second.Items.contains(item);
    }
    // Must be called only after the live native count and appended slot passed
    // readback. Reallocation due to this known append is expected and recorded.
    void CommitAppend(const Collection& collection,ArrayStorage storage,const Item& item) {
        auto found=m_entries.find(collection);
        if(found==m_entries.end()||storage.Count!=found->second.Expected.size()+1||found->second.Items.contains(item))
            throw std::runtime_error("Processing recipe append did not match its batch");
        found->second.Expected.push_back(item);found->second.Items.insert(item);found->second.Storage=storage;found->second.Changed=true;++Appends;
    }
    template<class Read>
    void VerifyOne(const Collection& collection,ArrayStorage storage,Read&& read) {
        const auto found=m_entries.find(collection);if(found==m_entries.end())return;
        SameStorage(found->second,storage);
        for(std::size_t index=0;index<storage.Count;++index){
            ++VerificationReads;
            if(read(index)!=found->second.Expected[index])
                throw std::runtime_error("Processing recipe batch ordered readback failed");
        }
        found->second.Changed=false;
    }
    template<class Observe,class Read>
    void VerifyAll(Observe&& observe,Read&& read) {
        for(const auto& [collection,entry]:m_entries)
            VerifyOne(collection,observe(collection),[&](std::size_t index){return read(collection,index);});
    }
};
}
