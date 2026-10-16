#include "Core/RecipeBatchWork.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
struct Object {bool Live=true,CorrectType=true;};
struct Handle {Object* Value=nullptr;Object* Get()const{return Value&&Value->Live?Value:nullptr;}};
int main(){
    using namespace PS::RecipeBatchWork;
    References<std::string,Handle> references;Object original,replacement,wrong;wrong.CorrectType=false;
    const auto validate=[](Object* value){return value->Live&&value->CorrectType;};
    const auto capture=[](Object* value){return Handle{value};};
    references.Remember("/A.A",Handle{&original});int loads=0;
    assert(references.Resolve("/A.A",[&]{++loads;return &replacement;},validate,capture)==&original&&loads==0);
    original.Live=false;
    assert(references.Resolve("/A.A",[&]{++loads;return &replacement;},validate,capture)==&replacement&&loads==1);
    assert(references.Resolve("/A.A",[&]{++loads;return &replacement;},validate,capture)==&replacement&&loads==1);
    assert(!references.Resolve("/B.B",[&]{++loads;return &wrong;},validate,capture));
    assert(!references.Resolve("/C.C",[]{return static_cast<Object*>(nullptr);},validate,capture));
    // A dependency appearing after the snapshot is resolved by the loader,
    // rather than incorrectly treated as absent or constructed a second time.
    assert(references.Resolve("/Late.Late",[&]{++loads;return &replacement;},validate,capture)==&replacement);
    assert(references.Hits==2&&references.Loads==4);

    const auto started=std::chrono::steady_clock::now();
    AppendIndex<std::string,std::size_t> index;std::vector<std::size_t> items{9,9,10};
    assert(index.Contains("station/category",items.data(),items.size(),9,[&](auto i){return items[i];}));
    std::size_t writes=0;
    for(std::size_t item=100;item<41507;++item){
        const auto read=[&](auto i){return items[i];};
        if(!index.Contains("station/category",items.data(),items.size(),item,read)){
            items.push_back(item);++writes;
            assert(items.back()==item);
            index.CommitAppend("station/category",items.data(),items.size(),item);
        }
        assert(index.Contains("station/category",items.data(),items.size(),item,read));
    }
    assert(writes==41407&&index.Scanned==3&&index.Rebuilds==1);
    // Same backing storage with a foreign size change is not trusted.
    items.push_back(888888);
    assert(index.Contains("station/category",items.data(),items.size(),888888,[&](auto i){return items[i];}));
    assert(index.Rebuilds==2);
    auto relocated=items;
    assert(index.Contains("station/category",relocated.data(),relocated.size(),9,[&](auto i){return relocated[i];}));
    assert(index.Rebuilds==3);
    bool rejected=false;try{index.CommitAppend("station/category",relocated.data(),relocated.size()+2,123);}catch(...){rejected=true;}assert(rejected);
    index.Clear();relocated[0]=7777777;
    assert(index.Contains("station/category",relocated.data(),relocated.size(),7777777,[&](auto i){return relocated[i];}));
    assert(index.Contains("other/category",nullptr,0,7777777,[](auto){return 0;})==false);
    relocated[0]=9999999;index.Invalidate(); // DataTable serialization, same size/address.
    assert(index.Contains("station/category",relocated.data(),relocated.size(),9999999,[&](auto i){return relocated[i];}));
    assert(!index.Contains("station/category",relocated.data(),relocated.size(),7777777,[&](auto i){return relocated[i];}));

    ProcessingAppends<int,std::size_t> processing;
    std::vector<std::size_t> native{0,9,9,10}; // Existing null/duplicate slots must be preserved.
    const auto storage=[&](int){return ArrayStorage{native.data(),native.size()};};
    const auto read=[&](int,std::size_t i){return native.at(i);};
    assert(processing.Contains(1,storage(1),9,[&](auto i){return native[i];}));
    for(std::size_t item=100;item<41507;++item){
        assert(!processing.Contains(1,storage(1),item,[&](auto i){return native[i];}));
        native.push_back(item);assert(native.back()==item);
        processing.CommitAppend(1,storage(1),item);
    }
    processing.VerifyAll(storage,read);
    assert(processing.Appends==41407&&processing.SnapshotReads==4&&processing.VerificationReads==native.size());
    const auto rejects=[](auto action){try{action();}catch(const std::runtime_error&){return true;}return false;};
    native[0]=42;assert(rejects([&]{processing.VerifyAll(storage,read);}));native[0]=0;
    std::swap(native[native.size()-1],native[native.size()-2]);assert(rejects([&]{processing.VerifyAll(storage,read);}));
    std::swap(native[native.size()-1],native[native.size()-2]);
    native.push_back(4444444);assert(rejects([&]{processing.VerifyAll(storage,read);}));native.pop_back();
    auto moved=native;native.swap(moved);assert(rejects([&]{processing.VerifyAll(storage,read);}));native.swap(moved);
    processing.VerifyOne(1,storage(1),[&](auto i){return native.at(i);});
    processing.Forget(1);native[0]=4444444; // Conservative replacement path runs outside the ledger.
    assert(processing.Contains(1,storage(1),4444444,[&](auto i){return native.at(i);}));
    assert(rejects([&]{processing.CommitAppend(1,{native.data(),native.size()+2},5555555);}));
    assert(rejects([&]{processing.CommitAppend(1,{native.data(),native.size()+1},4444444);}));
    processing.VerifyAll(storage,read);
    assert(!processing.Invalidate()); // Verified/no-op rows need no further certification.
    assert(!processing.Contains(1,storage(1),5555555,[&](auto i){return native.at(i);}));
    native.push_back(5555555);processing.CommitAppend(1,storage(1),5555555);
    assert(processing.Invalidate()); // Lost pending writes must reject native batch certification.
    processing.VerifyAll([](int)->ArrayStorage{throw std::runtime_error("stale observer invoked");},[](int,std::size_t)->std::size_t{throw std::runtime_error("stale reader invoked");});
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    std::cout<<"recipe-batch-work: PASS; 41407 category and processing appends; one initial scan plus ordered processing readback; existing nulls/duplicates, reorder/corruption, replacements, stale/foreign storage and native-load validation retained; synthetic elapsed="<<elapsed<<" ms (not in-game timing)\n";
}
