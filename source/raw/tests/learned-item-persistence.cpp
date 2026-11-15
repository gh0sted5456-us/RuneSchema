#define NOMINMAX
#include "Core/LearnedItemCatalog.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#undef assert
#define assert(condition) do { if(!(condition))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": "+#condition); } while(false)
// Learned discoveries must survive a failed write, a failed prior worker at shutdown,
// a context change before the first write, and a saturated pending queue.
using namespace PS;using Json=nlohmann::json;namespace fs=std::filesystem;
namespace {
Json Row(const std::string& name){
    return {{"Path","/Example/Items/ITEM_"+name+".ITEM_"+name},{"Name",name},{"SourceKind","modded"},{"SourceName","test-mod"}};
}
size_t Saved(const fs::path& cache,const fs::path& root,const HelpySourceCache::Snapshot& snapshot,const std::string& locale="en-US"){
    return CacheBundleStore::Read(cache,root,snapshot,locale,"items",false).size();
}
// A directory at the index path makes the atomic rename fail on Windows and POSIX alike.
fs::path BlockIndex(const fs::path& cache){const auto index=CacheBundleStore::Directory(cache,cache.parent_path())/"index.json";fs::create_directories(index);return index;}
void WaitIdle(LearnedItemCatalog::Store& store,bool expectError){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(true){store.Flush();if(expectError?!store.Error.empty():!store.HasWork())return;assert(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
}
int main(){
    const auto root=fs::temp_directory_path()/("rs-learned-persistence-"+std::to_string(GetCurrentProcessId()));fs::remove_all(root);
    HelpySourceCache::Snapshot active;active.Authoritative=true;active.Key="test";active.SourceKeys["example"]="content-v1";
    active.Owners["example"]={"test-mod"};active.DeclaredOwners=active.Owners;
    {   // 1. A failed write keeps its rows and retries once the disk recovers.
        const auto cache=root/"retry"/"cache"/"item-icons";const auto blocked=BlockIndex(cache);
        LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({Row("Retry")}));
        WaitIdle(writer,true);assert(writer.HasWork()); // The failure is reported and the row is still queued.
        fs::remove_all(blocked);WaitIdle(writer,false);assert(writer.Error.empty());
        assert(Saved(cache,root,active)==1);
    }
    {   // 2. Shutdown after a failed prior worker still drains both the failed and newer rows.
        const auto cache=root/"shutdown"/"cache"/"item-icons";const auto blocked=BlockIndex(cache);
        LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({Row("First")}));
        WaitIdle(writer,true); // The first write has failed before we add a newer observation.
        writer.Observe(cache,active,"en-US",Json::array({Row("Second")}));
        fs::remove_all(blocked);writer.Shutdown();
        assert(Saved(cache,root,active)==2);
    }
    {   // 3. A context change before the first write keeps the earlier context's rows.
        const auto cache=root/"context"/"cache"/"item-icons";
        LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({Row("English")}));
        writer.Observe(cache,active,"fr-FR",Json::array({Row("French")}));writer.Shutdown();assert(writer.Error.empty());
        assert(Saved(cache,root,active,"en-US")==1&&Saved(cache,root,active,"fr-FR")==1);
    }
#ifndef RS_BASELINE
    {   // 4. Saturation is counted, and the skipped paths persist when observed again.
        const auto cache=root/"saturation"/"cache"/"item-icons";
        LearnedItemCatalog::Store writer;auto rows=Json::array();
        for(size_t i=0;i<LearnedItemCatalog::Store::MaximumPending+2;++i)rows.push_back(Row("Bulk"+std::to_string(i)));
        writer.Observe(cache,active,"en-US",rows);assert(writer.Dropped==2);
        WaitIdle(writer,false); // A later search observes the two skipped rows again.
        writer.Observe(cache,active,"en-US",Json::array({rows[rows.size()-2],rows.back()}));writer.Shutdown();
        assert(Saved(cache,root,active)==LearnedItemCatalog::Store::MaximumPending+2);
    }
    {   // An active retry keeps its place ahead of newer rows, without growing the pending queue.
        const auto cache=root/"retry-capacity"/"cache"/"item-icons";const auto blocked=BlockIndex(cache);
        LearnedItemCatalog::Store writer;
        auto first=Row("First");first["Name"]="Old name";
        writer.Observe(cache,active,"en-US",Json::array({first}));
        WaitIdle(writer,true);
        auto rows=Json::array();
        first["Name"]="Current name";rows.push_back(first);
        for(size_t i=1;i<LearnedItemCatalog::Store::MaximumPending;++i)rows.push_back(Row("Next"+std::to_string(i)));
        writer.Observe(cache,active,"en-US",rows);
        writer.Observe(cache,active,"en-US",Json::array({Row("Dropped")}));
        assert(writer.Dropped==1);
        fs::remove_all(blocked);writer.Shutdown();
        const auto saved=CacheBundleStore::Read(cache,root,active,"en-US","items",false);
        assert(saved.size()==LearnedItemCatalog::Store::MaximumPending);
        const auto match=std::find_if(saved.begin(),saved.end(),[&](const auto& row){return row.at("Path")==first.at("Path");});
        assert(match!=saved.end()&&match->at("Data").at("Name")=="Current name");
    }
    {   // 5. A permanently failing batch is abandoned after bounded retries, never retried forever.
        const auto cache=root/"permanent"/"cache"/"item-icons";const auto blocked=BlockIndex(cache);
        LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({Row("Lost")}));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        while(writer.HasWork()){writer.Flush(true);assert(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        assert(!writer.Error.empty()&&writer.Dropped==1);fs::remove_all(blocked);
    }
    {   // 6. Re-observing a saved row queues nothing; a changed row or an external index change queues again.
        const auto cache=root/"unchanged"/"cache"/"item-icons";
        LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({Row("Seen")}));
        WaitIdle(writer,false);assert(Saved(cache,root,active)==1);
        writer.Observe(cache,active,"en-US",Json::array({Row("Seen")}));
        assert(writer.Unchanged==1&&!writer.HasWork()); // Already on disk: no batch, no commit.
        auto renamed=Row("Seen");renamed["Name"]="Seen again";
        writer.Observe(cache,active,"en-US",Json::array({renamed}));assert(writer.HasWork());WaitIdle(writer,false);
        writer.Observe(cache,active,"fr-FR",Json::array({Row("Seen")}));assert(writer.HasWork()); // A new context has no record.
        WaitIdle(writer,false);
        // Another writer replaces the index (a cache reset or icon-pack import): the record is dropped.
        writer.Observe(cache,active,"fr-FR",Json::array({Row("Seen")}));assert(!writer.HasWork());
        const auto index=CacheBundleStore::Directory(cache,cache.parent_path())/"index.json";
        fs::last_write_time(index,fs::last_write_time(index)+std::chrono::seconds(5));
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        writer.Observe(cache,active,"fr-FR",Json::array({Row("Seen")}));assert(writer.HasWork());writer.Shutdown();assert(writer.Error.empty());
    }
#endif
    fs::remove_all(root);std::cout<<"Learned discoveries survive failed writes, worker failures at shutdown, context changes and saturation\n";
}
